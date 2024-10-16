#include "load-balance-service.h"

#include "load-balance-def.h"
#include "forward.h"
#include "forward_arbitrate.h"
#include "client-mgr.h"

#include "utils/uri.h"
#include "webcore/sysconfig.h"
#include "webcore/logger.h"

namespace zas {
namespace load_balance {

using namespace zas::utils;
using namespace zas::webcore;

class load_balance_webapp_cb : public webapp_callback
{
public:
	load_balance_webapp_cb(lbservice* lbs)
	:_lbs(lbs){}
	~load_balance_webapp_cb(){}
	int oninit(void) {
		assert(nullptr != _lbs);
		return _lbs->oninit();
	}
	int onexit(void) {
		return _lbs->onexit();
	}
private:
	lbservice*	_lbs;

};

class load_balance_response : public wa_response
{
public:
	load_balance_response(lbservice* lbs)
	: _lbs(lbs){

	}
	virtual ~load_balance_response(){}
	int on_request(void* data, size_t sz) {
		assert(nullptr != _lbs);
		return _lbs->on_request(data, sz);
	}
private:
	lbservice*	_lbs;

};

lbservice::lbservice()
: _webapp_cb(nullptr)
, _forward_mgr(nullptr)
{
	
}

lbservice::~lbservice()
{
	if (_webapp_cb) {
		delete _webapp_cb;
		_webapp_cb = nullptr;
	}
	if (_forward_mgr) {
		delete _forward_mgr;
		_forward_mgr = nullptr;
	}
}

int lbservice::init_forward_arbitrate(void)
{
	if (!_forward_mgr) {
		_forward_mgr = new forward_arbitrate();
		assert(nullptr != _forward_mgr);
	}
	return _forward_mgr->init();
}

int lbservice::init_service_mgr(void)
{
	client_mgr::inst()->init();
	return 0;
}

int lbservice::run(void)
{
	auto* wa = webapp::inst();
	wa->set_backend(forward_backend::inst());
	if (!_webapp_cb) {
		_webapp_cb = new load_balance_webapp_cb(this);
		assert(nullptr != _webapp_cb);
	}
	wa->set_app_callback(_webapp_cb);
	wa->set_response_factory(this);
	wa->run(nullptr, false);
	wa->set_app_callback(nullptr);
	wa->set_response_factory(nullptr);
	return 0;
}

wa_response* lbservice::create_instance(void)
{
	return new load_balance_response(this);
}

void lbservice::destory_instance(wa_response *reply)
{
	delete reply;
}

int lbservice::oninit(void)
{
	init_service_mgr();

	init_forward_arbitrate();
	log.d(LOAD_BALANCE_MGR_TAG, 
		"forward init forward finished\n");
	
	return 0;
}

int lbservice::onexit(void)
{
	//TODO
	return 0;
}

int lbservice::on_request(void* data, size_t sz)
{
	log.e(LOAD_BALANCE_MGR_TAG, 
		"lbservice on_request %d\n", sz);

	return 0;
}

}}	//zas::vehicle_load_balance




