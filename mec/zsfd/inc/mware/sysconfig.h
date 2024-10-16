/** @file sysconfig.h
 * definition of system config object
 */

#ifndef __CXX_ZAS_MWARE_SYSCONFIG_H__
#define __CXX_ZAS_MWARE_SYSCONFIG_H__

#include "mware/mware.h"

namespace zas {
namespace mware {

/**
  Get the system configure by key
  @param key the key like "screen.width"
  @param defval the default value
  @param ret 0 means successful
  @return the value (note that boolean will also
  	be regarded as an integer)
 */
MWARE_EXPORT ssize_t get_sysconfig(const char* key,
	ssize_t defval = 0, int* ret = nullptr);

/**
  Get the system configure by key
  @param key the key like "screen.width"
  @param defval the default value
  @param ret 0 means successful
  @return the string value
 */
MWARE_EXPORT const char* get_sysconfig(const char* key,
	const char* defval = 0, int* ret = nullptr);


}} // end of namespace zas::mware
#endif // __CXX_ZAS_MWARE_SYSCONFIG_H__
/* EOF */
