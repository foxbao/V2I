/** @file sdmap-impl.h
 */

#ifndef __CXX_ZAS_MAPCORE_SDMAP_IMPL_H__
#define __CXX_ZAS_MAPCORE_SDMAP_IMPL_H__

#include "mapcore/sdmap.h"
#include "inc/mem-format.h"

namespace zas {
namespace mapcore {

using namespace osm_parser1;

struct sdmap_nearest_node_impl
{
	fullmap_nodeinfo_v1* node;
	double distance;
};

struct sdmap_nearest_link_impl
{
	fullmap_linkinfo_v1* link;
	fullmap_nodeinfo_v1* node;
	coord_t projection;
	double distance;
};

#define DEF_SEARCH_NODES	(16)
#define MAX_SEARCH_NODES	(256)
#define MAX_SEARCH_LINKS	(256)

class sdmap_impl
{
public:
	sdmap_impl();
	~sdmap_impl();

	int load_fromfile(const char* filename);

	int search_nearest_nodes(double lat, double lon,
		int count, sdmap_nearest_node_impl* set);

	int search_nearest_links(double lat, double lon,
		sdmap_nearest_link_impl* links,
		int node_range, int link_count,
		fullmap_nodeinfo_v1** nearest_node);
		
public:
	bool valid(void) {
		return (_f.valid) ? true : false;
	}

private:
	void reset(void);
	int check_validity(void);
	int loadfile(const char* filename);

	int fix_ref(void);
	int fix_ref_blocks(fullmap_file_keyinfo_v1*);
	int fix_ref_nodeinfo(fullmap_file_keyinfo_v1*, fullmap_blockinfo_v1*);
	int fix_ref_linkinfo(fullmap_file_keyinfo_v1*, fullmap_blockinfo_v1*);
	int fix_ref_wayinfo(fullmap_file_keyinfo_v1*, fullmap_blockinfo_v1*);

	int fix_kdtree(fullmap_file_keyinfo_v1* keyinfo,
		arridx_t ref, fullmap_nodeinfo_v1** node);

	void reset_linkdata(fullmap_file_keyinfo_v1*);

	fullmap_nodeinfo_v1* get_linknode(osm_parser1::arridx_t);
	fullmap_linkinfo_v1* get_link(size_t blkid, size_t linkid);

private:
	fullmap_fileheader_v1* header(void) {
		return reinterpret_cast<fullmap_fileheader_v1*>(_buffer);
	}

	fullmap_file_keyinfo_v1* file_keyinfo(void) {
		return (fullmap_file_keyinfo_v1*)(header() + 1);
	}

	size_t block_id(fullmap_blockinfo_v1* blkinfo) {
		auto* blk_start = (fullmap_blockinfo_v1*)(
			((fullmap_file_keyinfo_v1*)(header() + 1)) + 1);
		return blkinfo - blk_start;
	}

//	fullmap_nodeinfo_v1* search_nearest_node(
//		fullmap_nodeinfo_v1 * kdtree,
//		double lat, double lon, double &dist);
	
	double point_to_link(fullmap_linkinfo_v1* link,
		double srclon, double srclat,
		double& dstlon, double& dstlat);

	bool point_in_link(fullmap_linkinfo_v1* link,
		double lon, double lat);

	fullmap_linkinfo_v1* get_node_nearest_link(
		fullmap_nodeinfo_v1* node,
		double srclat, double srclon,
		double& dstlat, double& dstlon, double& dist);

private:
	uint8_t* _buffer;
	union {
		uint32_t _flags;
		struct {
			uint32_t valid : 1;
		} _f;
	};
};

}} // end of namespace zas::mapcore
#endif /* __CXX_ZAS_MAPCORE_SDMAP_IMPL_H__ */
/* EOF */
