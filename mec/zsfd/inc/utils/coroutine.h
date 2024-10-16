/** @file coroutine.h
 * definition of coroutine object and its manager
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_COROUTINE

#ifndef __CXX_ZAS_UTILS_EVL_COROUTINE_H__
#define __CXX_ZAS_UTILS_EVL_COROUTINE_H__

namespace zas {
namespace utils {

class coroutine_mgr;

class UTILS_EXPORT coroutine
{
	friend class coroutine_impl;
public:
	/**
	  Create a new coroutine and manager it
	  in the specific manager
	  @param manager the coroutine manager managing
	  	the current coroutine
	  @param maxstack the max size of the stack
	  	0 means using minimum stack size (4KB)
	 */
	coroutine(coroutine_mgr* manager, size_t maxstack = 0);
	virtual ~coroutine();

	/**
	  Start the coroutine
	  @return 0 for success
	 */
	int start(void);

	/**
	  Resume the running of the coroutine
	 */
	void resume(void);

	/**
	  Release the control of the coroutine
	  if the coroutine is the only active coroutine, yield
	  will result in switching to the main routine
	  @return if it is not return 0, an error happen
	  	and the coroutine is not yielded
	 */
	int yield(void);

	/**
	  Suspend the coroutine
	  if the coroutine is the only active coroutine, suspend
	  will result in switching to the main routine
	  @return 0 for success
	 */
	int suspend(void);

	/**
	 */
	virtual int run(void);
	virtual void release(void);
	virtual void destroy(void);

private:
	void* _data;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(coroutine);
};

class UTILS_EXPORT coroutine_mgr
{
	friend class coroutine;
public:
	/**
	  Identifiy the max coroutines the manager
	  could manage
	  @param maxcor max coroutine the manager could
		handle, 0 means no limitation
	 */
	coroutine_mgr(int maxcor = 0);
	~coroutine_mgr();

	/**
	  Get the current coroutine
	  @return NULL means the thread is not running
	  	in a coroutine
	 */
	coroutine* get_current(void);

	/**
	  Conduct a schedule between coroutines
	  if the sponser is not a coroutine, the call
	  will select an active coroutine and resume it
	  @return non-zero means there is not available coroutines
	  	that could be scheduled
	 */
	int schedule(void);

private:
	void* _data;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(coroutine_mgr);
};

}} // end of namespace zas::utils
#endif // __CXX_ZAS_UTILS_EVL_COROUTINE_H__
#endif // UTILS_ENABLE_FBLOCK_COROUTINE
/* EOF */
