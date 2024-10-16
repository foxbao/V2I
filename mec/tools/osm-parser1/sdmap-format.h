#ifndef __CXX_OSM_PARSER1_SDMAP_FORMAT_H__
#define __CXX_OSM_PARSER1_SDMAP_FORMAT_H__

#include "std/zasbsc.h"

namespace osm_parser1 {

typedef size_t pos_t;
typedef size_t uid_t;

typedef union {
	size_t all;
	struct {
		uint64_t index : 40;
		uint64_t blkid : 24; 
	} u;
} arridx_t;

typedef size_t easy_arridx_t;
typedef size_t str_t;
typedef pos_t pos32_t;

struct mapfile_scope
{
	double minlat, minlon;
	double maxlat, maxlon;
} PACKED;

// start of definition of planning map file format
#define PLANNING_MAP_MAGIC		"planning-map"

struct planning_map_fileheader
{
	char magic[12];

	int level_count;
	pos_t start_of_level_table;

	uint32_t version;
} PACKED;

struct planning_map_file_level
{
	uint8_t level;// 0 ~ 5
	int row, col;
	double latstep, lonstep;
	double minlat, maxlat, minlon, maxlon;

	int block_count;
	pos_t start_of_block_table;
	int node_count;
	pos_t start_of_node_table;
	int edge_count;
	pos_t start_of_edge_table;
	int kdtree_count;
	pos_t start_of_kdtree_table;
} PACKED;

struct planning_map_file_blockinfo
{
	uid_t id;

	double minlat, minlon;
	double maxlat, maxlon;

	int kdtree_count;
	pos_t start_of_kdtree_table;

	int node_count;
	pos_t start_of_node_table;
} PACKED;

struct planning_map_file_nodedata
{
	double lat, lon;
} PACKED;

struct planning_map_file_nodeinfo
{
	int64_t uid;
	planning_map_file_nodedata data;

	int64_t blockid;

	int edge_count;
	pos_t start_of_edge_table;
	// planning_file_edge edges[0];
} PACKED;

struct planning_map_file_edge
{
	uid_t uid;

	// the first node of the link
	arridx_t start_node;
	// the second node of the link
	arridx_t end_node;

	double distance;
	double weight;
} PACKED;

struct planning_map_file_node_kdtree
{
	uid_t id;
	
	planning_map_file_nodedata data;

	int split;// 0 - lat; 1 - lon;
	arridx_t nodeid;
	// pointer to left link
	arridx_t leftnode;
	// pointer to right link
	arridx_t rightnode;
} PACKED;


// start of definition of full map file format
#define FULLMAP_MAGIC			"full-map"
#define FULLMAP_VERSION			0x10000000

struct fullmap_fileheader_v1
{
	char magic[8];
	uint32_t version;
} PACKED;

struct fullmap_file_keyinfo_v1
{
	mapfile_scope fullmap_scope;
	mapfile_scope thisfile_scope;
	int block_rows;
	int block_cols;
	int block_count;

	// the full kd-tree for all nodes
	arridx_t node_kdtree_root;
} PACKED;

// data section format
// 1) blockinfo (all)
// 2) for each block:
// 3)    node table (sorted by its uid)
// 4)    node extra info table
// 5)	 node link ref table
// 6)    link table (sorted by its uid)
// 7)    way table (sorted by its uid)
// 8)    link table for a way

// memory version
// fullmap_blockinfo_v1
struct fullmap_file_blockinfo_v1
{
	int node_count;
	int node_extdata_count;
	int link_count;
	int ref_link_count;
	int way_count;

	// the absolute position of the block data
	pos_t start_of_blockdata;
	size_t block_size;

	// the relative position against the
	// start of block data
	pos32_t start_of_node_table;
	pos32_t start_of_link_table;
	pos32_t start_of_way_table;
	pos32_t start_of_string_table;
} PACKED;

struct fullmap_file_wayinfo_v1
{
	uid_t uid;
	double distance;

	// attributes
	struct {
		uint32_t oneway : 1;
		uint32_t highway : 1;
		uint32_t bridge : 1;
		uint32_t close_loop : 1;
		uint32_t lanes : 5;
		uint32_t maxspeed : 8;
		uint32_t highway_type : 8;
	} attr;

	str_t way_name;
} PACKED;

// memory version
// fullmap_linkinfo_v1
struct fullmap_file_linkinfo_v1
{
	uid_t uid;
	double distance;

	// which way this link belongs to
	arridx_t way_id;

	// the first node of the link
	arridx_t node1_id;
	// the second node of the link
	arridx_t node2_id;

	// pointer to prev link and next link
	uid_t prev_link;
	easy_arridx_t prev_link_blockid;
	uid_t next_link;
	easy_arridx_t next_link_blockid;
} PACKED;

// memory version
// fullmap_nodeinfo_v1
struct fullmap_file_nodeinfo_v1
{
	uid_t uid;
	double lat, lon;

	// kd-tree links
	arridx_t left_id, right_id;

	// all links
	uint32_t link_count;

	// if it is a key node, the extra_info
	struct {
		uint32_t keynode : 1;
		uint32_t kdtree_dimension : 1;
	} attr;
	easy_arridx_t extra_info;

	// for every node, we create a
	// search table for all related links
	easy_arridx_t link_start;
};

// memory version
// fullmap_node_extra_info_v1
struct fullmap_file_node_extra_info_v1
{
	int intersect_name_count;

	// the first intersect name
	str_t first;
} PACKED;

} // end of namespace osm_parser1
#endif // __CXX_OSM_PARSER1_SDMAP_FORMAT_H__
/* EOF */