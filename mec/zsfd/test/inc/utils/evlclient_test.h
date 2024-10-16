/**
 * @file evlclient_test.h
 * @brief cppunit test of evlclient function
 * @author wangyuqian ()
 * @version 1.0
 * @date 2021-08-31
 * 
 * @copyright Copyright (c) 2021
 * 
 * @par modify log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2021-08-31 <td>1.0     <td>wangyuqian     <td>test evlclient list add&remove
 * </table>
 */
#ifndef EVLCLIENTTEST_H
#define EVLCLIENTTEST_H

#include <cppunit/extensions/HelperMacros.h>
#include <string.h>


namespace zas{
namespace utilstest{

class evlclienttest : public CPPUNIT_NS::TestFixture
{
	CPPUNIT_TEST_SUITE( evlclienttest );
	CPPUNIT_TEST( testclientlist );
	CPPUNIT_TEST_SUITE_END();

	public:
	//cppunit init function.
	//it will be run before each test function beginning
	void setUp();
	//cppunit destory function.
	//it will be run after each test function finshed
	void tearDown();

	void testclientlist();

private:
    void init_evl_client_info(std::string clientname, std::string instname, int pid);
    void release_evl_client_info(void);
    bool checkclientinfo(std::string clientname, std::string instname);

private:
	char* _testclientdata;
};

}} // end of namespace zas::utilstest
#endif  // BUTTERTEST_H