// hdmap-render.h
// this is to define the binary format of the hdmap for rendering purpose
// the format is used to make the download data for rendering via http(s)

#ifndef __CXX_ZAS_MAPCORE_HDMAP_RENDER_H__
#define __CXX_ZAS_MAPCORE_HDMAP_RENDER_H__

#include <set>
#include "mapcore/mapcore.h"

namespace zas {
namespace mapcore {
namespace render_hdmap {

// hdmap downloading format for rendering purpose
// version 1.0.0.0
using namespace std;

struct offset
{
	uint32_t link;
	uint32_t offset;
} PACKED;

struct referring_id
{
	uint32_t link;
	uint32_t id;
} PACKED;

template <typename T> union pointer
{
	offset off;
	T* ptr;
} PACKED;

template <typename T> struct chunk
{
	uint32_t size;
	union {
		offset off;
		T* ptr;
	};
} PACKED;

template <typename T>
struct idxtbl_v1
{
	struct elements {
		T** objects;
		T* operator[](int idx) {
			return (T*)(size_t)((uint64_t*)objects)[idx];
		}
	} PACKED;
	idxtbl_v1() : count(0), placeholder(0) {}
	uint32_t count;
	union {
		offset indices_off;
		// make it work in 32-bit system
		uint64_t placeholder;
		elements indices;
	};
} PACKED;

enum object_type
{
	objtype_unknown = 0,
	objtype_reference_line,
	objtype_lane_section,
	objtype_super_elevation,
	objtype_road_object						= hdrm_robjtype_unclassfied,
	objtype_road_object_signal_light		= hdrm_robjtype_signal_light,
	objtype_road_object_rsd					= hdrm_robjtype_rsd,
	objtype_road_object_arrow				= hdrm_robjtype_arrow,
	objtype_road_object_sigboard			= hdrm_robjtype_sigboard,
	objtype_road_object_cared				= hdrm_robjtype_classified,
	objtype_road_object_junconver           = hdrm_robjtype_junconver,
};

template <typename T>
struct serialized_object_v1
{
	serialized_object_v1()
	: type(T::object_type)
	, extinfo_size(sizeof(T)) {
	}

	// indicate the object type
	int32_t type;

	// indicate the serialized data size
	// of the specified object in bytes
	uint32_t size_inbytes;

	// indicate how many bytes here-after have
	// it been resered for extra info
	uint32_t extinfo_size;

	// the extra info (could be empty
	// when the extinfo_size = 0)
	T extinfo;

	// the serialized data
	uint8_t sdata[0];
} PACKED;

struct lane_section_info_v1
{
	static const int32_t object_type = objtype_lane_section;

	// lane section unique id
	uint32_t id;

	// roadseg unique id
	uint32_t roadid;

	// this leaves 0
	// indicate which block generate this
	// lane section to 3D mesh
	int64_t meshgen_blockid;

	// this leaves nullptr
	// used by tile_cache
	union {
		set<uint32_t>* referred_blkids;
		uint64_t placeholder;
	};
} PACKED;

struct reference_line_info_v1
{
	static const int32_t object_type = objtype_reference_line;
	// reference line id
	uint32_t id;
} PACKED;

struct super_elevation_info_v1
{
	static const int32_t object_type = objtype_super_elevation;
	// super elevation line id
	uint32_t id;
} PACKED;

struct siglight_info_v1
{
	// the unique ID (independent to map) of
	// the signal light
	uint64_t siguid;

	// base position
	point3d base_xyz;

	// transfer matrix
	double matrix[9];
} PACKED;

struct roadside_device_info_v1
{
	// the spot ID of the road side device
	uint64_t spotid;

	// road side device type
	uint32_t devtype;

	// the junction ID, if the road side
	// device is within the junction scope
	uint32_t junctionid;

	// base position
	point3d base_xyz;

	// transfer matrix
	double matrix[9];
} PACKED;

struct arrow_info_v1
{
	// the unique ID of the arrow
	uint64_t arrow_uid;

	uint16_t type;
	uint16_t color;
} PACKED;

struct sigboard_info_v1
{
	// the unique ID of the signal board
	uint64_t sigboard_uid;

	// the width and length of the signal board
	double width, length;

	union {
		uint8_t attrs;
		struct {
			uint8_t shape_type : 4;
			uint8_t kind : 4;
		} a;
	};
	union {
		uint32_t type;
		struct {
			uint32_t major : 16;
			uint32_t sub : 16;
		} t;
	};
} PACKED;

struct road_object_info_v1
{
	static const int32_t object_type = objtype_road_object;

	uint32_t id;
	// road object unique id
	union {
		
		pointer<siglight_info_v1> siginfo;
		pointer<roadside_device_info_v1> rsdinfo;
		pointer<arrow_info_v1> arrowinfo;
		pointer<sigboard_info_v1> sigboardinfo;
	};

	// roadseg unique id
	uint32_t roadid;

	// this leaves 0
	// indicate which block generate this
	// lane section to 3D mesh
	int64_t meshgen_blockid;

	// this leaves nullptr
	// used by tile_cache
	union {
		set<uint32_t>* referred_blkids;
		uint64_t placeholder;
	};
} PACKED;

using lane_section_v1 = serialized_object_v1<lane_section_info_v1>;
using reference_line_v1 = serialized_object_v1<reference_line_info_v1>;
using super_elevation_v1 = serialized_object_v1<super_elevation_info_v1>;
using road_object_v1 = serialized_object_v1<road_object_info_v1>;

struct roadseg_info_v1
{
	// the unique id of the
	// road segment
	uint32_t id;
	double length;
};

struct roadseg_v1
{
	roadseg_info_v1 extinfo;

	// reference line
	pointer<reference_line_v1> refline;

	// super elevation line
	pointer<super_elevation_v1> superelev;
} PACKED;

struct approach_info_v1
{
	uint32_t uid_sz;
	uint8_t data[0];
} PACKED;

struct junction_info_v1
{
	// junction unique id
	uint32_t id;
} PACKED;

struct junction_v1
{
	junction_info_v1 extinfo;

	// center point of the junction
	double center_x;
	double center_y;

	// radius of the junction
	double radius;

	idxtbl_v1<approach_info_v1> approachs;
} PACKED;

struct blockinfo_v1
{
	// the row and colume of the block
	// in the rendering map
	int32_t row, col;

	// all the road segments referred
	// by this block
	idxtbl_v1<roadseg_v1> road_segs;
	uint32_t rid_list;

	// lane section table
	idxtbl_v1<lane_section_v1> lane_secs;
	uint32_t lsid_list;

	// road object table
	idxtbl_v1<road_object_v1> road_objs;
	uint32_t objid_list;

	// junction table
	idxtbl_v1<junction_v1> junctions;
} PACKED;

struct rendermap_v1
{
	// must be HDM_RENDER_MAGIC
	char magic[8];

	// the version of the format
	uint32_t version;

	union {
		uint32_t attrs;
		struct {
			// if this is a map file (used for server)
			uint32_t persistence : 1;

			// it this is an empty map object
			// means only containing map info
			uint32_t mapinfo_only: 1;
		} a;
	};

	// the unique id of the original map file
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

	// all objects index table
	idxtbl_v1<blockinfo_v1> blocks;
	idxtbl_v1<roadseg_v1> roadsegs;
	idxtbl_v1<junction_v1> junctions;
	idxtbl_v1<lane_section_v1> lanesecs;
	idxtbl_v1<road_object_v1> roadobjs;
	
	// projection
	char proj[0];
} PACKED;

#define HDM_RENDER_MAGIC	"rendrmap"
#define HDM_RENDER_VERSION	(0x10000000)

}}} // end of namespace zas::mapcore::render_hdmap
#endif // end of __CXX_ZAS_MAPCORE_HDMAP_RENDER_H__
/* EOF */
