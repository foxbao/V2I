#include "fusion-service.h"
#include "service-worker.h"
#include "fusion.h"

#include "utils/uri.h"
#include "webcore/sysconfig.h"
#include "webcore/logger.h"

namespace zas {
namespace fusion_service {

using namespace zas::utils;
using namespace zas::webcore;

class fusion_webapp_cb : public webapp_callback
{
public:
	fusion_webapp_cb(device_fusion_service* svc)
	:_device_fusion_service(svc){}
	~fusion_webapp_cb(){}
	int oninit(void) {
		assert(nullptr != _device_fusion_service);
		return _device_fusion_service->oninit();
	}
	int onexit(void) {
		assert(nullptr != _device_fusion_service);
		return _device_fusion_service->onexit();
	}
private:
	device_fusion_service*	_device_fusion_service;

};

class fusion_response : public wa_response
{
public:
	fusion_response(device_fusion_service* svc)
	: _device_fusion_service(svc){

	}
	virtual ~fusion_response(){}
	int on_request(void* data, size_t sz) {
		assert(nullptr != _device_fusion_service);
		return _device_fusion_service->on_request(this, data, sz);
	}
private:
	device_fusion_service*	_device_fusion_service;

};

device_fusion_service::device_fusion_service()
: _webapp_cb(nullptr)
, _device_fusion(nullptr)
, _flags(0)
{
	_device_fusion = new device_fusion();
}

device_fusion_service::~device_fusion_service()
{
	if (_device_fusion) {
		delete _device_fusion;
	}
}

int device_fusion_service::run(void)
{
	auto* wa = webapp::inst();
	wa->set_backend(service_backend::inst());
	if (!_webapp_cb) {
		_webapp_cb = new fusion_webapp_cb(this);
		assert(nullptr != _webapp_cb);
	}
	wa->set_app_callback(_webapp_cb);
	wa->set_response_factory(this);
	wa->run(nullptr, false);
	wa->set_app_callback(nullptr);
	wa->set_response_factory(nullptr);
	return 0;
}

wa_response* device_fusion_service::create_instance(void)
{
	return new fusion_response(this);
}

void device_fusion_service::destory_instance(wa_response *reply)
{
	delete reply;
}

int device_fusion_service::oninit(void)
{

	return 0;
}

int device_fusion_service::onexit(void)
{
	//TODO
	return 0;
}

int device_fusion_service::on_request(wa_response *wa_rep,
	void* data, size_t sz)
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
	std::string vincode = url.query_value("uid");
	// printf("uri is %s\n", hdr_uri->uri);
	if (_device_fusion) {
		_device_fusion->on_recv(wa_rep, vincode, udata, data_sz);
	}
	return 0;
}


}}	//zas::vehicle_device_fusion_service




