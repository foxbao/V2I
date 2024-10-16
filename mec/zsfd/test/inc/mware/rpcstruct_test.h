/**
 * @file rpcstruct_test.h
 * @brief 
 * @author wangyuqian ("")
 * @version 0.1
 * @date 2021-11-11
 * 
 * @copyright Copyright (c) 2021
 * 
 * @par modify log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2021-11-11 <td>1.0     <td>wangyuqian     <td>
 * </table>
 */
#ifndef __RPC_STRUCT_TEST_H__
#define __RPC_STRUCT_TEST_H__

#include <cppunit/extensions/HelperMacros.h>

namespace zas{
namespace rpcstructtest{

class rpcstructtest : public CPPUNIT_NS::TestFixture
{
	CPPUNIT_TEST_SUITE( rpcstructtest );
	CPPUNIT_TEST( test_struct2_test );
// add new fuction
	CPPUNIT_TEST_SUITE_END();

	public:
	//cppunit init function.
	//it will be run before each test function beginning
	void setUp();
	//cppunit destory function.
	//it will be run after each test function finshed
	void tearDown();

	// test the "test_struct2"
	void test_struct2_test();

};

}} // end of namespace zas::utilstest
#endif  // URITEST_H