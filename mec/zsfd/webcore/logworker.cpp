/** @file logworker.cpp
 * implementation of logger worker thread
 */

#include <time.h>
#include "utils/dir.h"
#include "utils/uuid.h"
#include "utils/timer.h"
#include "webcore/sysconfig.h"

#include "inc/logworker.h"
#include "inc/logwriter.h"

namespace zas {
namespace webcore {

logworker* logworker::_inst = nullptr;

logworker::logworker()
: looper_thread("logger-thread")
, _blkset(32)
, _index_file(nullptr)
, _content_file(nullptr)
{
}

logworker::~logworker()
{
	if (_content_file) {
		delete _content_file;
		_content_file = nullptr;
	}
}

logworker* logworker::getworker(void)
{
	return _inst;
}

logworker* logworker::inst(void)
{
	if (nullptr == _inst) {
		_inst = new logworker();
		assert(nullptr != _inst);

		// start the logger worker thread
		int ret = _inst->start();
		assert(ret == 0);

		// make sure it is started
		while (!_inst->running()) {
			msleep(1);
		}
	}
	return _inst;
}

int logworker::flush(void)
{
	// make all list timestamp as "current"
	timeval time;
	gettimeofday(&time, nullptr);
	int cnt = _blkset.getsize();
	for (int i = 0; i < cnt; ++i) {
		auto& blklist = _blkset.buffer()[i];
		blklist.max_timestamp = time;
	}

	// drain all logs
	for (;;) {
		log_node* node = get_next_log(&time);
		if (nullptr == node) break;
		if (write_node(node)) {
			return -1;
		}
	}

	// write all to disk
	if (_index_file) {
		delete _index_file;
		_index_file = nullptr;
	}
	if (_content_file) {
		delete _content_file;
		_content_file = nullptr;
	}
	return 0;
}

int logworker::handle_blocks(unsigned long int tid,
	uint8_t* buf, int bufsz, timeval* last_time)
{
	timeval time;
	if (nullptr == last_time) {
		gettimeofday(&time, nullptr);
		last_time = &time;
	}
	int idx = add_to_blocklist(tid, buf, bufsz, last_time);
	if (idx < 0) return -1;


	// handle all logs
	for (;;) {
		log_node* node = get_next_log(last_time);
		if (nullptr == node) break;
		if (write_node(node)) {
			return -2;
		}
	}
	return 0;
}

int logworker::normalize_logstr(char* str, int len)
{
	char* end = str + len - 2;
	for (; end != str; --end) {
		if (*end == '\n' || *end == '\r'
			|| *end == '\t' || *end == ' ') {
			*end = '\0';
		} else break;
	}
	for (char* t = str; t != end; ++t) {
		if (*t == '\n') *t = '-';
	}
	return end - str + 2;
}

int logworker::write_node(log_node* node)
{
	if (nullptr == _content_file) {
		_content_file = new log_file(log_file_type_content, "txt");
		if (nullptr == _content_file
			|| nullptr == _content_file->fp()) {
			return -1;
		}
		_content_file->set_begin_ts(node->time_stamp);
	}

	// write the log content
	_content_file->set_end_ts(node->time_stamp);

	tm t;
	FILE* fp = _content_file->fp();
	if (nullptr == fp) {
		return -2;
	}
	localtime_r(&node->time_stamp.tv_sec, &t);
	// write the time
	fprintf(fp, "%d/%02d/%02d %02d:%02d:%02d:%06ld ",
		t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
		t.tm_hour, t.tm_min, t.tm_sec,
		node->time_stamp.tv_usec);

	// write the tag
	char* tag = (char*)&node[1];
	normalize_logstr(tag, node->mod_length);
	fputs(tag, fp);
	
	// write the content
	fputc(' ', fp);
	char* content = (char*)&node[1] + node->mod_length;
	normalize_logstr(content, node->content_length);
	fputs(content, fp);
	fputc('\n', fp);

	// check if we exit the limits
	if (ftell(fp) > _content_file->limit()) {
		delete _content_file;
		_content_file = nullptr;
	}
	return 0;
}

log_node* logworker::get_next_log(timeval* last)
{
	assert(nullptr != last);
	log_node* ret = nullptr;
	log_block* target = nullptr;

	int cnt = _blkset.getsize();
	for (int i = 0; i < cnt; ++i)
	{
		auto& blklist = _blkset.buffer()[i];
		while (!listnode_isempty(blklist.list))
		{
			auto* block = list_entry(log_block, ownerlist,\
				blklist.list.next);

			// if the block is exhausted, release it
			log_node* log = (log_node*)(block->buf + block->curpos);
			if (block->curpos >= block->bufsz
				|| log->type == logtype_unknown) {
				listnode_del(block->ownerlist);
				delete block;
				continue;
			}

			if (timeval_compare(&log->time_stamp, last) > 0) {
				break;
			}

			if (nullptr == ret) {
				ret = log; target = block;
				break;
			}

			if (timeval_compare(&log->time_stamp, &ret->time_stamp) < 0) {
				ret = log; target = block;
			}
			break;
		}
	}
	
	if (nullptr == ret) {
		return ret;
	}

	target->curpos = (int)((size_t)(ret + 1) - (size_t)target->buf)
		+ ret->mod_length + ret->content_length;
	return ret;
}

int logworker::get_latest_break(timeval& time)
{
	// find out the min timestamp in the list
	time.tv_sec = LONG_MAX;
	time.tv_usec = LONG_MAX;
		
	int cnt = _blkset.getsize();
	assert(cnt > 0);

	for (int i = 0; i < cnt; ++i) {
		auto& blklist = _blkset.buffer()[i];
		if (timeval_compare(&blklist.max_timestamp, &time) < 0) {
			time = blklist.max_timestamp;
		}
	}
	return 0;
}

int logworker::timeval_compare(timeval* t1, timeval* t2)
{
	if (t1->tv_sec > t2->tv_sec) {
		return 1;
	}
	else if (t1->tv_sec < t2->tv_sec) {
		return -1;
	}
	else {
		if (t1->tv_usec > t2->tv_usec) {
			return 1;
		}
		else if (t1->tv_usec < t2->tv_usec) {
			return -1;
		}
		else return 0;
	}
	// shall never come here
}

int logworker::add_to_blocklist(unsigned long int tid,
	uint8_t* buf, int bufsz, timeval* last_time)
{
	// see if there is a list in the blockset
	int cnt = _blkset.getsize();
	for (int i = 0; i < cnt; ++i) {
		auto& blklist = _blkset.buffer()[i];
		if (blklist.tid == tid) {
			if (buf && bufsz) {
				auto* block = new log_block(buf, bufsz, last_time);
				if (nullptr == block) return -1;
				listnode_add(blklist.list, block->ownerlist);
			}

			// update the max_timestamp
			assert(timeval_compare(last_time, \
				&blklist.max_timestamp) >= 0);
			blklist.max_timestamp = *last_time;
			return i;
		}
	}

	// not found, create one
	log_blocklist* blklist = _blkset.new_object();
	if (nullptr == blklist) return -1;

	blklist->tid = tid;
	blklist->max_timestamp = *last_time;
	listnode_init(blklist->list);

	if (buf && bufsz) {
		auto* block = new log_block(buf, bufsz, last_time);
		if (nullptr == block) return -2;
		listnode_add(blklist->list, block->ownerlist);
	}
	return _blkset.getsize() - 1;
}

log_file::log_file(int type, const char* suffix)
: _fp(nullptr)
, _suffix(suffix)
, _limit(0)
{
	static const char* keys[] = {
		"log.index-limit",
		"log.content-limit",
	};
	const char* key = keys[type - log_file_type_index];
	// get file limit
	_limit = get_sysconfig(key, log_file_def_limit);

	create_file();
}

log_file::~log_file()
{
	finalize_file();
}

int log_file::finalize_file(void)
{
	if (nullptr == _fp) {
		return -1;
	}

	// close the file
	fclose(_fp);
	_fp = nullptr;

	// rename the file	
	char buf[256];
	tm tb, te;
	localtime_r(&_begin.tv_sec, &tb);
	localtime_r(&_end.tv_sec, &te);

	int sz = snprintf(buf, 256,
		// file name format:
		// yyyy_mm_dd_hh_mm_ss_ms-yyyy_mm_dd_hh_mm_ss_ms
		"%s%d_%d_%d_%d_%d_%d_%ld-%d_%d_%d_%d_%d_%d_%ld.%s",
		_logdir.c_str(),
		tb.tm_year + 1900, tb.tm_mon + 1, tb.tm_mday,
		tb.tm_hour, tb.tm_min, tb.tm_sec, _begin.tv_usec,
		te.tm_year + 1900, te.tm_mon + 1, te.tm_mday,
		te.tm_hour, te.tm_min, te.tm_sec, _end.tv_usec,
		_suffix.c_str());
	if (sz >= 256) { return -2; }

	if (!rename(_tmpfile.c_str(), buf)) {
		return -3;
	}
	return 0;
}

int log_file::create_file(void)
{
	assert(nullptr == _fp);

	// get log-root directory
	int ret;
	const char* dir = get_sysconfig("log.rootdir", nullptr, &ret);
	if (ret) return -1;

	_logdir.assign(dir);
	if (_logdir[_logdir.size() - 1] != '/') {
		_logdir += "/";
	}

	// try to create a file using a temp file name
	uuid uid;
	for (int i = 0; i < 3; ++i) {
		uid.generate();
		uid.to_hex(_tmpfile);
		_tmpfile = _logdir + _tmpfile;
		
		// if file already exists, re-generate it
		// however, we only try 3 times
		if (fileexists(_tmpfile.c_str())) {
			continue;
		}

		// create the file
		_fp = fopen(_tmpfile.c_str(), "wb");
		break;
	}
	if (nullptr == _fp) {
		return -2;
	}
	return 0;
}

log_block::log_block(uint8_t* buffer, int buffer_sz, timeval* time)
: buf(buffer), bufsz(buffer_sz), curpos(0)
{
	assert(nullptr != buffer && 0 != buffer_sz);
	timestamp = *time;
}

log_block::~log_block()
{
	if (nullptr != buf) {
		free(buf);
		buf = nullptr;
	}
}

logtask::logtask(uint8_t* buf, int bufsz, timeval* timestamp)
: _buf(buf), _bufsz(bufsz), _tid(gettid())
{
	if (timestamp) {
		_timestamp = *timestamp;
	} else {
		gettimeofday(&_timestamp, nullptr);
	}
}

logtask::~logtask()
{
}

void logtask::do_action(void)
{
	logworker::inst()->handle_blocks(
		_tid, _buf, _bufsz, &_timestamp);
}

logflush_task::logflush_task()
{
}

logflush_task::~logflush_task()
{
}

void logflush_task::do_action(void)
{
	logworker::inst()->flush();
}

}} // end of namespace zas::webcore
/* EOF */
