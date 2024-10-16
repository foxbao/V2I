/** @file sighdr.h
 * Definition of signal handler
 */

#ifndef __CXX_ZAS_UTILS_SIGNAL_HANDLER_H__
#define __CXX_ZAS_UTILS_SIGNAL_HANDLER_H__

#include "utils/utils.h"

namespace zas {
namespace utils {

typedef void (*sighdr_quit_t)(int sigid);

enum sigid {
	sigid_unknown = 0,

	// ctrl-c
	// handler: sig_quit
	sigid_interrupt,	// --> SIGINT

	// kill <pid>
	sigid_terminate,	// --> SIGTERM
};

class UTILS_EXPORT sigaction
{
public:
	/**
	  set the quit action handler
	  note: sigid_interrupt and sigid_terminate is 
	  	compatiable with this method
	  @param name the action handler name
	  @param sigid the action signal id
	  @param func the action func
	  @return 0 for success
	 */
	int add(const char* name, int sigid, sighdr_quit_t func);
};

extern sigaction sigact;

}} // end of namespace zas::utils
#endif // __CXX_ZAS_UTILS_SIGNAL_HANDLER_H__
/* EOF */
