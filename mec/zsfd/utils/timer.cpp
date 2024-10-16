/** @file timer.cpp
 * implementation of the timer
 */

#include "utils/utils.h"
#if (defined(UTILS_ENABLE_FBLOCK_TIMER) && defined(UTILS_ENABLE_FBLOCK_CMDLINE))

#include <limits.h>
#include <time.h>
#ifdef QNX_PLATEFORM
#include "qnx-poller.h"
#else
#include <sys/timerfd.h>
#endif

#include "utils/timer.h"
#include "inc/timer-impl.h"

namespace zas {
namespace utils {

class timer_impl;

timermgr_impl::timermgr_impl(int interval)
: timerfd(-1)
, active_timer_count(0)
, jiffies(0)
, timer_jiffies(0)
, prev(0), delta(0)
, min_interval(interval)
{
	t1.timer_list = NULL;
	timer_init();
	tl[0] = &t1;
	tl[1] = &t2;
	tl[2] = &t3;
	tl[3] = &t4;
	tl[4] = &t5;
}

timermgr_impl::~timermgr_impl()
{
	// todo:
	// release all timer object

	if (t1.timer_list) {
		free(t1.timer_list);
		t1.timer_list = NULL;
	}
}

void timermgr_impl::timer_init(void)
{
	auto_mutex am(syncobj);
	if (t1.timer_list) return;

	// initialize the timer queue
	uint32_t i;
	listnode_t* tmr_list = (listnode_t*)malloc(512 * sizeof(listnode_t));
	if (!tmr_list) {
		fputs("timer_init: out of memory.\n", stderr);
		exit(9998);
	}

	for (i = 0; i < 512; i++)
		listnode_init(tmr_list[i]);

	t1.index = t2.index = t3.index = t4.index = t5.index = 0;

	t1.timer_list = tmr_list;
	t2.timer_list = tmr_list + 256;
	t3.timer_list = tmr_list + 256 + 64;
	t4.timer_list = tmr_list + 256 + 64 + 64;
	t5.timer_list = tmr_list + 256 + 64 + 64 + 64;

	// init the timer fd
	timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	assert(timerfd != -1);
}

timermgr_impl* timermgr_impl::getdefault(void)
{
	static timermgr_impl* ti = NULL;
	if (NULL == ti) {
		// set minimum interval as 5
		ti = new timermgr_impl(5);
		assert(NULL != ti);
	}
	return ti;
}

void timermgr_impl::start_global_timer(void)
{
	if (timerfd == -1) return;
	itimerspec new_value;

	new_value.it_value.tv_sec = min_interval / 1000;
	new_value.it_value.tv_nsec = (min_interval % 1000) * 1000000;
		
	new_value.it_interval.tv_sec = min_interval / 1000;
	new_value.it_interval.tv_nsec = (min_interval % 1000) * 1000000;
	int ret = timerfd_settime(timerfd, 0, &new_value, NULL);
	assert(ret != -1);		
}

int timermgr_impl::check_start_global_timer(void)
{
	int32_t tmrcnt = __sync_fetch_and_add(&active_timer_count, 1);
	if (!tmrcnt) {
		start_global_timer();
	}
	return 0;
}

void timermgr_impl::stop_global_timer(void)
{
	if (timerfd == -1) return;
	itimerspec new_value = {0};
	printf("timermgr_impl stop_global_timer\n");
	int ret = timerfd_settime(timerfd, 0, &new_value, NULL);
	assert(ret != -1);
}

void timermgr_impl::check_stop_global_timer(void)
{
	int32_t tmrcnt = __sync_sub_and_fetch(&active_timer_count, 1);
	if (!tmrcnt) {
		stop_global_timer();
	}
}

void timermgr_impl::steprun(void)
{
	++jiffies;
	run_timer_list();
}

class timer_impl
{
	friend class timermgr_impl;
public:
	timer_impl(timer* t, timermgr_impl* tmgr, unsigned long itvl, unsigned long exp)
	: interval(itvl)
	, expire(exp)
	, tmr(t)
	, tmrmgr(tmgr)
	{
		listnode_init(list);
	}

	~timer_impl()
	{
		stop();
	}

	bool start(void)
	{
		if (!tmrmgr || !interval) return false;

		// need lock
		auto_mutex am(tmrmgr->getmutex());
		if (!listnode_issingle(list))
			return false;

		expire = tmrmgr->jiffies + interval;
		listnode_t *_list = tmrmgr->get_timer_list(this);
		listnode_add(*_list, list);

		tmrmgr->check_start_global_timer();
		return true;
	}

	void stop(bool longstop = false)
	{
		if (nullptr == tmrmgr) {
			return;
		}
		// need lock
		auto_mutex am(tmrmgr->getmutex());
		if (listnode_issingle(list))
			return;
		listnode_del(list);
		if (longstop) tmrmgr->check_stop_global_timer();
	}

	void restart(void)
	{
		stop(false);
		start();
	}

	void setinterval(uint32_t intv)
	{
		if (nullptr == tmrmgr) {
			return;
		}
		stop();
		interval = (intv + tmrmgr->min_interval - 1)
			/ tmrmgr->min_interval;
	}

	timermgr_impl* get_timermgr(void) {
		return tmrmgr;
	}

private:
    unsigned long int interval;
	unsigned long int expire;
	listnode_t list;
	timer* tmr;
	timermgr_impl* tmrmgr;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(timer_impl);
};

timer_impl* timermgr_impl::create_timer(timer* t, uint32_t timelen)
{
	unsigned long itvl = (timelen + min_interval - 1)
		/ min_interval;
	unsigned long exp = jiffies + itvl;
	return new timer_impl(t, this, itvl, exp);
}

void timermgr_impl::run_timer_list(void)
{
	while (jiffies >= timer_jiffies)
	{
		if (!t1.index)
		{
			uint32_t i = 1;
			do {
				move_timers(tl[i]);
			} while (tl[i]->index == 1 && ++i < 5);
		}

		run_timer(t1.timer_list + t1.index);
		t1.index = (t1.index + 1) & 255;
		++timer_jiffies; 
	}
}

listnode_t* timermgr_impl::get_timer_list(timer_impl* tmr)
{
	listnode_t *_list = NULL;
	unsigned long int _expire = tmr->expire;
	unsigned long int t = _expire - timer_jiffies;

	if (t < 256)
		_list = t1.timer_list + (_expire & 255);

	else if (t < (1 << (6 + 8)))
	{
		t = (_expire >> 8) & ((1 << 6) - 1);
		_list = t2.timer_list + t;
	}
	else if (t < (1 << (12 + 8)))
	{
		t = (_expire >> (8 + 6)) & ((1 << 6) - 1);
		_list = t3.timer_list + t;
	}
	else if (t < (1 << (18 + 8)))
	{
		t = (_expire >> (8 + 12)) & ((1 << 6) - 1);
		_list = t4.timer_list + t;
	}
	else if ((int)t < 0)
	{
		// can happen if you add a timer with expires == jiffies,
		// or you set a timer to go off in the past
		_list = t1.timer_list + t1.index;
	}
	else if (t <= ULONG_MAX)
	{
		t = (_expire >> (8 + 3 * 6)) & ((1 << 6) - 1);
		_list = t5.timer_list + t;
	}
	return _list;
}

void timermgr_impl::move_timers(timer_list *tv)
{
	listnode_t *head;
	listnode_t *curr;
	listnode_t *next;
	
	head = tv->timer_list + tv->index;
	curr = head->next;

	while (curr != head)
	{
		timer_impl *tmp;
		tmp = list_entry(timer_impl, list, curr);
		next = curr->next;
		listnode_del(*curr);
	
		listnode_t* _list = get_timer_list(tmp);
		listnode_add(*_list, tmp->list);
		curr = next;
	}
	tv->index = (tv->index + 1) & 63;
}

void timermgr_impl::run_timer(listnode_t *tmrlist)
{
	while (!listnode_isempty(*tmrlist))
	{
		listnode_t *nxt = tmrlist->next;
		timer_impl *tmr = list_entry(timer_impl, list, nxt);
		listnode_del(*nxt);
		if (tmr->tmr) tmr->tmr->on_timer();
	}
}

void timermgr_impl::periodic_runner(void)
{
	const long interval = 1000000 * min_interval;
	if (!prev) {
		prev = gettick_nanosecond();
		steprun();
	}
	else {
		long curr = gettick_nanosecond();
		for (delta += curr - prev;
			delta >= interval; delta -= interval) {
			steprun();
		}
		prev = curr;
	}
}

timer::timer()
{
	timermgr_impl* ti = timermgr_impl::getdefault();
	assert(NULL != ti);

	_data = reinterpret_cast<void*>(ti->create_timer(this, 0));
	assert(NULL != _data);
}

/**
  create the timer with the interval in ms
  @param intv the interval in ms
  */	
timer::timer(uint32_t intv)
{
	timermgr_impl* ti = timermgr_impl::getdefault();
	assert(NULL != ti);

	_data = reinterpret_cast<void*>(ti->create_timer(this, intv));
	assert(NULL != _data);
}

timer::timer(timermgr* tmrmgr)
{
	if (NULL == tmrmgr)
		return;
	timermgr_impl* ti = reinterpret_cast<timermgr_impl*>(tmrmgr);
	_data = reinterpret_cast<void*>(ti->create_timer(this, 0));
	assert(NULL != _data);
}

timer::timer(timermgr* tmrmgr, uint32_t intv)
{
	if (NULL == tmrmgr)
		return;
	timermgr_impl* ti = reinterpret_cast<timermgr_impl*>(tmrmgr);
	_data = reinterpret_cast<void*>(ti->create_timer(this, intv));
	assert(NULL != _data);
}

timer::~timer()
{
	if (_data) {
		timer_impl* t = reinterpret_cast<timer_impl*>(_data);
		delete t;
		_data = NULL;
	}
}

/**
  Start the timer
  @return true for success
  */
bool timer::start(void)
{
	timer_impl* t = reinterpret_cast<timer_impl*>(_data);
	if (NULL == t) return false;
	return t->start();
}

/**
  Stop the timer
  */	
void timer::stop(void)
{
	timer_impl* t = reinterpret_cast<timer_impl*>(_data);
	if (NULL == t) return;
	t->stop();
}

/**
  restart the timer
  */	
void timer::restart(void)
{
	timer_impl* t = reinterpret_cast<timer_impl*>(_data);
	if (NULL == t) return;
	return t->restart();
}

/**
  re-set the timer interval in ms
  @param intv the interval in ms
  */	
void timer::setinterval(uint32_t intv)
{
	timer_impl* t = reinterpret_cast<timer_impl*>(_data);
	if (NULL == t) return;
	t->setinterval(intv);
}

/**
 * Get the timermgr of the timer
 * @return timermgr
 */
timermgr* timer::get_timermgr(void)
{
	timer_impl* t = reinterpret_cast<timer_impl*>(_data);
	if (NULL == t) return nullptr;
	return reinterpret_cast<timermgr*>(t->get_timermgr());
}

int timermgr::getfd(void)
{
	timermgr_impl* ti = reinterpret_cast<timermgr_impl*>(this);
	return ti->getfd();
}

void timermgr::release(void)
{
	timermgr_impl* ti = reinterpret_cast<timermgr_impl*>(this);
	delete ti;
}

void timermgr::periodic_runner(void)
{
	timermgr_impl* ti = reinterpret_cast<timermgr_impl*>(this);
	ti->periodic_runner();
}

timermgr* create_timermgr(int interval)
{
	return reinterpret_cast<timermgr*>(
		new timermgr_impl(interval)
	);
}

/**
  the timer handler
  */
void timer::on_timer(void)
{
}

long gettick_millisecond(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

long gettick_nanosecond(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long)(ts.tv_sec * 1000000000 + ts.tv_nsec);
}

void msleep(unsigned int msec)
{
	struct timespec req, rem;
	req.tv_sec = msec / 1000;
	req.tv_nsec = (msec % 1000) * 1000000;
	nanosleep(&req, &rem);
}

}} // end of namespace zas::utils
#endif // (defined(UTILS_ENABLE_FBLOCK_TIMER) && defined(UTILS_ENABLE_FBLOCK_CMDLINE))
/* EOF */
