#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "servicemgr.h"
using namespace std;

namespace zas {
namespace service {

static void __attribute__ ((constructor)) zas_service_init(void)
{
	printf("rpc service is init\n");
	rpcmgr::inst()->register_service("rpc.service2", NULL,
		service_rpcfunciton::create_instance,
		service_rpcfunciton::destory_instance);
}

static void __attribute__ ((destructor)) zas_service_destroy(void)
{
	printf("rpc service is destory\n");
}

}} // end of namespace zas::utils
/* EOF */