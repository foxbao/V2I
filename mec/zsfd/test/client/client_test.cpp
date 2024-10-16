#include "mytest.h"
#include "utils/evloop.h"
#include "utils/timer.h"

using namespace mytest::proxy;
using namespace zas::utils;

class rpctestlistener :public myIF2_listener
{
	int32_t on_method1(int32_t val1, ::mytest::test_struct1& val2) {
		printf("value is %d\n", val1);
		size_t sz = val2.var_c.var_3.count();
		for (int i = 0; i < sz; i++) {
			printf("listen out val 3 is %d\n", val2.var_c.var_3[i]);
		}
		// val2.var_c.var_3.clear();
		// val2.var_c.var_3.append(15);
		// val2.var_c.var_3.append(17);
		// val2.var_c.var_3.append(19);
		// val2.var_c.var_3.append(21);
		// val2.var_c.var_3.append(22);
		return 3;
	}
};

class rpctestevent_param : public ::mytest::rpc_event_test_with_param
{
	void on_rpc_event_test_with_param_triggered(
			::mytest::test_struct2 &testdata){
		size_t sz = testdata.var_3.count();
		for (int i = 0; i < sz; i++) {
			printf("out val 3 is %d\n", testdata.var_3[i]);
		}
		printf("has data event called\n");
	}
};

class rpctestevent_noparam : public ::mytest::rpc_event_no_param
{
	void on_rpc_event_test_with_param_triggered(void){
		printf("ondata event called\n");
	}
};

int main()
{

	evloop *evl = evloop::inst();
	evl->setrole(evloop_role_client);

	// set the name of syssvr as "zas.genesis.syssvr"
	evl->updateinfo(evlcli_info_client_name, "zas.system")
		->updateinfo(evlcli_info_instance_name, "sysds")
		->updateinfo(evlcli_info_server_ipv4, "192.168.31.17")
		->updateinfo(evlcli_info_server_port, 5556)
		->updateinfo(evlcli_info_listen_port, 5555)
		->updateinfo(evlcli_info_commit);

	evl->start(true);
	msleep(1000);

	printf("test start service start\n");
	// rpcmgr::inst()->start_service("zas.system.daemons", "rpc.service2",NULL);
	// printf("test start service finished\n");
	// getchar();
	// getchar();

	// printf("test terminate service start\n");
	// rpcmgr::inst()->terminate_service("zas.system.daemons",
	// 	"rpc.service2",NULL);
	// printf("test terminiate service finished\n");
	// getchar();
	// getchar();
	printf("myIF3 instance start\n");
	myIF3_singleton::inst();

	printf("myIF3 instance\n");
	rpctestevent_param eventtest1;
	rpctestevent_noparam eventtest2;
	eventtest1.start_listen();
	eventtest2.start_listen();

	eventtest1.force_trigger_last_event();
	rpctestlistener listen;
	try {
		myIF1 testobj2;
		myIF1 testobj = myIF3_singleton::inst().method2(testobj2);
		printf("testobj instid %08X\n", testobj.get_instid());
		mytest::test_struct2 val;
		
		int testint = 5;
		val.var_3.clear();
		val.var_3.append(1);
		val.var_3.append(2);
		val.var_3.append(3);
		val.var_3.append(4);
		val.var_3.append(10);
		val.var_3.append(20);
		val.var_3.append(30);

		int result = 0;

		long timestamp = gettick_millisecond();
		std::string testdata = "this is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is testthis is test";
		int32_t testval = 0;
		for (int32_t i = 0 ; i < 10; i++) {
			testval = i;
			// printf("send %d \n", i);
			result = testobj.method1(testint, val);
			// printf("send %d finished\n", i);
		}
		printf("test object time %ld\n", gettick_millisecond() - timestamp);
		printf("test object return result is %d\n", result);
			myIF3_singleton::inst().method1(testdata, testval);
		// result = testobj.add_listener(15, listen);
		// printf("test listener return result is %d\n", result);
		// printf("out val 1 %d\n", int(val.var_1));
		// size_t sz = val.var_3.count();
		// for (int i = 0; i < sz; i++) {
		// 	printf("out val 3 is %d\n", val.var_3[i]);
		// }
	} catch(rpc_error& err)
	{
		printf("error test is %s\n", err.get_description().c_str());
	}
	getchar();
	getchar();
	eventtest1.force_trigger_last_event();

	getchar();
	getchar();
	getchar();
}
