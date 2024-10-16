#ifndef __CXX_OSM_PARSER1_CMD_PARSER_H__
#define __CXX_OSM_PARSER1_CMD_PARSER_H__

#include <vector>
#include <string>

#include "utils/json.h"
#include "utils/cmdline.h"

using namespace std;
using namespace zas::utils;

namespace osm_parser1 {

struct map_level;

enum srcfile_type {
	srcfile_type_unknown = 0,
	srcfile_type_osm,
	srcfile_type_odr,
};

struct hdmapcfg
{
	int block_width = 0;
	int block_height = 0;
	double eps = 0.1;
	struct siglight_info {
		double zoffset = 0;
		double height = 0;
		double radius = 0;
		double pitch = 0;
		double length = 0;
		double width = 0;
		double roll = 0;
		double hdg = 0;
		double pole_radius = 0;
	} siginfo;
	struct roadedge_info {
		double width = 0.3;
		double height = 0;
	} roadedge;
	struct sigboard_info {
		double height = 0.0;
		double pole_radius = 0.0;
		bool need_pole = false;
	} sigboard;
	struct hdmap_info {
		double epsilon = 0.5;
		union {
			uint32_t attrs = 0;
			struct {
				uint32_t gen_points : 1;
				uint32_t gen_geometry : 1;
				uint32_t use_extdata_file : 1;
			} a;
		};
		double block_width = 0.;
		double block_height = 0.;
		size_t filesize_limit = 2UL * 1024UL * 1024UL * 1024UL;	// 2GB
	} hdmap;
	string targetfile;
};

class cmdline_parser
{
public:
	cmdline_parser();
	~cmdline_parser();

	int parse(void);

	const char* get_srcfile(int = 0);
	const char* get_targetdir(void);
	const char* get_extdata_file(void);

	const map_level* get_maplevels(void);

public:
	bool need_statistics(void) {
		return (_attr.need_statistics) ? true : false;
	}

	bool use_wcj02(void) {
		return (_attr.use_wcj02) ? true : false;
	}

	srcfile_type get_srcfile_type(void) {
		return (srcfile_type)_attr.srcfile_type;
	}

	hdmapcfg* get_hdmap_info(void) {
		assert(nullptr != _hdmapcfg);
		return _hdmapcfg;
	}

	const string& get_version(void) {
		return _version;
	}

private:
	int check_parameters(void);

	string set_filename(const char* src);
	void get_home(void);

	int set_targetdir(const char* dir);
	int load_configfile(string filename);

	void load_configfile_get_osm_filename(jsonobject& jobj);
	void load_configfile_get_odr_filename(jsonobject& jobj);
	void load_configfile_get_target_dir(jsonobject& jobj);
	void load_configfile_get_extdata_file(jsonobject& jobj);
	int load_configfile_parse_level(jsonobject& jobj);
	int load_configfile_parse_hdmap(jsonobject& jobj);
	int load_configfile_parse_rendermap(jsonobject& jobj);
	int load_configfile_parse_hdmap_defaults(jsonobject& jobj);

	void fix_level_highway_types(int idx, int total);
	static int types_sorter(const void*, const void*);

	int set_srcfile_type(int type);

private:
	string _home;
	std::vector<string> _src_files;
	string _extdata_file;

	hdmapcfg* _hdmapcfg;

	// target dir must end with '/'
	string _target_dir;
	map_level* _maplevels;

	struct {
		uint32_t need_statistics : 1;
		uint32_t use_wcj02 : 1;
		uint32_t srcfile_type : 4;
	} _attr;
	string _version;
	static cmdline_option _opts[];
};


} // namespace osm_parser1
#endif // __CXX_OSM_PARSER1_CMD_PARSER_H__
/* EOF */
