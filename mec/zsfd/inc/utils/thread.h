/** @file thread.h
 * Definition of the thread
 */

#include "utils/utils.h"
#if (defined(UTILS_ENABLE_FBLOCK_THREAD) && defined(UTILS_ENABLE_FBLOCK_CMDLINE))

#ifndef __CXX_ZAS_UTILS_THREAD_H__
#define __CXX_ZAS_UTILS_THREAD_H__

#include "utils/utils.h"

namespace zas {
namespace utils {

enum {
	thread_attr_joinable = 1,
	thread_attr_detached,
};

class UTILS_EXPORT thread
{
public:
	void* operator new(size_t);
	void operator delete(void*);

	/**
	  Create a thread with a thread name
	  @param name specify the thread anme
	  */
	thread(const char* name = NULL);

	/**
	  Release the object
	 */
	void release(void);

public:

	/**
	  Check if the current running thread
	  is the current thread object
	  @return true means it is the thread object
	  */
	bool is_myself(void);
	
	/**
	  Set the thread as detached
	  Once it is detached, it will never be made
	  joinable again
	  */
	void detach(void);

	/**
	  Get the detach status
	  */
	int get_detachstate(void);

	/**
	  Set attribute
	  
	  @param the attribute to be set
	  @return the pointer to the thread object
	  */
	thread* setattr(int attr);

	/**
	  join the thread
	  @return 0 for success
	 */
	int join(void);

	/**
	  Start running the thread

	  @return 0 for success or an error occurred
	  */
	int start(void);

	/**
	  The callback to be run in the thread
	  note this is the only function to be
	  executed in the new thread

	  @return exitcode
	  */
	virtual int run(void);

	/**
	  Check if the thread is created and running
	  @return true means running
	  */
	bool running(void);

protected:
	void* _data;
	virtual ~thread();
	ZAS_DISABLE_EVIL_CONSTRUCTOR(thread);
};

class looper_thread;

class UTILS_EXPORT looper_task
{
public:
	looper_task();
	virtual ~looper_task();

	/**
	  please override this method to implement
	  your own task worker
	  */
	virtual void do_action(void);

	/**
	  Release the task object
	  used to customize the release operation
	  */
	virtual void release(void);

	/**
	  Add the task to the looper thread
	  @param thd the looper thread
	  @return 0 for success
	  */
	int addto(looper_thread* thd);

private:
	void* _data[4];
	ZAS_DISABLE_EVIL_CONSTRUCTOR(looper_task);
};

class UTILS_EXPORT looper_thread : public thread
{
	friend class looper_task;
public:
	/**
	  create a looper thread
	  @param name the name of the thread
	  @param max_pending new task will failed to be added
			to the task queue if the pending tasks exceed
			this value. 0 means no limitation
	  @param exit_timeout the thread will exit automatically
	  		after exit_timeout period without pending tasks
	 */
	looper_thread(const char* name = NULL,
		int max_pending = 0,
		int exit_timeout = 0);
	~looper_thread();
	int run(void);

	/**
	  do something when the before the looper
	  thread exits
	  @return the exit code of the thread
	 */
	virtual int on_exit(void);

	/**
	  Add a new task to the task queue
	  @param task the task to be appended
	  @return 0 for success
	  */
	int add(looper_task* task);

	/**
	  Get the task count
	  @return the count of pending tasks
	 */
	int get_task_count(void);

	/**
	  Stop the thread looper
	  @return 0 for success
	 */
	int stop(void);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(looper_thread);
};

/**
  Get the thread id
  @return the thread id
  */
UTILS_EXPORT unsigned long int gettid(void);

}} // end of namespace zas::utils
#endif /* __CXX_ZAS_UTILS_THREAD_H__ */
#endif // (defined(UTILS_ENABLE_FBLOCK_THREAD) && defined(UTILS_ENABLE_FBLOCK_CMDLINE))
/* EOF */
