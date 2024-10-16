/** @file evpoller.h
 * declaration of the event poller for evloop
 */

#ifndef __CXX_UTILS_EVPOLLER_H__
#define __CXX_UTILS_EVPOLLER_H__

#include <stdarg.h>

#include "std/list.h"
#include "utils/wait.h"
#include "utils/mutex.h"
#include "utils/timer.h"

namespace zas {
namespace utils {

class coroutine;
class evloop_impl;
class evpoller_impl;
class evp_eventlist;
class evpoller_event;
class package_header;

class evpoller_event_impl
{
	friend class evpoller_impl;
	friend class evp_eventlist;
public:
	evpoller_event_impl(int type, evpoller_impl* poller);
	virtual ~evpoller_event_impl();
	void trigger(void);

public:
	evpoller_impl* getpoller(void) {
		return _poller;
	}

	int gettype(void) {
		return _type;
	}

	virtual int submit(void) {
		return -ENOTSUPPORT;
	}

	void detach(void) {
		if (!listnode_isempty(_active)) {
			listnode_del(_active);
		}
	}

	void* allocate_inputbuf(size_t sz)
	{
		if (_input) return NULL;
		_input = malloc(sz);
		_input_size = (uint32_t)sz;
		return _input;
	}

	void* allocate_outputbuf(size_t sz)
	{
		if (_output) return NULL;
		_output = malloc(sz);
		_output_size = (uint32_t)sz;
		return _output;
	}

	void* get_inputbuf(size_t* sz = NULL) {
		if (sz) *sz = (size_t)_input_size;
		return _input;
	}

	void* get_outputbuf(size_t* sz = NULL) {
		if (sz) *sz = (size_t)_output_size;
		return _output;
	}

	int write_inputbuf(void* buf, size_t sz) {
		if (_input) return -EINVALID;
		_input = malloc(sz);
		assert(NULL != _input);
		_input_size = (uint32_t)sz;
		memcpy(_input, buf, sz);
		return 0;
	}

	int read_inputbuf(void* buf, size_t sz) {
		if (!_input) return -ENOTEXISTS;
		if (_input_size != sz) return -EBADPARM;
		memcpy(buf, _input, sz);
		return 0;
	}

	int write_outputbuf(void* buf, size_t sz) {
		if (_output) return -EINVALID;
		_output = malloc(sz);
		assert(NULL != _output);
		_output_size = (uint32_t)sz;
		memcpy(_output, buf, sz);
		return 0;
	}

	int read_outputbuf(void* buf, size_t sz) {
		if (!_output) return -ENOTEXISTS;
		if (_output_size != sz) return -EBADPARM;
		memcpy(buf, _output, sz);
		return 0;
	}

private:
	int _type;
	listnode_t _ownerlist;
	listnode_t _active;
	evpoller_impl* _poller;
	void* _input;
	void* _output;
	uint32_t _input_size;
	uint32_t _output_size;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(evpoller_event_impl);
};

class evpoller_pkgevent : public evpoller_event_impl
{
public:
	evpoller_pkgevent(evpoller_impl* poller,
		uint32_t pkgid);
	evpoller_pkgevent(evpoller_impl* poller,
		uint32_t pkgid, uint32_t seqid);

	int submit(void);
	bool get_seqid(uint32_t& sid);

public:
	uint32_t get_packageid(void) {
		return _pkgid;
	}

private:
	uint32_t _pkgid;
	uint32_t _seqid;
	union {
		uint32_t _flags;
		struct {
			uint32_t has_seqid : 1;
		} _f;
	};
	ZAS_DISABLE_EVIL_CONSTRUCTOR(evpoller_pkgevent);
};

class evpoller_timeout_event : public timer
	, evpoller_event_impl
{
public:
	evpoller_timeout_event(evpoller_impl* poller,
		int timeout);
	void on_timer(void);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(evpoller_timeout_event);
};

class evp_eventlist
{
public:
	evp_eventlist();
	virtual ~evp_eventlist();

	evpoller_event_impl* dequeue(void);
	int append_list_unlocked(listnode_t& hdr, evpoller_event_impl* ev);

public:
	virtual int append(evpoller_event_impl* ev) {
		auto_mutex am(_mut);
		return append_list_unlocked(_generic, ev);
	}

	evpoller_event_impl* get_event_unlocked(listnode_t* node, bool dq = false)
	{
		auto* ret = list_entry(evpoller_event_impl, _active, node);
		if (dq) listnode_del(ret->_active);
		return ret;
	}

protected:
	void detach_events_unlocked(void);

protected:
	static mutex _mut;

private:
	listnode_t _generic;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(evp_eventlist);
};

class evp_pkgevent_list : public evp_eventlist
{
public:
	evp_pkgevent_list();
	~evp_pkgevent_list();

	int append(evpoller_event_impl* ev);
	evpoller_event_impl* dequeue_specific(
		const package_header& pkghdr);

	void finalize(void) {
		_id = 1 - _id;
	}

private:
	void detach_events_unlocked(void);

private:
	int _id;
	listnode_t _specific[2];
	ZAS_DISABLE_EVIL_CONSTRUCTOR(evp_pkgevent_list);
};

class evpoller_impl
{
public:
	evpoller_impl(evloop_impl* looper);
	~evpoller_impl();

	evpoller_event_impl* create_event(int evid, va_list vl);

	void reset(void);
	int poll(int timeout);
	evpoller_event_impl* get_triggered_event(void);
	void trigger_event(evpoller_event_impl* ev);	
	void trigger_submit(void);

private:
	void release_events_unlocked(void);
	void release_inactive_unlocked(void);
	evpoller_event_impl* create_event_package(va_list vl);
	evpoller_event_impl* create_event_package_with_seqid(va_list vl);
	void submit_triggers_unlocked(void);
	
	int poll_evloop_thread(int timeout);
	int poll_nonevloop_thread(int timeout);
	int poll_evloop_coroutine(int timeout);

	bool verify_status(void);
	bool has_pending_events(void);
	evpoller_event_impl* get_event_by_type(int type, bool dq);

private:
	evloop_impl* _looper;
	listnode_t _events;
	listnode_t _pending_list;
	listnode_t _active_list;
	listnode_t _inactive_list;
	listnode_t _ownerlist;
	void* _input;
	void* _output;
	waitobject _wobj;
	mutex _mut;

	union {
		uint32_t _flags;
		struct {
			uint32_t in_evloop_thread : 1;
		} _f;
	};
	unsigned long int _threadid;
	coroutine* _cor;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(evpoller_impl);
};

}} // end of namespace zas::utils
#endif // __CXX_UTILS_EVPOLLER_H__
/* EOF */
