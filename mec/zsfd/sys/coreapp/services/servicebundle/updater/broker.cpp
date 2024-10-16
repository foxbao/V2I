/** @file inc/broker.h
 * implementation of FOTA (firm OTA) backoffice borker
 */

#include "inc/broker.h"
#include "inc/fota-mgr.h"

namespace zas {
namespace coreapp {
namespace servicebundle {

fota_broker_mgr::fota_broker_mgr()
: _fotamgr(nullptr)
, _pending_broker(nullptr)
{
	listnode_init(_broker_list);
}

fota_broker_mgr::~fota_broker_mgr()
{
	release_all();
}

void fota_broker_mgr::release_all(void)
{
	auto_mutex am(_mut);
	while (!listnode_isempty(_broker_list)) {
		auto* item = list_entry(fota_shelf_broker, _ownerlist, \
		_broker_list.next);
		listnode_del(item->_ownerlist);
		item->release();
	}

	if (_pending_broker) {
		_pending_broker->release();
		_pending_broker = nullptr;
	}
}

fota_broker_mgr* fota_broker_mgr::inst(void)
{
	static fota_broker_mgr _inst_mgr;
	return &_inst_mgr;
}

fota_mgr* fota_broker_mgr::set_fotamgr(fota_mgr* fotamgr)
{
	assert(_fotamgr == nullptr && fotamgr);
	auto_mutex am(_mut);
	_fotamgr = fotamgr;

	if (_pending_broker) {
		if (!fotamgr->set_broker(_pending_broker)) {
			_pending_broker->release();
			_pending_broker = nullptr;
		}
	}
	return fotamgr;
}

fota_shelf_broker* fota_broker_mgr::find_unlocked(const char* name)
{
	assert(nullptr != name);
	auto* item = _broker_list.next;
	for (; item != &_broker_list; item = item->next) {
		auto* broker = list_entry(fota_shelf_broker, _ownerlist, item);
		if (broker->_name == name) return broker;
	}
	return nullptr;
}

int fota_broker_mgr::register_broker(fota_shelf_broker* broker)
{
	auto_mutex am(_mut);
	if (nullptr == broker || broker->_name.empty()) {
		return -EBADPARM;
	}

	if (!listnode_isempty(broker->_ownerlist)) {
		return -ENOTALLOWED;
	}
	if (find_unlocked(broker->_name.c_str())) {
		return -ENOTALLOWED;
	}
	listnode_add(_broker_list, broker->_ownerlist);
	broker->addref();
	return 0;
}

int fota_broker_mgr::activate_broker(const char* name)
{
	if (!name || !*name) {
		return -EBADPARM;
	}

	auto_mutex am(_mut);
	auto* broker = find_unlocked(name);
	if (nullptr == broker) {
		return -ENOTFOUND;
	}

	if (!_fotamgr) {
		if (_pending_broker) {
			_pending_broker->release();
		}
		broker->addref();
		_pending_broker = broker;
	}
	else {
		assert(nullptr == _pending_broker);
		_fotamgr->set_broker(broker);
	}
	return 0;
}

fota_shelf_broker::fota_shelf_broker()
: _refcnt(0), _broker_mgr(nullptr) {
	listnode_init(_ownerlist);
}

fota_shelf_broker::~fota_shelf_broker()
{
}

int fota_shelf_broker::addref(void)
{
	return __sync_add_and_fetch(&_refcnt, 1);
}

int fota_shelf_broker::release(void)
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0) {
		delete this;
	}
	return cnt;
}

int fota_shelf_broker::set_name(const char* name)
{
	if (nullptr != name && *name) {
		_name = name;
		return 0;
	}
	return -EBADPARM;
}

}}} // end of namespace zas::coreapp:;servicebundle
/* EOF */
