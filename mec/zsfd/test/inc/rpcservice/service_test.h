#ifndef ___CXX_ZAS_RPC_DEMO_SERVICE_TEST_H__
#define ___CXX_ZAS_RPC_DEMO_SERVICE_TEST_H__

#include "rpc/mytest.h"


namespace zas {
namespace servicetest {

class myIF1_test: public ::mytest::myIF1
{
public:
	myIF1_test(){}
	~myIF1_test(){}
	virtual int32_t method1(int val1, ::mytest::test_struct2& val2);
	virtual int32_t add_listener(int val1,
		::mytest::skeleton::myIF2_listener &listener);
	void call_observable(void);
private:
	::mytest::skeleton::myIF2_listener _listener;
};

class myIF3_singleton_test: public mytest::myIF3_singleton
{
public:
	myIF3_singleton_test(){}
	~myIF3_singleton_test(){}
	virtual ::mytest::test_struct2 method1(const std::string& val1, int32_t &val2);
	virtual ::mytest::myIF1* method2(::mytest::myIF1& val1);
private:
	myIF1_test _val12;
	::mytest::test_struct2 _test_2;
};

}} // end of namespace zas::utils
/* EOF */

#endif