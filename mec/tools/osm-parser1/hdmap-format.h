#ifndef __CXX_OSM_PARSER1_HDMAP_FORMAT_H__
#define __CXX_OSM_PARSER1_HDMAP_FORMAT_H__

#include <math.h>
#include "mapcore/hdmap-render.h"

namespace osm_parser1 {

using namespace zas::mapcore;
using namespace zas::mapcore::render_hdmap;

class file_memory;

#define RMAP_ALIGN_BYTES	(8)

struct robj_internal_info_v1
{
	static const int32_t object_type = objtype_road_object;

	// road object unique id
	uint32_t id;

	// roadseg unique id
	uint32_t roadid;

	// this leaves 0
	// indicate which block generate this
	// lane section to 3D mesh
	int64_t meshgen_blockid;

	// this leaves nullptr
	// used by tile_cache
	set<uint32_t>* referred_blkids;
} PACKED;

using robj_internal_v1 = serialized_object_v1<robj_internal_info_v1>;

// start of definition of render map file format

struct hdrmap_file_lanesec_info_v1
{
	static const int32_t object_type = objtype_lane_section;

	// lane section unique id
	uint32_t id;

	// roadseg unique id
	uint32_t roadid;

	// all the blocks refers to this lane section
	chunk<uint32_t> referred_blockids;
} PACKED;

struct hdrmap_file_refline_info_v1
{
	static const int32_t object_type = objtype_reference_line;

	// reference line id
	uint32_t id;
} PACKED;


struct hdrmap_file_superelev_info_v1
{
	static const int32_t object_type = objtype_super_elevation;

	// super elevation line id
	uint32_t id;
} PACKED;

struct hdrmap_file_road_object_info_v1
{
	static const int32_t object_type = objtype_road_object;
	uint32_t id;

	union {
		// road object unique id
		uint64_t roid;
		pointer<siglight_info_v1> siginfo;
		pointer<roadside_device_info_v1> rsdinfo;
		pointer<arrow_info_v1> arrowinfo;
		pointer<sigboard_info_v1> sigboardinfo;
	};

	// roadseg unique id
	uint32_t roadid;

	// all the blocks refers to this lane section
	chunk<uint32_t> referred_blockids;
} PACKED;

using hdrmap_file_lanesec_v1 = serialized_object_v1<hdrmap_file_lanesec_info_v1>;
using hdrmap_file_refline_v1 = serialized_object_v1<hdrmap_file_refline_info_v1>;
using hdrmap_file_superelev_v1 = serialized_object_v1<hdrmap_file_superelev_info_v1>;
using hdrmap_file_road_object_v1 = serialized_object_v1<hdrmap_file_road_object_info_v1>;

struct hdrmap_file_roadseg_info_v1
{
	// road segment unique id
	uint32_t id;
	double length;
} PACKED;

struct hdrmap_file_roadseg_v1
{
	hdrmap_file_roadseg_info_v1 extinfo;

	// reference line
	pointer<hdrmap_file_refline_v1>	refline;

	// super elevation line
	pointer<hdrmap_file_superelev_v1> superelev;

	// index table of all lane sections
	idxtbl_v1<hdrmap_file_lanesec_v1> lane_secs;

	// road object table
	idxtbl_v1<hdrmap_file_road_object_v1> road_objs;
} PACKED;

struct hdrmap_file_approach_info_v1
{
	uint32_t uid_sz;
	uint8_t data[0];
} PACKED;

struct hdrmap_file_junction_info_v1
{
	// junction unique id
	uint32_t id;
} PACKED;

struct hdrmap_file_junction_v1
{
	hdrmap_file_junction_info_v1 extinfo;

	// center point of the junction
	double center_x;
	double center_y;

	// radius of the junction
	double radius;

	idxtbl_v1<hdrmap_file_approach_info_v1> approachs;

	// kd-tree
	struct {
		uint32_t split : 1;
		uint32_t KNN_selected : 1;
	} attrs;

	// leave it as nullptr
	// the kdtree will be built when
	// loading the render map
	union {
		hdrmap_file_junction_v1 *left;
		uint64_t placeholder1;
	};
	union {
		hdrmap_file_junction_v1 *right;
		uint64_t placeholder2;
	};

	double x(void) {
		return center_x;
	}
	double y(void) {
		return center_y;
	}
	int get_split(void) {
		return attrs.split;
	}
	void set_split(int s) {
		attrs.split = (s) ? 1 : 0;
	}
	bool KNN_selected(void) {
		return (attrs.KNN_selected) ? true : false;
	}
	void KNN_select(void) {
		attrs.KNN_selected = 1;
	}
	double distance(double x, double y) {
		auto dx = (center_x - x);
		auto dy = (center_y - y);
		return sqrt(dx * dx + dy * dy);
	}
	void set_left(hdrmap_file_junction_v1* l) {
		left = l;
	}
	void set_right(hdrmap_file_junction_v1* r) {
		right = r;
	}
	hdrmap_file_junction_v1* get_left(void) {
		return left;
	}
	hdrmap_file_junction_v1* get_right(void) {
		return right;
	}
	static int kdtree_x_compare(const void* a, const void* b);
	static int kdtree_y_compare(const void* a, const void* b);
} PACKED;

struct hdrmap_file_blockinfo_v1
{
	// the row and colume of the block
	// in the rendering map
	int32_t row, col;

	// all the road segments referred
	// by this block
	idxtbl_v1<hdrmap_file_roadseg_v1> road_segs;

	// lane section table
	idxtbl_v1<hdrmap_file_lanesec_v1> lane_secs;

	// road object table
	idxtbl_v1<hdrmap_file_road_object_v1> road_objs;

	// junction table
	idxtbl_v1<hdrmap_file_junction_v1> junctions;
} PACKED;

struct hdrmap_file_rendermap_v1
{
	// must be HDM_RENDER_MAGIC
	char magic[8];

	// the version of the format
	uint32_t version;

	// attributes
	union {
		uint32_t attrs;
		struct {
			// if this is a map file (used for server)
			uint32_t persistence : 1;
		} a;
	};

	// the unique id of this map file
	uint128_t uuid;

	// the size of the render map data
	uint64_t size;

	// maintain a link list for all offset
	uint32_t offset_fixlist;

	// the map offset
	double xoffset, yoffset;

	// the bounding box of the full rendering map
	double xmin, ymin, xmax, ymax;

	// the width and height for a block
	double block_width, block_height;

	// the rows and columes of blocks for
	// the full rendering map
	int32_t rows, cols;

	// leave it as nullptr
	union {
		hdrmap_file_junction_v1* junc_kdtree;
		uint64_t placeholder;
	};

	// all objects index table
	idxtbl_v1<hdrmap_file_blockinfo_v1> blocks;
	idxtbl_v1<hdrmap_file_roadseg_v1> roadsegs;
	idxtbl_v1<hdrmap_file_junction_v1> junctions;

	// projection
	char proj[0];
} PACKED;

// start of definition of full map file format
#define HDMAP_MAGIC			"hidenmap"
#define HDMAP_VERSION		0x10000000
#define HDMAP_DATA_MAGIC	"hdmpdata"
#define HDMAP_DATA_VERSION	0x10000000

typedef int32_t short_arridx_t;

struct hdmap_point_v1
{
	point3d pt;
	pointer<hdmap_point_v1> left, right;

	// attributes
	struct {
		// kd-tree
		uint32_t split : 1;
		uint32_t KNN_selected : 1;
	} attrs;

	void set_left(file_memory*, hdmap_point_v1* l);
	void set_right(file_memory*, hdmap_point_v1* r);
	hdmap_point_v1* get_left(void) {
		return left.ptr;
	}
	hdmap_point_v1* get_right(void) {
		return right.ptr;
	}
	double x(void) {
		return pt.xyz.x;
	}
	double y(void) {
		return pt.xyz.y;
	}
	int get_split(void) {
		return attrs.split;
	}
	void set_split(int s) {
		attrs.split = (s) ? 1 : 0;
	}
	bool KNN_selected(void) {
		return (attrs.KNN_selected) ? true : false;
	}
	void KNN_select(void) {
		attrs.KNN_selected = 1;
	}
	double distance(double x, double y) {
		auto dx = (pt.xyz.x - x);
		auto dy = (pt.xyz.y - y);
		return sqrt(dx * dx + dy * dy);
	}
	static int kdtree_x_compare(const void*, const void*);
	static int kdtree_y_compare(const void*, const void*);
} PACKED;

struct hdmap_refline_point_v1 : public hdmap_point_v1
{
	double s;
} PACKED;

struct hdmap_laneseg_point_v1 : public hdmap_point_v1
{
	uint64_t laneseg_index;
} PACKED;

struct hdmap_lane_boundary_point_v1 : public hdmap_point_v1
{
	uint64_t laneseg_index;
	double width;
	double s;
} PACKED;

enum hdmap_object_type
{
	hdmap_objtype_unknown = 0,
	hdmap_objtype_laneseg,
	hdmap_objtype_refline_seg,
};

struct hdmap_file_roadseg_v1;

struct hdmap_datafile_reflseg_info_v1
{
	static const int32_t object_type = hdmap_objtype_refline_seg;
	double s0;
	struct {
		uint32_t inverse : 1;
	} attrs;
	chunk<hdmap_refline_point_v1> pts;
	pointer<hdmap_file_roadseg_v1> roadseg;
} PACKED;

struct hdmap_datafile_laneseg_info_v1
{
	static const int32_t object_type = hdmap_objtype_laneseg;
	chunk<hdmap_lane_boundary_point_v1> inner_pts;
	chunk<hdmap_lane_boundary_point_v1> center_pts;
	chunk<hdmap_lane_boundary_point_v1> outer_pts;
} PACKED;

using hdmap_datafile_laneseg_data_v1 = serialized_object_v1<hdmap_datafile_laneseg_info_v1>;
using hdmap_datafile_reflseg_data_v1 = serialized_object_v1<hdmap_datafile_reflseg_info_v1>;

// this refl_cseg is 1:1 to refline points
struct hdmap_datafile_refl_cseg_v1
{
	uint64_t roadseg_refline_index : 16;
	uint64_t roadseg_id : 48;
	chunk<hdmap_refline_point_v1> pts;
} PACKED;

// this lane_cseg is 1:1 to boundary points
struct hdmap_datafile_lane_cseg_v1
{
	uint32_t laneseg_idcount;
	union {
		uint64_t laneseg_id;
		pointer<uint64_t> laneseg_ids;
	};
	chunk<hdmap_lane_boundary_point_v1> pts;
} PACKED;

// belows objects are stored in master data file
struct hdmap_file_lane_v1;

struct hdmap_file_laneseg_v1
{
	// parallel lanes
	pointer<hdmap_file_laneseg_v1> insider;
	pointer<hdmap_file_laneseg_v1> outsider;

	// the starting point and ending point
	// the point in on the center line of lane
	hdmap_laneseg_point_v1 start;
	hdmap_laneseg_point_v1 end;

	// reference to the lane
	pointer<hdmap_file_lane_v1> lane;

	// reference to the reference line

	// the lane segment data
	pointer<hdmap_datafile_laneseg_data_v1> data;
} PACKED;

struct hdmap_file_lane_v1
{
	// multiple predecessors and successors
	idxtbl_v1<hdmap_file_lane_v1> predecessors;
	idxtbl_v1<hdmap_file_lane_v1> successors;

	// the center line length of the lane
	double length;

	int id;

	// the reference to roadseg
	pointer<hdmap_file_roadseg_v1> roadseg;

	// lane segments of this lane
	chunk<hdmap_file_laneseg_v1> lanesegs;
} PACKED;

struct hdmap_file_roadseg_v1
{
	// the index of the junction, if the lane
	// is a connecting road in a junction, or
	// -1 if it is a pure road
	short_arridx_t junction_id;

	uint32_t rd_id;

	uint64_t road_id;

	// the s length
	double s_length;

	// all reference line segments
	idxtbl_v1<hdmap_datafile_reflseg_data_v1> reflines;

	// all lanes info
	chunk<pointer<hdmap_file_lane_v1>> lanes;
} PACKED;

struct hdmap_fileheader_v1
{
	// must be HDMAP_MAGIC
	char magic[8];

	// the version of the format
	uint32_t version;

	// attributes
	union {
		uint32_t attrs;
		struct {
			// if this is a hdmap file (used for server)
			uint32_t persistence : 1;

			// having geometries encoded in the map
			uint32_t geometries_avail : 1;

			// having calculated points encoded in the map
			uint32_t points_avail : 1;

			// indicating if we'll save the data to separate file
			uint32_t separate_datafile : 1;
		} a;
	};

	// the unique id of this map file
	uint128_t uuid;

	// the size of the hdmap data
	uint64_t size;

	// maintain a link list for all offset
	uint32_t offset_fixlist;

	// the map offset
	double xoffset, yoffset;

	// the bounding box of the full hdmap
	double xmin, ymin, xmax, ymax;

	// the width and height for a block
	double block_width, block_height;

	// the rows and columes of blocks for
	// the full hdmap
	int32_t rows, cols;

	// kd-tree for all end-points
	pointer<hdmap_laneseg_point_v1> endpoints_kdtree;
	
	// kd-tree for all boundary points
	// this field will only be valid when separate_datafile = false
	pointer<hdmap_lane_boundary_point_v1> boundary_points_kdtree;

	uint32_t header_sz;
	// the counts
	uint32_t roadseg_count;
	uint32_t junction_count;
	uint64_t lane_count;
	uint64_t laneseg_count;

	// projection
	char proj[0];
} PACKED;

struct hdmap_datafile_header_v1
{
	// must be HDMAP_DATA_MAGIC
	char magic[8];

	// the version of the format
	uint32_t version;

	// the unique id of the master map file
	uint128_t uuid;

	// the size of the render map data
	uint64_t size;

	// maintain a link list for all offset
	uint32_t offset_fixlist;

	// how many blocks are there in this data file
	uint32_t block_count;
} PACKED;

} // end of namespace osm_parser1
#endif // __CXX_OSM_PARSER1_SDMAP_FORMAT_H__
/* EOF */