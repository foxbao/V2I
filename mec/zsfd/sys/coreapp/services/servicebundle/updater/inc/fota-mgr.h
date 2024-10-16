/** @file inc/fota-mgr.h
 * definition of FOTA (firm OTA) manager
 */

#ifndef ___CXX_ZAS_COREAPP_SERVICE_UPDATER_FOTAMGR_H__
#define ___CXX_ZAS_COREAPP_SERVICE_UPDATER_FOTAMGR_H__

#include "utils/mutex.h"
#include "utils/timer.h"
#include "utils/thread.h"

using namespace zas::utils;

namespace zas {
namespace coreapp {
namespace servicebundle {

enum fota_state
{
	fota_state_unknown = 0,
	fota_state_checking,
	fota_state_check_pending,
	fota_state_downloading,
	fota_state_installing,
};

class fota_mgr;
class updater_service;
class fota_shelf_broker;

#define fotamgr(x)	(get_fotamgr()->x)

class fota_check_update_task : public looper_task
{
public:
	fota_check_update_task(fota_mgr* fotamgr);
	~fota_check_update_task();

	void do_action(void);
	void release(void);

private:
	fota_mgr* _fotamgr;
};

class fota_mgr
{
	friend class fota_broker_mgr;
	friend class fota_check_update_task;
public:
	class fota_timer : public timer
	{
	public:
		fota_timer();
		void on_timer(void);

	private:
		fota_mgr* get_fotamgr(void) {
			return zas_ancestor(fota_mgr, _timer);
		}
		void reset_random_interval(void);
	};

	fota_mgr(updater_service*);
	~fota_mgr();

	int set_broker(fota_shelf_broker* broker);

private:
	void polling_start(void);
	int request_check_update(void);

private:
	fota_state get_state(void) {
		return _state;
	}

	bool is_state(fota_state s) {
		return (_state == s) ? true : false;
	}

	fota_state set_state(fota_state s) {
		fota_state ret = _state;
		_state = s;
		return ret;
	}

private:
	updater_service* _service;
	fota_state _state;
	mutex _mut;

	fota_shelf_broker* _broker;

	// timer for FOTA
	fota_timer _timer;
};

}}} // end of namespace zas::coreapp::servicebundle
#endif // ___CXX_ZAS_COREAPP_SERVICE_UPDATER_FOTAMGR_H__
/* EOF */
