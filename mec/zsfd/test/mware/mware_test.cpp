#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

// -- test ota_installer ---
// shall move to a saperate file later
#include <string>
#include "mware/ota-installer.h"
using namespace std;
using namespace zas::mware;

class ota_listener : public ota_installer_listener
{
public:
  void on_state_changed(ota_state s)
  {
    printf("on_state_changed: %u\n", s);
  }
};
// --- end of test ota-installer ----

int main()
{
  // Get the top level suite from the registry
  CPPUNIT_NS::Test *suite = CPPUNIT_NS::TestFactoryRegistry::getRegistry("rpcconfig").makeTest();

  // Adds the test to the list of test to run
  CPPUNIT_NS::TextUi::TestRunner runner;
  runner.addTest( suite );

  // Change the default outputter to a compiler error format outputter
  runner.setOutputter( new CPPUNIT_NS::CompilerOutputter( &runner.result(),
                                                          CPPUNIT_NS::stdCOut() ) );
  
  ota_listener lnr;
  ota_installer ota(
    "/home/jimview/zsfd-bak",
    "/home/jimview/zsfd-otatest",
    "/home/jimview/ota-test"
  );
  ota.set_listener(&lnr);
  //ota.backup_partition(ota_installer::partition_dst, "ota-backup");
  ota.verify();
  ota.update();

  // Run the test.
  bool wasSucessful = runner.run();

  // Return error code 1 if the one of test failed.
  return wasSucessful ? 0 : 1;
}
