/** @file tilecache.cpp
 */

#include <float.h>
#include "inc/tilecache.h"
#include "Viewer/RoadNetworkMesh.h"

namespace zas {
namespace mapcore {

using namespace std;
using namespace zas::utils;

tilecache_impl::tilecache_impl()
: _maptree(nullptr)
{
	listnode_init(_maplist);
	memset(&_mapinfo.uuid, 0, sizeof(uint128_t));
	_mapoffset.v[0] = _mapoffset.v[1] = _mapoffset.v[2] = 0;
}

tilecache_impl::~tilecache_impl()
{
	auto_mutex as(_mut);
	release_all_unlocked();
}

void tilecache_impl::release_all_unlocked(void)
{
	while (!listnode_isempty(_maplist)) {
		auto* map = list_entry(rendermap_impl, _ownerlist, _maplist.next);
		listnode_del(map->_ownerlist);
		delete map;
	}
}

#define add_tile_error(m, e)	\
	do { release_map_unlocked(m); return e; } while (0)

int tilecache_impl::add_tile(const void* buffer,
	size_t sz, rendermap_impl** rm)
{
	if (nullptr == buffer || !sz) {
		return -EBADPARM;
	}
	rendermap_impl* rdmap = new rendermap_impl();
	if (nullptr == rdmap) {
		return -ENOMEMORY;
	}

	int ret = rdmap->load_frombuffer(buffer, sz);
	if (ret) {
		delete rdmap; return ret;
	}
	auto* map = rdmap->getmap();
	if (map->a.persistence) {
		delete rdmap; return -ENOTSUPPORT;
	}

	MUTEX_ENTER(_mut);
	if (!check_uuid_unlocked(map)) {
		add_tile_error(rdmap, -EINVALID);
	}

	// check if the "tile" is only the map info
	if (map->a.mapinfo_only) {
		delete map;
		if (rm) *rm = nullptr;
		return 0;
	}

	listnode_add(_maplist, rdmap->_ownerlist);
	ret = avl_insert(&_maptree,
		&rdmap->_avlnode, rendermap_impl::addr_avlcompare);
	assert(ret == 0);

	if (update_blockmap_unlocked(rdmap)) {
		add_tile_error(rdmap, -1);
	}

	if (fix_blocks_unlocked(rdmap)) {
		add_tile_error(rdmap, -2);
	}

	add_approach(rdmap);
	ret = map->blocks.count;
	if (rm) *rm = rdmap;
	MUTEX_EXIT();
	return ret;
}

int tilecache_impl::bind_map(const void* buf, size_t sz)
{
	rendermap_impl* map = nullptr;
	int ret = add_tile(buf, sz, &map);
	if (ret != 0) {
		// ret < 0 means error occurred
		// ret > 0 means a block loaded
		if (map) {
			auto_mutex am(_mut);
			release_map_unlocked(map);
			return -1;
		}
	}
	return 0;
}

int tilecache_impl::add_approach(rendermap_impl* rmap)
{
	auto* map = rmap->getmap();
	auto_mutex am(_mut);
	for (int i = 0; i < map->blocks.count; ++i)
	{
		auto* blk = map->blocks.indices[i];
		uint32_t junc_count = blk->junctions.count;
		for (int j = 0; j < junc_count; ++j) 
		{
			auto* junc = blk->junctions.indices[j];
			uint32_t appro_count = junc->approachs.count;
			for (int k = 0; k < appro_count; ++k) 
			{
				auto* appro = junc->approachs.indices[k];
				Vec2D v2d = {junc->center_x, junc->center_y};
				std::string appro_uuid = std::string((char*)appro->data, appro->uid_sz);
				_approachs[appro_uuid] = v2d;
			}
		}
	}
	return 0;
}

int tilecache_impl::fix_blocks_unlocked(rendermap_impl* rmap)
{
	auto* map = rmap->getmap();
	for (int i = 0; i < map->blocks.count; ++i)
	{
		auto* blk = map->blocks.indices[i];
		uint32_t link = blk->rid_list;
		while (link != 0)
		{
			auto* ids  = (referring_id*)(&((uint8_t*)map)[link]);
			link = ids->link;

			if (fix_roadsecid_unlocked(rmap, ids)) {
				return -1;
			}
		}

		for (link = blk->lsid_list; link;)
		{
			auto* ids  = (referring_id*)(&((uint8_t*)map)[link]);
			link = ids->link;

			if (fix_lanesecid_unlocked(rmap, ids)) {
				return -1;
			}
		}

		for (link = blk->objid_list; link;)
		{
			auto* ids  = (referring_id*)(&((uint8_t*)map)[link]);
			link = ids->link;

			if (fix_roadobject_unlocked(rmap, ids)) {
				return -1;
			}
		}
	}
	return 0;
}

void tilecache_impl::release_map_unlocked(rendermap_impl* map)
{
	remove_blockmap_unlocked(map);

	listnode_del(map->_ownerlist);
	avl_remove(&_maptree, &map->_avlnode);

	if (listnode_isempty(_maplist)) {
		memset(&_mapinfo, 0, sizeof(_mapinfo));
		_georef.clear();
		_mapoffset.v[0] = _mapoffset.v[1] = _mapoffset.v[2] = 0;
	}
	delete map;
}

int tilecache_impl::fix_roadobject_unlocked(
	rendermap_impl* rmap, referring_id* refid)
{
	auto* i = _maplist.next;
	for (; i != &_maplist; i = i->next) {
		auto* map = list_entry(rendermap_impl, _ownerlist, i);
		if (map == rmap) {
			continue;
		}

		auto& robj = map->getmap()->roadobjs;
		int idx = find(robj, refid->id);
		if (idx >= 0) {
			map->addref();
			*((road_object_v1**)refid) = robj.indices[idx];
			return 0;
		}
	}
	return -1;
}

int tilecache_impl::fix_roadsecid_unlocked(
	rendermap_impl* rmap, referring_id* refid)
{
	auto* i = _maplist.next;
	for (; i != &_maplist; i = i->next) {
		auto* map = list_entry(rendermap_impl, _ownerlist, i);
		if (map == rmap) {
			continue;
		}

		auto& rs = map->getmap()->roadsegs;
		int idx = find(rs, refid->id);
		if (idx >= 0) {
			map->addref();
			*((roadseg_v1**)refid) = rs.indices[idx];
			return 0;
		}
	}
	return -1;
}

int tilecache_impl::fix_lanesecid_unlocked(
	rendermap_impl* rmap, referring_id* refid)
{
	auto* i = _maplist.next;
	for (; i != &_maplist; i = i->next) {
		auto* map = list_entry(rendermap_impl, _ownerlist, i);
		if (map == rmap) {
			continue;
		}

		auto& ls = map->getmap()->lanesecs;
		int idx = find(ls, refid->id);
		if (idx >= 0) {
			map->addref();
			*((lane_section_v1**)refid) = ls.indices[idx];
			return 0;
		}
	}
	return -1;
}

int tilecache_impl::update_blockmap_unlocked(rendermap_impl* rmap)
{
	auto* map = rmap->getmap();
	for (int i = 0; i < map->blocks.count; ++i) {
		auto* blk = map->blocks.indices[i];
		int blkid = blk->row * map->cols + blk->col;
		_blocks.insert({blkid, {rmap, blk}});
	}
	return 0;
}

int tilecache_impl::remove_blockmap_unlocked(rendermap_impl* rmap)
{
	auto* map = rmap->getmap();
	for (int i = 0; i < map->blocks.count; ++i) {
		auto* blk = map->blocks.indices[i];
		int blkid = blk->row * map->cols + blk->col;
		_blocks.erase(blkid);
	}
	return 0;
}

bool tilecache_impl::check_uuid_unlocked(rendermap_v1* map)
{
	if (listnode_isempty(_maplist)) {
		// this is the first map tile, update
		// the uuid as this one
		uint128_t zero = {0};
		if (!memcmp(&_mapinfo.uuid, &zero, sizeof(zero))) {
			memcpy(&_mapinfo, map, sizeof(_mapinfo));
			_georef = map->proj;
			_mapoffset.v[0] = map->xoffset;
			_mapoffset.v[1] = map->yoffset;
			_mapoffset.v[2] = 0.;
		}
	}
	else {
		// check if this map has the same uuid as
		// previous committed tiles
		if (memcmp(&_mapinfo.uuid, &map->uuid, sizeof(uint128_t))) {
			return false;
		}
	}
	return true;
}

bool tilecache_impl::map_available(void)
{
	uint128_t zero = {0};
	if (!memcmp(&_mapinfo.uuid, &zero, sizeof(zero))) {
		return false;
	}
	return true;
}

bool tilecache_impl::get_mapid(uint128_t& id)
{
	auto ret = map_available();
	if (ret) {
		id = _mapinfo.uuid;
	}
	return ret;
}

const char* tilecache_impl::get_map_georef(void)
{
	if (!map_available()) {
		return nullptr;
	}
	return _georef.c_str();
}

const point3d& tilecache_impl::get_mapoffset(void)
{
	return _mapoffset;
}

int tilecache_impl::release_mesh(uint32_t blkid)
{
	set<uint32_t> pending;
	shared_ptr<const odr::RoadNetworkMesh> mesh;

	// we need lock here
	MUTEX_ENTER(_mut);

	auto blk = _blocks.find(blkid);
	if (blk == _blocks.end()) {
		return -ENOTFOUND; // block not found
	}

	auto block = blk->second.block;
	assert(nullptr != block);	
	if (entrust_mesh_unlocked(blk->first, block, pending)) {
		return -1;
	}

	// detach the mesh
	mesh = blk->second.map->release_mesh(blk->first);
	assert(nullptr != mesh);

	// insert all pendings
	_mesh_expired_blocks.insert(pending.begin(), pending.end());

	// we do release mesh after unlock to shorten
	// the lock time
	MUTEX_EXIT();

	// release the mesh
	mesh = nullptr;
	return 0;
}

int tilecache_impl::entrust_mesh_unlocked(uint32_t blkid,
	blockinfo_v1* block, set<uint32_t>& pending)
{
	map<uint32_t, ref_bitmap> refmap;
	zas::hwgrph::gbuf_<uint32_t> bitmap(64);

	int count = 0;
	int total = block->lane_secs.count + block->road_objs.count;
	if (!total) return 0;
	total = (total / 32) + ((total % 32) ? 1 : 0);

	count = merge_block_objects_unlocked(
		blkid, block->lane_secs, refmap, bitmap, total, count);
	if (count < 0) return -1;
	count = merge_block_objects_unlocked(
		blkid, block->road_objs, refmap, bitmap, total, count);
	if (count < 0) return -2;

	// check if there is any shared objects
	if (!count) return 0;

	set<ref_bitmap_> refset;
	for (auto ref : refmap) {
		refset.insert(ref.second);
	}

	uint32_t* tstbmp = bitmap.new_objects(total);
	if (nullptr == tstbmp) {
		return -3;
	}
	memset(tstbmp, 0, sizeof(uint32_t) * total);

	int dwords = (count / 32) + ((count % 32) ? 1 : 0);
	for (auto r : refset) {
		uint32_t* buf = r.ref.bmp;
		for (int i = 0; i < dwords; ++i) {
			tstbmp[i] |= buf[i];
		}
		// this ref shall be put to pending list
		pending.insert(r.ref.id);
		if (bitmap_check_allmarked(tstbmp, count)) {
			break;
		}
	}
	return 0;
}

bool tilecache_impl::bitmap_check_allmarked(uint32_t* bitmap, int count)
{
	if (!count) return true;

	int dwords = count / 32;
	for (int i = 0; i < dwords; ++i) {
		if (bitmap[i] != 0xFFFFFFFF) return false;
	}

	int rest = count % 32;
	if (!rest) return true;

	// check rest bits
	uint32_t final = bitmap[dwords];
	for (int i = 0; i < rest; ++i) {
		if (!(final & (1 << i))) return false;
	}
	return true;
}

void tilecache_impl::bitmap_setbit(uint32_t* bitmap, int index) {
	bitmap[index / 32] |= (1 << (index % 32));
}

template <typename T> int tilecache_impl::merge_block_objects_unlocked(
	uint32_t blkid, idxtbl_v1<T>& tbl, map<uint32_t,
	ref_bitmap>& refmap, zas::hwgrph::gbuf_<uint32_t>& bmp,
	int total, int idxstart)
{
	int index = idxstart;
	for (int i = 0; i < tbl.count; ++i) {
		auto* obj = tbl.indices[i];
		// is this object mesh generated
		if (nullptr == obj->extinfo.referred_blkids) {
			continue;
		}
		auto refs = obj->extinfo.referred_blkids;
		
		// is this object be rendered by this block
		if (obj->extinfo.meshgen_blockid != blkid) {
			refs->erase(blkid);
			continue;
		}
		// means this render object's owner
		// has changed to "anonymous"
		obj->extinfo.meshgen_blockid = -1;

		for (auto id : *refs) {
			if (id == blkid) {
				if (refs->size() == 1) {
					// I'm the only one, means this rendering
					// object is not shared
					--index;
				}
				continue;
			}

			auto ref = refmap.find(id);
			if (ref == refmap.end()) {
				// create new one
				uint32_t* bmpbuf = bmp.new_objects(total);
				assert(nullptr != bmpbuf);

				memset(bmpbuf, 0, sizeof(uint32_t) * total);
				bitmap_setbit(bmpbuf, index);
				refmap.insert({id, {id, bmpbuf}});
			}
			else {
				bitmap_setbit(ref->second.bmp, index);
				ref->second.addref();
			}
		}
		refs->erase(blkid);
		++index;
	}
	return index;
}

shared_ptr<const odr::RoadNetworkMesh>
tilecache_impl::get_mesh(uint32_t blkid, bool* g)
{
	bool gen = false;
	// we need lock here
	auto_mutex am(_mut);

	auto blk = _blocks.find(blkid);
	if (blk == _blocks.end()) {
		return nullptr; // block not found
	}

	assert(nullptr != blk->second.map);

	// check if this block is requested to
	// re-generate the mesh
	bool expired = (_mesh_expired_blocks.find(blkid)
		!= _mesh_expired_blocks.end());

	// get the generated mesh (even if it has expired)
	auto ret = blk->second.mesh.lock();

	if (nullptr == ret || expired) {
		// act mesh regeneration
		gen = true;
		ret = blk->second.map->generate_mesh(blkid);
		if (nullptr == ret) {
			return nullptr;
		}
		blk->second.mesh = ret;
	}

	if (expired) {
		_mesh_expired_blocks.erase(blkid);
	}
	if (g) *g = gen;
	return ret;
}

void tilecache_impl::get_bounding_blocks(
	vector<point>& polygon, int& ix1, int& iy1, int& ix2, int& iy2)
{
	double x1 = DBL_MAX, x2 = -DBL_MAX;
	double y1 = DBL_MAX, y2 = -DBL_MAX;
	for (auto& pt : polygon) {
		if (pt.x < x1) x1 = pt.x;
		if (pt.x > x2) x2 = pt.x;
		if (pt.y < y1) y1 = pt.y;
		if (pt.y > y2) y2 = pt.y;
	}

	if (x1 < _mapinfo.xmin) x1 = _mapinfo.xmin;
	if (y1 < _mapinfo.ymin) y1 = _mapinfo.ymin;

	double dx1 = x1 - _mapinfo.xmin, dx2 = x2 - _mapinfo.xmin;
	double dy1 = y1 - _mapinfo.ymin, dy2 = y2 - _mapinfo.ymin;

	ix1 = int(dx1 / _mapinfo.block_width);
	iy1 = int(dy1 / _mapinfo.block_height);
	ix2 = int(dx2 / _mapinfo.block_width) + 1;
	iy2 = int(dy2 / _mapinfo.block_height) + 1;

	if (ix2 > _mapinfo.cols) ix2 = _mapinfo.cols;
	if (iy2 > _mapinfo.rows) iy2 = _mapinfo.rows;
	assert(ix1 <= ix2 && iy1 <= iy2);
}

int tilecache_impl::extract_roadobjects(
	const vector<point2d>& pts,
	const set<uint32_t>& categories,
	set<const rendermap_roadobject*>& objects)
{
	set<uint32_t> blkids;

	// we need lock here
	auto_mutex am(_mut);
	int ret = query_loaded_blocks(pts, blkids);
	if (ret) { return ret; }

	objects.clear();
	for (auto blkid : blkids)
	{
		auto iter = _blocks.find(blkid);
		assert(iter != _blocks.end());

		if (iter->second.map->extract_roadobjects(blkid,
			categories, objects, true)) {
			objects.clear(); return -EINVALID;
		}
	}
	return 0;
}

int tilecache_impl::extract_roadobjects(int blkid,
	const set<uint32_t>& categories,
	set<const rendermap_roadobject*>& objects)
{
	// we need lock here
	auto_mutex am(_mut);
	auto iter = _blocks.find(blkid);
	if (iter == _blocks.end()) {
		return -ENOTEXISTS;
	}

	objects.clear();
	if (iter->second.map->extract_roadobjects(blkid,
		categories, objects, true)) {
		objects.clear(); return -EINVALID;
	}
	return 0;	
}

int tilecache_impl::query_unloaded_blocks(
	const vector<point2d>& pts, set<uint32_t>& blkids)
{
	if (pts.size() < 3) {
		return -EINVALID;
	}
	vector<point> polygon;
	normalize_polygon(pts, polygon);

	// get the bounding box
	int ix1, iy1, ix2, iy2;
	get_bounding_blocks(polygon, ix1, iy1, ix2, iy2);

	// we need lock here
	auto_mutex am(_mut);
	return get_unloaded_blkidset_unlocked(
		ix1, iy1, ix2, iy2, polygon, blkids);
}

int tilecache_impl::query_loaded_blocks(
	const vector<point2d>& pts, set<uint32_t>& blkids)
{
	if (pts.size() < 3) {
		return -EINVALID;
	}
	vector<point> polygon;
	normalize_polygon(pts, polygon);

	// get the bounding box
	int ix1, iy1, ix2, iy2;
	get_bounding_blocks(polygon, ix1, iy1, ix2, iy2);

	// we need lock here
	auto_mutex am(_mut);
	return get_loaded_blkidset_unlocked(
		ix1, iy1, ix2, iy2, polygon, blkids);
}

int tilecache_impl::get_loaded_blkidset_unlocked(
	int x1, int y1, int x2, int y2,
	const vector<point>& polygon,
	set<uint32_t>& blkids)
{
	blkids.clear();
	for (int j = y1; j < y2; ++j)
	{
		int start = j * _mapinfo.cols;
		int end = start + x2;
		for (start += x1; start < end; ++start) {
			if (block_in_polygon(start, polygon)) {
				blkids.insert(start);
			}
		}
	}
	return 0;
}

int tilecache_impl::get_unloaded_blkidset_unlocked(
	int x1, int y1, int x2, int y2,
	const vector<point>& polygon,
	set<uint32_t>& blkids)
{
	blkids.clear();
	for (int j = y1; j < y2; ++j)
	{
		int start, end;
		start = j * _mapinfo.cols;
		end = start + x2;
		start += x1;
		auto itr = _blocks.lower_bound(start);
		for (; start < end; itr++)
		{
			if (itr == _blocks.end() || end <= itr->first) {
				// all blocks in this row not exists
				for (; start < end; ++start) {
					if (block_in_polygon(start, polygon)) {
						blkids.insert(start);
					}
				}
				break; // finish this row
			}
			int pos = itr->first;
			for (; start < pos; ++start) {
				if (block_in_polygon(start, polygon)) {
					blkids.insert(start);
				}
			}
			start = pos + 1;
		}
	}
	return 0;
}

int tilecache_impl::block_in_polygon(uint32_t blkid, const vector<point>& polygon)
{
	int x = blkid % _mapinfo.cols;
	int y = blkid / _mapinfo.cols;
	vector<point> rect;

	rect.push_back({ _mapinfo.xmin + double(x) * _mapinfo.block_width,
		_mapinfo.ymin + double(y) * _mapinfo.block_height });
	rect.push_back({ _mapinfo.xmin + double(x + 1) * _mapinfo.block_width,
		_mapinfo.ymin + double(y) * _mapinfo.block_height });
	rect.push_back({ _mapinfo.xmin + double(x + 1) * _mapinfo.block_width,
		  _mapinfo.ymin + double(y + 1) * _mapinfo.block_height });
	rect.push_back({ _mapinfo.xmin + double(x) * _mapinfo.block_width,
		  _mapinfo.ymin + double(y + 1) * _mapinfo.block_height });

	return is_polygons_intersected(rect, polygon);
}

void tilecache_impl::normalize_polygon(
	const vector<point2d>& src, vector<point>& dst)
{
	dst.insert(dst.end(), src.begin(), src.end());
	double s = area(dst);
	if (s > 0) reverse(dst);
}

int tilecache_impl::filter_loaded_blocks(set<uint32_t>& blkids, bool lock)
{
	if (!blkids.size()) {
		return 0;
	}
	if (!map_available()) {
		blkids.clear();
		return 0;
	}

	// we need lock here
	auto_mutex am(_mut);
	for (auto id = blkids.begin(); id != blkids.end();) {
		auto iter = _blocks.find(*id);
		if (iter == _blocks.end()) {
			// not found
			id = blkids.erase(id);
			continue;
		}
		if (lock) {
			iter->second.a.locked = 1;
		}
		id++;
	}
	return 0;
}

int tilecache_impl::lock_blocks(const set<uint32_t>& blkids)
{
	if (!blkids.size()) {
		return 0;
	}
	if (!map_available()) {
		return -EINVALID;
	}

	// we need lock here
	auto_mutex am(_mut);
	for (auto id : blkids) {
		auto iter = _blocks.find(id);
		if (iter == _blocks.end()) {
			// we only handle blocks in active list
			continue;
		}
		iter->second.a.locked = 1;
	}
	return 0;
}

int tilecache_impl::unlock_blocks(const set<uint32_t>& blkids)
{
	if (!blkids.size()) {
		return 0;
	}
	if (!map_available()) {
		return -EINVALID;
	}

	// we need lock here
	auto_mutex am(_mut);
	for (auto id : blkids)
	{
		auto iter = _blocks.find(id);
		if (iter == _blocks.end()) {
			// if the blocks in active list not found
			// we search it in inactive list
			iter = _inactive_blocks.find(id);
			if (iter == _inactive_blocks.end()) {
				continue;
			}
			else {
				// erase must be called before "release_block"
				// since it is possible to erase block ID in
				// "release_block" method, calling "double free"
				_inactive_blocks.erase(iter);
				release_block_unlocked(iter->first, &iter->second);
			}
		}
		else iter->second.a.locked = 0;
	}
	return 0;
}

rendermap_impl* tilecache_impl::getmap_byaddr_unlocked(void* addr)
{
	uint8_t dumb[sizeof(rendermap_impl)];
	auto* dummy = (rendermap_impl*)dumb;
	dummy->_buffer = (uint8_t*)addr;
	dummy->_buffer_end = (size_t)addr;
	auto* node = avl_find(_maptree, &dummy->_avlnode,
		rendermap_impl::addr_avlcompare);
	if (nullptr == node) {
		return nullptr;
	}
	return AVLNODE_ENTRY(rendermap_impl, _avlnode, node);
}

int tilecache_impl::check_release_rendermap_unlocked(rendermap_impl* map)
{
	if (map->_active_blkcnt > 0 || map->_refcnt > 0) {
		return 1;
	}
	release_map_unlocked(map);
	return 0;
}

void tilecache_impl::release_block_unlocked(uint32_t blkid, blockcache* cache)
{
	auto* blk = cache->block;
	assert(nullptr != blk);

	// release all reference
	for (int i = 0; i < blk->road_segs.count; ++i) {
		auto* rs = blk->road_segs.indices[i];
		auto* map = getmap_byaddr_unlocked(rs);
		if (map != cache->map) {
			map->decref();	// release the reference
			check_release_rendermap_unlocked(map);
		}
	}

	for (int i = 0; i < blk->lane_secs.count; ++i) {
		auto* ls = blk->lane_secs.indices[i];
		auto* map = getmap_byaddr_unlocked(ls);
		if (map != cache->map) {
			map->decref();	// release the reference
			check_release_rendermap_unlocked(map);
		}
	}

	for (int i = 0; i < blk->road_objs.count; ++i) {
		auto* ro = blk->road_objs.indices[i];
		auto* map = getmap_byaddr_unlocked(ro);
		if (map != cache->map) {
			map->decref();	// release the reference
			check_release_rendermap_unlocked(map);
		}
	}

	for (int i = 0; i < blk->junctions.count; ++i) 
	{
		auto* junc = blk->junctions.indices[i];
		uint32_t appro_count = junc->approachs.count;
		for (int j = 0; j < appro_count; ++j) 
		{
			auto* appro = junc->approachs.indices[j];
			std::string appro_uuid = std::string((char*)appro->data, appro->uid_sz);
			std::map<std::string, Vec2D>::iterator iter = _approachs.find(appro_uuid);
			if (iter != _approachs.end())
			{
				_approachs.erase(iter);
			}
		}
	}

	// check if we need to release the current map
	--cache->map->_active_blkcnt;
	check_release_rendermap_unlocked(cache->map);
}

int tilecache_impl::release_blocks(const set<uint32_t>& blkids)
{
	if (!blkids.size() || !map_available()) {
		return 0;
	}

	// we need lock here
	auto_mutex am(_mut);
	for (auto id : blkids)
	{
		auto iter = _blocks.find(id);
		if (iter == _blocks.end()) {
			continue;
		}
		
		// check if 3DMesh exists
		if (!iter->second.mesh.expired()) {
			int ret = release_mesh(id);
			assert(ret == 0);
		}
		
		// check if the block is locked
		if (iter->second.a.locked) {
			// add this node to inactive list
			_inactive_blocks.insert(*iter);
			// remove it from the active list
			_blocks.erase(iter);
		}
		else {
			// erase must be called before "release_block"
			// since it is possible to erase block ID in
			// "release_block" method, calling "double free"
			_blocks.erase(iter);
			release_block_unlocked(iter->first, &iter->second);
		}
	}
	return 0;
}

double tilecache_impl::get_road_height(double x, double y)
{
	if (!_blocks.size() || !map_available()) {
		return 0.0;
	}
	if (x < _mapinfo.xmin || y < _mapinfo.ymin
		|| x > _mapinfo.xmax || y > _mapinfo.ymax) {
		return 0.0;
	}

	auto_mutex am(_mut);
	int px = (x - _mapinfo.xmin) / _mapinfo.block_width;
	int py = (y - _mapinfo.ymin) / _mapinfo.block_height;
	uint32_t block_id = (py * _mapinfo.cols) + px;

	double height = 0.0;
	uint32_t blockIndex = 1;
	do
	{
		uint32_t startBlockId = block_id - _mapinfo.cols * (blockIndex / 2) - (blockIndex / 2);
		for(uint32_t i = 0; i < blockIndex; i++) {
			for(uint32_t j = 0; j < blockIndex; j++) {
				uint32_t queryBlockId = startBlockId + (i * _mapinfo.cols) + j; 
				auto iter = _blocks.find(queryBlockId);
				if (iter == _blocks.end()) {
					continue;
				}
				assert(nullptr != iter->second.map);
				auto ret = iter->second.mesh.lock();
				if (nullptr == ret) {
					continue;
				}
				height = ret->get_road_height(x, y);
				if(height > 0) {
					break;
				}
				height = ret->get_road_height(x, y, 2500);
			}
		}

		blockIndex = blockIndex + 2;
		if(blockIndex > 12) {
			break;
		}
	} 
	while (height == 0.0);

	return height;
}

int tilecache_impl::find_junction_center_byappro(double& x, double& y, string appro_uuid)
{
	auto_mutex am(_mut);
	std::map<std::string, Vec2D>::iterator iter = _approachs.find(appro_uuid);
	if (iter != _approachs.end()) {
		printf("find approach.\n");
		x = iter->second[0];
		y = iter->second[1];
	} 
	else {
		printf("not find approach.\n");
		return -ENOTFOUND; // block not found
	}
	return 0;
}

tilecache::tilecache()
: _data(nullptr)
{
	auto* tc = new tilecache_impl();
	if (nullptr == tc) return;
	_data = reinterpret_cast<tilecache*>(tc);
}

tilecache::~tilecache()
{
	if (_data) {
		auto* tc = reinterpret_cast<tilecache_impl*>(_data);
		delete tc;
		_data = nullptr;
	}
}

int tilecache::add_tile(const void* buffer, size_t sz)
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* tc = reinterpret_cast<tilecache_impl*>(_data);
	return tc->add_tile(buffer, sz);
}

int tilecache::add_tile(const string& buffer)
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* tc = reinterpret_cast<tilecache_impl*>(_data);
	return tc->add_tile(buffer.data(), buffer.size());
}

shared_ptr<const odr::RoadNetworkMesh> tilecache::get_mesh(uint32_t blkid, bool* g)
{
	if (nullptr == _data) {
		return nullptr;
	}
	auto* tc = reinterpret_cast<tilecache_impl*>(_data);
	if (!tc->map_available()) {
		return nullptr;
	}
	return tc->get_mesh(blkid, g);
}

int tilecache::release_mesh(uint32_t blkid)
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* tc = reinterpret_cast<tilecache_impl*>(_data);
	if (!tc->map_available()) {
		return -EINVALID;
	}
	return tc->release_mesh(blkid);
}

bool tilecache::get_mapid(uint128_t& id) const
{
	if (nullptr == _data) {
		return false;
	}
	auto* tc = reinterpret_cast<tilecache_impl*>((void*)_data);
	return tc->get_mapid(id);
}

int tilecache::bind_map(const void* mapinfo, size_t sz)
{
	if (nullptr == _data) {
		return false;
	}
	auto* tc = reinterpret_cast<tilecache_impl*>(_data);
	return tc->bind_map(mapinfo, sz);
}

int tilecache::bind_map(const string& mapinfo)
{
	if (nullptr == _data) {
		return false;
	}
	auto* tc = reinterpret_cast<tilecache_impl*>(_data);
	return tc->bind_map(
		mapinfo.data(), mapinfo.size());
}

int tilecache::query_unloaded_blocks(const vector<point2d>& pts,
	set<uint32_t>& blkids) const
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* tc = reinterpret_cast<tilecache_impl*>((void*)_data);
	if (!tc->map_available()) {
		return -EINVALID;
	}
	return tc->query_unloaded_blocks(pts, blkids);
}

int tilecache::query_loaded_blocks(const vector<point2d>& pts,
	set<uint32_t>& blkids) const
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* tc = reinterpret_cast<tilecache_impl*>((void*)_data);
	if (!tc->map_available()) {
		return -EINVALID;
	}
	return tc->query_loaded_blocks(pts, blkids);
}

int tilecache::extract_roadobjects(const vector<point2d>& pts,
	const set<uint32_t>& categories,
	set<const rendermap_roadobject*>& objects) const
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* tc = reinterpret_cast<tilecache_impl*>((void*)_data);
	if (!tc->map_available()) {
		return -EINVALID;
	}
	return tc->extract_roadobjects(pts, categories, objects);
}

int tilecache::extract_roadobjects(int blkid,
	const set<uint32_t>& categories,
	set<const rendermap_roadobject*>& objects) const
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* tc = reinterpret_cast<tilecache_impl*>((void*)_data);
	if (!tc->map_available()) {
		return -EINVALID;
	}
	return tc->extract_roadobjects(blkid, categories, objects);
}

int tilecache::filter_loaded_blocks(set<uint32_t>& blkids, bool lock)
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* tc = reinterpret_cast<tilecache_impl*>(_data);
	return tc->filter_loaded_blocks(blkids, lock);
}

int tilecache::lock_blocks(const set<uint32_t>& blkids)
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* tc = reinterpret_cast<tilecache_impl*>(_data);
	return tc->lock_blocks(blkids);
}

int tilecache::unlock_blocks(const set<uint32_t>& blkids)
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* tc = reinterpret_cast<tilecache_impl*>(_data);
	return tc->unlock_blocks(blkids);
}

int tilecache::release_blocks(const set<uint32_t>& blkids)
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* tc = reinterpret_cast<tilecache_impl*>(_data);
	return tc->release_blocks(blkids);
}

const char* tilecache::get_projection(void) const
{
	if (nullptr == _data) {
		return nullptr;
	}
	auto* tc = reinterpret_cast<tilecache_impl*>(_data);
	return tc->get_map_georef();
}

const point3d& tilecache::get_mapoffset(void) const
{
	static point3d zero = {0, 0, 0};
	if (nullptr == _data) {
		return zero;
	}
	auto* tc = reinterpret_cast<tilecache_impl*>(_data);
	return tc->get_mapoffset();
}

}} // end of namespace zas::mapcore
/* EOF */
