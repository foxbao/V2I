/** @file logworker.h
 * definition of logger worker thread
 */

#ifndef __CXX_ZAS_WEBCORE_LOG_WORKER_H__
#define __CXX_ZAS_WEBCORE_LOG_WORKER_H__

#include <string>
#include <sys/time.h>

#include "std/list.h"
#include "utils/thread.h"
#include "webcore/logger.h"

// todo: re-arrange the following header
#include "./../hwgrph/inc/gbuf.h"

namespace zas {
namespace webcore {

using namespace std;
using namespace zas::utils;

struct log_node;

struct log_block
{
	log_block(uint8_t* buffer, int buffer_sz, timeval* time);
	~log_block();

	listnode_t ownerlist;
	timeval timestamp;
	uint8_t* buf;
	int bufsz;
	int curpos;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(log_block);
};

struct log_blocklist
{
	listnode_t list;
	// the biggest timestamp in
	// this list
	timeval max_timestamp;
	unsigned long int tid;
};

enum {
	log_file_type_unknown = 0,
	log_file_type_index,
	log_file_type_content,
	log_file_def_limit = 128 * 1024,	// 128MB
};

class log_file
{
public:
	log_file(int type, const char* suffix);
	~log_file();

public:
	FILE* fp(void) { return _fp; }
	void set_begin_ts(timeval& t) {
		_begin = t;
	}
	void set_end_ts(timeval& t) {
		_end = t;
	}

	long limit(void) {
		return _limit;
	}

private:
	int create_file();
	int finalize_file(void);
	
private:
	string _suffix;
	string _logdir;
	string _tmpfile;
	FILE* _fp;
	timeval _begin;
	timeval _end;
	long _limit;

	ZAS_DISABLE_EVIL_CONSTRUCTOR(log_file);
};

typedef zas::hwgrph::gbuf_<log_blocklist> log_blockset;

class logworker : public looper_thread
{
public:
	logworker();
	~logworker();
	static logworker* inst(void);
	static logworker* getworker(void);

	int handle_blocks(unsigned long int tid,
		uint8_t* buf, int bufsz, timeval* last_time);
	
	int flush(void);
	
private:
	int get_latest_break(timeval& time);
	log_node* get_next_log(timeval* last);
	int write_node(log_node* node);

	// return normalized length, or -1 if error
	int normalize_logstr(char* str, int len);

	// return the index if success, or -1 if error
	int add_to_blocklist(unsigned long int tid,
		uint8_t* buf, int bufsz, timeval* last_time);
	
	int timeval_compare(timeval* t1, timeval* t2);

private:
	log_blockset _blkset;

	log_file* _index_file;
	log_file* _content_file;

	static logworker* _inst;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(logworker);
};

class logtask : public looper_task
{
public:
	logtask(uint8_t* buf, int bufsz, timeval* timestamp);
	~logtask();

	void do_action(void);

private:
	uint8_t* _buf;
	int _bufsz;
	timeval _timestamp;
	unsigned long int _tid;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(logtask);
};

class logflush_task : public looper_task
{
public:
	logflush_task();
	~logflush_task();

	void do_action(void);
	ZAS_DISABLE_EVIL_CONSTRUCTOR(logflush_task);
};

}} // end of namespace zas
#endif // __CXX_ZAS_WEBCORE_LOG_WORKER_H__
/* EOF */
