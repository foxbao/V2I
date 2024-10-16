#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cppunit/config/SourcePrefix.h>
#include "utils/datapool.h"
#include "utils/datapool_test.h"
#include "utils/evloop.h"
#include "utils/timer.h"

namespace zas{
namespace utilstest{

using namespace std;
using namespace zas::utils;

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( datapooltest ,"datapooltest" );

const int buff_sz = 512;

class testlisner : public datapool_listener
{
    void on_notify(void* data, size_t sz)
    {
        char* testdata = (char*)data;
        if (NULL == testdata || 0 == sz) {
            printf("listner no data %d, %d\n", testdata, sz);
            return;
        }
        CPPUNIT_ASSERT(0 == strcmp("this is listner test", testdata));
        printf("listner noitfy %s, %d\n", testdata, sz);
    }
};

void 
datapooltest::setUp()
{
    evloop *evl = evloop::inst();
    evl->setrole(evloop_role_client);

    // set the name of syssvr as "zas.genesis.syssvr"
    evl->updateinfo(evlcli_info_client_name, "zas.system.daemon1")
        ->updateinfo(evlcli_info_instance_name, "sysds")
        ->updateinfo(evlcli_info_commit);
    
    evl->start(true);
    // evloop *evl = evloop::inst();
    // evl->setrole(evloop_role_server);

    // evl->updateinfo(evlcli_info_client_name, "zas.genesis")
    //     ->updateinfo(evlcli_info_instance_name, "syssvr")
    //     ->updateinfo(evlcli_info_commit);

    // // start the system server
    // evl->start(true);
	pdata = new char[512];
	pdata2 = new char[512];
    msleep(500);
}

void 
datapooltest::tearDown()
{
    delete pdata;
    delete pdata2;
}

void testnoify(void* owner, void* data, size_t sz)
{
    int testowner = *(int*)owner;
    char* testdata = (char*)data;
    CPPUNIT_ASSERT(0 == strncmp("this is noitfy test", testdata, sz));
    CPPUNIT_ASSERT(10 == testowner);
    printf("testoitfy %s, %d\n", testdata, sz);
}

void 
datapooltest::localdatapooltest()
{
    datapool *dp = datapool::inst();
    datapool_element enode1 = dp->create_element("testelement_1", false, false);
    datapool_element enode2 = dp->create_element("testelement_2", false, false);
    datapool_element enode3 = dp->create_element("testelement_3", false, false);
    datapool_element enode4 = dp->create_element("testelement_4", false, false);
    datapool_element enode5 = dp->create_element("testelement_5", false, false);
    datapool_element enode6 = dp->create_element("testelement_5", false, false);

    CPPUNIT_ASSERT(enode1.get());
    CPPUNIT_ASSERT(enode2.get());
    CPPUNIT_ASSERT(enode3.get());
    CPPUNIT_ASSERT(enode4.get());
    CPPUNIT_ASSERT(enode5.get());
    CPPUNIT_ASSERT(NULL == enode6.get());

    datapool_element enode7 = dp->get_element("testelement_4", false);
    CPPUNIT_ASSERT(enode7.get());

    int ret = dp->remove_element("testelement_4", false);
    CPPUNIT_ASSERT(0 == ret);
    enode6 = dp->get_element("testelement_4", false);
    CPPUNIT_ASSERT(NULL == enode6.get());
    ret = dp->remove_element("testelement_4", false);
    CPPUNIT_ASSERT(0 != ret);
    ret = enode4->setdata("test");
    CPPUNIT_ASSERT(0 != ret);

    enode1->setdata("this is test 1");
    enode7 = dp->get_element("testelement_1",false);
    std::string strdata1;
    std::string strdata2;
    ret = enode7->getdata(strdata1);
    CPPUNIT_ASSERT(strdata1.length() == ret);
    ret = enode1->getdata(strdata2);
    CPPUNIT_ASSERT(strdata2.length() == ret);
    CPPUNIT_ASSERT(strdata1 == strdata2);

    char data[] = {'A','b',0x00,'c','d'};
    enode1->setdata(data, 5);
    ret = enode7->getdata(strdata1);
    CPPUNIT_ASSERT(strdata1.length() == ret);
    ret = enode1->getdata(strdata2);
    CPPUNIT_ASSERT(strdata2.length() == ret);
    CPPUNIT_ASSERT(5 == ret);
    CPPUNIT_ASSERT('d' == strdata2[4]);
    CPPUNIT_ASSERT(strdata1 == strdata2);

    int noitfyid = 10;
    enode1->add_listener(testnoify, (void*)(&noitfyid));
    enode1->notify("this is noitfy test");
    ret = enode1->getdata(strdata2);
    CPPUNIT_ASSERT(strdata2.length() == ret);
    CPPUNIT_ASSERT(0 == strcmp("this is noitfy test", strdata2.c_str()));
    ret = enode1->remove_listener(testnoify, (void*)(&noitfyid));
    CPPUNIT_ASSERT(0 == ret);

    testlisner testobj;
    enode7->add_listener(&testobj);
    enode7->notify("this is listner test");
    ret = enode1->getdata(strdata2);
    CPPUNIT_ASSERT(0 == strcmp("this is listner test", strdata2.c_str()));
}

void 
datapooltest::globaldatapooltest()
{
    datapool *dp = datapool::inst();
    datapool_element enode1 = dp->create_element("testelement_1", false, true);
    datapool_element enode2 = dp->create_element("testelement_2", false, true);
    datapool_element enode3 = dp->create_element("testelement_3", false, true);
    datapool_element enode4 = dp->create_element("testelement_4", false, true);
    datapool_element enode5 = dp->create_element("testelement_5", false, true);
    datapool_element enode6 = dp->create_element("testelement_5", false, false);
    datapool_element enode10 = dp->create_element("testelement_5", true, false);
    datapool_element enode11 = dp->create_element("testelement_5", true, true);

    CPPUNIT_ASSERT(enode1.get());
    CPPUNIT_ASSERT(enode2.get());
    CPPUNIT_ASSERT(enode3.get());
    CPPUNIT_ASSERT(enode4.get());
    CPPUNIT_ASSERT(enode5.get());
    CPPUNIT_ASSERT(NULL == enode6.get());
    CPPUNIT_ASSERT(enode10.get());
    CPPUNIT_ASSERT(NULL == enode11.get());

    datapool_element enode7 = dp->get_element("testelement_4", false);
    CPPUNIT_ASSERT(enode7.get());

    int ret = dp->remove_element("testelement_4", false);
    CPPUNIT_ASSERT(0 == ret);
    enode6 = dp->get_element("testelement_4", false);
    CPPUNIT_ASSERT(NULL == enode6.get());
    ret = dp->remove_element("testelement_4", false);
    CPPUNIT_ASSERT(0 != ret);
    ret = enode4->setdata("test");
    CPPUNIT_ASSERT(0 != ret);

    std::string strdata1;
    strdata1 = "this is test 1";
    enode1->setdata(strdata1);
    enode7 = dp->get_element("testelement_1",false);
    std::string strdata2;
    ret = enode7->getdata(strdata1);
    CPPUNIT_ASSERT(strdata1.length() == ret);
    ret = enode1->getdata(strdata2);
    CPPUNIT_ASSERT(strdata2.length() == ret);
    CPPUNIT_ASSERT(strdata1 == strdata2);

    char data[] = {'A','b',0x00,'c','d'};
    enode1->setdata(data, 5);
    ret = enode7->getdata(strdata1);
    CPPUNIT_ASSERT(strdata1.length() == ret);
    ret = enode1->getdata(strdata2);
    CPPUNIT_ASSERT(strdata2.length() == ret);
    CPPUNIT_ASSERT('c' == strdata2[3]);
    CPPUNIT_ASSERT('d' == strdata2[4]);
    CPPUNIT_ASSERT(strdata1 == strdata2);

    int noitfyid = 10;
    enode1->add_listener(testnoify, (void*)(&noitfyid));
    enode1->notify("this is noitfy test");
    ret = enode1->getdata(strdata2);
    CPPUNIT_ASSERT(strdata2.length() == ret);
    CPPUNIT_ASSERT(0 == strcmp("this is noitfy test", strdata2.c_str()));
    ret = enode1->remove_listener(testnoify, (void*)(&noitfyid));
    CPPUNIT_ASSERT(0 == ret);

    testlisner testobj;
    enode7->add_listener(&testobj);
    strdata1 = "this is listner test";
    enode7->notify(strdata1);
    ret = enode1->getdata(strdata2);
    CPPUNIT_ASSERT(0 == strcmp("this is listner test", strdata2.c_str()));
}

void 
datapooltest::datapoolglobaltypetest()
{
    datapool *dp = datapool::inst();
    datapool_element enode1 = dp->create_element("testelement_1", true, true);
    datapool_element enode2 = dp->create_element("testelement_2", true, true);
    datapool_element enode3 = dp->create_element("testelement_3", true, true);
    datapool_element enode4 = dp->create_element("testelement_4", true, true);
    datapool_element enode5 = dp->create_element("testelement_5", true, true);
    datapool_element enode6 = dp->create_element("testelement_5", true, false);
    datapool_element enode10 = dp->create_element("testelement_5", false, false);
    datapool_element enode11 = dp->create_element("testelement_5", false, true);

    CPPUNIT_ASSERT(enode1.get());
    CPPUNIT_ASSERT(enode2.get());
    CPPUNIT_ASSERT(enode3.get());
    CPPUNIT_ASSERT(enode4.get());
    CPPUNIT_ASSERT(enode5.get());
    CPPUNIT_ASSERT(NULL == enode6.get());
    CPPUNIT_ASSERT(enode10.get());
    CPPUNIT_ASSERT(NULL == enode11.get());

    datapool_element enode7 = dp->get_element("testelement_4", true);
    CPPUNIT_ASSERT(enode7.get());

    int ret = dp->remove_element("testelement_4", true);
    CPPUNIT_ASSERT(0 == ret);
    enode6 = dp->get_element("testelement_4", true);
    CPPUNIT_ASSERT(NULL == enode6.get());
    ret = dp->remove_element("testelement_4", true);
    CPPUNIT_ASSERT(0 != ret);
    ret = enode4->setdata("test");
    CPPUNIT_ASSERT(0 != ret);

    enode1->setdata("this is test 1");
    enode7 = dp->get_element("testelement_1",true);
    std::string strdata1;
    std::string strdata2;
    ret = enode7->getdata(strdata1);
    CPPUNIT_ASSERT(strdata1.length() == ret);
    ret = enode1->getdata(strdata2);
    CPPUNIT_ASSERT(strdata2.length() == ret);
    CPPUNIT_ASSERT(strdata1 == strdata2);

    char data[] = {'A','b',0x00,'c','d'};
    enode1->setdata(data, 5);
    ret = enode7->getdata(strdata1);
    CPPUNIT_ASSERT(strdata1.length() == ret);
    ret = enode1->getdata(strdata2);
    CPPUNIT_ASSERT(strdata2.length() == ret);
    CPPUNIT_ASSERT('c' == strdata2[3]);
    CPPUNIT_ASSERT('d' == strdata2[4]);
    CPPUNIT_ASSERT(strdata1 == strdata2);

    int noitfyid = 10;
    enode1->add_listener(testnoify, (void*)(&noitfyid));
    enode1->notify("this is noitfy test");
    ret = enode1->getdata(strdata2);
    CPPUNIT_ASSERT(strdata2.length() == ret);
    CPPUNIT_ASSERT(0 == strcmp("this is noitfy test", strdata2.c_str()));
    ret = enode1->remove_listener(testnoify, (void*)(&noitfyid));
    CPPUNIT_ASSERT(0 == ret);

    testlisner testobj;
    enode7->add_listener(&testobj);
    enode7->notify("this is listner test");
    ret = enode1->getdata(strdata2);
    CPPUNIT_ASSERT(0 == strcmp("this is listner test", strdata2.c_str()));

    datapool_element enode21 = dp->create_element("testelement_10", true, false);
    testlisner testobj1;
    enode21->add_listener(&testobj1);
    enode21->notify(nullptr);
    ret = enode21->getdata(strdata2);
    CPPUNIT_ASSERT(0 == ret);

}

void 
datapooltest::twoclienttest()
{
    datapool *dp = datapool::inst();
    datapool_element enode1 = dp->create_element("testelement_1", true, true);
    datapool_element enode2 = dp->create_element("testelement_2", false, true);
    // create by another client
    datapool_element enode3 = dp->create_element("testelement_3", true, true);
    datapool_element enode4 = dp->create_element("testelement_4", false, false);


    CPPUNIT_ASSERT(enode1.get());
    CPPUNIT_ASSERT(enode2.get());
    CPPUNIT_ASSERT(NULL == enode3.get());
    CPPUNIT_ASSERT(enode4.get());
    int ret = dp->remove_element("testelement_3", true);
    CPPUNIT_ASSERT(0 != ret);

    enode3 = dp->get_element("testelement_3", true);
    CPPUNIT_ASSERT(enode3.get());
    std::string strdata1;
    std::string strdata2;
    ret = enode3->getdata(strdata1);
    CPPUNIT_ASSERT(strdata1.length() == ret);
    CPPUNIT_ASSERT(0 == strcmp(strdata1.c_str(), "test another client"));

    int noitfyid = 10;
    enode3->add_listener(testnoify, (void*)(&noitfyid));
    enode3->notify("this is noitfy test");
    ret = enode3->getdata(strdata2);
    CPPUNIT_ASSERT(strdata2.length() == ret);
    CPPUNIT_ASSERT(0 == strcmp("this is noitfy test", strdata2.c_str()));
    ret = enode3->remove_listener(testnoify, (void*)(&noitfyid));
    CPPUNIT_ASSERT(0 == ret);

    testlisner testobj;
    enode3->add_listener(&testobj);
    enode3->notify("this is listner test");
    ret = enode3->getdata(strdata2);
    CPPUNIT_ASSERT(0 == strcmp("this is listner test", strdata2.c_str()));
}


}} // end of namespace zas::utilstest