/**
 * @file json_test.h
 * @brief cppunit test of json funciton
 * @author wangyuqian ()
 * @version 1.0
 * @date 2021-08-31
 * 
 * @copyright Copyright (c) 2021
 * 
 * @par modify log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2021-08-31 <td>1.0     <td>wangyuqian     <td>test null-type of josn
 * </table>
 */
#ifndef JSONTEST_H
#define JSONTEST_H

#include <cppunit/extensions/HelperMacros.h>

namespace zas{
namespace utilstest{

class jsontest : public CPPUNIT_NS::TestFixture
{
	CPPUNIT_TEST_SUITE( jsontest );
	CPPUNIT_TEST( testjson );
	CPPUNIT_TEST_SUITE_END();

	public:
	//cppunit init function.
	//it will be run before each test function beginning
	void setUp();
	//cppunit destory function.
	//it will be run after each test function finshed
	void tearDown();

	void testjson();
};

}} // end of namespace zas::utilstest
#endif  // JSONTEST_H