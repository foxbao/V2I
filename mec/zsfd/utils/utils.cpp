#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "utils/cmdline.h"
#include "utils/shmem.h"

using namespace std;

namespace zas {
namespace utils {

void absfile_init(void);
void reset_semaphore(void);
void reset_shared_memory(void);
void certutils_init(void);

static void __attribute__ ((constructor)) zas_utils_init(void)
{
	// initialize the command line parser
#ifdef UTILS_ENABLE_FBLOCK_CMDLINE
	cmdline::inst();
	reset_semaphore();
#endif

#if (defined(UTILS_ENABLE_FBLOCK_SHRMEM) && defined(UTILS_ENABLE_FBLOCK_CMDLINE))
	reset_shared_memory();
#endif

#ifdef UTILS_ENABLE_FBLOCK_ABSFILE
	absfile_init();
#endif

#ifdef UTILS_ENABLE_FBLOCK_CERT
	certutils_init();
#endif
}

static void __attribute__ ((destructor)) zas_utils_destroy(void)
{
}

}} // end of namespace zas::utils
/* EOF */
