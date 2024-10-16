#include "hwgrph/hwgrph.h"

namespace zas {
namespace hwgrph {

// defintion
void init_imageloader(void);

static void __attribute__ ((constructor)) hwgrph_init(void)
{
	init_imageloader();
}

static void __attribute__ ((destructor)) hwgrph_destroy(void)
{
}

}} // end of namespace zas::hwgrph

/* EOF */

