/*
 * implementation of event loop
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_EVENTLOOP

#include <new>
#include <errno.h>
#include <unistd.h>
#ifdef QNX_PLATEFORM
#include "qnx-poller.h"
#else
#include <sys/epoll.h>
#endif
#include "inc/eventloop-impl.h"

#ifndef QNX_PLATEFORM
#define __close ::close
#define __write ::write
#define __read ::read
#endif

namespace zas {
namespace utils {

/* eventloop_impl */
eventloop_impl::eventloop_impl(int tmr_minitvl)
: _epollfd(-1), _flags(0)
, _tmrmgr_event_p(tmr_minitvl)
{
	// create the epoller
	_epollfd = epoll_create1(EPOLL_CLOEXEC);
	assert(_epollfd >= 0);

	// add the exit event
	int ret = attach_event(&_exit_event_p);
	assert(ret == 0);

	// add the timermgr event
	ret = attach_event(&_tmrmgr_event_p);
	assert(ret == 0);
}

eventloop_impl::~eventloop_impl()
{
	// detach all events
	while (!_events.empty()) {
		auto itr = _events.begin();
		detach_event(*itr);
	}
	if (_epollfd >= 0) {
		__close(_epollfd); _epollfd = -1;
	}
}

int eventloop_impl::epoll_fdctrl(int epollfd, event_base* p, int op, uint32_t events)
{
	int fd = p->getfd();
	if (fd < 0) {
		return -EINVALID;
	}
	epoll_event ep;
	ep.events = events;
	ep.data.ptr = p;
	return epoll_ctl(epollfd, op, fd, &ep);
}

int eventloop_impl::epoll_del(int efd, int fd)
{
	int ret = epoll_ctl(efd, EPOLL_CTL_DEL, fd , nullptr);
	if (ret == -1 && errno == ENOENT) return 0;
	return ret;
}

void eventloop_impl::exit_event::on_alarm(void) {
	ancestor()->_f.exit_requested = 1;
}

int eventloop_impl::attach_event(event_base* p, bool enable_input, bool enable_output)
{
	if (nullptr == p) {
		return -EINVALID;
	}

	if (!_events.emplace(p).second) {
		return -EEXISTS;
	}
	
	uint32_t events = EPOLLET | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
	if (enable_input) {
		events |= EPOLLIN;
	}
	if (enable_output) {
		events |= EPOLLOUT;
	}

	int ret = epoll_fdctrl(_epollfd, p, EPOLL_CTL_ADD, events);

	if (!ret) {
		p->addref();
		p->_evl = reinterpret_cast<eventloop*>(this);
	}
	return ret;
}

int eventloop_impl::attach_event(event_base* p)
{
	if (nullptr == p) {
		return -EINVALID;
	}

	if (!_events.emplace(p).second) {
		return -EEXISTS;
	}

	int ret = epoll_fdctrl(_epollfd, p, EPOLL_CTL_ADD,
		EPOLLET | EPOLLIN | EPOLLOUT
		| EPOLLERR | EPOLLHUP | EPOLLRDHUP);

	if (!ret) {
		p->addref();
		p->_evl = reinterpret_cast<eventloop*>(this);
	} else {
		// fail to add fd to epoll
		_events.erase(p);
	}
	return ret;
}

int eventloop_impl::detach_event(event_base* p)
{
	if (nullptr == p) {
		return -EINVALID;
	}

	if (!_events.erase(p)) {
		return -ENOTEXISTS;
	}

	int fd = p->getfd();
	if (fd < 0) {
		return -EINVALID;
	}
	int ret = epoll_del(_epollfd, fd);
	assert(ret == 0);

	p->_evl = nullptr;
	ret = p->release();
	assert(ret >= 0);

	return ret;
}

int eventloop_impl::run(void)
{
	// set as running
	_f.running = 1;

	// start running of the poller
	epoll_event events[EPOLL_MAX_EVENTS];
	while (!_f.exit_requested) {
		int count = epoll_wait(_epollfd, events, EPOLL_MAX_EVENTS, -1);

		// check interrupt
		if (count == -1 && errno == EINTR) {
			continue;
		}

		// make sure all events object is protected
		// by increasing its refcnt. the refcnt will be
		// restored later
		for (int i = 0; i < count; i++) {
			auto p = (event_base*)events[i].data.ptr;
			if (nullptr != p) {
				auto ret = p->addref();
				assert(ret > 1);
			}
		}

		for (int i = 0; i < count; i++)
		{
			auto p = (event_base*)events[i].data.ptr;
			if (nullptr == p) {
				continue;
			}
		
			auto evs = events[i].events;

			// if the hangup or error happened, we will not
			// handle data input/output even though there may
			// be some data left in the buffer to be read or write
			if (evs & (EPOLLRDHUP | EPOLLHUP))
			{
				fprintf(stdout, "EPOLLRDHUP/EPOLLHUP occurred\n");
				// remove all pending data if we have
				auto fd = p->getfd();
				if (fd >= 0) {
					if (!p->on_input()) {
						p->on_default(events[i].events);
					}
					detach_event(p);
				}
				p->on_hungup();
				p->release();
				continue;
			}

			if (evs & EPOLLERR) {
				fprintf(stdout, "EPOLLERR occurred");
				epoll_del(_epollfd, p->getfd());
				p->on_error();
				p->release();
				continue;
			}
	
			epoll_handle_rw(p, evs);
			// restore the refcnt increased earlier
			p->release();
		}
	}
	// reset the exit request flag
	_f.running = _f.exit_requested = 0;
	return 0;
}

void eventloop_impl::epoll_handle_rw(event_base* p, uint32_t events)
{
	int handled = 0;
	auto fd = p->getfd();

	if (events & EPOLLPRI) {
		// pass
	}
	if (events & EPOLLIN) {
		if (fd >= 0) {
			handled += (p->on_input() != 0);
		}
		// check if p is still available
		auto refcnt = p->getref();
		assert(refcnt >= 1);
		if (refcnt == 1) {
			return;
		}
	}
	if (events & EPOLLOUT) {
		if (fd >= 0) {
			handled += (p->on_output() != 0);
		}
		// check if p is still available
		auto refcnt = p->getref();
		assert(refcnt >= 1);
		if (refcnt == 1) {
			return;
		}
	}
	if (fd >= 0 && !handled) {
		p->on_default(events);
	}
}

/* eventloop */
eventloop* eventloop::create(int timer_min_interval)
{
	auto impl = new (std::nothrow) eventloop_impl(timer_min_interval);
	if (nullptr == impl) {
		return nullptr;
	}
	return reinterpret_cast<eventloop*>(impl);
}

void eventloop::release(void)
{
	auto impl = reinterpret_cast<eventloop_impl*>(this);
	if (nullptr == impl) return;
	delete impl;
}

int eventloop::attach_event(event_ptr ev)
{
	auto impl = reinterpret_cast<eventloop_impl*>(this);
	if (nullptr == impl) {
		return -EINVALID;
	}
	return impl->attach_event(ev.get());
}

int eventloop::attach_event(event_ptr ev, bool enable_input, bool enable_output)
{
	auto impl = reinterpret_cast<eventloop_impl*>(this);
	if (nullptr == impl) {
		return -EINVALID;
	}
	return impl->attach_event(ev.get(), enable_input, enable_output);
}

int eventloop::detach_event(event_ptr ev)
{
	auto impl = reinterpret_cast<eventloop_impl*>(this);
	if (nullptr == impl) {
		return -EINVALID;
	}
	return impl->detach_event(ev.get());
}

int eventloop::run(void)
{
	auto impl = reinterpret_cast<eventloop_impl*>(this);
	if (nullptr == impl) {
		return -EINVALID;
	}
	return impl->run();
}

void eventloop::terminate(void)
{
	auto impl = reinterpret_cast<eventloop_impl*>(this);
	if (nullptr == impl) {
		return;
	}
	impl->terminate();
}

timermgr* eventloop::get_timermgr(void)
{
	auto impl = reinterpret_cast<eventloop_impl*>(this);
	if (nullptr == impl) {
		return nullptr;
	}
	return impl->get_timermgr();
}

}} // end of namespace zas::utils
#endif // UTILS_ENABLE_FBLOCK_EVENTLOOP
/* EOF */
