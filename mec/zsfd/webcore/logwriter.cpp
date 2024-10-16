/** @file logwriter.h
 * definition of logger write
 */

#include "utils/timer.h"
#include "webcore/logger.h"
#include "inc/logwriter.h"
#include "inc/logworker.h"

namespace zas {
namespace webcore {

static __thread char* _logbuf = nullptr;
static __thread int _logbuf_sz = LOGWRITER_BUFSIZE;
static __thread int _logbuf_cursz = 0;

// the timestamp for the timing of writing the
// first log to the buffer. it is used to determine
// if we need a bigger buffer by evaluating the time
// length of consuming the whole buffer
static __thread long _logbuf_firstlog_ts = 0;

char timebuf[256];
char* get_printf_time_info(void)
{
	timeval tv;
	tm t;
	gettimeofday(&tv, nullptr);
	localtime_r(&tv.tv_sec, &t);
	memset(timebuf, 0, 256);
	snprintf(timebuf, 255, "%d/%02d/%02d %02d:%02d:%02d:%06ld",
		t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
		t.tm_hour, t.tm_min, t.tm_sec,
		tv.tv_usec);
	return timebuf;
}

logwriter::logwriter()
{
}

logwriter::~logwriter()
{
}

logwriter* logwriter::inst(void)
{
	static logwriter _inst;
	return &_inst;
}

int logwriter::write(int type, const char* mod,
	const char* content, va_list ap)
{
	if (type <= 0 || type >= logtype_max
		|| !content || *content == '\0') {
		return -EBADPARM;
	}

	if (nullptr == mod) {
		return -EBADPARM;
	}

	int modlen = (mod) ? (strlen(mod) + 1) : 0;
	int prefix_len = sizeof(log_node) + modlen;

	for (int i = 0; i < 2; ++i)
	{
		int cursz, remain;

		// make sure we have a buffer for the thread
		char* buf = (char*)_logbuf;
	
		if (buf) {
			cursz = _logbuf_cursz;
			remain = _logbuf_sz - cursz;
		} else {
			buf = (char*)malloc(_logbuf_sz);
			assert(nullptr != buf);
			_logbuf = buf;
			_logbuf_cursz = cursz = 0;
			remain = _logbuf_sz;
			_logbuf_firstlog_ts = gettick_millisecond();
		}
		assert(remain >= 0);

		if (prefix_len < remain) {
			// try directly write the log to this block
			size_t maxlen = remain - prefix_len;
			
			int ret = vsnprintf(buf + cursz + prefix_len, maxlen, content, ap);
			if (ret > 0xFFFF) {
				return -ETOOLONG;
			}
			if (ret < maxlen) {
				// success, write other information
				log_node node;
				gettimeofday(&node.time_stamp, nullptr);
				node.type = (uint8_t)type;
				node.mod_length = modlen;
				node.content_length = ret + 1;
				memcpy(buf + cursz, &node, sizeof(log_node));
				if (modlen) {
					memcpy(buf + cursz + sizeof(log_node), mod, modlen);
				}

				// see if we need to commit this block
				if (ret + 1 == maxlen) {
					if (commit_buffer(&node.time_stamp)) {
						return -EINVALID;
					}
				}

				// update the buffer status
				cursz += prefix_len + ret + 1;
				buf[cursz] = (char)logtype_unknown;
				_logbuf_cursz = cursz;
				return 0;
			}
		}

		// there is no enough space in this block
		// se first we commit this block
		// and then, allocate a new block
		if (commit_buffer(nullptr)) {
			return -EINVALID;
		}
	}
	return -EINVALID;
}

int logwriter::calc_new_buffer_size(void)
{
	int sz;
	long sec = gettick_millisecond();
	sec = (sec - _logbuf_firstlog_ts + 999) / 1000;
	if (sec >= LOGWRITER_MIN_INTERVAL) {
		// buffer size too small so that the consumption time is
		// too short, we enlarge the buffer
		double tmp = double(_logbuf_sz * sec) / double(LOGWRITER_MIN_INTERVAL);
		if (tmp > LOGWRITER_MAX_BUFSIZE) {
			tmp = LOGWRITER_MAX_BUFSIZE;
		}
		sz = int(tmp);
	}
	else sz = LOGWRITER_BUFSIZE;
	return sz;
}

int logwriter::commit_buffer(timeval* last_time)
{
	int commit_sz = _logbuf_sz;
	_logbuf_sz = calc_new_buffer_size();

	if (logworker::inst()->add(new logtask(
		(uint8_t*)_logbuf, _logbuf_sz, last_time))) {
		return -1;
	}
	_logbuf = nullptr;
	return 0;
}

logger_writer log;

void logger_writer::init(void)
{
	// initialize the log worker and
	// commit the starting point for this thread
	logworker::inst()->add(new logtask(
		nullptr, 0, nullptr));
}

int logger_writer::flush(void)
{
	return logwriter::inst()->commit_buffer(nullptr);
}

void logger_writer::finialize(void)
{
	if (!logworker::getworker()) {
		return;
	}
	logworker::inst()->add(new logflush_task());
}

int logger_writer::i(const char* tag, const char* msg, ...)
{
	va_list ap;
	printf("[%s] ", get_printf_time_info());
	va_start(ap, msg);
	vprintf(msg, ap);
	int ret = 0;
	// int ret = logwriter::inst()->
	// 	write(logtype_info, tag, msg, ap);
	va_end(ap);
	printf("\n");
	return ret;
}

int logger_writer::v(const char* tag, const char* msg, ...)
{
	va_list ap;
	printf("[%s] ", get_printf_time_info());
	va_start(ap, msg);
		vprintf(msg, ap);
	int ret = 0;
	// int ret = logwriter::inst()->
	// 	write(logtype_verbose, tag, msg, ap);
	va_end(ap);
	printf("\n");
	return ret;
}

int logger_writer::d(const char* tag, const char* msg, ...)
{
	va_list ap;
	printf("[%s] ", get_printf_time_info());
	va_start(ap, msg);
	vprintf(msg, ap);
	int ret = 0;
	// int ret = logwriter::inst()->
	// 	write(logtype_debug, tag, msg, ap);
	va_end(ap);
	printf("\n");
	return ret;
}

int logger_writer::w(const char* tag, const char* msg, ...)
{
	va_list ap;
	printf("[%s] ", get_printf_time_info());
	va_start(ap, msg);
	vprintf(msg, ap);
	int ret = 0;
	// int ret = logwriter::inst()->
	// 	write(logtype_warning, tag, msg, ap);
	va_end(ap);
	printf("\n");
	return ret;
}

int logger_writer::e(const char* tag, const char* msg, ...)
{
	va_list ap;
	printf("[%s] ", get_printf_time_info());
	va_start(ap, msg);
	vprintf(msg, ap);
	int ret = 0;
	// int ret = logwriter::inst()->
	// 	write(logtype_error, tag, msg, ap);
	va_end(ap);
	printf("\n");
	return ret;
}

}} // end of namespace zas::webcore
/* EOF */

