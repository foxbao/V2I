/** @file thread.cpp
 * implementation of the thread
 */

#include "utils/utils.h"
#if (defined(UTILS_ENABLE_FBLOCK_THREAD) && defined(UTILS_ENABLE_FBLOCK_CMDLINE))

#include <sys/time.h>
#include <pthread.h>
#include <errno.h>

#include "std/list.h"
#include "utils/wait.h"
#include "utils/thread.h"
#include "utils/mutex.h"

namespace zas {
namespace utils {

static mutex thdops_mut;
static listnode_t threadlist = LISTNODE_INITIALIZER(threadlist);

unsigned long int gettid() {
	return (unsigned long int)pthread_self();
}

static void* thread_routine(void* arg);

class thread_impl
{
public:
	thread_impl(thread* t, const char* name = NULL)
	: _thdobj(t), _f(0), _thd(0), _thdname(NULL) {
		assert(NULL != _thdobj);
		pthread_attr_init(&_attr);

		// save the thread name
		if (name && *name) {
			char* n = new char [strlen(name) + 1];
			assert(NULL != n);
			strcpy(n, name);
			_thdname = n;
		}

		// add the thread into list, need lock
		auto_mutex am(thdops_mut);
		listnode_add(threadlist, _ownerlist);
	}

	~thread_impl() {
		release();
	}

	bool check_set_can_release(void)
	{
		auto_mutex am(thdops_mut);
		return check_set_can_release_unlocked();
	}

	int release(void)
	{
		// we need lock
		auto_mutex am(thdops_mut);

		// see if the thread is still in running
		if (!check_set_can_release_unlocked()) {
			return -1;
		}

		if (!_flags.destroying)
		{
			_flags.destroying = 1;
			pthread_attr_destroy(&_attr);

			// must detach the thread or we will miss
			// thread resource release
			detach();

			// release the thread name if we have
			if (_thdname) {
				delete [] _thdname;
				_thdname = NULL;
			}

			// remove this node from list
			if (!listnode_isempty(_ownerlist)) {
				listnode_del(_ownerlist);
			}
		}

		if (_flags.destroy_pending) {
			_flags.destroy_pending = 0;
			assert(NULL != _thdobj);
			_thdobj->release();
		}
		_thdobj = NULL;
		
		// 0 indicate that the "thread" object
		// could be released safely
		return 0;
	}

	bool is_myself(void) {
		return (_thd == pthread_self())
		? true : false;
	}

	void detach(void) {
		if (!_thd) return;
		pthread_detach(_thd);
	}

	int get_detachstate(void)
	{
		if (!_thd) return -EINVALID;
		int val;
		pthread_attr_getdetachstate(&_attr, &val);

		if (val == PTHREAD_CREATE_DETACHED) {
			return thread_attr_detached;
		}
		else if (val == PTHREAD_CREATE_JOINABLE) {
			return thread_attr_joinable;
		}
		return -EINVALID;
	}

	thread* setattr(int attr)
	{
		if (!_thd) return _thdobj;
		switch (attr) {
		case thread_attr_joinable:
			pthread_attr_setdetachstate(&_attr, PTHREAD_CREATE_JOINABLE);
			break;

		case thread_attr_detached:
			pthread_attr_setdetachstate(&_attr, PTHREAD_CREATE_DETACHED);
			break;
		}
		return _thdobj;
	}

	int start(void)
	{
		// see if we already start
		if (_thd || _flags.started) {
			return -EEXIST;
		}
		_flags.started = 1;

		// start the thread
		if (pthread_create(&_thd, &_attr, thread_routine, (void*)this)) {
			// error occurred, we check if there
			// is a pending destroy request
			return -ELOGIC;
		}
		else return 0;
	}

	int join(void)
	{
		// check if the thread exists
		if (!_thd) return 0;

		void* ret = NULL;
		int stat = pthread_join(_thd, &ret);

		// EDEADLK: a deadlock is detected
		// ESRCH: no thread with the ID is found
		if (stat != EDEADLK && stat != ESRCH) {
			execute_pending_destroy();
		}
		return (int)(size_t)ret;
	}

	thread* getthread(void) {
		assert(NULL != _thdobj);
		return _thdobj;
	}

	bool running(void) {
		return _flags.running ? true : false;
	}

private:

	void set_running(bool r) {
		_flags.running = (r) ? 1 : 0;
	}

	void set_finished(void) {
		_thd = 0;
		set_running(false);
		_flags.started = 0;
	}

	bool check_set_can_release_unlocked(void)
	{
		if (_flags.started) {
			// we set the pending flag and wait
			// for the thread routine to exit in which
			// the real release will happen again
			// note that we need to detach the thread, otherwise
			// the resource of the thread may not released properly
			detach();
			_flags.destroy_pending = 1;
			return false;
		}
		return true;
	}

	void execute_pending_destroy(void)
	{
		// we need lock
		auto_mutex am(thdops_mut);

		if (!_flags.destroy_pending || _flags.started) {
			// no pending destory request
			return;
		}

		assert(!_flags.running);
		delete this;
	}

	static void* thread_routine(void* arg)
	{
		if (!arg) {
			bug("thread_routine with NULL arg.\n");
		}
		thread_impl* thd = reinterpret_cast<thread_impl*>(arg);

		// set the running flag
		thd->set_running(true);
		if (thd->_thdname) {
			printf("thread `%s' start running.\n", thd->_thdname);
		} else {
			printf("thread %lx start running.\n", gettid());
		}

		// run the actual thread routine
		size_t ret = (size_t)thd->getthread()->run();
		
		// check the detach status
		int stat = thd->get_detachstate();

		// finished running
		thd->set_finished();

		if (thread_attr_detached == stat) {
			// we shall manually release the thread object
			// since this is the final chance
			// otherwise, join() will do thread_release()
			thd->execute_pending_destroy();
		}

		if (thd->_thdname) {
			printf("thread `%s' exit with code %ld\n", thd->_thdname, ret);
		} else {
			printf("thread %lx exit with code %ld\n", gettid(), ret);
		}
		return (void*)ret;
	}

private:
	union {
		struct {
			uint32_t started : 1;
			uint32_t running : 1;
			uint32_t destroying : 1;
			uint32_t destroy_pending : 1;
		} _flags;
		uint32_t _f;
	};

	// use to record all thread items
	listnode_t _ownerlist;
	thread* _thdobj;

 	pthread_t _thd;
	pthread_attr_t _attr;
	const char* _thdname;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(thread_impl);
};

class looper_thread_impl;

struct looper_task_data
{
	listnode_t ownerlist;
	looper_task* task;
	looper_thread_impl* looper_thd;
};

class looper_thread_impl : public thread_impl
{
public:
	looper_thread_impl(looper_thread* t, const char* name,
	int max_pending, int exit_timeout)
	: thread_impl(t, name)
	, _looper_thd(t)
	, _pending_tasks(0), _flags(0)
	, _max_pending_tasks(max_pending)
	, _exit_timeout(100) {
		listnode_init(_tasklist);
		if (_max_pending_tasks < 0) {
			_max_pending_tasks = 0;
		}
		if (exit_timeout) {
			if (exit_timeout > _exit_timeout) {
				_exit_timeout = exit_timeout;
			}
			_f.exit_when_timeout = 1;
		}
	}

	~looper_thread_impl() {
		release_all_tasks();
	}

	void detach_task(looper_task_data* tsk)
	{
		if (NULL == tsk) return;

		// we need lock
		_waitobj.lock();
		if (listnode_isempty(tsk->ownerlist)) {
			_waitobj.unlock(); return;
		}
		listnode_del(tsk->ownerlist);
		tsk->looper_thd = NULL;

		// decrease the pending task count
		--_pending_tasks;
		_waitobj.unlock();
	}

	int add(looper_task_data* tskdata)
	{
		if (NULL == tskdata) return -EBADPARM;

		// if we already in the looper thread
		// we run the task directly
		if (is_myself()) {
			auto* task = tskdata->task;
			if (task) {
				task->do_action();
				task->release();
			}
			return 0;
		}

		// we need lock here
		_waitobj.lock();
		if (!listnode_isempty(tskdata->ownerlist)
			|| tskdata->looper_thd) {
			return -ENOTAVAIL;
		}

		if (_max_pending_tasks && _pending_tasks >= _max_pending_tasks) {
			_waitobj.unlock();
			return -ETOOMANYITEMS;
		}

		assert(NULL != tskdata->task);
		tskdata->looper_thd = this;

		bool need_notify = listnode_isempty(_tasklist)
			? true : false;

		// add task to the thread
		listnode_add(_tasklist, tskdata->ownerlist);
		++_pending_tasks;

		if (need_notify) _waitobj.notify();
		_waitobj.unlock();

		return 0;
	}

	int execute(void)
	{
		// handle all pending items before
		// the thread start running
		_waitobj.lock();
		handle_all_pending_tasks();
		_waitobj.unlock();

		for (;;) {
			_waitobj.lock();
			// we check the queue every 100ms
			while (_waitobj.wait(_exit_timeout)
				|| !listnode_isempty(_tasklist)) {
				// handle all pending tasks
				handle_all_pending_tasks();
	
				// see if we need exit the looper
				if (_f.need_stop_looper) {
					_f.need_stop_looper = 0;
					_waitobj.unlock();
					return (_looper_thd) ? _looper_thd->on_exit() : 0;
				}
			}

			// see if we need exit the looper
			if (_f.need_stop_looper || _f.exit_when_timeout) {
				_f.exit_when_timeout = 0;
				_f.need_stop_looper = 0;
				_waitobj.unlock();
				return (_looper_thd) ? _looper_thd->on_exit() : 0;
			}
			_waitobj.unlock();
		}
		// shall never be here
		return 1;
	}

	int stop(void)
	{
		if (!running() || is_myself()) {
			return -ENOTAVAIL;
		}

		_waitobj.lock();
		_f.need_stop_looper = 1;
		_waitobj.notify();
		_waitobj.unlock();
		return 0;
	}

	int get_task_count(void) {
		return _pending_tasks;
	}

private:
	void handle_all_pending_tasks(void)
	{
		while (!listnode_isempty(_tasklist)) {
			listnode_t* node = _tasklist.next;
			listnode_del(*node);

			// decrease the pending task count
			--_pending_tasks;
			_waitobj.unlock();

			// handle the task
			auto* data = list_entry(looper_task_data, ownerlist, node);
			data->looper_thd = nullptr;
			looper_task* task = data->task;
			if (task) {
				task->do_action();
				// release the task
				task->release();
			}

			//lock again for next task handling
			_waitobj.lock();
		}
	}

	void release_all_tasks(void)
	{
		_waitobj.lock();
		while (!listnode_isempty(_tasklist)) {
			listnode_t* node = _tasklist.next;
			looper_task_data* data = list_entry(looper_task_data, ownerlist, node);

			// decrease the pending task count
			--_pending_tasks;
			_waitobj.unlock();

			assert(NULL != data->task);
			data->task->release();
		}
	}

private:

	union {
		uint32_t _flags;
		struct {
			uint32_t need_stop_looper : 1;
			uint32_t exit_when_timeout : 1;
		} _f;
	};

	waitobject _waitobj;
	listnode_t _tasklist;
	int _pending_tasks;
	int _max_pending_tasks;
	int _exit_timeout;
	looper_thread* _looper_thd;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(looper_thread_impl);
};

looper_task::looper_task()
{
	assert(sizeof(looper_task_data) == sizeof(_data));
	looper_task_data* tskdata = reinterpret_cast<looper_task_data*>(_data);

	listnode_init(tskdata->ownerlist);
	tskdata->task = this;
	tskdata->looper_thd = NULL;
}

looper_task::~looper_task()
{
	assert(sizeof(looper_task_data) == sizeof(_data));
	looper_task_data* tskdata = reinterpret_cast<looper_task_data*>(_data);

	// in case of nested call of task->addto() may lead to a task
	// be directly run without assigning a looper thread
	if (nullptr != tskdata->looper_thd) {
		tskdata->looper_thd->detach_task(tskdata);
	}
}

void looper_task::do_action(void) {
}

void looper_task::release(void)
{
	delete this;
}

int looper_task::addto(looper_thread* thd)
{
	if (NULL == thd) {
		return -EBADPARM;
	}

	assert(sizeof(looper_task_data) == sizeof(_data));
	auto* tskdata = reinterpret_cast<looper_task_data*>(_data);

	// add the looper thread
	auto* looper = reinterpret_cast<looper_thread_impl*>(thd->_data);
	if (NULL == looper) return -ELOGIC;
	return looper->add(tskdata);
}

looper_thread::looper_thread(const char* name,
	int max_pending, int exit_timeout)
{
	_data = reinterpret_cast<void*>(
		new looper_thread_impl(this, name,
			max_pending, exit_timeout));
	assert(NULL != _data);
}

looper_thread::~looper_thread()
{
	if (_data) {
		delete reinterpret_cast<looper_thread_impl*>(_data);
		_data = NULL;
	}
}

int looper_thread::stop(void)
{
	auto* looper = reinterpret_cast<looper_thread_impl*>(_data);
	if (NULL == looper) return -ELOGIC;
	return looper->stop();
}

int looper_thread::run(void)
{
	auto* looper = reinterpret_cast<looper_thread_impl*>(_data);
	if (NULL == looper) return -ELOGIC;
	return looper->execute();
}

int looper_thread::on_exit(void)
{
	return 0;
}

int looper_thread::add(looper_task* task)
{
	if (NULL == task)
		return -ELOGIC;
	return task->addto(this);
}

int looper_thread::get_task_count(void)
{
	auto* looper = reinterpret_cast<looper_thread_impl*>(_data);
	if (NULL == looper) return -ELOGIC;
	return looper->get_task_count();	
}

thread::thread(const char* name)
{
	_data = reinterpret_cast<void*>(new thread_impl(this, name));
	assert(NULL != _data);
}

thread::~thread()
{
	if (_data) {
		int ret = reinterpret_cast<thread_impl*>(_data)->release();
		// ret = 0 means the thread object could be released
		// just mark _data as NULL to indicate it
		if (!ret) _data = NULL;
	}
}

void* thread::operator new(size_t sz) {
	return malloc(sz);
}

void thread::operator delete(void* ptr)
{
	auto* thd = reinterpret_cast<thread*>(ptr);
	assert(NULL != thd);

	if (!thd->_data) free(ptr);
}

void thread::release(void)
{
	auto* thd = reinterpret_cast<thread_impl*>(_data);
	if (!thd || !thd->check_set_can_release()) {
		return;
	}
	delete this;
}

void thread::detach(void)
{
	assert(NULL != _data);
	thread_impl* thd = reinterpret_cast<thread_impl*>(_data);
	thd->detach();
}

int thread::get_detachstate(void)
{
	assert(NULL != _data);
	thread_impl* thd = reinterpret_cast<thread_impl*>(_data);
	return thd->get_detachstate();
}

thread* thread::setattr(int attr)
{
	assert(NULL != _data);
	thread_impl* thd = reinterpret_cast<thread_impl*>(_data);
	return thd->setattr(attr);
}

int thread::start(void)
{
	assert(NULL != _data);
	thread_impl* thd = reinterpret_cast<thread_impl*>(_data);
	return thd->start();
}

bool thread::running(void)
{
	assert(NULL != _data);
	thread_impl* thd = reinterpret_cast<thread_impl*>(_data);
	return thd->running();
}

bool thread::is_myself(void)
{
	assert(NULL != _data);
	thread_impl* thd = reinterpret_cast<thread_impl*>(_data);
	return thd->is_myself();
}

int thread::join(void)
{
	assert(NULL != _data);
	thread_impl* thd = reinterpret_cast<thread_impl*>(_data);
	return thd->join();
}

// virtual callback empty implementation
int thread::run(void) {
	return 0;
}

}} // end of namespace zas::utils
#endif // (defined(UTILS_ENABLE_FBLOCK_THREAD) && defined(UTILS_ENABLE_FBLOCK_CMDLINE))
/* EOF */
