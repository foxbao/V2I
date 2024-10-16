/**
 * @file buffer_test.h
 * @brief cppunit test of buffer function
 * @author wangyuqian ()
 * @version 1.0
 * @date 2021-08-31
 * 
 * @copyright Copyright (c) 2021
 * 
 * @par modify log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2021-08-31 <td>1.0     <td>wangyuqian     <td>test buffer read&write
 * </table>
 */
#ifndef BUTTERTEST_H
#define BUTTERTEST_H

#include <cppunit/extensions/HelperMacros.h>

namespace zas{
namespace utilstest{

class buffertest : public CPPUNIT_NS::TestFixture
{
	CPPUNIT_TEST_SUITE( buffertest );
	CPPUNIT_TEST( testreadwrite );
	CPPUNIT_TEST_SUITE_END();

	public:
	//cppunit init function.
	//it will be run before each test function beginning
	void setUp();
	//cppunit destory function.
	//it will be run after each test function finshed
	void tearDown();

	void testreadwrite();

private:
	char* _testbuffer;
};

}} // end of namespace zas::utilstest
#endif  // BUTTERTEST_H