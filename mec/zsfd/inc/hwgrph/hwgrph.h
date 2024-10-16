/** @file hwgrph.h
 */

#ifndef __CXX_ZAS_HWGRPH_H__
#define __CXX_ZAS_HWGRPH_H__

#include "std/zasbsc.h"

#ifdef LIBHWGRPH
#define HWGRPH_EXPORT		__attribute__((visibility("default")))
#else
#define HWGRPH_EXPORT
#endif

#define HWGRPH_BPP		(4)

namespace zas {
namespace hwgrph {
/**
  Bind the whole hwgrph library with the
  current render context
 */
HWGRPH_EXPORT void* bind_resource(void* display, void* conf, void* context);

}} // end of namespace zas::hwgrph
#endif /* __CXX_ZAS_HWGRPH_H__ */
/* EOF */
