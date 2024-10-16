/** @file mem-format.h
 *  definition of all sdmap structures in memory
 */

#ifndef __CXX_MAPCORE_MEM_FORMAT_H__
#define __CXX_MAPCORE_MEM_FORMAT_H__

#include "mapcore/mapcore.h"
#include "sdmap-format.h"

using namespace osm_parser1;

namespace zas {
namespace mapcore {

struct fullmap_nodeinfo_v1;
struct fullmap_linkinfo_v1;
struct fullmap_wayinfo_v1;
struct fullmap_node_extra_info_v1;

// file version:
// fullmap_file_blockinfo_v1
struct fullmap_blockinfo_v1
{
	int node_count;
	int node_extdata_count;
	int link_count;
	int ref_link_count;
	int way_count;

	// the absolute position of the block data
	size_t start_of_blockdata;
	size_t block_size;

	fullmap_nodeinfo_v1* node_table;
	fullmap_linkinfo_v1* link_table;
	fullmap_wayinfo_v1* way_table;
	const char* string_table;
} PACKED;

// file version:
// fullmap_file_nodeinfo_v1
struct fullmap_nodeinfo_v1
{
	double x(void) { return lat; }
	double y(void) { return lon; }
	double distance(double x, double y) {
		return zas::mapcore::distance(lon, lat, y, x);
	}
	bool KNN_selected(void) {
		return (attr.KNN_selected) ? true : false;
	}
	void KNN_select(void) {
		attr.KNN_selected = 1;
	}
	int get_split(void) {
		return attr.kdtree_dimension;
	}
	void set_split(int s) {
		attr.kdtree_dimension = (s) ? 1 : 0;
	}

	osm_parser1::uid_t uid;
	double lat, lon;

	// kd-tree
	fullmap_nodeinfo_v1 *left, *right;
	void set_left(fullmap_nodeinfo_v1* l) {
		left = l;
	}
	void set_right(fullmap_nodeinfo_v1* r) {
		right = r;
	}
	fullmap_nodeinfo_v1* get_left(void) {
		return left;
	}
	fullmap_nodeinfo_v1* get_right(void) {
		return right;
	}
	// all links
	uint32_t link_count;

	// if it is a key node, the extra_info
	struct {
		uint32_t keynode : 1;
		uint32_t kdtree_dimension : 1;
		uint32_t KNN_selected : 1;
	} attr;
	fullmap_node_extra_info_v1* extra_info;

	// referred links
	fullmap_linkinfo_v1** links;
};

// file version:
// fullmap_file_node_extra_info_v1
struct fullmap_node_extra_info_v1
{
	int intersect_name_count;
	// the first intersect name
	const char* first;
} PACKED;

// file version:
// fullmap_file_linkinfo_v1
struct fullmap_linkinfo_v1
{
	void* data;
	double distance;

	// which way this link belongs to
	fullmap_wayinfo_v1* wayinfo;

	// the first node of the link
	fullmap_nodeinfo_v1* node1;
	// the second node of the link
	fullmap_nodeinfo_v1* node2;

	// pointer to prev link and next link
	fullmap_linkinfo_v1* prev;
	size_t prev_blockid;
	fullmap_linkinfo_v1* next;
	size_t next_blockid;
} PACKED;

struct fullmap_wayinfo_v1
{
	osm_parser1::uid_t uid;
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

	const char* name;
} PACKED;

}} // end of namespace zas::mapcore
#endif // __CXX_MAPCORE_MEM_FORMAT_H__
/* EOF */
