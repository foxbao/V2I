#include "indexing-service.h"

#include "current-snapshot.h"
#include "forward.h"
#include "forward_arbitrate.h"
#include "indexing-def.h"
#include "indexing-rabbitmq.h"
#include "vehicle-mgr.h"
#include "proto/snapshot_subscribe.pb.h"

#include "utils/uri.h"
#include "webcore/sysconfig.h"
#include "webcore/logger.h"

namespace zas {
namespace vehicle_indexing {

using namespace zas::utils;
using namespace zas::webcore;
using namespace vss;

class indexing_rabbitmq_cb : public rabbitmq_callback
{
public:
	indexing_rabbitmq_cb(indexing_service* svc)
	:_indexing_svc(svc){}
	~indexing_rabbitmq_cb(){}
	int on_consuming(char* routingkey, size_t rsz,
		void* data, size_t dsz){
		assert(nullptr != _indexing_svc);
		return _indexing_svc->on_consuming(routingkey, rsz, data, dsz);
	}
	void on_consuming_error(int errid){
		assert(nullptr != _indexing_svc);
		return _indexing_svc->on_consuming_error(errid);
	}
private:
	indexing_service*	_indexing_svc;

};

class indexing_webapp_cb : public webapp_callback
{
public:
	indexing_webapp_cb(indexing_service* svc)
	:_indexing_svc(svc){}
	~indexing_webapp_cb(){}
	int oninit(void) {
		assert(nullptr != _indexing_svc);
		return _indexing_svc->oninit();
	}
	int onexit(void) {
		return _indexing_svc->onexit();
	}
private:
	indexing_service*	_indexing_svc;

};

class indexing_response : public wa_response
{
public:
	indexing_response(indexing_service* svc)
	: _indexing_svc(svc){

	}
	virtual ~indexing_response(){}
	int on_request(void* data, size_t sz) {
		assert(nullptr != _indexing_svc);
		return _indexing_svc->on_request(data, sz);
	}
private:
	indexing_service*	_indexing_svc;

};


indexing_service::indexing_service()
: _current_snapshot(nullptr)
, _consuming_mq(nullptr)
, _rabbitmq_cb(nullptr)
, _webapp_cb(nullptr)
, _forward_mgr(nullptr)
{
	
}

indexing_service::~indexing_service()
{
	if (_current_snapshot) {
		delete _current_snapshot;
		_current_snapshot = nullptr;
	}
	if (_consuming_mq) {
		delete _consuming_mq;
		_consuming_mq = nullptr;
	}
	if (_rabbitmq_cb) {
		delete _rabbitmq_cb;
		_rabbitmq_cb = nullptr;
	}

	if (_webapp_cb) {
		delete _webapp_cb;
		_webapp_cb = nullptr;
	}
	if (_forward_mgr) {
		delete _forward_mgr;
		_forward_mgr = nullptr;
	}

}

int indexing_service::init_current_snapshot(void)
{
	if (!_current_snapshot) {
		void* ws_sock = webapp::inst()->get_context()->get_sock_context();
		_current_snapshot = new current_snapshot(forward_backend::inst(),
			 ws_sock);
		assert(nullptr != _current_snapshot);
	}
	_current_snapshot->init();
	return 0;
}

int indexing_service::init_rabbitmq(void)
{
	void* ws_sock = webapp::inst()->get_context()->get_sock_context();
	if (!_consuming_mq) {
		_consuming_mq = new indexing_rabbitmq(rabbitmq_role_consuming,
			"centermq",
			forward_backend::inst(), ws_sock,
			forward_backend::inst()->get_timermgr(ws_sock));
		assert(nullptr != _consuming_mq);
	}

	if (!_rabbitmq_cb) {
		_rabbitmq_cb = new indexing_rabbitmq_cb(this);
		assert(nullptr != _rabbitmq_cb);
	}
	_consuming_mq->set_consuming_callback(_rabbitmq_cb);
	_consuming_mq->init();
	return 0;
}

int indexing_service::init_forward_arbitrate(void)
{
	if (!_forward_mgr) {
		_forward_mgr = new forward_arbitrate();
		assert(nullptr != _forward_mgr);
	}
	return _forward_mgr->init();
}

int indexing_service::init_vehicle_mgr(void)
{
	return 0;
}

int indexing_service::run(void)
{
	auto* wa = webapp::inst();
	wa->set_backend(forward_backend::inst());
	if (!_webapp_cb) {
		_webapp_cb = new indexing_webapp_cb(this);
		assert(nullptr != _webapp_cb);
	}
	wa->set_app_callback(_webapp_cb);
	wa->set_response_factory(this);
	wa->run(nullptr, false);
	wa->set_app_callback(nullptr);
	wa->set_response_factory(nullptr);
	return 0;
}

wa_response* indexing_service::create_instance(void)
{
	return new indexing_response(this);
}

void indexing_service::destory_instance(wa_response *reply)
{
	delete reply;
}

int indexing_service::oninit(void)
{
	init_vehicle_mgr();
	//todo: 
	init_forward_arbitrate();
	log.d(INDEXING_SNAPSHOT_TAG, 
		"forward init forward finished\n");
	
	//todo: handle return value
	init_current_snapshot();
	log.d(INDEXING_SNAPSHOT_TAG, 
		"forward init snapshot finished\n");
	
	//toddo: handle return value;
	init_rabbitmq();
	log.d(INDEXING_SNAPSHOT_TAG, 
		"forward init rabbitmq finished\n");
	return 0;
}

int indexing_service::onexit(void)
{
	//TODO
	return 0;
}

int indexing_service::on_consuming(char* routingkey, size_t rsz,
	void* data, size_t dsz)
{
	log.d(INDEXING_SNAPSHOT_TAG,
		"indexing_service on_consuming %s\n", routingkey);
	bool bsubscribe = true;
	if (!strncmp(routingkey, "vehicle_subscribe.subscribe_vehicle", rsz)) {
		bsubscribe = true;
	} else if (!strncmp(routingkey,
		"vehicle_subscribe.unsubscribe_vehicle", rsz)) {
		bsubscribe = false;
	} else {
		log.w(INDEXING_SNAPSHOT_TAG,
			"rabit reqeust unknown reqeust  %s\n", routingkey);
		return 0;
	}

	assert(nullptr != _current_snapshot);
	vehicle_request req;
	req.ParseFromArray(data, dsz);
	int vidsz = req.vin_size();
	std::string vid;
	for(int i = 0; i < vidsz; i++) {
		vid = req.vin(i);
		log.w(INDEXING_SNAPSHOT_TAG,
			"rec request vin %s, %d\n", vid.c_str(), bsubscribe);
		_current_snapshot->request_snapshot(vid, bsubscribe);
	}
	return 0;
}

int indexing_service::on_request(void* data, size_t sz)
{
	auto* hdr_uri = reinterpret_cast<basicinfo_frame_uri*>(data);
	auto* hdr_ct = reinterpret_cast<basicinfo_frame_content*>((uint8_t*)data
		+ sizeof(basicinfo_frame_uri) + hdr_uri->uri_length);
	hdr_uri->uri[hdr_uri->uri_length] = '\0';
	void* udata = reinterpret_cast<void*>((hdr_ct + 1));
	size_t data_sz = sz - sizeof(basicinfo_frame_uri)
		- hdr_uri->uri_length - sizeof(basicinfo_frame_content);
	assert(nullptr != _forward_mgr);

	uri url(hdr_uri->uri);
	std::string svc_name = "snapshot";
	auto* service = _forward_mgr->find_service(url, svc_name);
	if (service && (service->type == 0 
		|| (service_type_internal & service->type))) {
		assert(nullptr != _current_snapshot);
		return _current_snapshot->on_recv(service->keyword, 
				url, udata, data_sz);
	}
	svc_name = "register";
	service = _forward_mgr->find_service(url, svc_name);
	if (service && (service->type == 0 
		|| (service_type_internal & service->type))) {
		return vehicle_mgr::inst()->vehicle_register(service->keyword, 
				url, udata, data_sz);
	}

	return 0;
}

void indexing_service::on_consuming_error(int errid)
{
	log.d(INDEXING_SNAPSHOT_TAG,
		"indexing_service on_consuming_error %d\n", errid);
	if (_current_snapshot) {
		_current_snapshot->remove_all_request();
	}
}

}}	//zas::vehicle_indexing




