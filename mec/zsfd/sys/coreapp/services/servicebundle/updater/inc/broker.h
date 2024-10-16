/** @file inc/broker.h
 * definition of FOTA (firm OTA) backoffice borker
 */

#ifndef ___CXX_ZAS_COREAPP_SERVICE_UPDATER_BROKER_H__
#define ___CXX_ZAS_COREAPP_SERVICE_UPDATER_BROKER_H__

#include <string>
#include "std/list.h"
#include "utils/mutex.h"
#include "mware/mware.h"

using namespace std;
using namespace zas::utils;

namespace zas {
namespace coreapp {
namespace servicebundle {

class fota_mgr;
class fota_shelf_broker;

class fota_broker_mgr
{
public:
	fota_broker_mgr();
	~fota_broker_mgr();
	static fota_broker_mgr* inst(void);

	fota_mgr* set_fotamgr(fota_mgr* fotamgr);
	int register_broker(fota_shelf_broker* broker);
	int activate_broker(const char* name);

private:
	fota_shelf_broker* find_unlocked(const char* name);
	void release_all(void);

private:
	mutex _mut;
	fota_mgr* _fotamgr;
	fota_shelf_broker* _pending_broker;
	listnode_t _broker_list;
};

class fota_shelf_broker
{
	friend class fota_broker_mgr;
public:
	fota_shelf_broker();
	virtual ~fota_shelf_broker();

	virtual int addref(void);
	virtual int release(void);
	int set_name(const char* name);

	virtual int check_update(void) = 0;

private:
	listnode_t _ownerlist;
	int _refcnt;
	string _name;
	fota_broker_mgr* _broker_mgr;
};

}}} // namespace zas::coreapp::servicebundle
#endif // ___CXX_ZAS_COREAPP_SERVICE_UPDATER_BROKER_H__
/* EOF */
