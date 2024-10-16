#include "utils/utils.h"
#if (defined(UTILS_ENABLE_FBLOCK_EVLOOP) && defined(UTILS_ENABLE_FBLOCK_LOG))

#ifndef __CXX_ZAS_UTILS_LOG_H__ 
#define __CXX_ZAS_UTILS_LOG_H__

#include "utils/utils.h"

namespace zas {
namespace utils {

enum class log_level : uint8_t {
	unknown = 0,
	error,
	warning,
	debug,
	info,
	verbose,
};

class evloop;

class UTILS_EXPORT log_collector
{
public:
	log_collector() = delete;
	~log_collector() = delete;

	/**
	 * @brief create the single instance of the log collector
	 * @return log_collector* the singleton object
	 */
	static log_collector* inst(void);

public:
	/**
	 * @brief bind the current collector to a server evloop
	 * 		note that it could only bind to a server
	 * @return int 0 for success
	 */
	int bindto(evloop*);

	ZAS_DISABLE_EVIL_CONSTRUCTOR(log_collector);
};

class UTILS_EXPORT log_client
{
public:
	log_client() = delete;
	~log_client() = delete;

	/**
	 * @brief output the log information
	 * 
	 * @param level the output level (see log_level)
	 * @param tag the tag string of this module
	 * @param content the content string of this module
	 * @return int 0 for success
	 */
	int write(log_level level, const char* tag, const char* content);

	/**
	 * @brief output an error log
	 * 
	 * @param tag the tag of the log
	 * @param fmt the content of the log (printf() style)
	 * @param ... the parameters
	 * @return int 
	 */
	int e(const char* tag, const char* fmt, ...);

	/**
	 * @brief output a warning log
	 * 
	 * @param tag the tag of the log
	 * @param fmt the content of the log (printf() style)
	 * @param ... the parameters
	 * @return int 
	 */
	int w(const char* tag, const char* fmt, ...);

	/**
	 * @brief output a debug log
	 * 
	 * @param tag the tag of the log
	 * @param fmt the content of the log (printf() style)
	 * @param ... the parameters
	 * @return int 
	 */
	int d(const char* tag, const char* fmt, ...);

	/**
	 * @brief output an info log
	 * 
	 * @param tag the tag of the log
	 * @param fmt the content of the log (printf() style)
	 * @param ... the parameters
	 * @return int 
	 */
	int i(const char* tag, const char* fmt, ...);

	/**
	 * @brief output an verbose log
	 * 
	 * @param tag the tag of the log
	 * @param fmt the content of the log (printf() style)
	 * @param ... the parameters
	 * @return int 
	 */
	int v(const char* tag, const char* fmt, ...);

	ZAS_DISABLE_EVIL_CONSTRUCTOR(log_client);
};

extern UTILS_EXPORT log_client* log;

}}
#endif // __CXX_ZAS_UTILS_LOG_H__
#endif // (defined(UTILS_ENABLE_FBLOCK_EVLOOP) && defined(UTILS_ENABLE_FBLOCK_LOG))
/* EOF */
