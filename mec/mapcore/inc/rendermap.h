/** @file rendermap.h
 */

#ifndef __CXX_ZAS_MAPCORE_RENDERMAP_IMPL_H__
#define __CXX_ZAS_MAPCORE_RENDERMAP_IMPL_H__

#include <map>
#include <memory>

#include "std/list.h"
#include "utils/avltree.h"
#include "mapcore/hdmap.h"
#include "hdmap-format.h"

#include "Road.h"
#include "OpenDriveMap.h"

namespace zas {
namespace mapcore {

using namespace std;
using namespace odr;
using namespace zas::utils;
using namespace osm_parser1;
using namespace render_hdmap;

struct trx_allocator
{
	void* operator new(size_t, size_t extsz, size_t&, void**) throw();
	void operator delete(void* ptr, size_t);
	void* operator new[](size_t, size_t&, void**) throw();
	void operator delete[](void* ptr, size_t);
};

struct trx_offset : public offset, public trx_allocator
{} PACKED;

struct trx_lane_section_v1 : public lane_section_v1, public trx_allocator
{
	int add_referred_blockid(uint32_t blkid);
} PACKED;

struct trx_approach_v1 : public approach_info_v1, public trx_allocator
{} PACKED;

struct trx_junction_v1 : public junction_v1, public trx_allocator
{} PACKED;

struct trx_reference_line_v1 : public reference_line_v1, public trx_allocator
{} PACKED;

struct trx_super_elevation_v1 : public super_elevation_v1, public trx_allocator
{} PACKED;

struct trx_road_object_v1 : public road_object_v1, public trx_allocator
{
	int add_referred_blockid(uint32_t blkid);
} PACKED;

struct trx_roadseg_v1 : public roadseg_v1, public trx_allocator
{} PACKED;

struct trx_blockinfo_v1 : public blockinfo_v1, public trx_allocator
{} PACKED;

struct trx_rendermap_v1 : public rendermap_v1, public trx_allocator
{} PACKED;

struct trx_siglight_info_v1 : public siglight_info_v1, public trx_allocator
{} PACKED;

struct trx_roadside_device_info_v1 : public roadside_device_info_v1, public trx_allocator
{} PACKED;

struct trx_arrow_info_v1 : public arrow_info_v1, public trx_allocator
{} PACKED;

struct trx_sigboard_info_v1 : public sigboard_info_v1, public trx_allocator
{} PACKED;

class memory
{
public:
	memory();
	~memory();

	void* get_addr(size_t _id);
	size_t get_offset(void* addr);

	template <typename T> T* allocate(size_t* = nullptr, size_t = 0);
	template <typename T> void* allocate(uint32_t size, chunk<T>& chk);
	template <typename T> offset* allocate(int cnt, idxtbl_v1<T>&, bool link = true);

	void add_fixlist(offset* off, uint32_t* = nullptr);
	void serialize(string& ret);
	rendermap_v1* create_rendermap(hdrmap_file_rendermap_v1* rdmap);

public:
	rendermap_v1* get_rendermap(void) {
		assert(nullptr != _rendermap);
		return _rendermap;
	}

private:
	struct bufnode
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
	rendermap_v1* _rendermap;
	avl_node_t* _bufnode_id_tree;
	avl_node_t* _bufnode_addr_tree;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(memory);
};

template <typename T> static int find(idxtbl_v1<T>& tbl, uint32_t uid)
{
	if (!tbl.count) return -1;

	int mid;
	int start = 0;
	int end = tbl.count - 1;

	// find the item
	while (start <= end)
	{
		mid = (start + end) / 2;
		uint32_t val = tbl.indices[mid]->extinfo.id;
		if (uid == val) return mid;
		else if (uid > val) start = mid + 1;
		else end = mid - 1;
	}
	// error
	return -2;
}

#define JUNC_SDMAP_NODES_MAX_SEARCH		(16)

template <typename T> struct render_object
{
	size_t pos;
	T* obj;
	bool operator<(const render_object<T>& other) const {
		return (obj->extinfo.id < other.obj->extinfo.id);
	}
	render_object(size_t p, T* o)
	: pos(p), obj(o) {}
};

class rendermap_impl
{
	friend class tilecache_impl;
public:
	rendermap_impl();
	~rendermap_impl();

	// load from file
	int load_fromfile(const char* filename);

	// load from memory
	int load_frombuffer(const void* buf, size_t sz);
	
	// extract data
	int extract_blocks(const set<uint32_t>& blkset,
		const set<uint32_t>& shrblkset,
		string& ret);

	// get dependencies
	int get_block_dependencies(
		const set<uint32_t>& input, set<uint32_t>& output);

	const shared_ptr<odr::RoadNetworkMesh> generate_mesh(uint32_t blkid);

	int extract_roadobjects(int blkid, const set<uint32_t>& categories,
		set<const rendermap_roadobject*>& objects, bool reset);
	bool check_classified_type_validty(const set<uint32_t>& categories);

	// find the nearest junction
	int find_junctions(double x, double y, int count,
		vector<rendermap_nearest_junctions>& set);
	junction_v1* get_junction(uint32_t juncid);
	junction_v1* get_junction_by_index(uint32_t index);

	const char* get_georef(void);
	int get_offset(double& dx, double& dy);
	int get_mapinfo(rendermap_info& mapinfo);

public:
	rendermap_v1* getmap(void) {
		return reinterpret_cast<rendermap_v1*>(header());
	}

	bool valid(void) {
		return (_f.valid) ? true : false;
	}

	bool inside(void* object)
	{
		size_t tmp = (size_t)object;
		auto* map = getmap();
		size_t start = (size_t)map;
		size_t end = start + map->size;
		return (tmp >= start && tmp < end) ? true : false;
	}

	int addref(void) {
		return __sync_add_and_fetch(&_refcnt, 1);
	}

	int decref(void) {
		return __sync_sub_and_fetch(&_refcnt, 1);
	}

	shared_ptr<odr::RoadNetworkMesh> release_mesh(uint32_t blkid)
	{
		auto iter = _meshbuf.find(blkid);
		if (iter == _meshbuf.end()) return nullptr;
		auto ret = iter->second;
		_meshbuf.erase(iter);
		return ret;
	}

	const proj& get_wgs84proj(void) const {
		return _wgs84prj;
	}

private:
	void release_all(void);
	int check_validity(void);
	int loadfile(const char* filename);
	int fix_ref(void);
	int optimize_junctions(void);
	int finalize(void);

	// check the dependency for all lane sections
	// in specific blocks
	int lanesec_dependencies(uint32_t blkid, set<uint32_t>& output);

	int roadobj_dependencies(uint32_t blkid, set<uint32_t>& output);

	int extract_rendermap_info(string& ret);
	int extract_roads(memory& m, hdrmap_file_blockinfo_v1* blk,
		trx_blockinfo_v1* trxblk, const set<uint32_t>& shared,
		set<render_object<trx_roadseg_v1>>&);

	int extract_roadobj(memory& m, hdrmap_file_blockinfo_v1* blk,
		trx_blockinfo_v1* trxblk, const set<uint32_t>& shared,
		set<render_object<trx_road_object_v1>>&);

	int extract_lanesecs(memory& m, hdrmap_file_blockinfo_v1* blk,
		trx_blockinfo_v1* trxblk, const set<uint32_t>& shared,
		set<render_object<trx_lane_section_v1>>&);

	int extract_junctions(memory& m, hdrmap_file_blockinfo_v1* blk,
		trx_blockinfo_v1* trxblk, set<render_object<trx_junction_v1>>&);

	int extract_road(memory&, roadseg_v1*, hdrmap_file_roadseg_v1*);
	int extract_lanesec_extinfo(memory&, lane_section_v1*,
		hdrmap_file_lanesec_v1*);
	int extract_roadobj_extinfo(memory&, road_object_v1*,
		hdrmap_file_road_object_v1*);
	int extract_classifed_roadobject_extension(memory&,
		road_object_v1*, hdrmap_file_road_object_v1*);
	int extract_junction_info(memory& m, junction_v1*,
		hdrmap_file_junction_v1*);

	bool road_exists_in_shared_blocks(memory& m,
		const set<uint32_t>& shared, uint32_t rid);
	bool lanesec_exists_in_shared_blocks(memory&,
		const set<uint32_t>& shared, uint32_t lid);
	bool roadobj_exists_in_shared_blocks(memory&,
		const set<uint32_t>& shared, uint32_t oid);

	blockinfo_v1* getblock(int blkid);
	shared_ptr<Road> get_block_odrv_road(blockinfo_v1*,
		uint32_t, OpenDriveMap&, map<uint32_t, shared_ptr<Road>>&);
	int deserialize_block_lanesecs(blockinfo_v1*,
		OpenDriveMap&, map<uint32_t, shared_ptr<Road>>&);
	int deserialize_block_roadobjs(blockinfo_v1*,
		OpenDriveMap&, map<uint32_t, shared_ptr<Road>>&);

	static int addr_avlcompare(avl_node_t*, avl_node_t*);

private:
	hdrmap_file_rendermap_v1* header(void) {
		return reinterpret_cast<hdrmap_file_rendermap_v1*>(_buffer);
	}

	void ultoa(uint64_t v, string& str) {
		char buf[64];
		sprintf(buf, "%lu", v);
		str = buf;
	}

	void reset(void) {
		release_all();
	}

private:
	int _refcnt;
	int _active_blkcnt;
	uint8_t* _buffer;
	size_t _buffer_end;
	union {
		uint32_t _flags;
		struct {
			uint32_t valid : 1;
		} _f;
	};
	listnode_t _ownerlist;
	avl_node_t _avlnode;
	proj _wgs84prj;
	map<uint32_t, shared_ptr<odr::RoadNetworkMesh>> _meshbuf;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(rendermap_impl);
};

}} // end of namespace zas::mapcore
#endif // __CXX_ZAS_MAPCORE_RENDERMAP_IMPL_H__
/* EOF */
