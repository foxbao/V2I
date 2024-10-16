/** @file logwriter.h
 * definition of logger write
 */

#ifndef __CXX_ZAS_WEBCORE_LOG_WRITER_H__
#define __CXX_ZAS_WEBCORE_LOG_WRITER_H__

#include <stdarg.h>
#include <sys/time.h>
#include "webcore/logger.h"

namespace zas {
namespace webcore {

enum {
	LOGWRITER_BUFSIZE = 128 * 1024,				// 128 KB
	LOGWRITER_MAX_BUFSIZE = 4 * 1024 * 1024,	// 4 MB
	LOGWRITER_MIN_INTERVAL = 8,					// 8 sec
};

struct log_node
{
	uint8_t type;
	timeval time_stamp;
	uint16_t mod_length;
	uint16_t content_length;
};

class logwriter
{
public:
	logwriter();
	~logwriter();
	static logwriter* inst(void);

	int write(int type, const char* mod,
		const char* content, va_list ap);

	int commit_buffer(timeval* last_time);

private:
	int calc_new_buffer_size(void);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(logwriter);
};

}} // end of namespace zas::webcore
#endif // __CXX_ZAS_WEBCORE_LOG_WRITER_H__
/* EOF */
