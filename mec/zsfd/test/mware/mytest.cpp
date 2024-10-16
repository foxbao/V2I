#include "mytest.h"
// #include "service_test.h"

namespace mytest {

myIF1::myIF1()
: skeleton_base("rpc.service2", "mytest.myIF1")
{

	// TODO: implement your constructor
}

myIF1::~myIF1()
{
	// TODO: implement your destructor
}

myIF1* myIF1::create_instance(void)
{
	// TODO: Check if this meets your need
	// Change to any possible derived class if necessary
	return new myIF1();//zas::servicetest::myIF1_test();
}

void myIF1::destroy_instance(myIF1* obj)
{
	delete obj;
}

int32_t myIF1::method1(int val1, test_struct2& val2)
{
	// TODO: write your code for implementation
	printf("original method callded\n");
	return 0;
}

int32_t myIF1::add_listener(int val1, skeleton::myIF2_listener &listener)
{
	// TODO: write your code for implementation
	printf("original add_listener callded\n");
	return 0;
}

myIF3_singleton::myIF3_singleton()
: skeleton_base("rpc.service2", "mytest.myIF3_singleton")
{
	// TODO: implement your constructor
}

myIF3_singleton::~myIF3_singleton()
{
	// TODO: implement your destructor
}

myIF3_singleton* myIF3_singleton::create_instance(void)
{
	// TODO: Check if this meets your need
	// Change to any possible derived class if necessary
	return new myIF3_singleton();//zas::servicetest::myIF3_singleton_test();
}

void myIF3_singleton::destroy_instance(myIF3_singleton* obj)
{
	delete obj;
}

::mytest::test_struct2 myIF3_singleton::method1(const std::string& val1, int32_t &val2)
{
	// TODO: write your code for implementation
	::mytest::test_struct2 tmp;
	return tmp;
}

myIF1* myIF3_singleton::method2(myIF1& val1)
{
	// TODO: write your code for implementation
	return &val1;
}

} // end of namespace mytest
/* EOF */
