/** @file cmdline.cpp
 * implementation of command line parser
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_CMDLINE

#include <unistd.h>
#include <getopt.h>

#include "utils/cmdline.h"

namespace zas {
namespace utils {

class cmdline_impl
{
public:
	cmdline_impl()
	: _cmdline_length(0)
	, _cmdline_buffer(NULL)
	, _argc(0), _argv(NULL)
	{
		reset(false);
		if (!load()) make_argcv();
	}

	~cmdline_impl()
	{
		if (_cmdline_buffer) {
			free((void*)_cmdline_buffer);
			_cmdline_buffer = NULL;
		}
		_cmdline_length = 0;
		if (_argv) {
			free((void*)_argv);
			_argv = NULL;
		}
		_argc = 0;
	}

	int get_argc(void) {
		return _argc;
	}

	const char* get_argv(int index) {
		if (index < 0) return nullptr;
		if (index >= _argc) return nullptr;
		return _argv[index];
	}

	int getopt(const char *optstring)
	{
		if (!optstring) {
			return -EBADPARM;
		}
		if (_argc < 1 || !_argv) {
			return -EBADOBJ;
		}
		return ::getopt(_argc, _argv, optstring);
	}

	int getopt_long(const char *optstring,
		const cmdline_option *longopts, int *longindex)
	{
		if (!optstring) {
			optstring = "";
		}
		if (_argc < 1 || !_argv) {
			return -EBADOBJ;
		}
		assert(sizeof(option) == sizeof(cmdline_option));
		return ::getopt_long(_argc, _argv,
			optstring, (option*)longopts, longindex);
	}

	int getopt_long_only(const char *optstring,
		const cmdline_option *longopts, int *longindex)
	{
		if (!optstring) {
			optstring = "";
		}
		if (_argc < 1 || !_argv) {
			return -EBADOBJ;
		}
		assert(sizeof(option) == sizeof(cmdline_option));
		return ::getopt_long_only(_argc, _argv,
			optstring, (option*)longopts, longindex);
	}

	const char* get_option_arg(void) {
		return optarg;
	}

	void reset(bool printerr) {
		optind = 1;
		opterr = (printerr) ? 1 : 0;
	}

private:
	int load(void)
	{
		FILE *fp = fopen("/proc/self/cmdline", "r");
		if (!fp) return -1;

		int c;
		char buf[256];
		char* cl = buf;
		size_t cursize = 0, totalsz = 256;

		while ((c = fgetc(fp)) != EOF)
		{
			if (cursize >= totalsz)
			{
				totalsz += 256;
				char* newptr = (char*)malloc(totalsz);
				memcpy(newptr, cl, cursize);
				if (cl != buf) free(cl);
				cl = newptr;
			}
			cl[cursize++] = (char)c;
		}
		if (cl == buf) {
			char *tmp = (char*)malloc(cursize);
			memcpy(tmp, cl, cursize);
			_cmdline_buffer = (char*)tmp;
		}
		else _cmdline_buffer = (char*)cl;
		_cmdline_length = cursize;
		fclose(fp);
		return 0;
	}

	int make_argcv(void)
	{
		if (!_cmdline_buffer || !_cmdline_length) {
			return -1;
		}

		// get arguments count
		char* start = _cmdline_buffer;
		char* end = start + _cmdline_length;
		for (_argc = 0; start < end; ++_argc) {
			start += strlen(start) + 1;
		}
		if (_argc < 1) return -2;

		// create the argv list
		_argv = (char**)malloc(sizeof(char*) * _argc);
		assert(NULL != _argv);

		start = _cmdline_buffer;
		for (int i = 0; start < end; ++i) {
			_argv[i] = start;
			start += strlen(start) + 1;
		}
		return 0;
	}

private:
	size_t _cmdline_length;
	char* _cmdline_buffer;
	int _argc;
	char** _argv;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(cmdline_impl);
};

cmdline::cmdline()
{
	cmdline_impl* obj = new cmdline_impl();
	assert(NULL != obj);
	_data = (void*)obj;
}

cmdline::~cmdline()
{
	if (NULL != _data) {
		cmdline_impl* obj = reinterpret_cast<cmdline_impl*>(_data);
		delete obj;
		_data = NULL;
	}
}

cmdline* cmdline::inst(void)
{
	static cmdline scl;
	return &scl;
}

int cmdline::get_argc(void)
{
	if (!_data) return -1;
	auto* impl = reinterpret_cast<cmdline_impl*>(_data);
	return impl->get_argc();
}

const char* cmdline::get_argv(int index)
{
	if (!_data) return nullptr;
	auto* impl = reinterpret_cast<cmdline_impl*>(_data);
	return impl->get_argv(index);
}

int cmdline::getopt(const char *optstring)
{
	if (!_data) return -1;
	auto* impl = reinterpret_cast<cmdline_impl*>(_data);
	return impl->getopt(optstring);
}

int cmdline::getopt_long(const char *optstring,
	const cmdline_option *longopts, int *longindex)
{
	if (!_data) return -1;
	auto* impl = reinterpret_cast<cmdline_impl*>(_data);
	return impl->getopt_long(optstring, longopts, longindex);
}

int cmdline::getopt_long_only(const char *optstring,
	const cmdline_option *longopts, int *longindex)
{
	if (!_data) return -1;
	auto* impl = reinterpret_cast<cmdline_impl*>(_data);
	return impl->getopt_long_only(optstring, longopts, longindex);
}

const char* cmdline::get_option_arg(void)
{
	if (!_data) return NULL;
	auto* impl = reinterpret_cast<cmdline_impl*>(_data);
	return impl->get_option_arg();
}

void cmdline::reset(bool printerr)
{
	if (!_data) return;
	auto* impl = reinterpret_cast<cmdline_impl*>(_data);
	impl->reset(printerr);
}
}} // end of namespace zas::utils
#endif // UTILS_ENABLE_FBLOCK_CMDLINE
/* EOF */
