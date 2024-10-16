/** @file timer.h
 * Definition of the timer and time related features
 */

#include "utils/utils.h"
#if (defined(UTILS_ENABLE_FBLOCK_TIMER) && defined(UTILS_ENABLE_FBLOCK_CMDLINE))

#ifndef __CXX_ZAS_UTILS_TIMER_H__
#define __CXX_ZAS_UTILS_TIMER_H__

namespace zas {
namespace utils {

class timermgr;

class UTILS_EXPORT timer
{
public:
	/**
	  Create a timer using the default timer manager
	  */
	timer();

	/**
	  create the timer with the interval in ms
	  @param intv the interval in ms
	  */	
	timer(uint32_t intv);

	/**
	  Create a timer with a specific timer manager
	  @param timer the timer manager
	 */
	timer(timermgr* tmrmgr);

	/**
	  Create a timer with a specific timer manager
	  @param timer the timer manager
	  @param intv interval for the timer
	 */
	timer(timermgr* tmrmgr, uint32_t intv);

	/**
	 * Get the timermgr of the timer
	 * @return timermgr
	 */
	timermgr* get_timermgr(void);

	virtual ~timer();
	ZAS_DISABLE_EVIL_CONSTRUCTOR(timer);

public:

	/**
	  Start the timer
	  @return true for success
	  */
	bool start(void);

	/**
	  Stop the timer
	  */	
	void stop(void);

	/**
	  restart the timer
	  */	
	void restart(void);

	/**
	  re-set the timer interval in ms
	  @param intv the interval in ms
	  */	
	void setinterval(uint32_t intv);

	/**
	  the timer handler
	  */
	virtual void on_timer(void);

private:
	void* _data;
};

class UTILS_EXPORT timermgr
{
public:
	timermgr() = delete;
	~timermgr() = delete;

	/**
	  Get the timerfd
	  this method is platform specific
	 */
	int getfd(void);

	/**
	  release the object
	 */
	void release(void);

	/**
	  Periodic runner of the timer manager
	  this is to drive all timers in the manager
	 */
	void periodic_runner(void);

	ZAS_DISABLE_EVIL_CONSTRUCTOR(timermgr);
};

/**
  Create a timer manager
  @param interval the minimum interval
  @return the timer manager
 */
UTILS_EXPORT timermgr* create_timermgr(int interval);

/**
  Get the current tickcount in millisecond precise
  @return the tick in millisecond
  */
UTILS_EXPORT long gettick_millisecond(void);

/**
  Get the current tickcount in nanosecond precise
  @return the tick in nanosecond
  */
UTILS_EXPORT long gettick_nanosecond(void);

/**
  sleep for millisecond
  @param msec millisecond to be sleep
  */
UTILS_EXPORT void msleep(unsigned int msec);

}} // end of namespace zas::utils
#endif // __CXX_ZAS_UTILS_TIMER_H__
#endif // (defined(UTILS_ENABLE_FBLOCK_TIMER) && defined(UTILS_ENABLE_FBLOCK_CMDLINE))
/* EOF */
