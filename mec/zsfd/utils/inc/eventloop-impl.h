/** @file eventloop-impl.h
 * definition of event loop 
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_EVENTLOOP

#ifndef __CXX_ZAS_UTILS_EVENT_LOOP_IMPL_H__
#define __CXX_ZAS_UTILS_EVENT_LOOP_IMPL_H__

#include <set>
#include "utils/eventloop.h"

namespace zas {
namespace utils {

#define EPOLL_MAX_EVENTS	16

class eventloop_impl
{
public:
	eventloop_impl(int);
	~eventloop_impl();

public:
	void terminate(void) {
		if (!_f.running) return;
		_exit_event_p.alarm();
	}

	timermgr* get_timermgr(void) {
		return _tmrmgr_event_p.get_timermgr();
	}

	int run(void);
	int attach_event(event_base*);
	int attach_event(event_base* p, bool, bool);
	int detach_event(event_base*);

private:
	// epoll operator
	int epoll_fdctrl(int, event_base*, int, uint32_t);
	int epoll_del(int efd, int fd);

	void epoll_handle_rw(event_base* p, uint32_t events);

private:
	int _epollfd;
	std::set<event_base*> _events;
	union {
		uint32_t _flags;
		struct {
			uint32_t running : 1;
			uint32_t exit_requested : 1;
		} _f;
	};

	class exit_event : public generic_event
	{
	protected:
		void on_alarm(void);
		int addref(void) {
			return 2;
		}
		int release(void) {
			return 1;
		}
		int getref(void) {
			return 2;
		}

	private:
		eventloop_impl* ancestor(void) {
			return zas_ancestor(eventloop_impl, _exit_event_p);
		}
	} _exit_event_p;

	class def_timermgr_event : public timermgr_event
	{
	public:
		def_timermgr_event(int min_interval)
		: timermgr_event(min_interval
		? min_interval : 5) {}

	protected:
		int addref(void) {
			return 2;
		}
		int release(void) {
			return 1;
		}
		int getref(void) {
			return 2;
		}
	} _tmrmgr_event_p;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(eventloop_impl);
};

}} // end of namespace zas::utils
#endif // end of __CXX_ZAS_UTILS_EVENT_LOOP_IMPL_H__
#endif // UTILS_ENABLE_FBLOCK_EVENTLOOP
/* EOF */