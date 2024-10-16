/** @file sysconfig.h
 * definition of system config object
 */

#include "utils/utils.h"
#if (defined(UTILS_ENABLE_FBLOCK_SYSCFG))

#ifndef __CXX_ZAS_UTILS_SYSCONFIG_H__
#define __CXX_ZAS_UTILS_SYSCONFIG_H__

#include "utils/utils.h"

namespace zas {
namespace utils {

/**
 * check if the sysconfig is loaded
 * @return true for loaded
 */
UTILS_EXPORT bool sysconfig_isloaded(void);

/**
  Load the system config
  @param the json format config file name
  @return 0 for success
 */
UTILS_EXPORT int load_sysconfig(const char* cfgfile);

/**
  Get the system configure by key
  @param key the key like "screen.width"
  @param defval the default value
  @param ret 0 means successful
  @return the value (note that boolean will also
  	be regarded as an integer)
 */
UTILS_EXPORT ssize_t get_sysconfig(const char* key,
	ssize_t defval = 0, int* ret = nullptr);

/**
  Get the system configure by key
  @param key the key like "screen.width"
  @param defval the default value
  @param ret 0 means successful
  @return the string value
 */
UTILS_EXPORT const char* get_sysconfig(const char* key,
	const char* defval = 0, int* ret = nullptr);


}} // end of namespace zas::utils
#endif // __CXX_ZAS_UTILS_SYSCONFIG_H__
#endif // (defined(UTILS_ENABLE_FBLOCK_SYSCFG))
/* EOF */
