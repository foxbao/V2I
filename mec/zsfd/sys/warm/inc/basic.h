#ifndef __CXX_ZAS_SYS_WARM_BASIC_H
#define __CXX_ZAS_SYS_WARM_BASIC_H

#define WLR_USE_UNSTABLE

#include <std/zasbsc.h>
extern "C" {
	#include <wlr/util/log.h>
}

// temporary log implementation
#define warm_loge(fmt, ...)	\
	wlr_log(WLR_ERROR, fmt, ##__VA_ARGS__)
#define warm_logi(fmt, ...)	\
	wlr_log(WLR_INFO, fmt,  ##__VA_ARGS__)
#define warm_logd(fmt, ...)	\
	wlr_log(WLR_DEBUG, fmt, ##__VA_ARGS__)

#define delete_and_reset(ptr)	\
do {	\
	if (nullptr != (ptr)) {	\
		delete (ptr);	\
		(ptr) = nullptr;	\
	}	\
} while (0)

namespace zas {
namespace sys {
namespace warm {
}}} // end of namespace zas::sys::warm
#endif // __CXX_ZAS_SYS_WARM_BASIC_H
/* EOF */
