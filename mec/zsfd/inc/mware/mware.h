/** @file mware.h
 */

#ifndef __CXX_ZAS_MIDWARE_H__
#define __CXX_ZAS_MIDWARE_H__

#include "std/zasbsc.h"

#ifdef LIBMWARE
#define MWARE_EXPORT		__attribute__((visibility("default")))
#else
#define MWARE_EXPORT
#endif

#endif /* __CXX_ZAS_MWARE_H__ */
/* EOF */
