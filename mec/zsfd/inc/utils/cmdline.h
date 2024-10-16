/** @file inc/utils/cmdline.h
 * definition of command line parser
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_CMDLINE

#ifndef __CXX_UTILS_CMDLINE_H__
#define __CXX_UTILS_CMDLINE_H__

namespace zas {
namespace utils {

struct cmdline_option
{
	const char* name;
	int has_arg;
	int* flag;
	int val;
};

#ifndef no_argument
enum cmdline_argtype
{
	no_argument = 0,
	required_argument = 1,
	optional_argument = 2,
};
#endif

class UTILS_EXPORT cmdline
{
public:
	cmdline();
	~cmdline();

	/**
	  Get the singleton instance of command line
	 */
	static cmdline* inst(void);

	/**
	  Get the count of argument
	  @return count of argc, < 0 when error happened
	 */
	int get_argc(void);

	/**
	  Get the argv[index]
	  @param index see above
	  @return the string of argv, NULL if index exceeds boundary
	 */
	const char* get_argv(int index);

	/* refer to getopt() in <unistd.h> */
	int getopt(const char *optstring);

	/* refer to getopt_long() in <getopt.h> */
	int getopt_long(const char *optstring,
		const cmdline_option *longopts, int *longindex);

	/* refer to getopt_long_only() in <getopt.h> */
	int getopt_long_only(const char *optstring,
		const cmdline_option *longopts, int *longindex);

	/**
	  Get the argument for current option
	  @return the string for the argument
	 */
	const char* get_option_arg(void);

	/**
	  reset the getopt sequence
	  @param printerr ture enables error printer
	  		while false disables
	 */
	void reset(bool printerr =  false);

private:
	void* _data;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(cmdline);
};

}} // end of namespace zas::utils
#endif // __CXX_UTILS_CMDLINE_H__
#endif // UTILS_ENABLE_FBLOCK_CMDLINE
/* EOF */
