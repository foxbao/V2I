/** @file inc/service.h
 * definition of service callbacks for updater
 */

#ifndef ___CXX_ZAS_COREAPP_SERVICE_UPDATER_SERVICE_H__
#define ___CXX_ZAS_COREAPP_SERVICE_UPDATER_SERVICE_H__

#include "utils/thread.h"
#include "mware/rpc/rpcmgr.h"

#include "fota-mgr.h"
#include "sota-mgr.h"

using namespace zas::utils;
using namespace zas::mware::rpc;

namespace zas {
namespace coreapp {
namespace servicebundle {

class updater_service: public service
{
public:
	static service* create_instance(void);
	static void destory_instance(service* s);

	updater_service();
	~updater_service();

	// on_create()
	void on_create(coroutine* cor);

	// on_start()
	void on_start(coroutine* cor);

	// on_destroy()
	void on_destroy(coroutine* cor);

	// on_bind
	int on_bind(coroutine* cor, evlclient cli);
	
	// on_unbind
	void on_unbind(coroutine* cor, evlclient cli);

	int register_all(void);

public:
	looper_thread& get_looper(void) {
		return _looper;
	}

private:
	looper_thread _looper;
	fota_mgr _fotamgr;
	sota_mgr _sotamgr;
};

}}} // end of namespace zas::coreapp::servicebundle
#endif // ___CXX_ZAS_COREAPP_SERVICE_UPDATER_SERVICE_H__
/* EOF */
