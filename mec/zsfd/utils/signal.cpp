/** @file inc/signal.cpp
 * implementation of the signal handler
 */

#include "std/list.h"
#include "utils/sighdr.h"

using namespace std;

namespace zas {
namespace utils {

extern sigaction sigact;

struct sighandler
{
	void* handle_func;
};

static bool check_sigid(int sigid, int* table)
{
	for (int* cur = table; *cur != sigid_unknown; ++cur) {
		if (*cur == sigid) {
			return true;
		}
	}
	return false;
}

int sigaction::add(const char* name, int sigid, sighdr_quit_t func)
{
	int sigid_tbl[] = {
		sigid_interrupt, sigid_terminate,
		sigid_unknown,
	};

	if (!check_sigid(sigid, sigid_tbl)) {
		return -EBADPARM;
	}
	return 0;
}

}} // end of namespace zas::utils
/* EOF */
