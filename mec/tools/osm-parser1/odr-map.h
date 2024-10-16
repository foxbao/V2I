#ifndef __CXX_OSM_PARSER1_ODR_MAP_H__
#define __CXX_OSM_PARSER1_ODR_MAP_H__

#include <string>
#include "std/list.h"
#include "utils/avltree.h"

#include "parser.h"
#include "hdmap-format.h"
#include "odr-utils.h"

#include "Lanes.h"
#include "RefLine.h"

#include "mapcore/hdmap-render.h"

void* operator new(size_t sz, size_t extsz, size_t& pos, void** r) throw();
void operator delete(void* ptr, size_t);
void* operator new[](size_t sz, size_t& pos, void** r) throw();
void operator delete[](void* ptr, size_t);

namespace osm_parser1 {

#define POINT_DISTANCE(x, y) (sqrt((x)*(x) + (y)*(y)))

using namespace odr;
using namespace std;
using namespace zas::utils;
using namespace zas::mapcore;
using namespace zas::mapcore::render_hdmap;

struct roadlink;
struct refl_geometry;

class odr_loader;
class odr_road;
class odr_object;
class odr_lane;
class odr_junction;
class odr_element;
class odr_laneoffset;
class odr_lane_section;
class odr_junction_connection;

struct curve_segment;
struct hdmap_lane;
struct hdmap_laneseg;
struct file_memory;
struct hdmap_memory;
struct hdmap_roadinfo;
struct hdmap_boundary;
struct hdmap_container;
struct hdmap_blockmgr;
struct refline_fragment;
struct curve_segment_ref;

struct exd_siglight_info;
struct exd_roadside_device_info;
struct exd_arrow_info;
struct exd_sigboard_info;

// the minimum interval of the polyline
// for the curve_segment
#define CSP_MIN_INTERVAL	(1.0)

enum curveseg_type
{
	curveseg_type_unknown = 0,
	curveseg_type_refline,
	curveseg_type_lanesec,
};

struct hdmap_point : public point3d
{
	hdmap_point(double x, double y, double z)
	: point3d(x, y, z) {}
};

struct refline_point : public hdmap_point
{
	refline_point(double x, double y, double z)
	: hdmap_point(x, y, z) {}
	refline_point(double x, double y, double z, double s)
	: hdmap_point(x, y, z), s(s) {}
	double s;
};

struct lane_point : public hdmap_point
{
	lane_point(double x, double y, double z)
	: hdmap_point(x, y, z) {}
	lane_point(double x, double y, double z, double w, double s)
	: hdmap_point(x, y, z), width(w), s(s) {}
	double width;
	double s;
};

struct box_point {
	double x,y;
};

struct bounding_box
{
	box_point left_top_point; 
	box_point left_bottom_point;
	box_point right_top_point;
	box_point right_bottom_point;
};

struct curve_segment
{
	curve_segment(odr_lane* lane);
	curve_segment(odr_road*, hdmap_blockmgr*, double);
	~curve_segment();

	int generate_points(hdmap_roadinfo* rinfo, hdmap_blockmgr* blkmgr, double eps) {
		return generate_points(rinfo, blkmgr, eps, true);
	}
	int get_lsegid_count(void);
	int generate_points(hdmap_roadinfo*, hdmap_blockmgr*, double, bool);
	void finalize(hdmap_blockmgr* blkmgr, double eps);

	int blockinfo_getmax(void);
	void blockinfo_addpoint(hdmap_blockmgr* blkmgr, int x, int y, int ptidx);

	union {
		uint32_t attrs;
		struct {
			uint32_t type : 4;
		} a;
	};

	struct cseg_ptseg {
		int pt_start, count;
		cseg_ptseg() : pt_start(-2), count(0) {}
		cseg_ptseg(int start) : pt_start(start), count(1) {}
	};

	struct cseg_blockinfo {
		int count = 0;
		vector<cseg_ptseg> ptsegs;
	};

	int blockid;
	map<int, cseg_blockinfo> cseg_blkinfo;

	listnode_t reflist;
	int64_t datafile_index;
	double length;
	set<double> s_vals;
	union {
		odr_road* road;
		odr_lane* lane;
	};
	union {
		vector<lane_point>* pts;
		vector<refline_point>* refline_pts;
	};
	vector<lane_point>* center_pts;

private:
	int gen_approx_linear_refline_points(hdmap_blockmgr* blkmgr);
	int gen_approx_linear_lane_points(hdmap_blockmgr* blkmgr, bool outer);
	void update_blockinfo(hdmap_blockmgr*, int blkid);
	int calculate_blockid(hdmap_blockmgr* blkmgr);
};

enum {
	hdmap_container_unknown = 0,
	hdmap_container_road,
	hdmap_container_junction,
	// the connecting road inside of a junciton
	hdmap_container_connecting,
};

struct hdmap_roadinfo
{
	hdmap_roadinfo(hdmap_blockmgr* blkmgr);
	~hdmap_roadinfo();

	odr_road* get_lane_road(odr_lane*, odr_lane_section*&);
	hdmap_lane* get_create_lane(odr_lane*, bool);
	curve_segment_ref* append_boundary(hdmap_boundary&,
		odr_lane*, double, bool, curve_segment*, curve_segment* = nullptr);

	hdmap_lane* get_lane(int id, bool last);

	double get_refline_length(void);

	refline_fragment* get_refline(void);
	void finalize_reference_line(double eps);

	int get_lanecount(void) {
		int ret = 0;
		for (auto i = lanes.next; i != &lanes; i = i->next, ++ret);
		return ret;
	}

	hdmap_blockmgr* blockmgr;
	hdmap_container* container;
	// reference lane
	vector<refline_fragment*> reference_line;

	// all lanes belongs to the road
	listnode_t lanes;

	// all boundary belongs to the road
	listnode_t boundaries;
};

struct hdmap_container
{
	hdmap_container(int type)
	: attrs(0), id(0), roadinfo(nullptr) {
		a.type = type;
		listnode_init(roadseg_list);
	}
	virtual ~hdmap_container();

	odr_road* get_first(void);
	odr_road* get_last(void);

	listnode_t ownerlist;
	union {
		uint32_t attrs;
		struct {
			uint32_t type : 4;
		} a;
	};
	string name;
	size_t id;
	listnode_t roadseg_list;
	hdmap_roadinfo* roadinfo;
};

struct refline_fragment
{
	refline_fragment(odr_road*, double, hdmap_blockmgr*, double);
	double length(void);
	void finalize(hdmap_blockmgr*, double);

	union {
		uint32_t attrs;
		struct {
			uint32_t inverse : 1;
		} a;
	};
	odr_road* road;
	double s0;
	curve_segment refline_seg;
};

struct hdmap_road : public hdmap_container
{
	hdmap_road(uint32_t id);
	~hdmap_road();

	hdmap_container *prev;
	hdmap_container *next;
	uint32_t road_id;
};

struct hdmap_junction_connection : public hdmap_container
{
	hdmap_junction_connection(uint32_t id, int junciton_id);

	odr_junction_connection* odr_conn;
	int junction_id;
	uint32_t road_id;
};

struct hdmap_junction_roadpair
{
	listnode_t ownerlist;
	odr_road* incoming_road;
	odr_road* outgoing_road;
	set<hdmap_junction_connection*> connections;
};

struct hdmap_junction : public hdmap_container
{
	hdmap_junction();
	~hdmap_junction();

	odr_junction* odr_junc;

	listnode_t road_pairs;
	listnode_t connections;
};

struct curve_segment_ref
{
	curve_segment_ref(hdmap_blockmgr*, odr_lane*, curve_segment*);
	listnode_t ownerlist;
	listnode_t cseg_ownerlist;
	curve_segment* cseg;
	odr_lane* lane;
	hdmap_laneseg* laneseg;
};

struct hdmap_boundary
{
	hdmap_boundary();
	listnode_t ownerlist;
	int id;
	static int count;
	// curve_segment list
	listnode_t seg_list;
};

struct hdmap_laneseg
{
	hdmap_laneseg(hdmap_blockmgr*, curve_segment_ref*, curve_segment_ref*);
	listnode_t ownerlist;
	uint64_t index;
	hdmap_laneseg* insider;
	hdmap_laneseg* outsider;

	curve_segment_ref* inner;
	curve_segment_ref* outer;

	static uint64_t count;
};

struct hdmap_lane
{
	hdmap_lane();

	// calculate the length of center line of a lane
	double calculate_length(hdmap_roadinfo*);

	// calculate the endpoints of a lane
	void calculate_endpoints(Vec3D& start, Vec3D& end);

	static uint32_t count;
	listnode_t ownerlist;
	
	uint32_t index;
	// the last odr_lane
	odr_lane* last;

	set<hdmap_lane*> prevs;
	set<hdmap_lane*> nexts;
	hdmap_boundary inner;
	hdmap_boundary outer;

	// laneseg
	listnode_t lanesegs;
};

struct hdmap_renderblock
{
	set<uint32_t> shared_blocks;
	vector<odr_lane_section*> lanesecs;
	vector<odr_object*> objects;
	vector<odr_junction*> junctions;
};

struct area_node
{
	area_node() : id(-1), s(-1.) {}
	area_node(int id, double s)
		: id(id), s(s) {}
	int id;
	double s;
	bool operator<(const area_node& other) const {
		return (s < other.s) ? true : false;
	}
};

struct hdmap_blockinfo
{
	size_t lane_curveseg_count = 0;
	size_t refline_curveseg_count = 0;
	size_t reflseg_total_datasz = 0;
	size_t laneseg_total_datasz = 0;
	size_t laneseg_id_count = 0;

	size_t refline_point_count = 0;
	size_t lane_point_count = 0;

	hdmap_memory* m = nullptr;

	size_t get_size(void);
};

struct hdmap_blockmgr
{
	~hdmap_blockmgr();
	hdmap_memory* master_memory = nullptr;
	file_memory* last_memory = nullptr;

	double x0 = 0., y0 = 0.;
	double map_width = 0., map_height = 0.;
	double block_width = 0., block_height = 0.;
	int cols = 1;
	map<int, hdmap_blockinfo> blkinfo_map;

	int cached_blkid = -1;
	hdmap_blockinfo* cached_blkinfo = nullptr;

	bool is_multiple_blocks(void) {
		return (map_width == block_width && map_height == block_height)
		? false : true;
	}
};

struct file_memory
{
	file_memory() : _rdmap_bufnode_id_tree(nullptr)
	, _rdmap_bufnode_addr_tree(nullptr)
	, _databuf_sz(0) ,_fixlist(nullptr) {}
	~file_memory();
	void reset(void);

	void* find_addr_byid(size_t id);
	size_t find_id_byaddr(void* addr);

	void set_fixlist(uint32_t* fixlist) {
		_fixlist = fixlist;
	}

	template <typename T> T* get_first(void)
	{
		auto* node = avl_first(_rdmap_bufnode_id_tree);
		auto* tmp = list_entry(rdmap_bufnode, id_avlnode, node);
		return (T*)tmp->addr_start;
	}

	size_t total_size(void) {
		return _databuf_sz;
	}

	int write_file(FILE *fp)
	{
		auto* node = avl_first(_rdmap_bufnode_id_tree);
		while (node) {
			auto* bufobj = list_entry(rdmap_bufnode, id_avlnode, node);
			size_t sz = bufobj->addr_end - bufobj->addr_start;
			if (sz != fwrite((void*)bufobj->addr_start, 1, sz, fp)) {
				return -1;
			}
			node = avl_next(node);
		}
		return 0;
	}

	template <typename T> int write_data(T* maphdr, FILE *fp)
	{
		auto* node = avl_first(_rdmap_bufnode_id_tree);
		auto* tmp = list_entry(rdmap_bufnode, id_avlnode, node);
		assert((tmp->addr_start == (size_t)maphdr) &&
			(tmp->addr_end - tmp->addr_start) ==  (sizeof(T)
				+ ((strlen(maphdr->proj) + RMAP_ALIGN_BYTES) & ~(RMAP_ALIGN_BYTES - 1))));
		maphdr->size = _databuf_sz;
		return write_file(fp);
	}

	template <typename T> void* allocate(uint32_t size, chunk<T>& chk)
	{
		if (!size) return nullptr;

		auto* node = new rdmap_bufnode();
		if (nullptr == node) exit(118);

		node->id = _databuf_sz;
		node->buf = malloc(size);
		node->addr_start = (size_t)node->buf;
		if (!node->addr_start) exit(119);
		node->addr_end = node->addr_start + size;

		int avl = avl_insert(&_rdmap_bufnode_id_tree,
			&node->id_avlnode, rdmap_bufnode::id_compare);
		assert(avl == 0);

		avl = avl_insert(&_rdmap_bufnode_addr_tree,
			&node->addr_avlnode, rdmap_bufnode::addr_compare);
		assert(avl == 0);

		chk.size = size;
		chk.off.offset = _databuf_sz;
		chk.off.link = *_fixlist;
		*_fixlist = find_id_byaddr(&chk.off);
		_databuf_sz += size;
		return (void*)node->addr_start;
	}

	template <typename T> T** allocate(int cnt, idxtbl_v1<T>& ret)
	{
		if (!cnt) {
			ret.count = 0;
			ret.indices_off.link = 0;
			ret.indices_off.offset = 0;
			return nullptr;
		}

		ret.count = cnt;
		size_t pos = _databuf_sz;
		ret.indices_off.offset = pos;
		ret.indices_off.link = *_fixlist;
		*_fixlist = find_id_byaddr(&ret.indices_off);

		auto* node = new rdmap_bufnode();
		if (nullptr == node) exit(117);
		auto buf = (size_t) new(_databuf_sz, &node->buf) offset[cnt];

		node->id = pos;
		node->addr_start = buf;
		node->addr_end = buf + _databuf_sz - pos;
		int avl = avl_insert(&_rdmap_bufnode_id_tree,
			&node->id_avlnode, rdmap_bufnode::id_compare);
		assert(avl == 0);

		avl = avl_insert(&_rdmap_bufnode_addr_tree,
			&node->addr_avlnode, rdmap_bufnode::addr_compare);
		assert(avl == 0);

		// add all index node to the fix list
		auto *offs = (offset*)buf;
		for (int i = 0; i < cnt; ++i) {
			offs[i].link = *_fixlist;
			*_fixlist = find_id_byaddr(&offs[i]);
		}
		return (T**)buf;
	}

	template <typename T> T* allocate(size_t* p = nullptr, size_t extsz = 0)
	{
		size_t pos = _databuf_sz;
		auto* node = new rdmap_bufnode();
		if (nullptr == node) exit(116);

		T* ret = new(extsz, _databuf_sz, &node->buf) T();
		if (p) *p = pos;
		
		node->id = pos;
		node->addr_start = (size_t)ret;
		node->addr_end = (size_t)ret + _databuf_sz - pos;
		int avl = avl_insert(&_rdmap_bufnode_id_tree,
			&node->id_avlnode, rdmap_bufnode::id_compare);
		assert(avl == 0);

		avl = avl_insert(&_rdmap_bufnode_addr_tree,
			&node->addr_avlnode, rdmap_bufnode::addr_compare);
		assert(avl == 0);

		return ret;
	}

	void register_fixlist(offset& off, uint32_t dataoffset)
	{
		off.offset = dataoffset;
		off.link = *_fixlist;
		*_fixlist = find_id_byaddr(&off);
	}

private:
	struct rdmap_bufnode
	{
		avl_node_t id_avlnode;
		avl_node_t addr_avlnode;
		size_t id;
		void* buf;
		size_t addr_start;
		size_t addr_end;

		static int id_compare(avl_node_t*, avl_node_t*);
		static int addr_compare(avl_node_t*, avl_node_t*);
	};

	size_t _databuf_sz;
	uint32_t* _fixlist;
	avl_node_t* _rdmap_bufnode_id_tree;
	avl_node_t* _rdmap_bufnode_addr_tree;
};

struct hdmap_memory
{
	hdmap_memory(file_memory* memory);
	file_memory* m;
	hdmap_file_roadseg_v1* roadsegs;
	hdmap_file_lane_v1* lanes;
	hdmap_file_laneseg_v1* lanesegs;
	hdmap_datafile_lane_cseg_v1* lane_csegs;
	size_t lane_csegs_curpos;
	hdmap_lane_boundary_point_v1* lane_pts;
	size_t lane_pts_curpos;
	uint64_t* laneseg_ids;
	size_t laneseg_ids_curpos;
};

struct odr_map_intersection_point_distance
{
	lane_point start_pt;
	lane_point end_pt;
	double distance;
};

struct odr_map_lane_intersection
{
	std::vector<std::shared_ptr<lane_point>> intersection_pts;
	std::map<std::shared_ptr<lane_point>, double> intersection_length;
	std::map<std::shared_ptr<lane_point>, int> intersection_lanes;
};

struct odr_map_lanesect_transition
{
	uint64_t lanesec_index;
	uint64_t road_id;
	double length;
	odr_lane* lane;
	hdmap_roadinfo* rdinfo;
	std::set<uint64_t> lsc_prev_index;
	std::set<uint64_t> lsc_next_index;
	std::map<uint64_t, uint64_t> lsc_left_index;
	std::map<uint64_t, uint64_t> lsc_right_index;
	std::vector<lane_point>* center_pts;
	odr_map_lane_intersection intersect_lane;
};

class odr_map
{
	friend class odr_extdata;
public:
	odr_map(cmdline_parser& psr);
	~odr_map();

	int add_road(odr_road*);
	bool road_exists_and_merged(odr_road*);
	int add_junction(odr_junction*);
	bool junction_exists_and_merged(odr_junction*);
	void junction_merge_lanelinks(odr_junction_connection*,
		odr_junction_connection*);

	int generate(void);

	// helper
	void update_xy_offset(refl_geometry* geo);
	void set_road_object(odr_object* obj);

public:
	cmdline_parser& get_parser(void) {
		return _psr;
	}

	void set_vendor(const char* v) {
		_vendor = v;
	}

	void set_georef(const char* s);
	const string& get_vendor(void) {
		return _vendor;
	}

	odr_object* get_road_object(size_t id) {
		auto it = _odr_objectmap.find(id);
		if (it == _odr_objectmap.end()) {
			return nullptr;
		}
		return it->second;
	}

	uint32_t get_road_object_index(size_t id) {
		auto it = _odr_objectmap.find(id);
		if (it == _odr_objectmap.end()) {
			return 0;
		}
		return distance(_odr_objectmap.begin(), it) + 1;
	}
	
	void set_mapoffset(double dx, double dy);

	point3d get_mapoffset(void) {
		return {_xoffset, _yoffset};
	}

	const proj& get_georef(void) {
		return _wgs84prj;
	}

	size_t get_last_objectid(void) {
		return _last_objectid;
	}

	odr_road* getroad_byid(uint32_t id);
private:
	file_memory& hfm(void) {
		return *_hfm.m;
	}

private:
	void release_all(void);
	odr_loader* getloader(void);
	int generate_hdmap(void);
	int write_hdmap(void);
	int write_roads(void);
	int write_multiple_data(void);
	int write_road_lane_info(hdmap_file_roadseg_v1&, hdmap_roadinfo*);
	int write_road_refline_info(hdmap_file_roadseg_v1&, hdmap_roadinfo*);
	int write_road_laneseg_info(hdmap_lane*, hdmap_file_lane_v1*);
	int write_road_laneseg_data(hdmap_laneseg*, hdmap_file_laneseg_v1*);
	int write_new_curve_segment(curve_segment_ref*, chunk<hdmap_lane_boundary_point_v1>&);
	int write_new_curve_center(curve_segment_ref*, chunk<hdmap_lane_boundary_point_v1>&);

	int write_new_refline_segment(curve_segment*, chunk<hdmap_refline_point_v1>&);

	int write_laneseg_kdtree(void);
	int write_boundary_points_kdtree(void);

	void init_hdmap_datafile(hdmap_memory* m);

	void allocate_roads(void);
	void allocate_block_memory(void);

	void hdmap_prepare(void);
	int generate_roads(void);
	int generate_transition(void);
	odr_lane* find_odr_lane_by_id(odr_lane_section* odr_ls, int id);
	int add_road_lanesect_transition(odr_lane_section* lss, int lane_id, std::set<int> &lstset);
	int add_lanesect_transition_by_lanelink(odr_road* r, odr_lane* cur_lane, std::set<int> &lstset, bool bnext);
	int write_lanesect_transition(void);
	int generate_lanesect_transition(void);
	int generate_bounding_box(bounding_box &box, std::vector<lane_point>* center_pts);
	int calc_min_intersection_bounding_box(bounding_box box1, bounding_box box2, int32_t index1, int32_t index2);
	int check_intersection_bounding_box(bounding_box box1, bounding_box box2);
	int split_bounding_box(bounding_box box, std::vector<lane_point>* center_pts, bounding_box &box1, bounding_box &box2);
	int point_in_bounding_box(bounding_box box, lane_point p);
	int cut_bounding_box(bounding_box &box, std::vector<lane_point>* center_pts);
	int foreach_lane_connect_junctions(void);
	int calc_intersection_lane(odr_road* road1, odr_road* road2);
	int get_lanes_from_road(odr_road* road, std::vector<odr_lane*> &lanes);

	int generate_junctions(void);
	hdmap_container* get_container(odr_element*);

	int create_odr_road_container(odr_road*);
	int create_odr_junction_container(odr_junction*);
	int verify_road_container(void);

	odr_junction* getjunction_byid(uint32_t id);
	odr_junction* getjunction_byapproache(string uuid);
	odr_element* get_roadlink_object(roadlink* l);

	// road methods
	int road_similar_check(odr_road*, odr_road*);
	int merge_similar_road(odr_road*, odr_road*);

	int walk_roads(odr_road*, hdmap_container*);
	int generate_roadinfo(hdmap_container*);
	int generate_roadseg_lanesecs(hdmap_roadinfo*, odr_road*);
	int generate_roadseg_lanesec(hdmap_roadinfo*, odr_lane_section*, bool);
	bool lane_need_handle(odr_lane*);
	curve_segment* generate_roadseg_lanesec_lane(hdmap_roadinfo*,
		odr_lane_section*, odr_lane*, bool,
		curve_segment*, hdmap_laneseg*&);

	// check if the two element can share the same
	// container. the caller must make sure the
	// odr-element shall be odr-road or odr-junction 
	bool can_share_container(odr_element*, odr_element*);

	// junction methods
	int hdmap_junction_add_connection(
		hdmap_junction*, odr_junction_connection*);
	int hdmap_junction_link_incoming(odr_road*, odr_road*, odr_junction_connection*);
	int hdmap_junction_link_outgoing(odr_road*, odr_road*);
	odr_junction_connection* junction_find_connection(odr_junction*,
		uint32_t, uint32_t, int);

	odr_road* get_outgoing_road(odr_road*, hdmap_junction_connection*);
	hdmap_junction_roadpair* check_create_road_pair(
		hdmap_junction*, odr_road*, odr_road*,
		hdmap_junction_connection*);
	int create_junction_connection_reference_line(
		hdmap_junction*, hdmap_junction_connection*, odr_road*);

	// common methods
	uint64_t allocate_id(void);
	void update_bounding_box(double x, double y);
	void update_bounding_box(odr_road*, odr_lane*, odr_lane_section*);

private:
	// these methods are for render map
	int generate_rendermap(void);
	void rendermap_release_all(void);
	int write_rendermap(void);
	int generate_render_junctions(odr_junction* junc);
	int generate_render_lanesections(odr_road* r, int& count);
	int generate_render_roadobject(odr_road* r);

	// generate meshs for all objects
	int generate_mesh(void);

	int build_blocks(void);
	int find_obj_owner_block(odr_object*, int, int, int, int, set<uint32_t>&);
	int find_lane_owner_blocks(odr_lane*, int, int, int, int, set<uint32_t>&);
	bool is_intersect_with_obj(vector<point>&, odr_object*);
	bool is_intersect_with_lane(vector<point>&, odr_lane*);
	int get_object_blockid(map<int, double>& blkmap);
	void roadobject_addto_blocks(odr_object*, set<uint32_t>&);
	void lanesection_addto_blocks(odr_lane_section*, set<uint32_t>&);

	int write_rendermap_header(void);
	int write_lane_sections(void);
	int write_junctions(void);
	int write_junction_table(void);
	int write_roadseg(void);
	int write_blocks(void);

	void create_rendermap_road(odr_road* r);
	void create_rendermap_lane_section(odr_lane_section*, int, set<uint32_t>&);
	void create_rendermap_road_object(odr_object*, odr_road*, set<uint32_t>&);
	void init_rendermap_road_object(odr_object*, odr_road*, set<uint32_t>&);

	int serialized_datasize(odr_object*, odr_road*, set<uint32_t>&);

	// different kinds of road objects
	void init_road_object_signal_light(odr_object*, odr_road*,
		hdrmap_file_road_object_v1*,
		exd_siglight_info*, set<uint32_t>&);
	void init_road_object_rsd(odr_object*, odr_road*,
		hdrmap_file_road_object_v1*,
		exd_roadside_device_info*, set<uint32_t>&);
	void init_road_object_sigboard(odr_object*, odr_road*,
		hdrmap_file_road_object_v1*,
		exd_sigboard_info*, set<uint32_t>&);
	void init_road_object_arrow(odr_object*, odr_road*,
		hdrmap_file_road_object_v1*,
		exd_arrow_info*, set<uint32_t>&);

	// serialized_datasize special case handling
	void update_siglight_object(odr_object*, odr_road*);
	void update_sigboard_pole_object(odr_object*, odr_road*, set<uint32_t>&);

	int64_t find_block_by_pos(const point3d& pt);
	void update_rendermap_junction(hdrmap_file_junction_v1*, odr_junction*);

private:
	cmdline_parser& _psr;
	uint64_t _idcount;
	string _vendor;

	// map parameters
	string _georef;
	proj _wgs84prj;
	double _xoffset, _yoffset;

	// roadobject set
	map<size_t, odr_object*> _odr_objectmap;
	size_t _last_objectid;

	// hdmap range
	// (x0, y0) - (x1, y1)
	double _x0, _y0, _x1, _y1;

	// hdmap key elements
	listnode_t _hdm_roads;
	listnode_t _hdm_juncs;
	int _hdm_road_count;
	int _hdm_junc_count;

	// key odr object lists
	listnode_t _odr_roads;
	avl_node_t* _odr_road_idtree;

	listnode_t _odr_junctions;
	avl_node_t* _odr_junction_idtree;

	// OpenDrive
	double _x_offs;
	double _y_offs;
	size_t _geometry_count;

	// hdmap
	hdmap_fileheader_v1* _hdmap;
	hdmap_memory _hfm;
	hdmap_blockmgr _hdm_blkmgr;

	// render map
	int _rows, _cols;
	hdmap_renderblock* _rblks;
	hdrmap_file_rendermap_v1* _rendermap;
	FILE* _rendermap_fp;
	file_memory _rfm;
	std::map<int, std::shared_ptr<odr_map_lanesect_transition>> _lsc_transitions;
	std::map<odr_lane*, int> _lane_index;
};

} // end of namespace osm_parser1
#endif // __CXX_OSM_PARSER1_ODR_MAP_H__
/* EOF */
