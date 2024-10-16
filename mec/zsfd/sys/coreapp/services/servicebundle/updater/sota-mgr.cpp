/** @file inc/sota-mgr.h
 * implementation of FOTA (firm OTA) manager
 */

#include "inc/sota-mgr.h"

namespace zas {
namespace coreapp {
namespace servicebundle {

sota_mgr::sota_mgr(updater_service* service)
: _service(service)
{
}

}}} // end of namespace zas::coreapp::servicebundle
/* EOF */
