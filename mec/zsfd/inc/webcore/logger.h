/** @file logger.h
 * definition of logger
 */

#ifndef __CXX_ZAS_WEBCORE_LOGGER_H__
#define __CXX_ZAS_WEBCORE_LOGGER_H__

#include "webcore/webcore.h"

namespace zas {
namespace webcore {

enum logtype
{
	logtype_unknown = 0,
	logtype_verbose,
	logtype_debug,
	logtype_info,
	logtype_warning,
	logtype_error,
	logtype_max,
};

class WEBCORE_EXPORT logger_writer
{
public:
	/**
	  finish all time-consumed initialization
	  without calling this manually will not cause any
	  problem, but may cause some time (be called
	  internally) in the first time of outputting log
	 */
	void init(void);

	/**
	  flush all log contents
	  @return 0 for success
	 */
	int flush(void);

	/**
	  finalize the logger
	 */
	void finialize(void);
	
	/**
	  write an "information" level log
	  @param tag a tag string
	  @param msg printf() style format of log message
	  @return 0 for success
	 */
	int i(const char* tag, const char* msg, ...);

	/**
	  write a "verbose" level log
	  @param tag a tag string
	  @param msg printf() style format of log message
	  @return 0 for success
	 */
	int v(const char* tag, const char* msg, ...);

	/**
	  write a "debug" level log
	  @param tag a tag string
	  @param msg printf() style format of log message
	  @return 0 for success
	 */
	int d(const char* tag, const char* msg, ...);

	/**
	  write a "warning" level log
	  @param tag a tag string
	  @param msg printf() style format of log message
	  @return 0 for success
	 */
	int w(const char* tag, const char* msg, ...);

	/**
	  write a "error" level log
	  @param tag a tag string
	  @param msg printf() style format of log message
	  @return 0 for success
	 */
	int e(const char* tag, const char* msg, ...);
};

extern WEBCORE_EXPORT logger_writer log;

}} // end of namespace zas::webcore
#endif // __CXX_ZAS_WEBCORE_LOGGER_H__
/* EOF */
