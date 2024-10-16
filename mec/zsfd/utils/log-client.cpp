#include "utils/utils.h"
#if (defined(UTILS_ENABLE_FBLOCK_EVLOOP) && defined(UTILS_ENABLE_FBLOCK_LOG))

#include "utils/log.h"
#include "inc/evlmsg.h"
#include "inc/evloop-impl.h"

namespace zas {
namespace utils {

class log_client_impl
{
public:
	log_client_impl()
	: _level(log_level::verbose) {
	}
	~log_client_impl() {
	}

	int write(log_level level, const char* tag, const char* fmt, va_list ap)
	{
		const int bufsz = 256;
		char fmtbuf[bufsz];
		
		// check level to see if we output the log
		if (level > _level) {
			return 0;
		}
		char* content = fmtbuf;
		int ret = vsnprintf(content, bufsz, fmt, ap);
		if (ret >= bufsz) {
			// not enough space, we allocate
			// large space here
			if (ret > 0xFFF) {
				return -ETOOLONG;
			}
			content = new char [ret + 4];
			if (nullptr == content) {
				return -ENOMEMORY;
			}
			// this time, we must have enough space
			vsnprintf(content, ret + 3, fmt, ap);
		}
		ret = write(level, tag, content);
		// free space if necessary
		if (content != fmtbuf) delete [] content;
		return ret;
	}

	int write(log_level level, const char* tag, const char* content)
	{
		auto evl = validity_check();
		if (nullptr == evl) {
			return -ENOTALLOWED;
		}

		// save the time stamp first
		timeval ts;
		gettimeofday(&ts, nullptr);

		if (level == log_level::unknown || level > log_level::verbose) {
			return -EINVALID;
		}
		// check level to see if we output the log
		if (level > _level) {
			return 0;
		}
		if (nullptr == tag || nullptr == content) {
			return -EINVALID;
		}

		size_t tag_len, content_len;
		tag = normalize_log_string(tag, tag_len);
		content = normalize_log_string(content, content_len);

		if (tag_len > UINT8_MAX || content_len > UINT16_MAX) {
			return -ETOOLONG;
		}

		// allocate extra 2 bytes for '\0' (tag & content)
		auto sz = tag_len + content_len + 2;
		evl_ssvc_log_submission_pkg* pkg = new(alloca(sizeof(*pkg) + sz))
			evl_ssvc_log_submission_pkg(sz);
		
		// get the paylog of the package
		auto& logdata = pkg->payload();

		// fill in the structure
		logdata.level = (uint16_t)level;
		memcpy(&logdata.timestamp, &ts, sizeof(timeval));
		logdata.tag_length = (uint8_t)tag_len;
		logdata.content_length = (uint16_t)content_len;

		// dump the tag and content
		if (tag_len) memcpy(logdata.data, tag, tag_len);
		logdata.data[tag_len] = '\0';
		if (content_len) memcpy(&logdata.data[tag_len + 1], content, content_len);
		logdata.data[tag_len + content_len + 1] = '\0';
	
		// send the log to log-collector
		sz += sizeof(evl_ssvc_log_submission_pkg);
		if (sz < 50) {printf("sz = %lu\n", sz);}
		return evl->sendto_svr(pkg, sz, 3000);
	}

private:
	evloop_impl* validity_check(void) {
		auto evl = reinterpret_cast<evloop_impl*>(evloop::inst());
		assert(nullptr != evl);

		if (evl->get_state() != evloop_state_connected) {
			return nullptr;
		}
		if (evl->getrole() != evloop_role_client) {
			return nullptr;
		}
		return evl;
	}

	const char* normalize_log_string(const char* str, size_t& szr)
	{
		assert(nullptr != str);
		if (nullptr == str) {
			return nullptr;
		}
		auto sz = strlen(str);
		
		// from start
		for (; *str == '\r' || *str == '\n'; ++str, --sz);
		// from end
		for (const char* end = str + sz - 1; (end >= str)
			&& (*end == '\r' || *end == '\n'); --end, --sz);

		szr = sz;
		return str;
	}

private:
	log_level _level;
};

static log_client_impl _log_client;
log_client* log = reinterpret_cast<log_client*>(&_log_client);

int log_client::write(log_level level, const char* tag, const char* content)
{
	if (nullptr == this) {
		return -EINVALID;
	}
	auto logc = reinterpret_cast<log_client_impl*>(this);
	return logc->write(level, tag, content);
}

int log_client::e(const char* tag, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	if (nullptr == this) {
		return -EINVALID;
	}
	auto logc = reinterpret_cast<log_client_impl*>(this);
	int ret = logc->write(log_level::error, tag, fmt, ap);
	va_end(ap);

	return ret;
}

int log_client::w(const char* tag, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	if (nullptr == this) {
		return -EINVALID;
	}
	auto logc = reinterpret_cast<log_client_impl*>(this);
	int ret = logc->write(log_level::warning, tag, fmt, ap);
	va_end(ap);

	return ret;
}

int log_client::d(const char* tag, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	if (nullptr == this) {
		return -EINVALID;
	}
	auto logc = reinterpret_cast<log_client_impl*>(this);
	int ret = logc->write(log_level::debug, tag, fmt, ap);
	va_end(ap);

	return ret;
}

int log_client::i(const char* tag, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	if (nullptr == this) {
		return -EINVALID;
	}
	auto logc = reinterpret_cast<log_client_impl*>(this);
	int ret = logc->write(log_level::info, tag, fmt, ap);
	va_end(ap);

	return ret;
}

int log_client::v(const char* tag, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	if (nullptr == this) {
		return -EINVALID;
	}
	auto logc = reinterpret_cast<log_client_impl*>(this);
	int ret = logc->write(log_level::verbose, tag, fmt, ap);
	va_end(ap);

	return ret;
}

}} // end of namespace zas::utils
#endif // (defined(UTILS_ENABLE_FBLOCK_EVLOOP) && defined(UTILS_ENABLE_FBLOCK_LOG))
/* EOF */
