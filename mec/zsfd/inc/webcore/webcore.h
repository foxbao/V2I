/** @file webcore.h
 */

#ifndef __CXX_ZAS_WEBCORE_H__
#define __CXX_ZAS_WEBCORE_H__

#include "std/zasbsc.h"

#ifdef LIBWEBCORE
#define WEBCORE_EXPORT		__attribute__((visibility("default")))
#else
#define WEBCORE_EXPORT
#endif

#endif /* __CXX_ZAS_WEBCORE_H__ */
/* EOF */
