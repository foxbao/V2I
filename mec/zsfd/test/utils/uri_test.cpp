#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cppunit/config/SourcePrefix.h>
#include "utils/uri.h"
#include "utils/uri_test.h"

namespace zas{
namespace utilstest{

using namespace std;
using namespace zas::utils;

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( uritest ,"uritest" );

const int buf1_sz = 7000;

void 
uritest::setUp()
{
}

void 
uritest::tearDown()
{
}

void 
uritest::testuri()
{
	std::string header_uri = "ztcp://vehicle-snapshot:8236/update?vid=32efd9ca8&seqid=292403";
	zas::utils::uri test(header_uri);
	CPPUNIT_ASSERT(test.get_fullpath() == "vehicle-snapshot:8236/update");
	CPPUNIT_ASSERT(test.get_host() == "vehicle-snapshot");
	CPPUNIT_ASSERT(test.get_port() == 8236);

	uri uri("file://test.cpp");
	CPPUNIT_ASSERT(uri.get_scheme() == "file");
	string user, passwd;
	CPPUNIT_ASSERT(uri.get_user_passwd(user, passwd) == 0);
	CPPUNIT_ASSERT(uri.get_fullpath() == "test.cpp");
	CPPUNIT_ASSERT(uri.has_port() == 0);
	CPPUNIT_ASSERT(uri.has_filename() == 0);

  	uri.change("http://gitee.com:8090/index.php?name=test&age=32");
	CPPUNIT_ASSERT(uri.valid() && !uri.has_user_passwd() && uri.has_port());
	uint8_t buf[32];
	uri.digest(buf);

	// check query count
	CPPUNIT_ASSERT(uri.query_count() == 2);
	CPPUNIT_ASSERT(uri.query_key(0) == "age");
	CPPUNIT_ASSERT(uri.query_value(0) == "32");
	CPPUNIT_ASSERT(uri.query_key(1) == "name");
	CPPUNIT_ASSERT(uri.query_value(1) == "test");

	CPPUNIT_ASSERT(uri.query_value("name") == "test");
	CPPUNIT_ASSERT(uri.query_value(string("age")) == "32");

	uri.change("http://192.168.0.3:8080");
	CPPUNIT_ASSERT(uri.get_scheme() == "http");
	CPPUNIT_ASSERT(uri.get_fullpath() == "192.168.0.3:8080");
	CPPUNIT_ASSERT(uri.get_host() == "192.168.0.3");
	CPPUNIT_ASSERT(uri.has_filename() == 0);

	// test if we have "/"
	uri.change("http://192.168.0.3:8080/");
	CPPUNIT_ASSERT(uri.get_scheme() == "http");
	CPPUNIT_ASSERT(uri.get_fullpath() == "192.168.0.3:8080/");
	CPPUNIT_ASSERT(uri.get_host() == "192.168.0.3");
	CPPUNIT_ASSERT(uri.has_filename() == 0);

	uri.change("tcp://jimview:12345@/");
	CPPUNIT_ASSERT(!uri.valid());

	uri.change("tcp://jimview:12345@");
	CPPUNIT_ASSERT(!uri.valid());

	uri.change("tcp://jimview:12345@localhost:8080/index.html");
	CPPUNIT_ASSERT(uri.valid());
	CPPUNIT_ASSERT(uri.get_scheme() == "tcp");
	CPPUNIT_ASSERT(uri.get_host() == "localhost");
	CPPUNIT_ASSERT(uri.get_port() == 8080);
	CPPUNIT_ASSERT(!strcmp(uri.get_filename(), "index.html"));
	CPPUNIT_ASSERT(uri.get_fullpath() == "localhost:8080/index.html");
	CPPUNIT_ASSERT(uri.get_user_passwd(user, passwd) != 0);
	CPPUNIT_ASSERT(user == "jimview");
	CPPUNIT_ASSERT(passwd == "12345");

	// port 76543 exceeds the boundary
	uri.change("tcp://localhost:76543");
	CPPUNIT_ASSERT(!uri.valid());
}

}} // end of namespace zas::utilstest