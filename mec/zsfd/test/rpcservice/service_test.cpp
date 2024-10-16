#include "service_test.h"
#include "utils/thread.h"
#include "utils/timer.h"

namespace zas {
namespace servicetest {

using namespace zas::utils;

class test_thread : public thread
{
public:
	test_thread(myIF1_test* sysl)
	: _sysl(sysl) {}

	~test_thread() {

	}

	int run(void) {
		_sysl->call_observable();
	}
	
private:
	myIF1_test* _sysl;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(test_thread);
};

int myIF1_test::add_listener(int val1,
	::mytest::skeleton::myIF2_listener &listener)
{
	printf("recv add listner %d\n", val1);
	_listener = listener;
	return 0;
}

int myIF1_test::method1(int val1, ::mytest::test_struct2& val2)
{
	printf("value is %d\n", val1);
	// size_t sz = val2.var_3.count();
	// for (int i = 0; i < sz; i++) {
	// 	printf("out val 3 is %d\n", val2.var_3[i]);
	// }
	// val2.var_3.clear();
	// val2.var_3.append(5);
	// val2.var_3.append(7);
	// val2.var_3.append(9);
	// val2.var_3.append(11);
	// val2.var_3.append(12);
	// sz = val2.var_3.count();
	// for (int i = 0; i < sz; i++) {
	// 	printf("out val 3 is %d\n", val2.var_3[i]);
	// }
	// auto *plaunch = new test_thread(this);
	// plaunch->start();
	// plaunch->release();
	return 2;
}

void myIF1_test::call_observable(void)
{
	std::string testdata = "observable test";
	printf("call obserable");
	::mytest::test_struct2 val2;
	val2.var_3.clear();
	val2.var_3.append(5);
	val2.var_3.append(7);
	val2.var_3.append(9);
	val2.var_3.append(11);
	val2.var_3.append(12);
	// rpc_status status = rpcstatus_deactivate;
	// int ret = _rpchandle.onrpcactive(true, testdata, status);
	// printf("call_observable finished %d\n", ret);

	// Skeleton::rpc_event_test_with_param event;
	// rpcfunciton::rpc_status st = rpcfunciton::rpcstatus_actived;
	auto* paramevent = new ::mytest::skeleton::rpc_event_test_with_param();
	auto* event = new 	::mytest::skeleton::rpc_event_no_param();
	int cnt = 0;
	::mytest::test_struct1 val1;

	for (;;) {
		msleep(1000);
		paramevent->trigger(val2);
		event->trigger();
		val2.var_3[0]++;
		val2.var_3[1]++;
		val2.var_3[2]++;
		val2.var_3[3]++;
		val2.var_3[4]++;
		printf("send trigger\n");
		val1.var_c = val2;
		int ret =_listener.on_method1(32, val1);
		printf("listener return value is %d\n", ret);
		cnt++;
		if (cnt > 10) return;
	}
}

::mytest::test_struct2 myIF3_singleton_test::method1(
	const std::string& val1, int32_t &val2)
{
	// TODO: write your code for implementation
	printf("val %s, val %d\n", val1.c_str(), val2);
	::mytest::test_struct2 test123;
	return test123;
}

::mytest::myIF1* myIF3_singleton_test::method2(::mytest::myIF1& val1)
{
	// TODO: write your code for implementation
	printf("myIF3_singleton_test::method2\n");
	return &_val12;
}

}} // end of namespace zas::servicetest
/* EOF */