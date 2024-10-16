/*
 * definition of event loop
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_EVENTLOOP

#ifndef __CXX_ZAS_UTILS_EVENT_LOOP_H__
#define __CXX_ZAS_UTILS_EVENT_LOOP_H__

#include "utils/events.h"

namespace zas {
namespace utils {

class timermgr;

class UTILS_EXPORT eventloop
{
public:
	eventloop() = delete;
	~eventloop() = delete;

	/**
	 * constructor
	 * @param timer_min_interval set the minimal interval
	 * 	  for the internal default timermgr
	 */
	static eventloop* create(int timer_min_interval = 0);

	/**
	 * release the object
	 */
	void release(void);

	/**
	 * terminate the event loop
	 */
	void terminate(void);

	/**
	 * get the timer manager which could be used
	 * to create timers
	 */
	timermgr* get_timermgr(void);

	/**
	 * attach an event
	 * @param event pointer
	 * @return 0 for success
	 */
	int attach_event(event_ptr ev);

	/**
	 * attach an event
	 * @param event pointer
	 * @param enable_input enable input data to the event
	 * @param enable_output enable event to output data
	 * @return 0 for success
	 */
	int attach_event(event_ptr ev, bool enable_input, bool enable_output);

	/**
	 * detach an event
	 * @param event the pointer of the event to detach
	 * @return 0 for success
	 */
	int detach_event(event_ptr ev);

	/**
	 * running the event loop
	 * @return 0 for success
	 */
	int run(void);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(eventloop);
};

}} // end of namespace zas::utils
#endif // __CXX_ZAS_UTILS_EVENT_LOOP_H__
#endif // UTILS_ENABLE_FBLOCK_EVENTLOOP
/* EOF */