
#ifndef DATA_POOL_TEST_H
#define DATA_POOL_TEST_H

#include <cppunit/extensions/HelperMacros.h>
#include <string.h>


namespace zas{
namespace utilstest{

class datapooltest : public CPPUNIT_NS::TestFixture
{
	CPPUNIT_TEST_SUITE( datapooltest );
	// CPPUNIT_TEST( localdatapooltest );
	CPPUNIT_TEST( datapoolglobaltypetest );
	// CPPUNIT_TEST( datapoolglobaltypetest );
	// CPPUNIT_TEST( twoclienttest );
	CPPUNIT_TEST_SUITE_END();

	public:
	//cppunit init function.
	//it will be run before each test function beginning
	void setUp();
	//cppunit destory function.
	//it will be run after each test function finshed
	void tearDown();

	void localdatapooltest();
	void globaldatapooltest();
	void datapoolglobaltypetest();
	void twoclienttest();

private:
    char *pdata;
    char *pdata2;

};

}} // end of namespace zas::utilstest
#endif  // DATA_POOL_TEST_H