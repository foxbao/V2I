/** @file coroutine.cpp
 * implementation of coroutine object and its manager
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_COROUTINE

#include <stddef.h>
#include <ucontext.h>

#include "std/list.h"
#include "utils/mutex.h"
#include "utils/thread.h"
#include "utils/coroutine.h"

namespace zas {
namespace utils {

class coroutine_mgr_impl;

enum coroutine_state
{
	coroutine_state_zombie = 0,
	coroutine_state_ready,
	coroutine_state_running,
	coroutine_state_suspend,
};

static void coroutine_mainfunc(uint32_t, uint32_t);

class coroutine_impl
{
	friend class coroutine_mgr;
	friend class coroutine_mgr_impl;
	friend void coroutine_mainfunc(uint32_t, uint32_t);
public:
	coroutine_impl(coroutine* cor, uint32_t maxstack,
		coroutine_mgr_impl* manager);

	~coroutine_impl();

	void recycle(void);

	int bind(coroutine* cor, uint32_t maxstack);
	int start(void);
	void resume(void);
	int yield(void);
	int suspend(void);

private:
	listnode_t _ownerlist;
	coroutine* _cor;
	ucontext_t _ctx;
	coroutine_mgr_impl* _manager;

	int _round;
	int _status;
	uint32_t _stack_size;
	char* _stack;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(coroutine_impl);
};

class coroutine_mgr_impl
{
public:
	coroutine_mgr_impl(int maxcor)
	: _count(0), _total(maxcor), _curr_round(0)
	, _thread(0), _current(NULL)
	{
		if (!_total) _total = 0x7fffffff;
		listnode_init(_active_list);
		listnode_init(_suspend_list);
		listnode_init(_free_list);
	}

	~coroutine_mgr_impl()
	{
		assert(!_thread || _thread == gettid());
		_mut.lock();
		release_list_unlocked(_active_list);
		release_list_unlocked(_suspend_list);
		release_list_unlocked(_free_list);
		_mut.unlock();
	}

	void set_thread(void) {
		if (_thread) return;
		_thread = gettid();
	}

	coroutine_impl* allocate(coroutine* cor, uint32_t maxstack)
	{
		coroutine_impl* ret;
		auto_mutex am(_mut);

		// try to get one from free list
		if (!listnode_isempty(_free_list)) {
			ret = list_entry(coroutine_impl, _ownerlist, \
				_free_list.next);
			listnode_del(ret->_ownerlist);

			// udate the current coroutine info
			ret->bind(cor, maxstack);
			listnode_add(_suspend_list, ret->_ownerlist);
			return ret;
		}

		// check if the manager is full
		if (_count >= _total) return NULL;

		// allocate a new object
		ret = new coroutine_impl(cor, maxstack, this);
		if (NULL == ret) return NULL;

		// add to sleep list, waiting for running
		listnode_add(_suspend_list, ret->_ownerlist);
		++_count;
		return ret;
	}

	void reap(coroutine_impl* cor)
	{
		assert(NULL != cor);
		cor->_cor = NULL;

		auto_mutex am(_mut);
		assert(!listnode_iscleared(cor->_ownerlist));
		listnode_del(cor->_ownerlist);
		listnode_add(_free_list, cor->_ownerlist);
		--_count;
	}

	int term(coroutine_impl* cor)
	{
		assert(NULL != cor);
		if (cor != _current) return -ENOTALLOWED;

		// see if there is any other coroutine
		_mut.lock();
		if (listnode_isempty(_active_list)) {
			_mut.unlock();
			return -ENOTEXISTS;
		}
		auto* nextcor = list_entry(coroutine_impl, _ownerlist, \
			_active_list.next);
		_mut.unlock();

		// switch to new coroutine
		set_current(nextcor);
		setcontext(&nextcor->_ctx);

		// shall never come here
		return 0;
	}

	coroutine_impl* get_current(void) {
		return _current;
	}

	int resume(coroutine_impl* cor)
	{
		assert(NULL != cor);
		if (cor->_manager != this) {
			return -EINVALID;
		}
		if (_current == cor) return 0;

		size_t ptr;
		coroutine_impl* cur;
		switch (cor->_status)
		{
		case coroutine_state_ready:
			getcontext(&cor->_ctx);
			cor->_ctx.uc_stack.ss_sp = cor->_stack;
			cor->_ctx.uc_stack.ss_size = cor->_stack_size;
			cor->_ctx.uc_link = &_main;

			ptr = (size_t)cor;
			makecontext(&cor->_ctx, (void (*)())coroutine_mainfunc, 2,
				(uint32_t)ptr, (uint32_t)(ptr >> 32));

		// and then do the same thing as suspend state
		case coroutine_state_suspend:
			assert(get_current() != cor);
			assert(!listnode_iscleared(cor->_ownerlist));
			_mut.lock();
			listnode_del(cor->_ownerlist);

			// get and set the current round
			if (!listnode_isempty(_active_list)) {
				cur = list_entry(coroutine_impl, _ownerlist, \
					_active_list.next);
				cor->_round = cur->_round;
			}
			listnode_insertfirst(_active_list, cor->_ownerlist);

			// update the status
			cor->_status = coroutine_state_running;
			cur = set_current(cor);
			_mut.unlock();
			break;

		default: return 0;
		}
		// switch to the new coroutine
		swapcontext((!cur) ? &_main : &cur->_ctx, &cor->_ctx);
		return 0;
	}

	int suspend(coroutine_impl* cor)
	{
		if (cor->_status != coroutine_state_running) {
			return 0;
		}
		
		assert(!listnode_iscleared(cor->_ownerlist));
		coroutine_impl* nextcor = NULL;

		_mut.lock();
		listnode_del(cor->_ownerlist);

		// get next avaliable coroutine and switch to it
		if (!listnode_isempty(_active_list)) {
			nextcor = list_entry(coroutine_impl, _ownerlist, _active_list.next);
			if (nextcor->_round != cor->_round) {
				// we finished a round of schedule for all active coroutine
				// we need to return back to main process
				nextcor = NULL;
			}
		}

		// move to suspend list
		listnode_add(_suspend_list, cor->_ownerlist);

		// update status
		bool need_switch = false;
		cor->_status = coroutine_state_suspend;
		if (_current == cor) {
			set_current(nextcor);
			need_switch = true;
		}
		_mut.unlock();

		// switch to the new coroutine
		if (need_switch) {
			swapcontext(&cor->_ctx, (nextcor) ? &nextcor->_ctx : &_main);
		}
		return 0;
	}

	int yield(coroutine_impl* cor)
	{
		if (cor->_status != coroutine_state_running
			|| _current != cor) {
			return -ENOTALLOWED;
		}
		assert(!listnode_iscleared(cor->_ownerlist));

		coroutine_impl* nextcor = NULL;
		_mut.lock();
		listnode_del(cor->_ownerlist);

		// get another active coroutine and switch to it
		if (!listnode_isempty(_active_list)) {
			nextcor = list_entry(coroutine_impl, _ownerlist, _active_list.next);
			if (nextcor->_round != cor->_round) {
				// we finished a round of schedule for all active coroutine
				// we need to return back to main process
				nextcor = NULL;
			}
		}

		// add it back to active list
		round_next_unlocked(cor->_round);
		listnode_add(_active_list, cor->_ownerlist);

		// set current to the new coroutine
		auto* prev = set_current(nextcor);
		_mut.unlock();

		// switch coroutine
		return swapcontext(&prev->_ctx, (nextcor) ? &nextcor->_ctx : &_main);
	}

	int schedule(void)
	{
		coroutine_impl *cor = NULL, *nextcor = NULL;

		// we need lock
		_mut.lock();

		if (_current) {
			cor = _current;
			assert(cor->_status == coroutine_state_running);
			assert(!listnode_iscleared(cor->_ownerlist));
			listnode_del(cor->_ownerlist);
		}

		// see if we can have a candidate
		if (listnode_isempty(_active_list) && !cor) {
			_mut.unlock();
			return -ENOTALLOWED;
		}
		else nextcor = list_entry( \
			coroutine_impl, _ownerlist, _active_list.next);

		// add the previous coroutine to the end of active list
		if (cor) {
			round_next_unlocked(cor->_round);
			listnode_add(_active_list, cor->_ownerlist);
		}

		// set the candidate as the next coroutine
		set_current(nextcor);
		_mut.unlock();

		// switch to the new coroutine
		assert(cor != nextcor);
		swapcontext((cor) ? &cor->_ctx : &_main,
				(nextcor) ? &nextcor->_ctx : &_main);
		return 0;
	}

private:

	void release_list_unlocked(listnode_t& header)
	{
		while (!listnode_isempty(header)) {
			auto* cor = list_entry(coroutine_impl, _ownerlist, header.next);
			listnode_del(cor->_ownerlist);
			delete cor;
		}
	}

	coroutine_impl* set_current(coroutine_impl* cor) {
		auto* ret = _current;
		_current = cor;
		return ret;
	}

	void round_next_unlocked(int& round) {
		if (round == INT32_MAX) {
			round = 0;
		}
		else ++round;
	}

private:
	mutex _mut;
	int _count, _total;
	int _curr_round;
	listnode_t _active_list;

	// all coroutines with READY and SUSPEND
	// status will be linked to this list
	listnode_t _suspend_list;
	listnode_t _free_list;
	ucontext_t _main;

	coroutine_impl* _current;
	unsigned long int _thread;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(coroutine_mgr_impl);
};

coroutine_impl::coroutine_impl(coroutine* cor,
	uint32_t maxstack, coroutine_mgr_impl* manager)
: _cor(cor), _manager(manager), _round(0)
, _status(coroutine_state_ready)
, _stack(NULL)
{
	assert(NULL != manager);
	manager->set_thread();
	_stack_size = (maxstack)
		? (maxstack + ZAS_PAGESZ - 1) & ~(ZAS_PAGESZ - 1)
		: ZAS_PAGESZ;	// minimum stack size

	listnode_clear(_ownerlist);
}

coroutine_impl::~coroutine_impl()
{
	if (_cor) {
		_cor->release();
		_cor = NULL;
	}
	if (NULL != _stack) {
		free(_stack);
		_stack = NULL;
		_stack_size = 0;
	}
}

void coroutine_impl::recycle(void)
{
	assert(_manager && _cor);
		
	// release the coroutine object
	if (_cor) {
		auto* cor = _cor;
		_cor->_data = NULL;

		if (_status == coroutine_state_running)
			cor->destroy();
		else cor->release();
	}
	_manager->reap(this);
	_manager->term(this);
}

int coroutine_impl::bind(coroutine* cor, uint32_t maxstack)
{
	assert(cor && !_cor && _manager);
	if (maxstack < _stack_size) {
		return 0;
	}

	// adjust the stack to the requested size
	_stack_size = (maxstack)
		? (maxstack + ZAS_PAGESZ - 1) & ~(ZAS_PAGESZ - 1)
		: ZAS_PAGESZ;
	if (_stack) {
		free(_stack); _stack = NULL;
	}

	// update information
	_cor = cor;
	_status = coroutine_state_ready;
	return 0;
}

int coroutine_impl::start(void)
{
	if (NULL == _manager) {
		return -ELOGIC;
	}

	// allocate stack if necessary
	if (!_stack) {
		assert(_stack_size >= ZAS_PAGESZ);
		_stack = (char*)malloc(_stack_size);
		if (NULL == _stack) {
			return -ENOMEMORY;
		}
	}
	resume();
	return 0;
}

static void coroutine_mainfunc(uint32_t low32, uint32_t hi32)
{
	uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hi32 << 32);
	auto* cor = reinterpret_cast<coroutine_impl*>(ptr);

	// run the coroutine
	if (cor->_cor) {
		cor->_cor->run();
	}

	// destroy the object
	cor->recycle();
}

void coroutine_impl::resume(void)
{
	assert(NULL != _stack && NULL != _manager);
	_manager->resume(this);
}

int coroutine_impl::yield(void)
{
	assert(NULL != _stack && NULL != _manager);
	return _manager->yield(this);
}

int coroutine_impl::suspend(void)
{
	assert(NULL != _stack && NULL != _manager);
	return _manager->suspend(this);
}

coroutine::coroutine(coroutine_mgr* manager, size_t maxstack)
: _data(NULL)
{
	if (NULL == manager) return;

	auto* mgr = reinterpret_cast<coroutine_mgr_impl*>
		(manager->_data);
	if (NULL == mgr) return;

	coroutine_impl* cor = mgr->allocate(this, maxstack);
	if (NULL == cor) return;

	// save the object
	_data = reinterpret_cast<void*>(cor);
}

coroutine::~coroutine()
{
	auto* cor = reinterpret_cast<coroutine_impl*>(_data);
	if (cor) {
		cor->recycle();
	}
}

int coroutine::start(void)
{
	auto* cor = reinterpret_cast<coroutine_impl*>(_data);
	if (NULL == cor) return -EINVALID;
	return cor->start();
}

void coroutine::resume(void)
{
	auto* cor = reinterpret_cast<coroutine_impl*>(_data);
	if (NULL == cor) return;
	cor->resume();
}

int coroutine::yield(void)
{
	auto* cor = reinterpret_cast<coroutine_impl*>(_data);
	if (NULL == cor) return -EINVALID;
	return cor->yield();
}

int coroutine::suspend(void)
{
	auto* cor = reinterpret_cast<coroutine_impl*>(_data);
	if (NULL == cor) return -EINVALID;
	return cor->suspend();
}

int coroutine::run(void) {
	return 0;
}

void coroutine::release(void) {
}

void coroutine::destroy(void) {
}

coroutine_mgr::coroutine_mgr(int maxcor)
: _data(NULL)
{
	auto* mgr = new coroutine_mgr_impl(maxcor);
	if (NULL == mgr) return;
	_data = reinterpret_cast<void*>(mgr);
}

coroutine_mgr::~coroutine_mgr()
{
	if (_data) {
		auto* mgr = reinterpret_cast<coroutine_mgr_impl*>(_data);
		delete mgr; _data = NULL;
	}
}

coroutine* coroutine_mgr::get_current(void)
{
	auto* mgr = reinterpret_cast<coroutine_mgr_impl*>(_data);
	if (NULL == mgr) return NULL;

	auto* cor = mgr->get_current();
	if (NULL == cor) return NULL;

	auto* ret = cor->_cor;
	assert(NULL != ret);
	return ret;
}

int coroutine_mgr::schedule(void)
{
	if (NULL == _data) {
		return -EINVALID;
	}
	auto* cmgr = reinterpret_cast<coroutine_mgr_impl*>(_data);
	return cmgr->schedule();
}

class test {
public:
	class mycoroutine : public coroutine
	{
	public:
		mycoroutine(coroutine_mgr* mgr)
		: coroutine(mgr) {}

		int run(void) {
			printf("mycoroutine\n");
			yield();
			printf("mycoroutine resume - finished\n");
			return 0;
		}
		
		void release(void) {}
		void destroy(void) {}
	};
	class mycoroutine1 : public coroutine
	{
	public:
		mycoroutine1(coroutine_mgr* mgr)
		: coroutine(mgr) {}

		int run(void) {
			printf("mycoroutine1\n");
			yield();
			printf("mycoroutine1 resume - try suspend\n");
			suspend();
			printf("mycoroutine1 resume again - finish\n");
			return 0;
		}
		
		void release(void) {}
		void destroy(void) {}
	};

	test() {
		mycoroutine r(&_manager);
		mycoroutine1 r1(&_manager);
		r.start();
		r1.start();
		_manager.schedule();	// return to mycoroutine
		r1.resume();
		printf("finish\n");
	}
private:
	coroutine_mgr _manager;
};

// static test t;

}} // end of namespace zas::utils
#endif // UTILS_ENABLE_FBLOCK_COROUTINE
/* EOF */
