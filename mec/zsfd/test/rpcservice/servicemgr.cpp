#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <assert.h>

#include "servicemgr.h"
#include "mware/rpc/rpcmgr.h"
#include "utils/mutex.h"
#include "mytest.h"

namespace zas {
namespace service {

using namespace std;
using namespace zas::utils;
using namespace zas::mware::rpc;

static zas::utils::mutex servciemgr;

int service_rpcfunciton::register_all(void) {
	printf("rpcserver rgister_all \n");
	mytest::skeleton::import_myIF1();
	mytest::skeleton::import_myIF3_singleton();
}

zas::mware::rpc::service* service_rpcfunciton::create_instance()
{
	auto_mutex am(servciemgr);
	return new service_rpcfunciton();;
}

void service_rpcfunciton::destory_instance(service* s)
{
	auto_mutex am(servciemgr);
	delete s;
}


}} // end of namespace zas::utils
/* EOF */