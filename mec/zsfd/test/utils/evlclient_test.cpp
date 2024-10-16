#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cppunit/config/SourcePrefix.h>
#include "utils/evlclient_test.h"



namespace zas{
namespace utilstest{

using namespace std;

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( evlclienttest ,"alltest" );


void 
evlclienttest::setUp()
{
    _testclientdata = NULL;
}

void 
evlclienttest::tearDown()
{
    if (_testclientdata)
        free(_testclientdata);
}

void 
evlclienttest::init_evl_client_info(std::string clientname,
    std::string instname, int pid)
{
    // int clientsz = clientname.length() + 1;
    // int instsz = instname.length() + 1;
    // _testclientdata = (char*)malloc(
    //         sizeof(evl_bctl_client_info) + clientsz + instsz);
    // evl_bctl_client_info *info = (evl_bctl_client_info*)_testclientdata;
    // info->client_name = 0;
    // info->inst_name = clientname.length() + 1;
    // memset(info->buf, 0, (clientsz + instsz));
    // strcpy(info->buf + info->client_name, clientname.c_str());
    // strcpy(info->buf + info->inst_name, instname.c_str());
    // info->pid = pid;
    // info->flags.type = CLINFO_TYPE_CLIENT;
    // info->validity.m.client_name = 1;
    // info->validity.m.instance_name = 1;
    // info->validity.m.client_pid = 1;
    // info->validity.m.flags = 1;
}

void 
evlclienttest::release_evl_client_info(void)
{
    if (_testclientdata)
        free(_testclientdata);
    _testclientdata = NULL;
}

bool 
evlclienttest::checkclientinfo(std::string clientname, std::string instname)
{
    // evlclient_impl *cli = evlclient_impl::getclient(clientname.c_str(),
    //     instname.c_str());
    // if (cli != NULL) {
    //     printf("client %s, inst %s, pid %d\n", 
    //         clientname.c_str(), instname.c_str(),cli->getpid());
    //     cli->release();
    //     return true;
    // }
    // return false;
}

void 
evlclienttest::testclientlist()
{
    // evlclient_impl* srvcli1 = NULL;
    // evlclient_impl* srvcli2 = NULL;
    // evlclient_impl* srvcli3 = NULL;
    // evlclient_impl* srvcli4 = NULL;
    // evlclient_impl* hostcli1 = NULL;
    // evlclient_impl* hostcli2 = NULL;
    // evlclient_impl* hostcli3 = NULL;
    // evlclient_impl* hostcli4 = NULL;
    
    // srvcli1 = evlclient_impl::create(-1, client_type_unix_socket);
    // CPPUNIT_ASSERT(srvcli1 != NULL);
    // srvcli2 = evlclient_impl::create(-1, client_type_unix_socket);
    // CPPUNIT_ASSERT(srvcli2 != NULL);
    // srvcli3 = evlclient_impl::create(-1, client_type_unix_socket);
    // CPPUNIT_ASSERT(srvcli3 != NULL);
    // srvcli4 = evlclient_impl::create(-1, client_type_unix_socket);
    // CPPUNIT_ASSERT(srvcli4 != NULL);
    // hostcli1 = evlclient_impl::create(-1, client_type_unix_socket);
    // CPPUNIT_ASSERT(hostcli1 != NULL);
    // hostcli2 = evlclient_impl::create(-1, client_type_unix_socket);
    // CPPUNIT_ASSERT(hostcli2 != NULL);
    // hostcli3 = evlclient_impl::create(-1, client_type_unix_socket);
    // CPPUNIT_ASSERT(hostcli3 != NULL);
    // hostcli4 = evlclient_impl::create(-1, client_type_unix_socket);
    // CPPUNIT_ASSERT(hostcli4 != NULL);

    // clinfo_validity_u val;
    // init_evl_client_info("sysserver", "client1", 200);
    // srvcli1->update_info((evl_bctl_client_info*)_testclientdata, val);
    // release_evl_client_info();
    // init_evl_client_info("sysserver", "client2", 300);
    // srvcli2->update_info((evl_bctl_client_info*)_testclientdata, val);
    // release_evl_client_info();
    // CPPUNIT_ASSERT(checkclientinfo("sysserver", "client1"));
    // CPPUNIT_ASSERT(checkclientinfo("sysserver", "client2"));
    // CPPUNIT_ASSERT(!checkclientinfo("sysserver", "client3"));
    // CPPUNIT_ASSERT(!checkclientinfo("sysserver", "client4"));

    // srvcli1->release();
    // srvcli1 = NULL;
    // CPPUNIT_ASSERT(!checkclientinfo("sysserver", "client1"));
    // CPPUNIT_ASSERT(checkclientinfo("sysserver", "client2"));
    // CPPUNIT_ASSERT(!checkclientinfo("sysserver", "client3"));
    // CPPUNIT_ASSERT(!checkclientinfo("sysserver", "client4"));

    // init_evl_client_info("sysserver", "client3", 400);
    // srvcli3->update_info((evl_bctl_client_info*)_testclientdata, val);
    // release_evl_client_info();
    // init_evl_client_info("sysserver", "client4", 500);
    // srvcli4->update_info((evl_bctl_client_info*)_testclientdata, val);
    // release_evl_client_info();
    // CPPUNIT_ASSERT(!checkclientinfo("sysserver", "client1"));
    // CPPUNIT_ASSERT(checkclientinfo("sysserver", "client2"));
    // CPPUNIT_ASSERT(checkclientinfo("sysserver", "client3"));
    // CPPUNIT_ASSERT(checkclientinfo("sysserver", "client4"));

    // init_evl_client_info("hostserver", "client1", 600);
    // hostcli1->update_info((evl_bctl_client_info*)_testclientdata, val);
    // release_evl_client_info();
    // init_evl_client_info("hostserver", "client2", 700);
    // hostcli2->update_info((evl_bctl_client_info*)_testclientdata, val);
    // release_evl_client_info();
    // init_evl_client_info("hostserver", "client3", 800);
    // hostcli3->update_info((evl_bctl_client_info*)_testclientdata, val);
    // release_evl_client_info();
    // init_evl_client_info("hostserver", "client4", 900);
    // hostcli4->update_info((evl_bctl_client_info*)_testclientdata, val);
    // release_evl_client_info();
    // CPPUNIT_ASSERT(checkclientinfo("hostserver", "client1"));
    // CPPUNIT_ASSERT(checkclientinfo("hostserver", "client2"));
    // CPPUNIT_ASSERT(checkclientinfo("hostserver", "client3"));
    // CPPUNIT_ASSERT(checkclientinfo("hostserver", "client4"));

    // hostcli1->release();
    // hostcli1 = NULL;
    // CPPUNIT_ASSERT(!checkclientinfo("hostserver", "client1"));
    // CPPUNIT_ASSERT(checkclientinfo("hostserver", "client2"));
    // CPPUNIT_ASSERT(checkclientinfo("hostserver", "client3"));
    // CPPUNIT_ASSERT(checkclientinfo("hostserver", "client4"));

    // srvcli2->release();
    // srvcli2 = NULL;
    // srvcli3->release();
    // srvcli3 = NULL;
    // srvcli4->release();
    // srvcli4 = NULL;
    // hostcli4->release();
    // hostcli4 = NULL;
    // hostcli2->release();
    // hostcli2 = NULL;
    // hostcli3->release();
    // hostcli3 = NULL;
    // CPPUNIT_ASSERT(!checkclientinfo("hostserver", "client1"));
    // CPPUNIT_ASSERT(!checkclientinfo("hostserver", "client2"));
    // CPPUNIT_ASSERT(!checkclientinfo("hostserver", "client3"));
    // CPPUNIT_ASSERT(!checkclientinfo("hostserver", "client4"));  
    // CPPUNIT_ASSERT(!checkclientinfo("sysserver", "client1"));
    // CPPUNIT_ASSERT(!checkclientinfo("sysserver", "client2"));
    // CPPUNIT_ASSERT(!checkclientinfo("sysserver", "client3"));
    // CPPUNIT_ASSERT(!checkclientinfo("sysserver", "client4"));      
}

}} // end of namespace zas::utilstest