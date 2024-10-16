#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cppunit/config/SourcePrefix.h>
#include "mware/rpcstruct_test.h"

#include "mytest.h"

namespace zas{
namespace rpcstructtest{

using namespace std;
using namespace mytest;

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( rpcstructtest ,"rpctest" );

void 
rpcstructtest::setUp()
{
}

void 
rpcstructtest::tearDown()
{
}

void 
rpcstructtest::test_struct2_test()
{
	test_struct2 s1;
	int32_t v1 = s1.var_1;
	CPPUNIT_ASSERT(v1 == 0);

	// test set const value
	s1.var_1 = 1234;
	CPPUNIT_ASSERT(1234 == s1.var_1);
	v1 = s1.var_1;
	CPPUNIT_ASSERT(v1 == 1234);

	// test set variable value
	v1 = -12345;
	s1.var_1 = v1;
	CPPUNIT_ASSERT(-12345 == s1.var_1);
	CPPUNIT_ASSERT(v1 == s1.var_1);
}


}} // end of namespace zas::rpcstructtest