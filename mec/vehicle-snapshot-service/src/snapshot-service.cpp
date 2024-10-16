#include "snapshot-service.h"
#include "service-worker.h"
#include "snapshot.h"
#include "kafka-sender.h"
#include "snapshot-rabbitmq.h"
#include "proto/center_junction_device_subscribe.pb.h"
#include "proto/snapshot_subscribe.pb.h"
#include "proto/upload_error_info.pb.h"

#include <sys/time.h>

#include "utils/uri.h"
#include "webcore/sysconfig.h"
#include "webcore/logger.h"
#include "device-snapshot.h"

namespace zas {
namespace vehicle_snapshot_service {

using namespace zas::utils;
using namespace zas::webcore;
using namespace jos;
using namespace vss;

enum rabbitmq_subs_type {
	rabbitmq_subs_type_unknown = 0,
	rabbitmq_subs_type_junction,
	rabbitmq_subs_type_device,
	rabbitmq_subs_type_vehicle,
};

class snapshot_rabbitmq_cb : public ss_rabbitmq_callback
{
public:
	snapshot_rabbitmq_cb(snapshot_service* svc)
	:_ss_service(svc){}
	~snapshot_rabbitmq_cb(){}
	int on_consuming(char* routingkey, size_t rsz,
		void* data, size_t dsz){
		assert(nullptr != _ss_service);
		return _ss_service->on_consuming(routingkey, rsz, data, dsz);
	}
	void on_consuming_error(int errid){
		assert(nullptr != _ss_service);
		return _ss_service->on_consuming_error(errid);
	}
private:
	snapshot_service*	_ss_service;

};

class snapshot_update_timer : public timer
{
public:
	snapshot_update_timer(timermgr* mgr, uint32_t interval,
		snapshot_service* vss_svc)
	: timer(mgr, interval)
	, _vss_svc(vss_svc) {
	}

	void on_timer(void) {
		assert(nullptr != _vss_svc);
		_vss_svc->on_timer();
		start();
	}

private:
	snapshot_service* _vss_svc;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(snapshot_update_timer);
};

class snapshot_webapp_cb : public webapp_callback
{
public:
	snapshot_webapp_cb(snapshot_service* svc)
	:_ss_service(svc){}
	~snapshot_webapp_cb(){}
	int oninit(void) {
		assert(nullptr != _ss_service);
		return _ss_service->oninit();
	}
	int onexit(void) {
		assert(nullptr != _ss_service);
		return _ss_service->onexit();
	}
private:
	snapshot_service*	_ss_service;

};

class snapshot_response : public wa_response
{
public:
	snapshot_response(snapshot_service* svc)
	: _ss_service(svc){

	}
	virtual ~snapshot_response(){}
	int on_request(void* data, size_t sz) {
		assert(nullptr != _ss_service);
		return _ss_service->on_request(data, sz);
	}
private:
	snapshot_service*	_ss_service;

};

class snapshot_request : public wa_request
{
public:
	snapshot_request(uri& url)
	: wa_request(url){}
	~snapshot_request(){}
	int on_reply(void* context, void* data, size_t sz)
	{
		printf("snapshot_request size [%ld]: %s\n", sz, (char*)data);
		return 0;
	}

};

snapshot_service::snapshot_service()
: _webapp_cb(nullptr)
, _vss_kafka_to_center(nullptr)
, _consuming_mq(nullptr)
, _rabbitmq_cb(nullptr)
, _flags(0)
, _update_timer(nullptr)
{
	_vss_kafka_to_center = new vss_kafka_sender();
}

snapshot_service::~snapshot_service()
{
	if (_vss_kafka_to_center) {
		delete _vss_kafka_to_center;
		_vss_kafka_to_center = nullptr;
	}
	if (_consuming_mq) {
		delete _consuming_mq;
		_consuming_mq = nullptr;
	}
	if (_rabbitmq_cb) {
		delete _rabbitmq_cb;
		_rabbitmq_cb = nullptr;
	}
	if (_update_timer) {
		delete _update_timer;
		_update_timer = nullptr;
	}
}

int snapshot_service::run(void)
{
	auto* wa = webapp::inst();
	wa->set_backend(service_backend::inst());
	if (!_webapp_cb) {
		_webapp_cb = new snapshot_webapp_cb(this);
		assert(nullptr != _webapp_cb);
	}
	wa->set_app_callback(_webapp_cb);
	wa->set_response_factory(this);
	wa->run(nullptr, false);
	wa->set_app_callback(nullptr);
	wa->set_response_factory(nullptr);
	return 0;
}

wa_response* snapshot_service::create_instance(void)
{
	return new snapshot_response(this);
}

void snapshot_service::destory_instance(wa_response *reply)
{
	delete reply;
}

int snapshot_service::oninit(void)
{
	auto_mutex am(_mut);
	if (service_backend::inst()->has_worker()) {
		if (!_f.init_snapshot) {
			service_backend::inst()->init_worker();
			vss_snapshot::inst()->init();
			_f.init_snapshot = 1;
			device_snapshot::inst()->init();
			init_rabbitmq();
			//　ｋａｆｋａ实时发送，不是使用定时发送
			// if (!_update_timer) {
			// 	auto* tmrmgr = service_backend::inst()->get_timermgr();
			// 	assert(nullptr != tmrmgr);
			// 	_update_timer = new snapshot_update_timer(tmrmgr, SNAPSHOT_SERVICE_TIMER_MIN_INTERVAL, this);
			// 	assert(nullptr != _update_timer);
			// 	_update_timer->start();
			// }
		}
	} else {
		if (!_f.init_kafka_center) {
			assert(nullptr != _vss_kafka_to_center);
			_vss_kafka_to_center->init();
			_f.init_kafka_center = 1;
		}
	}
	return 0;
}

int snapshot_service::onexit(void)
{
	//TODO
	return 0;
}

int snapshot_service::on_timer(void)
{
	// device_snapshot::inst()->send_to_kafka();
	// vss_snapshot::inst()->kafuka_send_snapshot();
	return 0;
}

int snapshot_service::on_request(void* data, size_t sz)
{
	auto* hdr_uri = reinterpret_cast<basicinfo_frame_uri*>(data);
	auto* hdr_ct = reinterpret_cast<basicinfo_frame_content*>((uint8_t*)data
		+ sizeof(basicinfo_frame_uri) + hdr_uri->uri_length);
	hdr_uri->uri[hdr_uri->uri_length] = '\0';
	void* udata = reinterpret_cast<void*>((hdr_ct + 1));
	size_t data_sz = sz - sizeof(basicinfo_frame_uri)
		- hdr_uri->uri_length - sizeof(basicinfo_frame_content);
	std::string req_uri(hdr_uri->uri, hdr_uri->uri_length);

	uri url(hdr_uri->uri);
	std::string vincode = url.query_value("access-token");
	std::string name;
	if (vincode.length() == 0) {
		printf("message is no id\n");
		send_error_reason(url, 1);
		return -ENOTAVAIL;
	}
	// log.e(SNAPSHOT_SNAPSHOT_TAG, "snapshot recv %s\n", hdr_uri->uri);
	if (get_urlname_from_uri(url, name)) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, "uri not find name %s\n", hdr_uri->uri);
		return -ENOTAVAIL;
	}
	if (!name.compare("/digdup/vss/v1/update")) {
		vss_snapshot::inst()->on_recv(vincode, url, udata, data_sz);
	} else if (!name.compare("/digdup/jos/v1/update")) {
		device_snapshot::inst()->on_recv(vincode, url, udata, data_sz);
	}
	return 0;
}

int snapshot_service::send_error_reason(uri &info, int reason)
{
	upload_error_info errinfo;
	timeval tv;
	gettimeofday(&tv, nullptr);
	errinfo.set_timestamp_sec(tv.tv_sec);
	errinfo.set_timestamp_usec(tv.tv_usec);
	errinfo.set_reason(reason);
	std::string data;
	errinfo.SerializeToString(&data);

	std::string cli_identity = info.query_value("cli_identity");
	std::string cli_ipaddr = info.query_value("issued_ipaddr");
	std::string cli_port = info.query_value("issued_port");
	std::string rec_time = info.query_value("start_time");
	if (cli_identity.length() == 0 || cli_ipaddr.length() == 0
		|| cli_port.length() == 0) {
		return -ENOTAVAIL;
	}
	std::string iussed_uri = "ztcp://";
	iussed_uri += cli_ipaddr + ":";
	iussed_uri += cli_port + "/digdup/jos/v1/updateerr?";
	iussed_uri += "identity=" + cli_identity;
	iussed_uri += "&start_time=" + rec_time;
	uri iussed_url(iussed_uri.c_str());
	// printf("data_collector creat ztcp req %s\n", iussed_uri.c_str());
	auto distribute = snapshot_request(iussed_url);
	distribute.send((const uint8_t*)data.c_str(), data.length());

	return 0;
}

int snapshot_service::get_urlname_from_uri(uri &url, std::string &url_name)
{
	std::string fullpath = url.get_fullpath();
	const char* name = fullpath.c_str();
	if (nullptr == name) {
		return -ENOTAVAIL;
	}
	const char* first_slash = strchr(name, '/');
	if (nullptr == first_slash) {
		return -ENOTFOUND;
	}
	size_t sz = strlen(name) - (first_slash - name);
	url_name.clear();
	url_name.append(first_slash, sz);
	return 0;
}


int snapshot_service::init_rabbitmq(void)
{
	void* ws_sock = webapp::inst()->get_context()->get_sock_context();
	if (!_consuming_mq) {
		_consuming_mq = new snapshot_rabbitmq(rabbitmq_role_consuming,
			"centermq",
			service_backend::inst(), ws_sock, service_backend::inst()->get_timermgr());
		assert(nullptr != _consuming_mq);
	}

	if (!_rabbitmq_cb) {
		_rabbitmq_cb = new snapshot_rabbitmq_cb(this);
		assert(nullptr != _rabbitmq_cb);
	}
	_consuming_mq->set_consuming_callback(_rabbitmq_cb);
	_consuming_mq->init();
	return 0;
}

int snapshot_service::on_consuming(char* routingkey, size_t rsz,
	void* data, size_t dsz)
{
	log.d(SNAPSHOT_SNAPSHOT_TAG,
		"indexing_service on_consuming %s\n", routingkey);
	bool bsubscribe = true;
	rabbitmq_subs_type type = rabbitmq_subs_type_unknown;
	if (!strncmp(routingkey, "junction_subscribe.subscribe_junction", rsz)) {
		bsubscribe = true;
		type = rabbitmq_subs_type_junction;
	} else if (!strncmp(routingkey,
		"junction_subscribe.unsubscribe_junction", rsz)) {
		bsubscribe = false;
		type = rabbitmq_subs_type_junction;
	} else if (!strncmp(routingkey,
		"device_subscribe.subscribe_device", rsz)) {
		bsubscribe = true;
		type = rabbitmq_subs_type_device;
	} else if (!strncmp(routingkey,
		"device_subscribe.unsubscribe_device", rsz)) {
		bsubscribe = false;
		type = rabbitmq_subs_type_device;
	} else if (!strncmp(routingkey,
		"vehicle_edge_subscribe.subscribe_edge_vehicle", rsz)) {
		bsubscribe = true;
		type = rabbitmq_subs_type_vehicle;
	} else if (!strncmp(routingkey,
		"vehicle_edge_subscribe.unsubscribe_edge_vehicle", rsz)) {
		bsubscribe = false;
		type = rabbitmq_subs_type_vehicle;
	} else {
		log.w(SNAPSHOT_SNAPSHOT_TAG,
			"rabit reqeust unknown reqeust  %s\n", routingkey);
		return 0;
	}
	if (rabbitmq_subs_type_junction == type) {
		junction_request req;
		req.ParseFromArray(data, dsz);
		int jun_size = req.junction_id_size();
		std::string id;
		for(int i = 0; i < jun_size; i++) {
			id = req.junction_id(i);
			log.w(SNAPSHOT_SNAPSHOT_TAG,
				"rec jucntion snapshot request vin %s, %d\n",
				id.c_str(), bsubscribe);
			device_snapshot::inst()->request_junction(id, bsubscribe);
		}
	} else if (rabbitmq_subs_type_device == type) {
		device_request req;
		req.ParseFromArray(data, dsz);
		int dev_size = req.device_id_size();
		std::string id;
		for(int i = 0; i < dev_size; i++) {
			id = req.device_id(i);
			log.w(SNAPSHOT_SNAPSHOT_TAG,
				"rec device snapshot request vin %s, %d\n",
				id.c_str(), bsubscribe);
			device_snapshot::inst()->request_device(id, bsubscribe);
		}
	} else if (rabbitmq_subs_type_vehicle == type) {
		vehicle_request req;
		req.ParseFromArray(data, dsz);
		int veh_size = req.vin_size();
		std::string id;
		for(int i = 0; i < veh_size; i++) {
			id = req.vin(i);
			log.w(SNAPSHOT_SNAPSHOT_TAG,
				"rec vehicle snapshot request vin %s, %d\n",
				id.c_str(), bsubscribe);
			vss_snapshot::inst()->request_snapshot(id, bsubscribe);
		}
	}
	return 0;
}

void snapshot_service::on_consuming_error(int errid)
{
	log.d(SNAPSHOT_SNAPSHOT_TAG,
		"snapshot_service on_consuming_error %d\n", errid);

	device_snapshot::inst()->remove_all_request();
	vss_snapshot::inst()->remove_all_request();
}

}}	//zas::vehicle_snapshot_service




