/** @file sysconfig.h
 * definition of system config object
 */

#ifndef __CXX_ZAS_WEBCORE_SYSCONFIG_H__
#define __CXX_ZAS_WEBCORE_SYSCONFIG_H__

#include "webcore/webcore.h"

namespace zas {
namespace webcore {

/**
  Load the system config
  @param the json format config file name
  @return 0 for success
 */
WEBCORE_EXPORT int load_sysconfig(const char* cfgfile);

/**
  Get the system configure by key
  @param key the key like "screen.width"
  @param defval the default value
  @param ret 0 means successful
  @return the value (note that boolean will also
  	be regarded as an integer)
 */
WEBCORE_EXPORT ssize_t get_sysconfig(const char* key,
	ssize_t defval = 0, int* ret = nullptr);

/**
  Get the system configure by key
  @param key the key like "screen.width"
  @param defval the default value
  @param ret 0 means successful
  @return the string value
 */
WEBCORE_EXPORT const char* get_sysconfig(const char* key,
	const char* defval = 0, int* ret = nullptr);


}} // end of namespace zas::mware
#endif // __CXX_ZAS_WEBCORE_SYSCONFIG_H__
/* EOF */
