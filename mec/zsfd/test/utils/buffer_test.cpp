#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cppunit/config/SourcePrefix.h>
#include "utils/buffer.h"
#include "utils/buffer_test.h"

namespace zas{
namespace utilstest{

using namespace std;
using namespace zas::utils;

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( buffertest ,"alltest" );

const int buf1_sz = 7000;

void 
buffertest::setUp()
{
	_testbuffer = (char*)malloc(buf1_sz);
	CPPUNIT_ASSERT(NULL != _testbuffer);
	char buf[32], *b = _testbuffer;
	for (int i = 0;; ++i) {
		sprintf(buf, "%d,", i);
		size_t len = strlen(buf);
		if (b + len < _testbuffer + buf1_sz - 1) {
			memcpy(b, buf, len);
			b += len;
		}
		else break;
	}
	*b = '\0';
}

void 
buffertest::tearDown()
{
	if (_testbuffer) {
		free(_testbuffer);
	}
	_testbuffer = NULL;
}

void 
buffertest::testreadwrite()
{
	fifobuffer* fb = create_fifobuffer();
	size_t sz = fb->append((void*)_testbuffer, buf1_sz);
	CPPUNIT_ASSERT(sz == buf1_sz);
	CPPUNIT_ASSERT(fb->getsize() == buf1_sz);
	sz = fb->append((void*)"end", 4);
	CPPUNIT_ASSERT(sz == 4);
	CPPUNIT_ASSERT(fb->getsize() == buf1_sz + 4);

	char buffer[32];
	sz = fb->peekdata(0, buffer, 4);
	CPPUNIT_ASSERT(sz == 4);
	CPPUNIT_ASSERT(!memcmp(buffer, "0,1,", 4));
	sz = fb->peekdata(buf1_sz, buffer, 16);
	CPPUNIT_ASSERT(sz == 4);	// we only have 4 remain bytes
	CPPUNIT_ASSERT(!strcmp(buffer, "end"));

	sz = fb->peekdata(4076, buffer, 16);
	CPPUNIT_ASSERT(sz == 16);

	// test seek
	int ret = fb->seek(4500);
	CPPUNIT_ASSERT(!ret);
	ret = fb->seek(buf1_sz - 4500, seek_cur);
	CPPUNIT_ASSERT(!ret);
	sz = fb->read(buffer, 1);
	CPPUNIT_ASSERT(sz == 1 && buffer[0] == 'e');
	sz = fb->read(buffer + 1, 3);
	CPPUNIT_ASSERT(sz == 3);
	CPPUNIT_ASSERT(!strcmp(buffer, "end"));
	ret = fb->seek(1, seek_cur);
	CPPUNIT_ASSERT(ret == -EEOF);
}

}} // end of namespace zas::utilstest