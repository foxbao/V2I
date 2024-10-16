/** @file inc/fota-mgr.h
 * implementation of FOTA (firm OTA) manager
 */

#include "inc/service.h"
#include "inc/broker.h"
#include "inc/fota-mgr.h"

namespace zas {
namespace coreapp {
namespace servicebundle {

fota_mgr::fota_mgr(updater_service* service)
: _service(service)
, _broker(nullptr)
, _state(fota_state_check_pending)
{
	polling_start();
}

fota_mgr::~fota_mgr()
{
}

void fota_mgr::polling_start(void)
{
	if (nullptr == _broker) {
		return;
	}
	_timer.setinterval(100);
	_timer.start();
}

fota_mgr::fota_timer::fota_timer()
{
}

void fota_mgr::fota_timer::on_timer(void)
{
	if (fotamgr(get_state()) == fota_state_check_pending) {
		fotamgr(request_check_update());
		return;
	}
	reset_random_interval();
}

void fota_mgr::fota_timer::reset_random_interval(void)
{
	long interval = random() * 10000 / RAND_MAX;
	interval += 10000; // base: 10 sec
	setinterval(interval);
	start();
}

fota_check_update_task::fota_check_update_task(fota_mgr* fotamgr)
: _fotamgr(fotamgr) {
}

fota_check_update_task::~fota_check_update_task()
{
}

void fota_check_update_task::do_action(void)
{
	printf("fota_check_update_task::do_action\n");
	// we need lock here
	auto_mutex am(_fotamgr->_mut);
	if (_fotamgr->_broker == nullptr) {
		return;
	}
}

void fota_check_update_task::release(void) {
	delete this;
}

int fota_mgr::request_check_update(void)
{
	auto_mutex am(_mut);
	if (is_state(fota_state_check_pending)) {
		return -1;
	}
	set_state(fota_state_checking);

	auto* task = new fota_check_update_task(this);
	if (nullptr == task) {
		return -2;
	}
	_service->get_looper().add(task);
	return 0;
}

int fota_mgr::set_broker(fota_shelf_broker* broker)
{
	auto_mutex am(_mut);
	if (is_state(fota_state_downloading)
		|| is_state(fota_state_installing)) {
		return -1;
	}

	if (_broker == broker) return 0;

	if (_broker) _broker->release();
	_broker = broker;
	if (broker) broker->addref();

	if (nullptr != broker) {
		// start the working thread
		_service->get_looper().start();
		// start FOTA polling (check update)
		polling_start();
	}
	return 0;
}

}}} // end of namespace zas::coreapp::servicebundle
/* EOF */
