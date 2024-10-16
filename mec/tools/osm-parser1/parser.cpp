#include <math.h>
#include "parser.h"
#include "osm-map.h"
#include "generator.h"

void show_help(const char* filename);

namespace osm_parser1 {

cmdline_parser::cmdline_parser()
: _maplevels(nullptr)
, _hdmapcfg(nullptr) {
	get_home();
	memset(&_attr, 0, sizeof(_attr));
}

cmdline_parser::~cmdline_parser()
{
	if (_maplevels) {
		delete [] _maplevels;
		_maplevels = nullptr;
	}
}

int cmdline_parser::parse(void)
{
	int i, c, ret = 0;
	auto* cmd = cmdline::inst();
	cmd->reset(true);
	while ((c = cmd->getopt_long("", _opts, &i)) != -1)
	{
		switch (c) {
		case 1000:	// help
			show_help(cmd->get_argv(0));
			exit(0);
			break;

		case 1001: {	// osm-file
			string tmp;
			tmp = set_filename(cmd->get_option_arg());
			_src_files.push_back(tmp); 
			set_srcfile_type(srcfile_type_osm);
		}	break;

		case 1002:	// target-dir
			if (set_targetdir(cmd->get_option_arg())) {
				fprintf(stderr, "invalid target directory.\n");
				return -3;
			}
			break;

		case 1003:	// target-prefix
			break;

		case 1004:	// config-file
			if (load_configfile(
				set_filename(cmd->get_option_arg()))) {
				fprintf(stderr, "fail to load the config file.\n");
				return -4;
			}
			break;

		case 1005:	// need-statistics
			_attr.need_statistics = 1;
			break;

		case 1006:	// wcj02
			_attr.use_wcj02 = 1;
			break;

		case 1007: {	// odr-file
			string tmp;
			tmp = set_filename(cmd->get_option_arg());
			_src_files.push_back(tmp);
			set_srcfile_type(srcfile_type_odr);
		}	break;
		}
	}
	if (ret) {
		return ret;
	}
	return check_parameters();
}

int cmdline_parser::set_srcfile_type(int type)
{
	assert(type != srcfile_type_unknown);
	if (_attr.srcfile_type != srcfile_type_unknown) {
		printf("multiple source map file specified.\n");
		printf("currently use '%s' as the map file to parse.\n",
			(type == srcfile_type_osm) ? "osm-file" : "odr-file");
	}
	_attr.srcfile_type = type;
	return 0;
}

int cmdline_parser::set_targetdir(const char* dir)
{
	if (!dir || !*dir) {
		return -1;
	}

	_target_dir = set_filename(dir);
	if (_target_dir[_target_dir.length() - 1] != '/') {
		_target_dir += "/";
	}
	return 0;
}

const char* cmdline_parser::get_srcfile(int i)
{
	size_t sz = _src_files.size();
	if (i >= sz) return nullptr;
	auto& ret = _src_files[i];
	return ret.c_str();
}

const char* cmdline_parser::get_extdata_file(void) {
	return _extdata_file.c_str();
}

const char* cmdline_parser::get_targetdir(void) {
	return _target_dir.c_str();
}

const map_level* cmdline_parser::get_maplevels(void) {
	return _maplevels;
}

int cmdline_parser::check_parameters(void)
{
	// check osm-file name
	if (!_src_files.size()) {
		fprintf(stderr, "error: invalid osm-filename "
			"(check if you specify the osm-filename)\n");
		return -1;
	}
	return 0;
}

string cmdline_parser::set_filename(const char* src)
{
	string dst;
	if (nullptr == src) {
		return dst;
	}
	for(; *src && (*src == ' ' || *src == '\t'); ++src);
	if (!_home.empty() && !strncmp(src, "~/", 2)) {
		src += 2;
		dst = _home;
	}
	dst += src;
	return dst;
}

void cmdline_parser::get_home(void)
{
	const char* home = getenv("HOME");
	if (nullptr != home) {
		_home = home;
		if (_home[_home.length() - 1] != '/') {
			_home += "/";
		}
	}
}

int cmdline_parser::load_configfile(string filename)
{
	string name = filename;
	if (strncmp(name.c_str(), "file://", 7)) {
		name = "file://" + name;
	}
	jsonobject& jobj = json::loadfromfile(name.c_str());
	if (jobj.is_null()) {
		return -1;
	}
	jsonobject& ver = jobj["version"];
	if (ver.is_string()) {
		_version = ver.to_string();
	}
	load_configfile_get_osm_filename(jobj);
	load_configfile_get_odr_filename(jobj);
	load_configfile_get_target_dir(jobj);
	load_configfile_get_extdata_file(jobj);
	if (load_configfile_parse_level(jobj)) {
		return -2;
	}
	if (load_configfile_parse_rendermap(jobj)) {
		return -3;
	}
	if (load_configfile_parse_hdmap(jobj)) {
		return -4;
	}
	if (load_configfile_parse_hdmap_defaults(jobj)) {
		return -5;
	}

	jobj.release();
	return 0;
}

void cmdline_parser::load_configfile_get_extdata_file(jsonobject& jobj)
{
	jsonobject& jextdata = jobj["extra-datafile"];
	if (jextdata.is_string()) {
		_extdata_file = set_filename(jextdata.to_string());
	}
}

void cmdline_parser::load_configfile_get_osm_filename(jsonobject& jobj)
{
	_src_files.clear();
	jsonobject& josm_file = jobj["osm-file"];
	const char* osm_file = josm_file.is_string() ?
		josm_file.to_string() : nullptr;
	if (osm_file && *osm_file) {
		// todo: add multiple map file support if necessary
		string tmp;
		tmp = set_filename(osm_file);
		_src_files.push_back(tmp);
		set_srcfile_type(srcfile_type_osm);
	}
}

void cmdline_parser::load_configfile_get_odr_filename(jsonobject& jobj)
{
	jsonobject& jodr_files = jobj["odr-files"];
	if (!jodr_files.is_array()) {
		return;
	}
	_src_files.clear();
	for (int i = 0; i < jodr_files.count(); ++i) {
		auto& item = jodr_files.get(i);
		if (!item.is_string()) continue;
		string tmp;
		tmp = set_filename(item.to_string());
		_src_files.push_back(tmp);
	}
	set_srcfile_type(srcfile_type_odr);
}

void cmdline_parser::load_configfile_get_target_dir(jsonobject& jobj)
{
	jsonobject& jtardir = jobj["target-dir"];
	const char* tardir = jtardir.is_string() ?
		jtardir.to_string() : nullptr;
	if (tardir && *tardir) {
		_target_dir = set_filename(tardir);
		if (_target_dir[_target_dir.length() - 1] != '/') {
			_target_dir += "/";
		}
	}
}

int cmdline_parser::load_configfile_parse_hdmap_defaults(jsonobject& jobj)
{
	jsonobject& jrm = jobj["hdmap-defaults"];
	if (!jrm.is_object()) return 0;

	if (nullptr == _hdmapcfg) {
		_hdmapcfg = new hdmapcfg;
		assert(nullptr != _hdmapcfg);
	}	

	jsonobject& jsl = jrm["signal-light"];
	if (!jsl.is_object()) return 0;

	_hdmapcfg->siginfo.zoffset = jsl.get("zoffset").to_double();
	if (fabs(_hdmapcfg->siginfo.zoffset) < 1e-3) {
		_hdmapcfg->siginfo.zoffset = 6.0;	// default: 6.0
	}
	_hdmapcfg->siginfo.height = jsl.get("height").to_double();
	if (fabs(_hdmapcfg->siginfo.height) < 1e-3) {
		_hdmapcfg->siginfo.height = 0.1;	// default: 0.1 meter
	}
	_hdmapcfg->siginfo.radius = jsl.get("radius").to_double();
	if (fabs(_hdmapcfg->siginfo.radius) < 1e-3) {
		_hdmapcfg->siginfo.radius = 0.1;	// default: 10cm
	}
	_hdmapcfg->siginfo.pitch = jsl.get("pitch").to_double();
	if (fabs(_hdmapcfg->siginfo.pitch) < 1e-3) {
		_hdmapcfg->siginfo.pitch = M_PI_2;
	}
	_hdmapcfg->siginfo.length = jsl.get("length").to_double();
	if (fabs(_hdmapcfg->siginfo.length) < 1e-3) {
		_hdmapcfg->siginfo.length = 0.;		// default: 0m
	}
	_hdmapcfg->siginfo.width = jsl.get("width").to_double();
	if (fabs(_hdmapcfg->siginfo.width) < 1e-3) {
		_hdmapcfg->siginfo.width = 0.;		// default: 0m
	}
	_hdmapcfg->siginfo.roll = jsl.get("roll").to_double();
	if (fabs(_hdmapcfg->siginfo.roll) < 1e-3) {
		_hdmapcfg->siginfo.roll = 0.;		// default: 0
	}
	_hdmapcfg->siginfo.hdg = jsl.get("hdg").to_double();
	if (fabs(_hdmapcfg->siginfo.hdg) < 1e-3) {
		_hdmapcfg->siginfo.hdg = 0.;		// default: 0
	}
	_hdmapcfg->siginfo.pole_radius = jsl.get("pole-radius").to_double();
	if (fabs(_hdmapcfg->siginfo.pole_radius) < 1e-3) {
		_hdmapcfg->siginfo.pole_radius = 0.;
	}

	jsonobject& jsb = jrm["signal-board"];
	if (!jsl.is_object()) return 0;
	_hdmapcfg->sigboard.height = jsb.get("height").to_double();
	if (fabs(_hdmapcfg->sigboard.height) < 1e-3) {
		_hdmapcfg->sigboard.height = 0.0;	// default 0.
	}
	_hdmapcfg->sigboard.need_pole = jsb.get("need-pole").to_bool();
	_hdmapcfg->sigboard.pole_radius = jsb.get("pole-radius").to_double();
	if (fabs(_hdmapcfg->sigboard.pole_radius < 1e-3)) {
		_hdmapcfg->sigboard.pole_radius = 0.0;	// default 0.
	}
	return 0;
}

int cmdline_parser::load_configfile_parse_hdmap(jsonobject& jobj)
{
	// this method is only for odr-map
	if (_attr.srcfile_type != srcfile_type_odr) {
		return 0;
	}

	jsonobject& jhm = jobj["hdmap"];
	if (!jhm.is_object()) return -1;

	if (nullptr == _hdmapcfg) {
		_hdmapcfg = new hdmapcfg;
		assert(nullptr != _hdmapcfg);
	}
	_hdmapcfg->hdmap.epsilon = jhm.get("epsilon").to_double();
	if (_hdmapcfg->hdmap.epsilon < 1e-3) {
		_hdmapcfg->hdmap.epsilon = 0.5;	// use 0.5 as defaults
	}
	_hdmapcfg->hdmap.a.gen_points = jhm.get("generate-points")
		.to_bool() ? 1 : 0;
	if (!_hdmapcfg->hdmap.a.gen_points) {
		_hdmapcfg->hdmap.epsilon = 0.0;	// no points
	}
	_hdmapcfg->hdmap.a.gen_geometry = jhm.get("generate-geometry")
		.to_bool() ? 1 : 0;
	_hdmapcfg->hdmap.a.use_extdata_file = jhm.get("use-extdatafile")
		.to_bool() ? 1 : 0;
	if (_hdmapcfg->hdmap.a.use_extdata_file) {
		jsonobject& jhm_extd = jhm.get("extdatafile");
		if (jhm_extd.is_object()) {
			_hdmapcfg->hdmap.block_width = jhm_extd.get("block-width")
				.to_double();
			if (_hdmapcfg->hdmap.block_width < 1e-3) {
				_hdmapcfg->hdmap.block_width = 0.0;
			}
			_hdmapcfg->hdmap.block_height = jhm_extd.get("block-height")
				.to_double();
			if (_hdmapcfg->hdmap.block_height < 1e-3) {
				_hdmapcfg->hdmap.block_height = 0.0;
			}
			_hdmapcfg->hdmap.filesize_limit = size_t(jhm_extd.get("filesize-limit")
				.to_double());
			if (!_hdmapcfg->hdmap.filesize_limit) {
				_hdmapcfg->hdmap.filesize_limit = 2UL * 1024UL * 1024UL * 1024UL;
			}
		}
	}
	return 0;
}

int cmdline_parser::load_configfile_parse_rendermap(jsonobject& jobj)
{
	// this method is only for odr-map
	if (_attr.srcfile_type != srcfile_type_odr) {
		return 0;
	}

	jsonobject& jrm = jobj["render-map"];
	if (!jrm.is_object()) return -1;

	if (nullptr == _hdmapcfg) {
		_hdmapcfg = new hdmapcfg;
		assert(nullptr != _hdmapcfg);
	}	
	_hdmapcfg->block_width = jrm.get("block-width").to_double();
	_hdmapcfg->block_height = jrm.get("block-height").to_double();
	if (!_hdmapcfg->block_width
		|| !_hdmapcfg->block_height) {
		return -2;
	}
	_hdmapcfg->eps = jrm.get("epsilon").to_double();
	if (fabs(_hdmapcfg->eps) < 1E-6) {
		_hdmapcfg->eps = 0.1;
	}
	jsonobject& jroadedge = jrm.get("road-edge");
	if (!jroadedge.is_object()) {
		return -3;
	}
	jsonobject& jroadedge_width = jroadedge.get("width");
	if (jroadedge_width.is_number()) {
		_hdmapcfg->roadedge.width = jroadedge_width.to_double();
	}
	jsonobject& jroadedge_height = jroadedge.get("height");
	if (jroadedge_height.is_number()) {
		_hdmapcfg->roadedge.height = jroadedge_height.to_double();
	}
	return 0;
}

#define parse_level_rel_ret(r) do {	\
	delete [] _maplevels;	\
	_maplevels = nullptr;	\
	return (r);	\
} while (0)

int cmdline_parser::load_configfile_parse_level(jsonobject& jobj)
{
	// this method is only for osm-file
	if (_attr.srcfile_type != srcfile_type_osm) {
		return 0;
	}

	jsonobject& jlevels = jobj["levels"];
	if (!jlevels.is_array()) return -1;
	int cnt = jlevels.count();
	if (!cnt) return -2;

	_maplevels = new map_level [cnt + 1];
	assert(nullptr != _maplevels);

	for (int i = 0; i < cnt; ++i) {
		jsonobject& jitem = jlevels[i];
		if (!jitem.is_object()) {
			parse_level_rel_ret(-3);
		}
		// level
		jsonobject& jlevel = jitem["level"];
		if (!jlevel.is_number()) {
			parse_level_rel_ret(-4);
		}
		int level = jlevel.to_number();
		if (level < 0 || level >= cnt) {
			parse_level_rel_ret(-5);
		}
		auto& maplevel = _maplevels[level];
		if (maplevel.row) parse_level_rel_ret(-6);

		// block max nodes
		jsonobject& jblkmaxnodes = jitem["block-max-nodes"];
		if (jblkmaxnodes.is_number()) {
			maplevel.blk_max_nodes = jblkmaxnodes.to_number();
		}
		// type list
		jsonobject& jtypes = jitem["road-types"];
		if (!jtypes.is_null()) {
			if (!jtypes.is_array()) {
				parse_level_rel_ret(-7);
			}
			
			maplevel.types.reset();
			int type_cnt = jtypes.count();
			if (i + 1 != cnt) for (int j = 0; j < type_cnt; ++j) {
				jsonobject& jtype = jtypes[j];
				if (!jtype.is_string()) {
					parse_level_rel_ret(-8);
				}
				int tid = osm_map::get_highway_type(jtype.to_string());
				if (tid < 0) {
					parse_level_rel_ret(-9);
				}
				// make sure no duplication
				for (int k = 0; k < maplevel.types.getsize(); ++k) {
					if (maplevel.types.buffer()[k] == tid) {
						tid = -1;
					}
				}
				if (tid >= 0) maplevel.types.add(tid);
			}
		}
		else if (i + 1 != cnt) {
			parse_level_rel_ret(-10);
		}
		// make a temp value of row and col
		maplevel.row = maplevel.col = -1;	
	}

	// verify all levels
	for (int i = 0; i < cnt; ++i) {
		map_level& maplevel = _maplevels[i];
		if (!maplevel.row) parse_level_rel_ret(-11);

		// fix highway types
		fix_level_highway_types(i, cnt);
	}

	// turn on the "statistics" since we need the data
	// to calcuate the row and col
	if (cnt) _attr.need_statistics = 1;
	return 0;
}

void cmdline_parser::fix_level_highway_types(int idx, int total)
{
	auto& curr = _maplevels[idx];

	if (idx + 1 == total) {
		assert(_maplevels[idx].types.getsize() == 0);
		return;		
	}
	for (int i = 0; i < idx; ++i) {
		auto& level = _maplevels[i];
		int cnt = level.types.getsize();
		for (int j = 0; j < cnt; ++j) {
			curr.types.add(level.types.buffer()[j]);
		}
		qsort(level.types.buffer(), level.types.getsize(),
			sizeof(int), types_sorter);
	}
}

int cmdline_parser::types_sorter(const void* a, const void* b)
{
	auto aa = *((int*)a);
	auto bb = *((int*)b);
	if (aa > bb) return 1;
	else if (aa < bb) return -1;
	else return 0;
}

cmdline_option cmdline_parser::_opts[] =
{
	{ "help", no_argument, nullptr, 1000 },
	{ "osm-file", optional_argument, nullptr, 1001 },
	{ "target-dir", optional_argument, nullptr, 1002 },
	{ "target-prefix", optional_argument, nullptr, 1003},
	{ "config-file", optional_argument, nullptr, 1004 },
	{ "need-statistics", optional_argument, nullptr, 1005 },
	{ "wcj02", optional_argument, nullptr, 1006 },
	{ "odr-file", optional_argument, nullptr, 1007 },
	{ 0, 0, nullptr, 0 },
};

} // end of namespace osm_parser1
/* EOF */
