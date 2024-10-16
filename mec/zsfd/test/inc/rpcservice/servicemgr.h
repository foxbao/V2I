#ifndef ___CXX_ZAS_RPC_DEMO_SERVICE_IMPL_H__
#define ___CXX_ZAS_RPC_DEMO_SERVICE_IMPL_H__

#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <assert.h>

#include "mware/rpc/rpcmgr.h"

using namespace std;
using namespace zas::utils;
using namespace zas::mware::rpc;

namespace zas {
namespace service {

class service_rpcfunciton: public zas::mware::rpc::service
{
public:
	static zas::mware::rpc::service* create_instance(void);
	static void destory_instance(service* s);

	// it will be called once, when service created
	void on_create(utils::coroutine* cor) {
		printf("service mgr on_create\n");
	}

	void on_start(utils::coroutine* cor) {
		printf("service mgr on_start\n");		
	}

	void on_destroy(utils::coroutine* cor) {
		printf("service mgr on_destroy\n");
	}

	int on_bind(utils::coroutine* cor, utils::evlclient cli) {
		printf("service mgr on_bind\n");
	}

	void on_unbind(utils::coroutine* cor, utils::evlclient cli) {
		printf("service mgr on_unbind\n");
	}

	int register_all(void);
private:

};

}} // end of namespace zas::utils
/* EOF */

#endif