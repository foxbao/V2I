#include <pthread.h>

#include "webapp-impl.h"

namespace zas {
namespace webcore {

extern "C" {

static void __attribute__ ((constructor)) webcore_init(void)
{
	
}

static void __attribute__ ((destructor)) webcore_destroy(void)
{
}

}; // end of extern "C"
}} // end of namespace zas::webcore
/* EOF */
