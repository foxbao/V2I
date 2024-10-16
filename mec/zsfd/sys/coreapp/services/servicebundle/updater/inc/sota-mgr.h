/** @file inc/sota-mgr.h
 * definition of FOTA (firm OTA) manager
 */

#ifndef ___CXX_ZAS_COREAPP_SERVICE_UPDATER_SOTAMGR_H__
#define ___CXX_ZAS_COREAPP_SERVICE_UPDATER_SOTAMGR_H__

#include "utils/timer.h"

using namespace zas::utils;

namespace zas {
namespace coreapp {
namespace servicebundle {

class updater_service;

class sota_mgr
{
public:
	sota_mgr(updater_service* service);

private:
	updater_service* _service;
};

}}} // end of namespace zas::coreapp::servicebundle
#endif // ___CXX_ZAS_COREAPP_SERVICE_UPDATER_SOTAMGR_H__
/* EOF */
