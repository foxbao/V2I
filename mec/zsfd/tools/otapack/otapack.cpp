#include "utils/cmdline.h"
#include "config.h"
#include "filelist.h"
#include "merge.h"
#include "pack.h"

using namespace zas::utils;

otapack_config::otapack_config()
{
	memset(this, 0, sizeof(otapack_config));
	
	// set default values
	need_compress = 0;							// we compress the ota package
	compression_block_size = 4 * 1024 * 1024;	// the compress block is set as 8MB
	compress_level= 9;							// compress level set as 9 (smallest)
}

const char* helpstr[][2] =
{
	{ "--help", "Display this information" },
	{ "--config-file=<file>",		"use a json configuration file for inputs" },
	{ "",							"all command line parameters will applied later than" },
	{ "",							"the json configuration file" },
	{ "--src-dir=<dir>",			"specify the source directory" },
	{ "--dst-dir=<dir>",			"specify the destination directory" },
	{ "-c, --compress",				"compress the OTA package content" },
	{ "--full-compress",			"compress the whole OTA package" },
	{ "--clevel=<1-9>",				"specify the compression level (1 - fastest, 9 - best)" },
	{ "-o <name>",					"same as --pkg-name (see below)" },
	{ "--pkg-name=<name>",			"specify the OTA package name to be generated" },
	{ "-f, --force",				"no prompt before critical operations" },
	{ nullptr, nullptr },
};

static void show_help(const char* filename)
{
	const char* raw = filename;
	for (; *raw; ++raw) {
		if (*raw == '/') filename = raw + 1;
	}
	printf("Usage: %s [options]\nOptions\n", filename);
	for (int i = 0; helpstr[i][0]; ++i) {
		printf("%-28s%s\n", helpstr[i][0], helpstr[i][1]);
	}
}

class cmdline_parser
{
public:
	cmdline_parser(otapack_config& cfg)
	: _cfg(cfg) {
		get_home();
	}

	int parse(void)
	{
		int i, c, ret = 0;
		auto* cmd = cmdline::inst();
		cmd->reset(true);
		while ((c = cmd->getopt_long("co:f", _opts, &i)) != -1)
		{
			switch (c) {
			case 1000:	// help
				show_help(cmd->get_argv(0));
				exit(0);
				break;

			case 1001:	// config-file
				break;

			case 1002:	// src-dir
				_cfg.src_dir = set_filename(cmd->get_option_arg());
				break;

			case 1003:	// dst-dir
				_cfg.dst_dir = set_filename(cmd->get_option_arg());
				break;

			case 'c':	// compress
				_cfg.need_compress = 1;
				break;

			case 1004:		// compression level
				ret = set_clevel(cmd->get_option_arg());
				break;

			case 'o':	// OTA package name
				ret = set_otapack_name();
				break;

			case 'f':		// force
				_cfg.force = 1;
				break;

			case 1005:	// full-compress
				_cfg.need_full_compress = 1;
				break;
			}
		}
		if (ret) {
			return ret;
		}
		return check_parameters();
	}

private:
	int set_otapack_name(void)
	{
		const char* opt = cmdline::inst()->get_option_arg();
		for (; *opt && (*opt == ' ' || *opt == '\t'); ++opt);

		if (is_cmd_parameter(opt)) {
			fprintf(stderr, "error: \"%s\" is not a valid file name.\n", opt);
			return -1;
		}
		_cfg.otapack_name = set_filename(opt);
		return 0;
	}

	void get_home(void)
	{
		const char* home = getenv("HOME");
		if (nullptr != home) {
			_home = home;
			if (_home[_home.length() - 1] != '/') {
				_home += "/";
			}
		}
	}

	int check_parameters(void)
	{
		if (check_folders()) return -1;
		if (check_pkgname()) return -2;
		return 0;
	}

	string& set_filename(const char* src)
	{
		static string dst;
		dst = "";
		if (nullptr == src) {
			return dst;
		}
		for(; *src && (*src == ' ' || *src == '\t'); ++src);
		if (!_home.empty() && !strncmp(src, "~/", 2)) {
			src += 2;
			dst = _home;
		}
		dst += src;
	}

	int is_otapack_in_dir(const char* dir)
	{
		string dst_dir = get_fullname(dir);
		const char* start = _cfg.otapack_name.c_str();
		const char* end = start + _cfg.otapack_name.length();
		for (; end > start && *end != '/'; --end);
		string filename = string(start, 0, end - start);
		if (!filename.length()) filename = ".";
		filename = get_fullname(filename.c_str());

		if (dst_dir.empty() || filename.empty()) {
			return -1;
		}
		return (strncmp(dst_dir.c_str(), filename.c_str(),
			dst_dir.length())) ? false : true;
	}

	int check_pkgname(void)
	{
		if (_cfg.otapack_name.empty()) {
			fprintf(stderr, "error: OTA package name not specified.\n");
			return -1;
		}

		// check if the generate package file is in the destination directory
		// which is not allowed
		int ret = is_otapack_in_dir(_cfg.dst_dir.c_str());
		if (ret < 0) { return -2; }
		else if (ret > 0) {
			printf("error: OTA package file could not be created "
				"in destination directory.\n");
			return -3;
		}
		
		if (fileexists(_cfg.otapack_name.c_str())) {
			int c;
			if (!_cfg.force) {
				do {
					printf("\rOTA package \"%s\" exists. Overwrite? (y/n):",
						_cfg.otapack_name.c_str());
					c = getchar();
				} while (c != 'Y' && c != 'y' && c != 'N' && c != 'n');
				if (c == 'n' || c == 'N') {
					fprintf(stderr, "exit because user selected not to overwrite.\n");
					return -4;
				}
			}
		}
		return 0;
	}

	int check_folders(void)
	{
		if (_cfg.src_dir.empty()) {
			fprintf(stderr, "error: source directory is not specified.\n");
			return -1;
		}
		if (!isdir(_cfg.src_dir.c_str())) {
			fprintf(stderr, "error: \"%s\" is not a valid source directory.\n",
				_cfg.src_dir.c_str());
			return -2;
		}
		if (_cfg.dst_dir.empty()) {
			fprintf(stderr, "error: destination directory is not specified.\n");
			return -3;
		}
		if (!isdir(_cfg.dst_dir.c_str())) {
			fprintf(stderr, "error: \"%s\" is not a valid destination directory.\n",
				_cfg.dst_dir.c_str());
			return -4;
		}
		return 0;
	}

	int set_clevel(const char* str)
	{
		long val = strtol(str, NULL, 10);
		if (errno == EINVAL) {
			fprintf(stderr, "error: unrecognized compress level: %s\n", str);
			return 0;
		}
		if (val < 1) {
			fprintf(stderr, "warning: fix the compress level to 1.\n");
			val = 1;
		}
		else if (val > 9) {
			fprintf(stderr, "warning: fix the compress level to 9.\n");
			val = 9;
		}
		_cfg.compress_level = (uint32_t)val;
		return 0;
	}

	string get_fullname(const char* filename)
	{
		string tmp;
		char *ret = canonicalize_file_name(filename);
		if(nullptr != ret) {
			tmp = ret;
		}
		free(ret);
		return tmp;
	}

	bool is_cmd_parameter(const char* tar)
	{
		if (*tar != '-') return false;

		cmdline_option *c = _opts;
		if (tar[1] == '-') {
			// long param ("--")
			tar += 2;
			for (; c->name != nullptr; ++c) {
				if (!strcmp(c->name, tar)) return true;
			}
		}
		else if (!tar[2]) {
			// short param ("-")
			++tar;
			for (; c->name != nullptr; ++c) {
				if (c->val >= 1000) continue;
				if (c->val == *tar) return true;
			}
		}
		return false;
	}

private:
	otapack_config& _cfg;
	string _home;
	static cmdline_option _opts[];
};

cmdline_option cmdline_parser::_opts[] =
{
	{ "help", no_argument, nullptr, 1000 },
	{ "config-file", optional_argument, nullptr, 1001 },
	{ "src-dir", optional_argument, nullptr, 1002 },
	{ "dst-dir", optional_argument, nullptr, 1003},
	{ "compress", no_argument, nullptr, 'c' },
	{ "clevel", optional_argument, nullptr, 1004 },
	{ "pkg-name", optional_argument, nullptr, 'o' },
	{ "force", no_argument, nullptr, 'f' },
	{ "full-compress", no_argument, nullptr, 1005 },
	{ 0, 0, nullptr, 0 },
};

class pkgen_listener : public pack_generator_listener
{
public:
	pkgen_listener(otapack_config& cfg) : _cfg(cfg) {}

private:
	void on_generating_progress(int percentage) {
		printf("\rgenerating %s ... [%u%%]",
			_cfg.otapack_name.c_str(), percentage);
	}

	void on_compressing_progress(int percentage) {
		printf("\rCompress package ... %u%%", percentage);
		fflush(nullptr);
	}

	otapack_config& _cfg;
};

static int generate_otapack(pack_generator& pkgen,
	filelist& dst_filelist,
	const char* pack_name)
{
	printf("generating %s ...", pack_name);
	fflush(nullptr);
	
	if (pkgen.generate(pack_name)) {
		printf("\nfail in generating OTA package.\n");
		printf("error description: %s.\n", pkgen.err_string());
		return 1;
	}

	size_t orig = dst_filelist.get_totalsize();
	size_t curr = pkgen.get_totalsize();
	printf("\r\"%s\" generated. [%lu bytes (%.1f MB) -> %lu bytes (%.1f MB)]\n",
		pack_name,
		orig, double(orig) / (1024. * 1024.),
		curr, double(curr) / (1024. * 1024.));

	return 0;
}

static int compress_otapack(pack_generator& pkgen)
{
	otapack_config& cfg = pkgen.getconfig();
	if (!cfg.need_compress) {
		return 0;
	}

	printf("Compress package ...");
	fflush(nullptr);
	if (pkgen.compress_package()) {
		printf("[failed]\n");
		printf("err descirption: %s.\n", pkgen.err_string());
		pkgen.set_listener(nullptr);
		return 1;
	}

	// show successfully finished
	printf("\rOTApack compressed successfully.\n");
	pkgen.set_listener(nullptr);
	return 0;
}

int main(int argc, char* argv[])
{
	if (argc == 1) {
		show_help(argv[0]);
		return 0;
	}

	otapack_config cfg;
	cmdline_parser parser(cfg);
	if (parser.parse()) {
		return 1;
	}
	// test code. TODO: need modify
	cfg.prev_version = 0x01000000;
	cfg.curr_version = 0x01000001;

	filelist src_filelist;
	pack_source_listener src_lnr(src_filelist);
	src_filelist.add_listener(&src_lnr);

	if (src_filelist.loaddir(cfg.src_dir.c_str())) {
		fprintf(stderr, "\rScanning ...\n"
			"fail in scanning the source folder.\n");
		return 2;
	}
	
	size_t dircnt, filecnt;
	src_filelist.get_countinfo(dircnt, filecnt);
	if (dircnt + filecnt == 0) {
		fprintf(stderr, "Warning: there is no file to be packed.\n"
			"Exit with no package created.\n");
		return 0;
	}

	filelist dst_filelist;
	merge_file_listener mlnr(src_filelist, dst_filelist);
	dst_filelist.add_listener(&mlnr);

	if (dst_filelist.loaddir(cfg.dst_dir.c_str())) {
		fprintf(stderr, "\rScanning ...\n"
			"fail in scanning the destination folder.\n");
		return 3;
	}

	// re-scan all pending items, most of which is not referred
	// in new package, meaning it shall be deleted
	src_lnr.rescan_delete_objects(mlnr.get_deleted_list());

	pack_generator pkgen(src_lnr, mlnr, cfg);
	pkgen_listener lnr(cfg);
	pkgen.set_listener(&lnr);

	if (generate_otapack(pkgen, dst_filelist,
		cfg.otapack_name.c_str())) {
		return 4;
	}
	if (compress_otapack(pkgen)) {
		return 5;
	}
	return 0;
}