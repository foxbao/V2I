#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cppunit/config/SourcePrefix.h>
#include "utils/json.h"
#include "utils/json_test.h"

namespace zas{
namespace utilstest{

using namespace std;
using namespace zas::utils;

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( jsontest ,"alltest" );

const int buf1_sz = 7000;

void 
jsontest::setUp()
{
}

void 
jsontest::tearDown()
{
}

void 
jsontest::testjson()
{
	jsonobject& obj = json::new_object();
	CPPUNIT_ASSERT(obj.get_type() != json_null);
	obj.release();
}

}} // end of namespace zas::utilstest