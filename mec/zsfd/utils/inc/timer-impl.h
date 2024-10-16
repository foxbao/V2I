/** @file timer-impl.h
 * definition of timer manager
 */

#ifndef __CXX_ZAS_UTILS_TIMER_IMPL_H__
#define __CXX_ZAS_UTILS_TIMER_IMPL_H__

#include "std/list.h"
#include "utils/mutex.h"

namespace zas {
namespace utils {

class timer;
class timer_impl;

struct timer_list
{
	uint32_t index;
	listnode_t *timer_list;
};

class timermgr_impl
{
	friend class timer_impl;
public:
	timermgr_impl(int interval);
	~timermgr_impl();

	static timermgr_impl* getdefault(void);

	// create a timer
	timer_impl* create_timer(timer* t, uint32_t timelen);

	void periodic_runner(void);

	mutex& getmutex(void) {
		return syncobj;
	}

	int getfd(void) {
		return timerfd;
	}

private:
	void timer_init(void);
	void start_global_timer(void);
	int check_start_global_timer(void);

	void stop_global_timer(void);
	void check_stop_global_timer(void);

	// timer list runner
	void run_timer_list(void);
	listnode_t* get_timer_list(timer_impl* tmr);
	void move_timers(timer_list *tv);
	void run_timer(listnode_t *tmrlist);
	void steprun(void);

private:
	int timerfd;
	int32_t active_timer_count;
	unsigned long int volatile jiffies;
	unsigned long int volatile timer_jiffies;
	long prev, delta, min_interval;
	mutex syncobj;
	timer_list t1, t2, t3, t4, t5, *tl[5];
};

}} // end of namespace zas::utils
#endif // __CXX_ZAS_UTILS_TIMER_IMPL_H__
/* EOF */
