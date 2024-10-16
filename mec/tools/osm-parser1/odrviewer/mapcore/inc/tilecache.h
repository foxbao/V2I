/** @file tilecache.h
 */

#ifndef __CXX_ZAS_MAPCORE_TILECACHE_IMPL_H__
#define __CXX_ZAS_MAPCORE_TILECACHE_IMPL_H__

#include <map>
#include "std/list.h"
#include "utils/mutex.h"
#include "rendermap.h"

#include "inc/maputils.h"

// todo: re-arrange the following header
#include "inc/gbuf.h"

namespace zas {
namespace mapcore {

using namespace std;
using namespace zas::utils;
using namespace render_hdmap;

struct blockcache
{
	blockcache(rendermap_impl* m, blockinfo_v1* b)
	: attrs(0), map(m), block(b) {}
	union {
		uint32_t attrs;
		struct {
			uint32_t locked : 1;
		} a;
	};
	rendermap_impl* map;
	blockinfo_v1* block;
	weak_ptr<RoadNetworkMesh> mesh;
};

struct ref_bitmap
{
	ref_bitmap(uint32_t blkid, uint32_t* m)
	: id(blkid), refcnt(1), bmp(m) {}
	void addref(void) { ++refcnt; }
	uint32_t id;
	int refcnt;
	uint32_t* bmp;
};

struct ref_bitmap_
{
	ref_bitmap_(ref_bitmap& b) : ref(b) {}
	bool operator<(const ref_bitmap_& other) const {
		return (ref.refcnt < other.ref.refcnt)
		? true : false;
	}
	ref_bitmap& ref;
};

class tilecache_impl
{
public:
	tilecache_impl();
	~tilecache_impl();

	int bind_map(const void* buffer, size_t sz);
	int add_tile(const void* buffer, size_t sz,
		rendermap_impl** = nullptr);
	shared_ptr<const odr::RoadNetworkMesh> get_mesh(uint32_t blkid, bool* g);
	int release_mesh(uint32_t blkid);
	int query_loaded_blocks(const vector<point2d>& pts, set<uint32_t>& blkids);
	int query_unloaded_blocks(const vector<point2d>& pts, set<uint32_t>& blkids);
	int extract_roadobjects(const vector<point2d>&,
		const set<uint32_t>&, set<const rendermap_roadobject*>&);
	int extract_roadobjects(int blkid,
		const set<uint32_t>&, set<const rendermap_roadobject*>&);
	bool get_mapid(uint128_t& id);
	const char* get_map_georef(void);
	const point3d& get_mapoffset(void);
	bool map_available(void);
	int filter_loaded_blocks(set<uint32_t>& blkids, bool lock);
	int lock_blocks(const set<uint32_t>& blkids);
	int unlock_blocks(const set<uint32_t>& blkids);
	int release_blocks(const set<uint32_t>& blkids);
	double get_road_height(double x, double y);
	int add_approach(rendermap_impl* rmap);
	int find_junction_center_byappro(double& x, double& y, string);

private:
	void release_all_unlocked(void);
	void release_map_unlocked(rendermap_impl* map);

	bool check_uuid_unlocked(rendermap_v1* map);
	int update_blockmap_unlocked(rendermap_impl* rmap);
	int remove_blockmap_unlocked(rendermap_impl* rmap);

	int fix_blocks_unlocked(rendermap_impl* rmap);
	int fix_roadsecid_unlocked(rendermap_impl* rmap, referring_id*);
	int fix_lanesecid_unlocked(rendermap_impl* rmap, referring_id*);
	int fix_roadobject_unlocked(rendermap_impl* rmap, referring_id*);

	int entrust_mesh_unlocked(uint32_t, blockinfo_v1*, set<uint32_t>&);
	template <typename T> int merge_block_objects_unlocked(
		uint32_t, idxtbl_v1<T>&, map<uint32_t, ref_bitmap>&,
		zas::hwgrph::gbuf_<uint32_t>&, int, int);
	void bitmap_setbit(uint32_t* bitmap, int index);
	bool bitmap_check_allmarked(uint32_t* bitmap, int count);	// count in bits

	void normalize_polygon(const vector<point2d>&, vector<point>&);
	void get_bounding_blocks(vector<point>&, int&, int&, int&, int&);
	int get_unloaded_blkidset_unlocked(int, int, int, int,
		const vector<point>&, set<uint32_t>&);
	int get_loaded_blkidset_unlocked(int, int, int, int,
		const vector<point>&, set<uint32_t>&);
	int block_in_polygon(uint32_t blkid, const vector<point>&);
	
	rendermap_impl* getmap_byaddr_unlocked(void* addr);
	void release_block_unlocked(uint32_t, blockcache*);
	int check_release_rendermap_unlocked(rendermap_impl* map);

private:
	// quick access for blocks
	map<uint32_t, blockcache> _blocks;
	map<uint32_t, blockcache> _inactive_blocks;
	map<std::string, Vec2D> _approachs;

	// indicate the pending mesh re-generation blocks
	set<uint32_t> _mesh_expired_blocks;

	listnode_t _maplist;
	avl_node_t* _maptree;
	rendermap_v1 _mapinfo;
	string _georef;
	point3d _mapoffset;

	mutex _mut;
};

}} // end of namespace zas::mapcore
#endif // __CXX_ZAS_MAPCORE_TILECACHE_IMPL_H__
/* EOF */
