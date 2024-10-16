/** @file evpoller.cpp
 * implementation of the event poller for evloop
 */

#include "utils/utils.h"
#if (defined(UTILS_ENABLE_FBLOCK_EVLOOP) && defined(UTILS_ENABLE_FBLOCK_EVLCLIENT) && defined(UTILS_ENABLE_FBLOCK_BUFFER))

#include "utils/timer.h"

#include "inc/evpoller.h"
#include "inc/evloop-impl.h"

namespace zas {
namespace utils {

evpoller_event_impl::evpoller_event_impl(int type, evpoller_impl* poller)
: _type(type), _poller(poller), _input(NULL), _output(NULL)
, _input_size(0), _output_size(0)
{
	assert(poller);
	listnode_init(_active);
	listnode_init(_ownerlist);
}

evpoller_event_impl::~evpoller_event_impl()
{
	assert(_poller);
	// FIXME: need add lock
	if (!listnode_isempty(_active)) {
		listnode_del(_active);
	}
	if (!listnode_isempty(_ownerlist)) {
		listnode_del(_ownerlist);
	}
	if (_input) {
		free(_input); _input = NULL;
	}
	if (_output) {
		free(_output); _output = NULL;
	}
}

void evpoller_event_impl::trigger(void)
{
	if (_poller) {
		_poller->trigger_event(this);
	}
}

evpoller_pkgevent::evpoller_pkgevent(evpoller_impl* poller,
	uint32_t pkgid)
	: evpoller_event_impl(evp_evid_package, poller), _pkgid(pkgid)
	, _seqid(0), _flags(0)
{
}

evpoller_pkgevent::evpoller_pkgevent(evpoller_impl* poller,
	uint32_t pkgid, uint32_t seqid)
	: evpoller_event_impl(evp_evid_package_with_seqid, poller)
	, _pkgid(pkgid), _seqid(seqid), _flags(0)
{
	_f.has_seqid = 1;
}

int evpoller_pkgevent::submit(void)
{
	auto* inst = reinterpret_cast<evloop_impl*>(evloop::inst());
	assert(NULL != inst);

	if (!inst->is_running()) return -ENOTALLOWED;
	return inst->get_package_listener_manager()
		->add_package_event(
		get_packageid(),
		reinterpret_cast<evpoller_event*>(
		static_cast<evpoller_event_impl*>(this)));
}

bool evpoller_pkgevent::get_seqid(uint32_t& sid)
{
	if (_f.has_seqid) {
		sid = _seqid; return true;
	}
	return false;
}

evpoller_timeout_event::evpoller_timeout_event
	(evpoller_impl* poller, int timeout)
: timer(timeout)
, evpoller_event_impl(evp_evid_timeout, poller)
{
}

void evpoller_timeout_event::on_timer(void)
{
	trigger();
	auto* poller = getpoller();
	poller->trigger_submit();
}

// static mutex
mutex evp_eventlist::_mut;

evp_eventlist::evp_eventlist()
{
	listnode_init(_generic);
}

evp_eventlist::~evp_eventlist()
{
	auto_mutex am(_mut);
	detach_events_unlocked();
}

void evp_eventlist::detach_events_unlocked(void)
{
	while (!listnode_isempty(_generic)) {
		listnode_t* item = _generic.next;
		auto* ev = list_entry(evpoller_event_impl, _active, item);
		listnode_del(ev->_active);
	}
}

int evp_eventlist::append_list_unlocked(listnode_t& hdr, evpoller_event_impl* ev)
{
	if (!listnode_isempty(ev->_active)) {
		return -ENOTAVAIL;
	}
	listnode_add(hdr, ev->_active);
	return 0;
}

evpoller_event_impl* evp_eventlist::dequeue(void)
{
	_mut.lock();
	if (listnode_isempty(_generic)) {
		_mut.unlock();
		return NULL;
	}

	auto* ev = get_event_unlocked(_generic.next, true);
	_mut.unlock();

	if (!ev) return NULL;

	// trigger the event
	ev->trigger();
	return ev;
}

evp_pkgevent_list::evp_pkgevent_list()
: _id(0)
{
	listnode_init(_specific[0]);
	listnode_init(_specific[1]);
}

evp_pkgevent_list::~evp_pkgevent_list()
{
	auto_mutex am(_mut);
	detach_events_unlocked();
}

int evp_pkgevent_list::append(evpoller_event_impl* ev)
{
	int type = ev->gettype();
	if (type == evp_evid_package) {
		return zas::utils::evp_eventlist::append(ev);
	}
	else if (type == evp_evid_package_with_seqid) {
		auto_mutex am(_mut);
		return append_list_unlocked(_specific[_id], ev);
	}
	else return -ENOTSUPPORT;
}

evpoller_event_impl* evp_pkgevent_list::dequeue_specific(
	const package_header& pkghdr)
{
	listnode_t& header = _specific[_id];
	listnode_t& next_hdr = _specific[1 - _id];

	_mut.lock();
	while (!listnode_isempty(header))
	{
		auto* ev = get_event_unlocked(header.next, true);

		assert(NULL != ev);
		auto* pkgev = zas_downcast(evpoller_event_impl, evpoller_pkgevent, ev);
		
		// get the sequence id
		uint32_t seqid = 0;
		bool ret = pkgev->get_seqid(seqid);
		assert(ret == true);

		if (pkghdr.seqid == seqid) {
			// trigger the event
			_mut.unlock();
			ev->trigger();
			return ev;
		}
		append_list_unlocked(next_hdr, ev);
	}
	_mut.unlock();
	return NULL;
}

void evp_pkgevent_list::detach_events_unlocked(void)
{
	listnode_t& l0 = _specific[0];
	while (!listnode_isempty(l0)) {
		get_event_unlocked(l0.next, true);
	}
	listnode_t& l1 = _specific[1];
	while (!listnode_isempty(l1)) {
		get_event_unlocked(l1.next, true);
	}

	zas::utils::evp_eventlist::detach_events_unlocked();
}

evpoller_impl::evpoller_impl(evloop_impl* looper)
: _looper(looper), _flags(0)
, _threadid(gettid()), _cor(NULL)
{
	auto* evl = reinterpret_cast<evloop_impl*>(evloop::inst());
	assert(NULL != evl);
	if (evl->is_evloop_thread()) {
		_f.in_evloop_thread = 1;
		auto* cormgr = evl->get_coroutine_manager();
		_cor = cormgr->get_current();
	}

	listnode_init(_events);
	listnode_init(_active_list);
	listnode_init(_inactive_list);
	listnode_init(_pending_list);
}

evpoller_impl::~evpoller_impl()
{
	reset();
}

void evpoller_impl::reset(void)
{
	auto_mutex am(_mut);
	release_events_unlocked();
}

void evpoller_impl::release_events_unlocked(void)
{
	while (!listnode_isempty(_events)) {
		listnode_t* item = _events.next;
		auto* ev = list_entry(evpoller_event_impl, _ownerlist, item);
		listnode_del(ev->_ownerlist);
		delete ev;
	}
}

void evpoller_impl::release_inactive_unlocked(void)
{
	while (!listnode_isempty(_inactive_list)) {
		auto* ev = list_entry(evpoller_event_impl, _active, \
			_inactive_list.next);
		listnode_del(ev->_active);
		listnode_del(ev->_ownerlist);
		delete ev;
	}
}

evpoller_event_impl* evpoller_impl::get_triggered_event(void)
{
	auto_mutex am(_mut);
	if (listnode_isempty(_active_list)) {
		return NULL;
	}

	auto* ev = list_entry(evpoller_event_impl, \
		_active, _active_list.next);
	listnode_del(ev->_active);
	listnode_add(_inactive_list, ev->_active);
	return ev;
}

void evpoller_impl::trigger_event(evpoller_event_impl* ev)
{
	auto_mutex am(_mut);
	if (!listnode_isempty(ev->_active)) {
		listnode_del(ev->_active);
	}
	listnode_add(_pending_list, ev->_active);
}

void evpoller_impl::trigger_submit(void)
{
	if (_f.in_evloop_thread) {
		_mut.lock();
		submit_triggers_unlocked();
		_mut.unlock();

		// check if we are in a coroutine
		if (_cor) {
			_cor->resume();
		}
	} else {
		_wobj.lock();
		submit_triggers_unlocked();
		_wobj.broadcast();
		_wobj.unlock();
	}
}

void evpoller_impl::submit_triggers_unlocked(void)
{
	while (!listnode_isempty(_pending_list)) {
		auto* node = list_entry(evpoller_event_impl, \
			_active, _pending_list.next);
		listnode_del(node->_active);
		listnode_add(_active_list, node->_active);
	}
}

evpoller_event_impl* evpoller_impl::create_event_package(va_list vl)
{
	evpoller_event_impl* event = NULL;
	uint32_t pkgid = va_arg(vl, uint32_t);
	event = new evpoller_pkgevent(this, pkgid);
	assert(NULL != event);
	return event;
}

evpoller_event_impl* evpoller_impl::create_event_package_with_seqid(va_list vl)
{
	evpoller_event_impl* event = NULL;
	uint32_t pkgid = va_arg(vl, uint32_t);
	uint32_t seqid = va_arg(vl, uint32_t);
	event = new evpoller_pkgevent(this, pkgid, seqid);
	assert(NULL != event);
	return event;
}

evpoller_event_impl* evpoller_impl::create_event(int evid, va_list vl)
{
	evpoller_event_impl* event = NULL;
	switch (evid)
	{
	case evp_evid_package:
		event = create_event_package(vl);
		if (!event) return NULL;
		break;

	case evp_evid_package_with_seqid:
		event = create_event_package_with_seqid(vl);
		if (!event) return NULL;
		break;

	default: return NULL;
	}

	if (!event) return event;

	// add to the evpoller
	auto_mutex am(_mut);
	assert(listnode_isempty(event->_ownerlist));
	listnode_add(_events, event->_ownerlist);
	return event;
}

int evpoller_impl::poll(int timeout)
{
	if (!verify_status()) {
		return -EINVALID;
	}
	// release inactive list and check if there
	// is any pending triggered events
	_mut.lock();
	release_inactive_unlocked();
	if (!listnode_isempty(_active_list)) {
		_mut.unlock();
		return 0;
	}
	_mut.unlock();

	if (_f.in_evloop_thread) {
		// try handle as an evloop thread poller
		int ret = poll_evloop_thread(timeout);
		if (!ret) return 0;
	}
	// try handle as in an external thread
	return poll_nonevloop_thread(timeout);
}

int evpoller_impl::poll_nonevloop_thread(int timeout)
{
	_wobj.lock();
	_wobj.wait((timeout < 0) ? infinite : timeout);
	submit_triggers_unlocked();
	_wobj.unlock();

	// make sure there is pending events
	assert(!listnode_isempty(_active_list));
	return 0;
}

bool evpoller_impl::has_pending_events(void)
{
	_mut.lock();
	submit_triggers_unlocked();

	bool ret = (listnode_isempty(_active_list))
		? false : true;

	_mut.unlock();
	return ret;
}

evpoller_event_impl* evpoller_impl::get_event_by_type(
	int type, bool dq)
{
	_mut.lock();
	listnode_t* node = _active_list.next;
	for (; node != &_active_list; node = node->next) {
		auto* ev = list_entry(evpoller_event_impl, _active, node);
		if (ev->gettype() == type) {
			if (dq) listnode_del(ev->_active);
			_mut.unlock();
			return ev;
		}
	}
	_mut.unlock();
	return NULL;
}

int evpoller_impl::poll_evloop_coroutine(int timeout)
{
	assert(NULL != _cor);
	if (timeout > 0) {
		// add a timer for triggering timeout
		auto* timeout_event = new evpoller_timeout_event(this, timeout);
		assert(NULL != timeout_event);
		timeout_event->start();
	}

	for (;;) {
		// check if there is any pending events
		if (has_pending_events()) {
			// check if there is timeout event
			// and remove all timeout event
			bool is_timeout = false;
			evpoller_event_impl* ev;
			while (get_event_by_type(evp_evid_timeout, true)) {
				delete ev;
				is_timeout = true;
			}
			if (is_timeout) return -ETIMEOUT;
			return 0;
		}
		_cor->suspend();
	}
	// shall never come here
	return -EINVALID;
}

int evpoller_impl::poll_evloop_thread(int timeout)
{
	if (_cor) {
		return poll_evloop_coroutine(timeout);
	}

	long prev = gettick_nanosecond();
	auto* evl = reinterpret_cast<evloop_impl*>(evloop::inst());
	assert(NULL != evl);

	if (has_pending_events()) {
		return 0;
	}

	for (;;) {
		int ret = evl->wait_handle(timeout);
		if (ret == -ETIMEOUT) return ret;

		if (has_pending_events()) {
			return 0;
		}

		// check timeout
		if (timeout >= 0) {
			long curr = gettick_nanosecond();
			long timedout = long(timeout) * 1000000;
			if (curr - prev < timedout) {
				timeout = (timedout + prev - curr) / 1000000;
				prev = curr;
			}
			else return -ETIMEOUT;
		}
	}
	// shall never come here
	return -EINVALID;
}

bool evpoller_impl::verify_status(void)
{
	auto* evl = reinterpret_cast<evloop_impl*>(evloop::inst());
	assert(NULL != evl);
	if ((int)evl->is_evloop_thread() != _f.in_evloop_thread) {
		return false;
	}
	if (_threadid != gettid()) {
		return false;
	}
	if (_f.in_evloop_thread) {
		auto* cormgr = evl->get_coroutine_manager();
		if (_cor != cormgr->get_current()) {
			return false;
		}
	}
	return true;
}

int evpoller_event::submit(void)
{
	auto* impl = reinterpret_cast<evpoller_event_impl*>(this);
	return impl->submit();
}

void* evpoller_event::allocate_inputbuf(size_t sz)
{
	auto* impl = reinterpret_cast<evpoller_event_impl*>(this);
	if (NULL == impl) return NULL;
	return impl->allocate_inputbuf(sz);
}

void* evpoller_event::allocate_outputbuf(size_t sz)
{
	auto* impl = reinterpret_cast<evpoller_event_impl*>(this);
	if (NULL == impl) return NULL;
	return impl->allocate_outputbuf(sz);
}

void* evpoller_event::get_inputbuf(size_t* sz)
{
	auto* impl = reinterpret_cast<evpoller_event_impl*>(this);
	if (NULL == impl) return NULL;
	return impl->get_inputbuf(sz);
}

void* evpoller_event::get_outputbuf(size_t* sz)
{
	auto* impl = reinterpret_cast<evpoller_event_impl*>(this);
	if (NULL == impl) return NULL;
	return impl->get_outputbuf(sz);
}

int evpoller_event::read_inputbuf(void* buf, size_t sz)
{
	auto* impl = reinterpret_cast<evpoller_event_impl*>(this);
	if (NULL == impl) return -EINVALID;
	return impl->read_inputbuf(buf, sz);
}

int evpoller_event::read_outputbuf(void* buf, size_t sz)
{
	auto* impl = reinterpret_cast<evpoller_event_impl*>(this);
	if (NULL == impl) return -EINVALID;
	return impl->read_outputbuf(buf, sz);
}

int evpoller_event::write_inputbuf(void* buf, size_t sz)
{
	auto* impl = reinterpret_cast<evpoller_event_impl*>(this);
	if (NULL == impl) return -EINVALID;
	return impl->write_inputbuf(buf, sz);
}

int evpoller_event::write_outputbuf(void* buf, size_t sz)
{
	auto* impl = reinterpret_cast<evpoller_event_impl*>(this);
	if (NULL == impl) return -EINVALID;
	return impl->write_outputbuf(buf, sz);
}

evpoller::evpoller()
: _data(NULL)
{
	auto* impl = reinterpret_cast<evloop_impl*>(evloop::inst());
	_data = reinterpret_cast<void*>(new evpoller_impl(impl));
	assert(NULL != _data);
}

evpoller::~evpoller()
{
	if (_data) {
		auto* obj = reinterpret_cast<evpoller_impl*>(_data);
		delete obj;
		_data = NULL;
	}
}

evpoller_event* evpoller::create_event(int evid, ...)
{
	auto* impl = reinterpret_cast<evpoller_impl*>(_data);
	if (NULL == impl) return NULL;

	va_list vl;
	va_start(vl, evid);
	auto* ret = impl->create_event(evid, vl);
	va_end(vl);

	return reinterpret_cast<evpoller_event*>(ret);
}

int evpoller::poll(int timeout)
{
	auto* impl = reinterpret_cast<evpoller_impl*>(_data);
	if (NULL == impl) return -EINVALID;
	return impl->poll(timeout);
}

int evpoller::reset(void)
{
	auto* impl = reinterpret_cast<evpoller_impl*>(_data);
	if (NULL == impl) return -EINVALID;
	impl->reset();
	return 0;
}

evpoller_event* evpoller::get_triggered_event(void)
{
	auto* impl = reinterpret_cast<evpoller_impl*>(_data);
	if (NULL == impl) return NULL;
	return reinterpret_cast<evpoller_event*>
		(impl->get_triggered_event());
}

}} // end of namespace zas::utils
#endif // (defined(UTILS_ENABLE_FBLOCK_EVLOOP) && defined(UTILS_ENABLE_FBLOCK_EVLCLIENT) && defined(UTILS_ENABLE_FBLOCK_BUFFER))
/* EOF */
