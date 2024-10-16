/** @file service.cpp
 * implementation of service callbacks for updater
 */

#include "inc/broker.h"
#include "inc/service.h"

namespace zas {
namespace coreapp {
namespace servicebundle {

using namespace zas::utils;
using namespace zas::mware::rpc;

service* updater_service::create_instance(void)
{
	auto* ret = new updater_service();
	assert(nullptr != ret);
	return ret;
}

void updater_service::destory_instance(service* s)
{
	if (nullptr != s) {
		delete s;
	}
}

updater_service::updater_service()
: _fotamgr(this)
, _sotamgr(this) {
	fota_broker_mgr::inst()->set_fotamgr(&_fotamgr);
}

updater_service::~updater_service()
{
	_looper.stop();
}

void updater_service::on_create(coroutine* cor)
{
}

void updater_service::on_start(coroutine* cor)
{
}

void updater_service::on_destroy(coroutine* cor)
{
}

int updater_service::on_bind(coroutine* cor, evlclient cli)
{
	return 0;
}
	
void updater_service::on_unbind(coroutine* cor, evlclient cli)
{
}

int updater_service::register_all(void)
{
	return 0;
}

void init_service_updater(void)
{
	printf("init updater service\n");
	rpcmgr::inst()->register_service("updater", nullptr,
		updater_service::create_instance,
		updater_service::destory_instance);
}

}}} // end of namespace zas::coreapp::services
/* EOF*/
