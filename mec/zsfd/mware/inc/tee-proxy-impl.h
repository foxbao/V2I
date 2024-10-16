/** @file tee-proxy-impl.h
 * defintion of filelist classes
 */

#ifndef __CXX_ZAS_MWARE_TEE_PROXY_IMPL_H__
#define __CXX_ZAS_MWARE_TEE_PROXY_IMPL_H__

#include "mware/mware.h"

namespace zas {
namespace mware {

// hashed trace code
typedef uint8_t tracecode[32];

class tee_hardware_impl
{
public:
	tracecode get_trace_code(void);
};

class tee_proxy_impl
{
};

}} // end of namespace zas::mware
#endif // __CXX_ZAS_MWARE_TEE_PROXY_IMPL_H__
/* EOF */
