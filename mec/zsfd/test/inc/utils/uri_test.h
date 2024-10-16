/**
 * @file uri_test.h
 * @brief cppuint test of uri funciton
 * @author wangyuqian ()
 * @version 1.0
 * @date 2021-08-31
 * 
 * @copyright Copyright (c) 2021
 * 
 * @par modify log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2021-08-31 <td>1.0     <td>wangyuqian     <td>test localuri&remoteuri
 * </table>
 */
#ifndef URITEST_H
#define URITEST_H

#include <cppunit/extensions/HelperMacros.h>

namespace zas{
namespace utilstest{

class uritest : public CPPUNIT_NS::TestFixture
{
	CPPUNIT_TEST_SUITE( uritest );
	CPPUNIT_TEST( testuri );
	CPPUNIT_TEST_SUITE_END();

	public:
	//cppunit init function.
	//it will be run before each test function beginning
	void setUp();
	//cppunit destory function.
	//it will be run after each test function finshed
	void tearDown();

	void testuri();
};

}} // end of namespace zas::utilstest
#endif  // URITEST_H