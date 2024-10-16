#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cppunit/config/SourcePrefix.h>
#include "utils/thread_test.h"

namespace zas{
namespace utilstest{

using namespace std;
using namespace zas::utils;

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( threadtest ,"alltest" );

const int buf1_sz = 7000;

void 
threadtest::setUp()
{
}

void 
threadtest::tearDown()
{
}

void 
threadtest::testthread()
{
	auto *thd = new test_thread1();
	thd->release();
	thd = new test_thread1();
	thd->start();
	thd->release();
	msleep(20);
	thd->notify();
	thd->join();

	auto *thd1 = new test_thread2();
	thd1->start();
	msleep(20);
	thd1->release();

	auto *tsk = new mytask();
	auto *thd2 = new looper_thread();
	thd2->start();
	msleep(20);
	thd2->release();
	
	thd2->add(tsk);
	msleep(20);
	assert(0 == thd2->stop());
	msleep(20);
}

}} // end of namespace zas::utilstest