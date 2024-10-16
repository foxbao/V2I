/** @file uicore.h
 */

#ifndef __CXX_ZAS_UICORE_H__
#define __CXX_ZAS_UICORE_H__

#include "std/zasbsc.h"

#ifdef LIBUICORE
#define UICORE_EXPORT		__attribute__((visibility("default")))
#else
#define UICORE_EXPORT
#endif

#define ZAS_BPP		32

#endif /* __CXX_ZAS_UICORE_H__ */
/* EOF */
