/**
 * @file pkgconfig_test.h
 * @brief 
 * @author wangyuqian ("")
 * @version 0.1
 * @date 2021-10-02
 * 
 * @copyright Copyright (c) 2021
 * 
 * @par modify log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2021-10-02 <td>1.0     <td>wangyuqian     <td>content
 * </table>
 */
#ifndef __PKGCONFIG_TEST_H__
#define __PKGCONFIG_TEST_H__

#include <cppunit/extensions/HelperMacros.h>

namespace zas{
namespace pkgcfgtest{

class pkgconfigtest : public CPPUNIT_NS::TestFixture
{
	CPPUNIT_TEST_SUITE( pkgconfigtest );
	CPPUNIT_TEST( testpkgconfig_compiler );
	CPPUNIT_TEST_SUITE_END();

	public:
	//cppunit init function.
	//it will be run before each test function beginning
	void setUp();
	//cppunit destory function.
	//it will be run after each test function finshed
	void tearDown();

	void testpkgconfig();
	void testpkgconfig_read();
	void testpkgconfig_compiler();
};

}} // end of namespace zas::utilstest
#endif  // URITEST_H