/**
 * @file thread_test.h
 * @brief cppunit test of thread&task
 * @author wangyuqian ()
 * @version 1.0
 * @date 2021-08-31
 * 
 * @copyright Copyright (c) 2021
 * 
 * @par modify log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2021-08-31 <td>1.0     <td>wangyuqian     <td>test thread create&stop and theadloop&task
 * </table>
 */
#ifndef THREADTEST_H
#define THREADTEST_H

#include <cppunit/extensions/HelperMacros.h>

#include "utils/thread.h"
#include "utils/wait.h"
#include "utils/timer.h"
namespace zas{
namespace utilstest{

using namespace std;
using namespace zas::utils;

class test_thread1 : public thread
{
public:
	int run(void) {
		_wobj.lock();
		_wobj.wait();
		_wobj.unlock();
		return 0;
	}

	void notify(void) {
		_wobj.lock();
		_wobj.notify();
		_wobj.unlock();
	}

private:
	waitobject _wobj;
};

class test_thread2 : public thread
{
public:
	int run(void) {
		return 0;
	}
};

class mytask : public looper_task
{
public:
	mytask() : _flag(0) {}

	void do_action(void) {
		_flag = 1234;
	}

	void release(void) {
		assert(_flag == 1234);
		looper_task::release();
	}
private:
	int _flag;
};

class threadtest : public CPPUNIT_NS::TestFixture
{
	CPPUNIT_TEST_SUITE( threadtest );
	CPPUNIT_TEST( testthread );
	CPPUNIT_TEST_SUITE_END();

	public:
	//cppunit init function.
	//it will be run before each test function beginning
	void setUp();
	//cppunit destory function.
	//it will be run after each test function finshed
	void tearDown();

	void testthread();
};

}} // end of namespace zas::utilstest
#endif  // THREADTEST_H