#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cppunit/config/SourcePrefix.h>
#include "mware/rpc/rpcservice.h"
#include "mware/rpcservercfg_test.h"
#include "utils/uri.h"
#include "utils/shmem.h"
#include "utils/absfile.h"

namespace zas{
namespace rpcsvrscfgtest{

using namespace std;
using namespace zas::utils;
using namespace zas::mware;
using namespace zas::mware::rpc;

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( rpcservercfgtest ,"rpcconfig" );

void 
rpcservercfgtest::setUp()
{
}

void 
rpcservercfgtest::tearDown()
{
}

void 
rpcservercfgtest::testrpcsvrconfig_compiler()
{
	service_mgr smgr;
	uri file("file:///zassys/etc/syssvrconfig/hostconfig.bin");
	int ret = smgr.load_config_file(file);
	CPPUNIT_ASSERT(-ENOTEXISTS == ret);
	service svc1 = smgr.add_service("zas.system", "servicebundle",
		"services/servicebundle.so", nullptr, 0, 65536, 52);
	service svc2 = smgr.add_service("zas.digdup", "dataprovider",
		"services/libdigdup_dataprovider.so", nullptr, 0, 65536, 52);
	CPPUNIT_ASSERT(NULL != svc1.get());
	// service svc2 = smgr.add_service("test", "rpc.rpcservice",
	// 	"service/rpcservice.so", nullptr, 0, 65536, 3);
	// CPPUNIT_ASSERT(NULL != svc2.get());

	// ret = svc2->add_instance("serviceinst1");
	// CPPUNIT_ASSERT(0 == ret);
	// ret = svc2->add_instance("serviceinst2");
	// CPPUNIT_ASSERT(0 == ret);
	// ret = svc2->add_instance("serviceinst3");
	// CPPUNIT_ASSERT(0 == ret);
	
	ret = smgr.save_config_file(file);
	CPPUNIT_ASSERT(0 == ret);


}

}} // end of namespace zas::utilstest