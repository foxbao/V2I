/** @file rendermap.cpp
 */

#include <map>
#include "mapcore/sdmap.h"
#include "inc/rendermap.h"

#include "RefLine.h"
#include "Viewer/ViewerUtils.h"
#include "Viewer/RoadNetworkMesh.h"
#include "Viewer/ViewerUtils.h"

#include "inc/kdtree.h"

namespace zas {
namespace mapcore {

using namespace std;

void* trx_allocator::operator new(
	size_t sz, size_t extsz, size_t& pos, void** r) throw()
{
	sz += extsz;
	pos += sz;
	void* ret = malloc(sz);
	if (r) *r = ret;
	return ret;
}

void trx_allocator::operator delete(void* ptr, size_t)
{
	free(ptr);
}

void* trx_allocator::operator new[](size_t sz, size_t& pos, void** r) throw()
{
	pos += sz;
	void* ret = malloc(sz);
	if (r) *r = ret;
	return ret;
}

void trx_allocator::operator delete[](void* ptr, size_t)
{
	free(ptr);
}

int memory::bufnode::id_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(bufnode, id_avlnode, a);
	auto* bb = AVLNODE_ENTRY(bufnode, id_avlnode, b);
	if (aa->id > bb->id) { return 1; }
	else if (aa->id < bb->id) { return -1; }
	else return 0;
}

int memory::bufnode::addr_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(bufnode, addr_avlnode, a);
	auto* bb = AVLNODE_ENTRY(bufnode, addr_avlnode, b);
	if (aa->addr_start >= bb->addr_start
		&& aa->addr_end <= bb->addr_end) return 0;
	else if (aa->addr_start >= bb->addr_end) return 1;
	else if (aa->addr_end <= bb->addr_start) return -1;
	else assert(0);
}

int trx_lane_section_v1::add_referred_blockid(uint32_t blkid)
{
	if (nullptr == extinfo.referred_blkids) {
		extinfo.referred_blkids = new set<uint32_t>();
		if (nullptr == extinfo.referred_blkids) {
			return -1;
		}
	}
	extinfo.referred_blkids->insert(blkid);
	return 0;
}

int trx_road_object_v1::add_referred_blockid(uint32_t blkid)
{
	if (nullptr == extinfo.referred_blkids) {
		extinfo.referred_blkids = new set<uint32_t>();
		if (nullptr == extinfo.referred_blkids) {
			return -1;
		}
	}
	extinfo.referred_blkids->insert(blkid);
	return 0;
}

static void extract_mapheader(
	rendermap_v1* ret, hdrmap_file_rendermap_v1* rdmap)
{
	// magic and version
	memcpy(ret->magic, HDM_RENDER_MAGIC, sizeof(ret->magic));
	ret->version = HDM_RENDER_VERSION;
	ret->attrs = 0;

	ret->uuid = rdmap->uuid;

	ret->offset_fixlist = 0;
	ret->xoffset = rdmap->xoffset;
	ret->yoffset = rdmap->yoffset;
	ret->xmin = rdmap->xmin;
	ret->ymin = rdmap->ymin;
	ret->xmax = rdmap->xmax;
	ret->ymax = rdmap->ymax;

	ret->block_width = rdmap->block_width;
	ret->block_height = rdmap->block_height;

	ret->rows = rdmap->rows;
	ret->cols = rdmap->cols;
	strcpy(ret->proj, rdmap->proj);
}

memory::memory()
: _databuf_sz(0)
, _rendermap(nullptr)
, _bufnode_id_tree(nullptr)
, _bufnode_addr_tree(nullptr)
{
}

memory::~memory()
{
	for (;;) {
		avl_node_t* node = avl_first(_bufnode_id_tree);
		if (nullptr == node) break;
		auto* buf = AVLNODE_ENTRY(bufnode, id_avlnode, node);
		free(buf->buf);
		avl_remove(&_bufnode_id_tree, &buf->id_avlnode);
		delete buf;
	}
}

void* memory::get_addr(size_t _id)
{
	auto* ret = avl_find(_bufnode_id_tree,
		MAKE_FIND_OBJECT(_id, bufnode, id, id_avlnode),
		bufnode::id_compare);
	assert(nullptr != ret);

	return (void*)AVLNODE_ENTRY(bufnode, \
		id_avlnode, ret)->addr_start;
}

size_t memory::get_offset(void* addr)
{
	bufnode dummy;
	dummy.addr_start = (size_t)addr;
	dummy.addr_end =  (size_t)addr;
	auto* ret = avl_find(_bufnode_addr_tree,
		&dummy.addr_avlnode, bufnode::addr_compare);
	assert(nullptr != ret);

	auto* node = AVLNODE_ENTRY(bufnode, addr_avlnode, ret);

	assert((size_t)addr >= node->addr_start \
		&& (size_t)addr < node->addr_end);
	size_t delta = (size_t)addr - node->addr_start;
	return node->id + delta;
}

void memory::add_fixlist(offset* off, uint32_t* list)
{
	if (!list) {
		assert(nullptr != _rendermap);
		off->link = _rendermap->offset_fixlist;
		_rendermap->offset_fixlist = get_offset(off);
	}
	else {
		off->link = *list;
		*list = get_offset(off);
	}
}

void memory::serialize(string& ret)
{
	ret.clear();
	if (!_databuf_sz) {
		return;
	}

	ret.reserve(_databuf_sz);
	if (_rendermap) {
		_rendermap->size = _databuf_sz;
	}
	avl_node_t* node = avl_first(_bufnode_id_tree);
	while (node != nullptr) {
		auto* buf = AVLNODE_ENTRY(bufnode, id_avlnode, node);
		ret.append((char*)buf->addr_start,
			buf->addr_end - buf->addr_start);
		node = avl_next(node);
	}
}

template <typename T>
void* memory::allocate(uint32_t size, chunk<T>& chk)
{
	if (!size) return nullptr;

	auto* node = new bufnode();
	if (nullptr == node) {
		return nullptr;
	}

	node->id = _databuf_sz;
	node->buf = malloc(size);
	node->addr_start = (size_t)node->buf;
	if (!node->addr_start) {
		delete node; return nullptr;
	}
	node->addr_end = node->addr_start + size;

	int avl = avl_insert(&_bufnode_id_tree,
		&node->id_avlnode, bufnode::id_compare);
	assert(avl == 0);

	avl = avl_insert(&_bufnode_addr_tree,
		&node->addr_avlnode, bufnode::addr_compare);
	assert(avl == 0);

	assert(nullptr != _rendermap);
	chk.size = size;
	chk.off.offset = _databuf_sz;
	add_fixlist(&chk.off);
	_databuf_sz += size;

	return (void*)node->addr_start;
}

template <typename T>
offset* memory::allocate(int cnt, idxtbl_v1<T>& ret, bool link)
{
	if (!cnt) return nullptr;
	size_t pos = _databuf_sz;

	auto* node = new bufnode();
	if (nullptr == node) {
		return nullptr;
	}

	auto* tmp = new(_databuf_sz, &node->buf) trx_offset[cnt];
	if (nullptr == tmp) {
		_databuf_sz = pos; delete node;
		return nullptr;
	}

	auto buf = (size_t)tmp;

	assert(nullptr != _rendermap);
	ret.count = cnt;
	ret.indices_off.offset = pos;
	add_fixlist(&ret.indices_off);

	node->id = pos;
	node->addr_start = buf;
	node->addr_end = buf + _databuf_sz - pos;
	int avl = avl_insert(&_bufnode_id_tree,
		&node->id_avlnode, bufnode::id_compare);
	assert(avl == 0);

	avl = avl_insert(&_bufnode_addr_tree,
		&node->addr_avlnode, bufnode::addr_compare);
	assert(avl == 0);

	if (link == true) {
		// add all index node to the fix list
		auto *offs = (offset*)buf;
		for (int i = 0; i < cnt; ++i) {
			add_fixlist(&offs[i]);
		}
	}
	return (offset*)buf;
}

template <typename T>
T* memory::allocate(size_t* p, size_t extsz)
{
	size_t pos = _databuf_sz;

	auto* node = new bufnode();
	if (nullptr == node) {
		return nullptr;
	}

	T* ret = new(extsz, _databuf_sz, &node->buf) T();
	if (nullptr == ret) {
		_databuf_sz = pos; delete node;
		return nullptr;
	}

	if (p) *p = pos;
	
	node->id = pos;
	node->addr_start = (size_t)ret;
	node->addr_end = (size_t)ret + _databuf_sz - pos;
	int avl = avl_insert(&_bufnode_id_tree,
		&node->id_avlnode, bufnode::id_compare);
	assert(avl == 0);

	avl = avl_insert(&_bufnode_addr_tree,
		&node->addr_avlnode, bufnode::addr_compare);
	assert(avl == 0);

	return ret;
}

rendermap_impl::rendermap_impl()
: _refcnt(0)
, _active_blkcnt(0)
, _buffer(nullptr)
, _buffer_end(0)
, _flags(0)
{
	listnode_init(_ownerlist);
}

rendermap_impl::~rendermap_impl()
{
	release_all();	
}

void rendermap_impl::release_all(void)
{
	assert(listnode_isempty(_ownerlist));
	if (nullptr == _buffer) {
		return;
	}

	// release referred block ID set in lane sections
	auto* map = (rendermap_v1*)header();
	if (_f.valid && !map->a.persistence) {
		for (int i = 0; i < map->lanesecs.count; ++i) {
			auto* ls = map->lanesecs.indices[i];
			if (ls->extinfo.referred_blkids) {
				delete ls->extinfo.referred_blkids;
				ls->extinfo.referred_blkids = nullptr;
			}
		}

		for (int i = 0; i < map->roadobjs.count; ++i) {
			auto* roadobj = map->roadobjs.indices[i];
			if (roadobj->extinfo.referred_blkids) {
				delete roadobj->extinfo.referred_blkids;
				roadobj->extinfo.referred_blkids = nullptr;
			}
		}
	}

	delete [] _buffer;
	_buffer = nullptr;

	_wgs84prj.reset(nullptr, nullptr);
	_f.valid = 0;
}

int rendermap_impl::load_fromfile(const char* filename)
{
	if (!filename || !*filename) {
		return -ENOTFOUND;
	}

	if (nullptr != _buffer) {
		return -ENOTALLOWED;
	}

	// clean the old one and start
	// loading the new one
	reset();
	int ret = loadfile(filename);
	if (ret) {
		reset(); return ret;
	}
	if (check_validity()) {
		reset(); return -EINVALID;
	}
	if (fix_ref()) {
		reset(); return -EINVALID;
	}
	if (optimize_junctions()) {
		reset(); return -EINVALID;
	}
	if (finalize()) {
		reset(); return -EINVALID;
	}

	// set success
	_f.valid = 1;
	return 0;
}

int rendermap_impl::load_frombuffer(const void* buf, size_t sz)
{
	if (nullptr == buf) {
		return -EBADPARM;
	}
	if (sz < sizeof(hdrmap_file_rendermap_v1)) {
		return -EBADPARM;
	}

	// clean the old one and start
	// loading the new one
	reset();

	_buffer = new uint8_t [sz];
	if (nullptr == _buffer) {
		return -ENOMEMORY;
	}

	memcpy(_buffer, buf, sz);
	if (check_validity()) {
		reset(); return -EINVALID;
	}
	if (fix_ref()) {
		reset(); return -EINVALID;
	}
	if (optimize_junctions()) {
		reset(); return -EINVALID;
	}
	if (finalize()) {
		reset(); return -EINVALID;
	}

	// set success
	_f.valid = 1;
	return 0;
}

int rendermap_impl::finalize(void)
{
	// init the projection
	_wgs84prj.reset(GREF_WGS84, get_georef());
	return 0;
}

int rendermap_impl::check_validity(void)
{
	auto* hdr = header();
	if (memcmp(hdr->magic, HDM_RENDER_MAGIC, sizeof(hdr->magic))) {
		return -1;
	}
	// check version
	if (hdr->version > HDM_RENDER_VERSION) {
		return -2;
	}
	if (hdr->a.persistence) {
		// hdrmap_file_rendermap_v1
		_active_blkcnt = hdr->blocks.count;
		_buffer_end = ((size_t)hdr) + hdr->size;
	}
	else {
		// rendermap_v1
		auto* map = getmap();
		_active_blkcnt = map->blocks.count;
		_buffer_end = ((size_t)map) + map->size;
	}
	return 0;
}

int rendermap_impl::fix_ref(void)
{
	auto* hdr = header();
	uint32_t link = hdr->offset_fixlist;

	while (0 != link) {
		auto* offobj = (offset*)&_buffer[link];
		link = offobj->link;
		auto* fixobj = (uint64_t*)offobj;
		*fixobj = (uint64_t)&_buffer[offobj->offset];
	}
	return 0;
}

int rendermap_impl::optimize_junctions(void)
{
	auto* hdr = header();
	if (!hdr->a.persistence) {
		return 0;
	}
	hdrmap_file_junction_v1* tree = nullptr;
	if (build_kdtree(tree, hdr->junctions.indices.objects,
		hdr->junctions.count)) {
		return -1;
	}
	hdr->junc_kdtree = tree;
	return 0;
}

int rendermap_impl::loadfile(const char* filename)
{
	FILE* fp = fopen(filename, "rb");
	if (nullptr == fp) {
		return -EFAILTOLOAD;
	}

	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);
	if (sz < sizeof(hdrmap_file_rendermap_v1)) {
		fclose(fp); return -EINVALID;
	}

	// load the file to memory
	assert(nullptr == _buffer);
	_buffer = new uint8_t [sz];
	if (nullptr == _buffer) {
		fclose(fp); return -ENOMEMORY;
	}

	rewind(fp);
	if (sz != fread(_buffer, 1, sz, fp)) {
		fclose(fp); return -EINVALID;
	}

	fclose(fp);
	return 0;
}

int rendermap_impl::extract_rendermap_info(string& ret)
{
	auto* rdmap = header();
	size_t sz = sizeof(rendermap_v1) + ((strlen(rdmap->proj)
		+ (RMAP_ALIGN_BYTES) & ~(RMAP_ALIGN_BYTES - 1)));
	auto* hdr = (rendermap_v1*)alloca(sz);

	extract_mapheader(hdr, rdmap);

	// this is not a rendermap file
	hdr->a.persistence = 0;
	hdr->a.mapinfo_only = 1;

	ret.clear();
	ret.reserve(sz);
	ret.append((char*)hdr, sz);
	return 0;
}

int rendermap_impl::extract_blocks(const set<uint32_t>& blkset,
	const set<uint32_t>& shrblkset, string& ret)
{
	auto* hdr = header();
	// see if this is the full stored render map
	if (!hdr->a.persistence) {
		return -ENOTSUPPORT;
	}

	if (!blkset.size()) {
		// we only extract the rendermap database
		return extract_rendermap_info(ret);
	}

	// check if all the blkset is valid
	int total_blocks = hdr->cols * hdr->rows;
	for (auto& blkid : blkset) {
		if (blkid >= total_blocks) {
			return -EOUTOFOSCOPE;
		}
	}

	memory m;
	// start creating the download map
	auto* map = m.create_rendermap(hdr);
	if (nullptr == map) {
		return -ENOMEMORY;
	}

	set<size_t> trxblks;
	set<render_object<trx_roadseg_v1>> roadsegs;
	set<render_object<trx_lane_section_v1>> lanesecs;
	set<render_object<trx_road_object_v1>> roadobjs;
	set<render_object<trx_junction_v1>> junctions;

	for (auto blkid : blkset)
	{
		size_t pos;
		auto* blk = hdr->blocks.indices[blkid];
		auto* trxblk = m.allocate<trx_blockinfo_v1>(&pos, 0);
		if (nullptr == trxblk) {
			return -1;
		}
		trxblk->row = blkid / hdr->cols;
		trxblk->col = blkid % hdr->cols;
		trxblk->rid_list = 0;
		trxblk->lsid_list = 0;
		trxblk->objid_list = 0;

		if (extract_roads(m, blk, trxblk, shrblkset, roadsegs)) {
			return -2;
		}
		if (extract_junctions(m, blk, trxblk, junctions)) {
			return -3;
		}
		if (extract_lanesecs(m, blk, trxblk, shrblkset, lanesecs)) {
			return -4;
		}
		if (extract_roadobj(m, blk, trxblk, shrblkset, roadobjs)) {
			return -5;
		}
		trxblks.insert(pos);
	}

	// add all blocks (in order) to the rendermap
	auto* blkoffs = m.allocate<blockinfo_v1>(trxblks.size(), map->blocks);
	if (nullptr == blkoffs) {
		return -4;
	}
	int idx = 0;
	for (auto blk : trxblks) {
		blkoffs[idx++].offset = (uint32_t)blk;
	}

	// add all road segments (in order) to the rendermap
	auto* rsoffs = m.allocate<roadseg_v1>(roadsegs.size(),
		map->roadsegs);
	if (nullptr == rsoffs && roadsegs.size()) {
		return -5;
	}
	idx = 0; for (auto& rs : roadsegs) {
		rsoffs[idx++].offset = rs.pos;
	}

	// add all junctions (in order) to the rendermap
	auto* junoffs = m.allocate<junction_v1>(junctions.size(),
		map->junctions);
	if (nullptr == junoffs && junctions.size()) {
		return -6;
	}
	idx = 0; for (auto& j : junctions) {
		junoffs[idx++].offset = j.pos;
	}

	// add all lane sections (in order) to the rendermap
	auto* lsoffs = m.allocate<lane_section_v1>(
		lanesecs.size(), map->lanesecs);
	if (nullptr == lsoffs && lanesecs.size()) {
		return -7;
	}
	idx = 0; for (auto& ls : lanesecs) {
		lsoffs[idx++].offset = ls.pos;
	}

	auto* rboffs = m.allocate<road_object_v1>(
		roadobjs.size(), map->roadobjs);
	if (nullptr == rboffs && roadobjs.size()) {
		return -8;
	}
	idx = 0; for(auto& ro : roadobjs) {
		rboffs[idx++].offset = ro.pos;
	}

	// seralize all generated data to buffer
	m.serialize(ret);
	return 0;
}

int rendermap_impl::extract_roads(memory& m,
	hdrmap_file_blockinfo_v1* blk, trx_blockinfo_v1* trxblk,
	const set<uint32_t>& shared,
	set<render_object<trx_roadseg_v1>>& rss)
{
	if (!blk->road_segs.count) {
		return 0;
	}
	auto* blk_rsoffs = m.allocate<roadseg_v1>(
		blk->road_segs.count,
		trxblk->road_segs, false);
	if (nullptr == blk_rsoffs) {
		return -1;
	}

	for (int i = 0; i < blk->road_segs.count; ++i)
	{
		auto* roadseg = blk->road_segs.indices[i];

		if (shared.size() && road_exists_in_shared_blocks(
			m, shared, roadseg->extinfo.id)) {
			// it is already loaded in front-end
			// just add it to blocks (in ID form)
			
			blk_rsoffs[i].offset = roadseg->extinfo.id;
			m.add_fixlist(&blk_rsoffs[i], &trxblk->rid_list);
		}
		else {
			size_t pos;
			auto* r = m.allocate<trx_roadseg_v1>(&pos, 0);
			if (nullptr == r) {
				return -1;
			}
			if (extract_road(m, r, roadseg)) {
				return -2;
			}
			blk_rsoffs[i].offset = pos;
			m.add_fixlist(&blk_rsoffs[i]);	
			rss.insert({pos, r});
		}
	}
	return 0;
}

int rendermap_impl::extract_road(memory& m,
	roadseg_v1* r, hdrmap_file_roadseg_v1* roadseg)
{
	r->extinfo.id = roadseg->extinfo.id;
	r->extinfo.length = roadseg->extinfo.length;
	
	// copy the reference line object
	size_t refl_pos;
	auto* refl = m.allocate<trx_reference_line_v1>(&refl_pos,
		roadseg->refline.ptr->size_inbytes);

	if (nullptr == refl) {
		return -1;
	}

	refl->extinfo.id = roadseg->refline.ptr->extinfo.id;
	refl->size_inbytes = roadseg->refline.ptr->size_inbytes;
	memcpy(refl->sdata, roadseg->refline.ptr->sdata,
		roadseg->refline.ptr->size_inbytes);

	// add to link
	r->refline.off.offset = refl_pos;
	m.add_fixlist(&r->refline.off);

	size_t superelev_pos;
	auto* superelev = m.allocate<trx_super_elevation_v1>(&superelev_pos,
		roadseg->superelev.ptr->size_inbytes);
	if (nullptr == superelev) {
		return -2;
	}

	superelev->extinfo.id = roadseg->superelev.ptr->extinfo.id;
	superelev->size_inbytes = roadseg->superelev.ptr->size_inbytes;
	memcpy(superelev->sdata, roadseg->superelev.ptr->sdata,
		roadseg->superelev.ptr->size_inbytes);
	r->superelev.off.offset = superelev_pos;
	m.add_fixlist(&r->superelev.off); 
	
	return 0;
}

bool rendermap_impl::road_exists_in_shared_blocks(
	memory& m, const set<uint32_t>& shared, uint32_t rid)
{
	auto* map = header();
	int blkcnt = map->cols * map->rows;
	for (auto blkid : shared) {
		if (blkid >= blkcnt) {
			continue;
		}
		auto* blk = map->blocks.indices[blkid];
		if (find<hdrmap_file_roadseg_v1>(blk->road_segs, rid) >= 0) {
			return true;
		}
	}
	return false;
}

bool rendermap_impl::roadobj_exists_in_shared_blocks(memory& m,
	const set<uint32_t>& shared, uint32_t oid)
{
	auto* map = header();
	int blkcnt = map->cols * map->rows;
	for (auto blkid : shared) {
		if (blkid >= blkcnt) {
			continue;
		}
		auto* blk = map->blocks.indices[blkid];
		if (find<hdrmap_file_road_object_v1>(blk->road_objs, oid) >= 0) {
			return true;
		}
	}
	return false;
}

bool rendermap_impl::lanesec_exists_in_shared_blocks(memory& m,
	const set<uint32_t>& shared, uint32_t lid)
{
	auto* map = header();
	int blkcnt = map->cols * map->rows;
	for (auto blkid : shared) {
		if (blkid >= blkcnt) {
			continue;
		}
		auto* blk = map->blocks.indices[blkid];
		if (find<hdrmap_file_lanesec_v1>(blk->lane_secs, lid) >= 0) {
			return true;
		}
	}
	return false;
}

int rendermap_impl::extract_roadobj(memory& m, hdrmap_file_blockinfo_v1* blk,
	trx_blockinfo_v1* trxblk, const set<uint32_t>& shared,
	set<render_object<trx_road_object_v1>>& roadobjs)
{
	if (!blk->road_objs.count) {
		return 0;
	}
	auto* blk_lsoffs = m.allocate<road_object_v1>(blk->road_objs.count,
		trxblk->road_objs, false);
	if (nullptr == blk_lsoffs) {
		return -1;
	}

	for (int i = 0; i < blk->road_objs.count; ++i)
	{
		auto* ro = blk->road_objs.indices[i];
		if (roadobj_exists_in_shared_blocks(m, shared, ro->extinfo.id)) {
			// it is already loaded in front-end
			// just add it to blocks (in ID form)
			blk_lsoffs[i].offset = ro->extinfo.id;
			m.add_fixlist(&blk_lsoffs[i], &trxblk->objid_list);
		}
		else {
			size_t pos;
			auto* obj = m.allocate<trx_road_object_v1>(&pos, ro->size_inbytes);
			if (nullptr == obj) {
				return -1;
			}
			if (extract_roadobj_extinfo(m, obj, ro)) {
				return -2;
			}
			if (ro->size_inbytes > 0) {
				memcpy(obj->sdata, ro->sdata, ro->size_inbytes);
			}
			blk_lsoffs[i].offset = pos;
			m.add_fixlist(&blk_lsoffs[i]);
			roadobjs.insert({pos, obj});
		}
	}
	return 0;
}

int rendermap_impl::extract_lanesecs(memory& m, hdrmap_file_blockinfo_v1* blk,
	trx_blockinfo_v1* trxblk, const set<uint32_t>& shared,
	set<render_object<trx_lane_section_v1>>& lanesecs)
{
	if (!blk->lane_secs.count) {
		return 0;
	}
	auto* blk_lsoffs = m.allocate<lane_section_v1>(blk->lane_secs.count,
		trxblk->lane_secs, false);
	if (nullptr == blk_lsoffs) {
		return -1;
	}

	for (int i = 0; i < blk->lane_secs.count; ++i)
	{
		auto* ls = blk->lane_secs.indices[i];
		if (shared.size() && lanesec_exists_in_shared_blocks(
			m, shared, ls->extinfo.id)) {
			// it is already loaded in front-end
			// just add it to blocks (in ID form)
			blk_lsoffs[i].offset = ls->extinfo.id;
			m.add_fixlist(&blk_lsoffs[i], &trxblk->lsid_list);
		}
		else {
			size_t pos;
			auto* l = m.allocate<trx_lane_section_v1>(&pos, ls->size_inbytes);
			if (nullptr == l) {
				return -1;
			}
			if (extract_lanesec_extinfo(m, l, ls)) {
				return -2;
			}
			memcpy(l->sdata, ls->sdata, ls->size_inbytes);

			blk_lsoffs[i].offset = pos;
			m.add_fixlist(&blk_lsoffs[i]);
			lanesecs.insert({pos, l});
		}
	}
	return 0;
}

int rendermap_impl::extract_junctions(memory& m, hdrmap_file_blockinfo_v1* blk,
	trx_blockinfo_v1* trxblk, set<render_object<trx_junction_v1>>& juncs)
{
	if (!blk->junctions.count) {
		return 0;
	}
	auto* blk_junoffs = m.allocate<junction_v1>(blk->junctions.count,
		trxblk->junctions);
	if (nullptr == blk_junoffs) {
		return -1;
	}

	for (int i = 0; i < blk->junctions.count; ++i)
	{
		size_t pos;
		auto* junc = blk->junctions.indices[i];
		auto* j = m.allocate<trx_junction_v1>(&pos, 0);
		if (nullptr == j) {
			return -1;
		}
		if (extract_junction_info(m, j, junc)) {
				return -2;
		}
		blk_junoffs[i].offset = pos;
		juncs.insert({pos, j});
	}
	return 0;
}

int rendermap_impl::extract_junction_info(memory& m, junction_v1* to,
	hdrmap_file_junction_v1* from)
{
	to->extinfo.id = from->extinfo.id;
	to->center_x = from->center_x;
	to->center_y = from->center_y;
	to->radius = from->radius;

	if(from->approachs.count > 0) {
		auto* junc_approach = m.allocate<approach_info_v1>(from->approachs.count,
			to->approachs);
		if (nullptr == junc_approach) {
			return -1;
		}
		
		for (int i = 0; i < from->approachs.count; ++i)
		{
			size_t pos;
			auto* appr = from->approachs.indices[i];
			auto* j = m.allocate<trx_approach_v1>(&pos, appr->uid_sz);
			if (nullptr == j) {
				return -1;
			}
			j->uid_sz = appr->uid_sz;
			memcpy(j->data, appr->data, appr->uid_sz);
			junc_approach[i].offset = pos;
		}
	}

	return 0;
}

int rendermap_impl::extract_lanesec_extinfo(memory& m,
	lane_section_v1* to,
	hdrmap_file_lanesec_v1* from)
{
	to->size_inbytes = from->size_inbytes;
	to->extinfo.id = from->extinfo.id;
	to->extinfo.roadid = from->extinfo.roadid;
	to->extinfo.meshgen_blockid = -1;
	to->extinfo.referred_blkids = nullptr;
	return 0;
}

int rendermap_impl::extract_roadobj_extinfo(memory& m, 
	road_object_v1* to, hdrmap_file_road_object_v1* from)
{
	to->size_inbytes = from->size_inbytes;
	to->extinfo.id = from->extinfo.id;
	to->extinfo.roid = from->extinfo.roid;
	if (from->type != objtype_road_object) {
		// there is still an object in extinfo.id union, extract it
		int ret = extract_classifed_roadobject_extension(m, to, from);
		if (ret) return ret;
	}

	to->extinfo.roadid = from->extinfo.roadid;
	to->extinfo.meshgen_blockid = -1;
	to->extinfo.referred_blkids = nullptr;
	return 0;
}

int rendermap_impl::extract_classifed_roadobject_extension(memory& m, 
	road_object_v1* to, hdrmap_file_road_object_v1* from)
{
	size_t pos = 0;
	void* buffer;

	switch (from->type) {
	case objtype_road_object_signal_light:
		buffer = (void*)m.allocate<trx_siglight_info_v1>(&pos);
		if (nullptr == buffer) return -2;
		memcpy(buffer, from->extinfo.siginfo.ptr, sizeof(siglight_info_v1));
		break;

	case objtype_road_object_rsd:
		buffer = (void*)m.allocate<trx_roadside_device_info_v1>(&pos);
		if (nullptr == buffer) return -3;
		memcpy(buffer, from->extinfo.rsdinfo.ptr, sizeof(roadside_device_info_v1));
		break;

	case objtype_road_object_arrow:
		buffer = (void*)m.allocate<trx_arrow_info_v1>(&pos);
		if (nullptr == buffer) return -4;
		memcpy(buffer, from->extinfo.arrowinfo.ptr, sizeof(arrow_info_v1));
		break;

	case objtype_road_object_sigboard:
		buffer = (void*)m.allocate<trx_sigboard_info_v1>(&pos);
		if (nullptr == buffer) return -4;
		memcpy(buffer, from->extinfo.sigboardinfo.ptr, sizeof(sigboard_info_v1));
		break;

	case objtype_road_object_cared: break;
	case objtype_road_object_junconver: break;
	default: return -1;
	}

	to->type = from->type;
	if (pos) {
		to->extinfo.siginfo.off.offset = pos;
		m.add_fixlist(&to->extinfo.siginfo.off);
	}
	return 0;
}

rendermap_v1* memory::create_rendermap(hdrmap_file_rendermap_v1* rdmap)
{
	assert(nullptr == _rendermap);

	// calculate the proj string size
	size_t extsz = strlen(rdmap->proj) + 1;
	size_t tmp = sizeof(hdrmap_file_rendermap_v1) + extsz;
	tmp = (tmp + (RMAP_ALIGN_BYTES - 1)) & ~(RMAP_ALIGN_BYTES - 1);
	extsz = tmp - sizeof(hdrmap_file_rendermap_v1);

	auto* ret = allocate<trx_rendermap_v1>(nullptr, extsz);
	if (nullptr == ret) {
		return nullptr;
	}

	extract_mapheader(ret, rdmap);

	// means this is the downloading rendermap format
	// not a <server> stored rendermap
	ret->a.persistence = 0;

	_rendermap = ret;
	return ret;
}

int rendermap_impl::get_block_dependencies(
	const set<uint32_t>& input, set<uint32_t>& output)
{
	output.clear();
	if (!input.size()) {
		return 0;
	}

	for (auto& blkid : input) {
		if (!blkid) continue;
		if (lanesec_dependencies(blkid, output)) {
			return -1;
		}
	}

	for (auto& blkid : input) {
		if (!blkid) continue;
		if(roadobj_dependencies(blkid, output)) {
			return -2;
		}
	}
	return 0;
}

int rendermap_impl::roadobj_dependencies(uint32_t blkid, set<uint32_t>& output)
{
	auto* hdr = header();
	auto* blk = hdr->blocks.indices[blkid];
	for (int i = 0; i < blk->road_objs.count; ++i) {
		auto* roadobj = blk->road_objs.indices[i];
		int cnt = roadobj->extinfo.referred_blockids.size;
		cnt /= sizeof(uint32_t);
		auto* idbuf = roadobj->extinfo.referred_blockids.ptr;
		for (int j = 0; j < cnt; ++j) {
			uint32_t id = idbuf[j];
			if (blkid != id) output.insert(id);
		}
	}
	return 0;
}

int rendermap_impl::lanesec_dependencies(uint32_t blkid, set<uint32_t>& output)
{
	auto* hdr = header();
	auto* blk = hdr->blocks.indices[blkid];
	for (int i = 0; i < blk->lane_secs.count; ++i) {
		auto* lanesec = blk->lane_secs.indices[i];
		int cnt = lanesec->extinfo.referred_blockids.size;
		cnt /= sizeof(uint32_t);
		auto* idbuf = lanesec->extinfo.referred_blockids.ptr;
		for (int j = 0; j < cnt; ++j) {
			uint32_t id = idbuf[j];
			if (blkid != id) output.insert(id);
		}
	}
	return 0;
}

blockinfo_v1* rendermap_impl::getblock(int blkid)
{
	auto* map = getmap();
	if (map->a.persistence) {
		auto hdr = header();
		if (blkid >= hdr->blocks.count) {
			return nullptr;
		}
		return (blockinfo_v1*)hdr->blocks.indices[blkid];
	}

	int mid;
	int start = 0;
	assert(map->blocks.count != 0);
	int end = map->blocks.count - 1;

	// find the item
	while (start <= end)
	{
		mid = (start + end) / 2;
		auto* blk = map->blocks.indices[mid];
		uint32_t val = blk->row * map->cols + blk->col;

		if (blkid == val) return blk;
		else if (blkid > val) start = mid + 1;
		else end = mid - 1;
	}
	// error
	return nullptr;
}

const shared_ptr<odr::RoadNetworkMesh> rendermap_impl::generate_mesh(uint32_t blkid)
{
	// this method only applied for memory rendermap
	if (getmap()->a.persistence) {
		return nullptr;
	}

	auto* blk = getblock(blkid);
	if (nullptr == blk) {
		return nullptr;
	}

	OpenDriveMap odrm;
	map<uint32_t, shared_ptr<Road>> roads;

	// handle all lane sections
	int ret = deserialize_block_lanesecs(blk, odrm, roads);
	if (ret) {
		return nullptr;
	}

	ret = deserialize_block_roadobjs(blk, odrm, roads);
	if (ret) {
		return nullptr;
	}
	// finally we update the mesh in meshbuf
	_meshbuf.erase(blkid);

	// generate the 3D mesh for OpenDriveMap
	auto mesh = make_shared<odr::RoadNetworkMesh>(get_road_network_mesh(odrm, 0.1));
	mesh->refline = get_refline_segments(odrm, 0.1);
	_meshbuf.insert({blkid, mesh});
	return mesh;
}

int rendermap_impl::deserialize_block_roadobjs(blockinfo_v1* blk,
	OpenDriveMap& odrm, map<uint32_t, shared_ptr<Road>>& r)
{
	auto* map = getmap();
	for (int i = 0; i < blk->road_objs.count; ++i) {
		auto* ro = blk->road_objs.indices[i];
		if (hdrm_robjtype_unclassfied == ro->type) {
			continue;
		}
		int64_t blkid = ro->extinfo.meshgen_blockid;
		int cur_blkid = blk->row * map->cols + blk->col;

		// add myself as a (mesh-gen) referred blockid
		if (zas_downcast(road_object_v1,
			trx_road_object_v1, ro)->add_referred_blockid(cur_blkid)) {
			return -1;
		}

		// check if we need to render this road object
		if (blkid < 0) {
			// there is no owner for this block
			// make us be the owner
			ro->extinfo.meshgen_blockid = cur_blkid;
		}
		else if (blkid != cur_blkid) {
			continue;
		}

		// I'm the one to generate mesh for this road object
		auto roadseg = get_block_odrv_road(blk, ro->extinfo.roadid, odrm, r);
		if (nullptr == roadseg) {
			return -2;
		}

		if (ro->size_inbytes) {
			void* buf = (void*)ro->sdata;
			shared_ptr<RoadObject> roadobj = make_shared<RoadObject>(&buf);
			roadobj->road = roadseg;
			roadseg->id_to_object.insert(std::map<std::string,
				std::shared_ptr<RoadObject>>::value_type(roadobj->id, roadobj));
		}
	}
	return 0;
}

int rendermap_impl::deserialize_block_lanesecs(blockinfo_v1* blk,
	OpenDriveMap& odrm, map<uint32_t, shared_ptr<Road>>& r)
{
	auto* map = getmap();
	for (int i = 0; i < blk->lane_secs.count; ++i) {
		auto* ls = blk->lane_secs.indices[i];
		int64_t blkid = ls->extinfo.meshgen_blockid;
		int cur_blkid = blk->row * map->cols + blk->col;

		// add myself as a (mesh-gen) referred blockid
		if (zas_downcast(lane_section_v1,
			trx_lane_section_v1, ls)->add_referred_blockid(cur_blkid)) {
				return -1;
		}

		// check if we need to render this lane section
		if (blkid < 0) {
			// there is no owner for this block
			// make us be the owner
			ls->extinfo.meshgen_blockid = cur_blkid;
		}
		else if (blkid != cur_blkid) {
			continue;
		}
		// I'm the one to generate mesh for this lane section
		auto roadseg = get_block_odrv_road(blk, ls->extinfo.roadid, odrm, r);
		if (nullptr == roadseg) {
			return -2;
		}

		assert(ls->size_inbytes != 0);
		void* buf = (void*)ls->sdata;
		auto lanesec = make_shared<LaneSection>(&buf);
		if (nullptr == lanesec) {
			return -3;
		}
		lanesec->road = roadseg;
		for (auto lane_iter : lanesec->id_to_lane) {
			lane_iter.second->road = roadseg;
			lane_iter.second->lane_section = lanesec;
		}
		roadseg->s_to_lanesection.insert(std::map<double,
			std::shared_ptr<LaneSection>>::value_type(lanesec->s0, lanesec));
	}
	return 0;
}

shared_ptr<Road> rendermap_impl::get_block_odrv_road(
	blockinfo_v1* blk, uint32_t roadid, OpenDriveMap& odrm,
	map<uint32_t, shared_ptr<Road>>& roads)
{
	// we already have the odr-road?
	auto rs = roads.find(roadid);
	if (rs != roads.end()) {
		return rs->second;
	}

	// generate the roadseg
	int ridx = find<roadseg_v1>(blk->road_segs, roadid);
	if (ridx < 0) {
		return nullptr;
	}
	auto* r = blk->road_segs.indices[ridx];
	auto road = make_shared<Road>();
	if (nullptr == road) {
		return nullptr;
	}
	roads.insert({roadid, road});

	// deserialize the road
	void* buf = (void*)r->refline.ptr->sdata;
	road->ref_line = make_shared<RefLine>(&buf);
	road->superelevation.deserialize(r->superelev.ptr->sdata);

	// add the road to OpenDriveMap
	string idstr;
	ultoa(roadid, idstr);
	road->id = idstr;
	road->length = r->extinfo.length;
	odrm.roads[idstr] = road;
	return road;
}

junction_v1* rendermap_impl::get_junction(uint32_t juncid)
{
	junction_v1* junc;
	auto hdr = header();
	if (hdr->a.persistence) {
		if (!hdr->junctions.count) {
			return nullptr;
		}
		int ret = find(hdr->junctions, juncid);
		if (ret < 0) {
			return nullptr;
		}
		junc = (junction_v1*)hdr->junctions.indices[ret];
	}
	else {
		auto map = getmap();
		if (!map->junctions.count) {
			return nullptr;
		}
		int ret = find(map->junctions, juncid);
		if (ret < 0) {
			return nullptr;
		}
		junc = map->junctions.indices[ret];
	}
	assert(nullptr != junc);
	return junc;
}

junction_v1* rendermap_impl::get_junction_by_index(uint32_t index)
{
	junction_v1* ret;
	auto hdr = header();
	if (hdr->a.persistence) {
		if (index >= hdr->junctions.count) {
			return nullptr;
		}
		ret = (junction_v1*)hdr->junctions.indices[index];
	}
	else {
		auto map = getmap();
		if (index >= map->junctions.count) {
			return nullptr;
		}
		ret = map->junctions.indices[index];
	}
	return ret;
}


int rendermap_impl::find_junctions(double x, double y, int count,
	vector<rendermap_nearest_junctions>& set)
{
	if (!count) {
		return -EBADPARM;
	}

	auto* hdr = header();
	if (!hdr->a.persistence) {
		return -ENOTSUPPORT;
	}

	if (!hdr->junctions.count) {
		return -ENOTFOUND;
	}
	if (count > hdr->junctions.count) {
		count = hdr->junctions.count;
	}

	if (x < hdr->xmin || x > hdr->xmax || y < hdr->ymin || y > hdr->ymax) {
		return -ENOTFOUND;
	}

	for (int i = 0; i < count; ++i) {
		rendermap_nearest_junctions item;
		auto* junc = kdtree2d_search(hdr->junc_kdtree,
			x, y, item.distance);
		item.junction = reinterpret_cast<rendermap_junction*>(junc);
		set.push_back(item);
	}

	// restore "selected" flags
	for (auto& item : set) {
		if (nullptr == item.junction) {
			continue;
		}
		auto* junc = reinterpret_cast<hdrmap_file_junction_v1*>
			(item.junction);
		junc->attrs.KNN_selected = 0;
	}
	return count;
}

template <typename T>
static void extract_roadobjects_from_block(T* block,
	const set<uint32_t>& categories,
	set<const rendermap_roadobject*>& objects)
{
	for (int i = 0; i < block->road_objs.count; ++i) {
		auto ro = block->road_objs.indices[i];
		if (ro->type == hdrm_robjtype_unclassfied) {
			continue; // todo: check details
		}
		if (categories.find(ro->type) == categories.end()) {
			continue; // not found
		}
		auto res = reinterpret_cast<const rendermap_roadobject*>(ro);
		objects.insert(res);
	}
}

int rendermap_impl::extract_roadobjects(int blkid,
	const set<uint32_t>& categories,
	set<const rendermap_roadobject*>& objects, bool reset)
{
	if (!categories.size()) {
		return -EINVALID;
	}
	if (!check_classified_type_validty(categories)) {
		return -EINVALID;
	}

	auto block = getblock(blkid);
	if (nullptr == block) {
		return -ENOTFOUND;
	}
	
	if (reset) objects.clear();
	if (getmap()->a.persistence) {
		extract_roadobjects_from_block((hdrmap_file_blockinfo_v1*)block,
			categories, objects);
	}
	else {
		extract_roadobjects_from_block(block, categories, objects);
	}
	return 0;
}

bool rendermap_impl::check_classified_type_validty(const set<uint32_t>& categories)
{
	for (auto cat : categories) {
		if (cat <= hdrm_robjtype_unknown) {
			return false;
		}
		if (cat >= hdrm_robjtype_max) {
			return false;
		}
	}
	return true;
}

const char* rendermap_impl::get_georef(void)
{
	auto hdr = header();
	if (hdr->a.persistence) {
		return (*hdr->proj) ? hdr->proj : nullptr;
	}
	else {
		auto map = getmap();
		return (*map->proj) ? map->proj : nullptr;
	}
	// shall never be here
}

int rendermap_impl::get_offset(double& dx, double& dy)
{
	auto hdr = header();
	if (hdr->a.persistence) {
		dx = hdr->xoffset;
		dy = hdr->yoffset;
	}
	else {
		auto map = getmap();
		dx = map->xoffset;
		dy = map->yoffset;
	}
	return 0;
}

template <typename T>
static void extract_mapinfo(T* map, rendermap_info& mapinfo)
{
	mapinfo.uuid = map->uuid;
	mapinfo.rows = map->rows;
	mapinfo.cols = map->cols;
	mapinfo.junction_count = map->junctions.count;
	mapinfo.xmin = map->xmin;
	mapinfo.ymin = map->ymin;
	mapinfo.xmax = map->xmax;
	mapinfo.ymax = map->ymax;
}

int rendermap_impl::get_mapinfo(rendermap_info& mapinfo)
{
	auto hdr = header();
	if (hdr->a.persistence) {
		extract_mapinfo(hdr, mapinfo);
	} else {
		auto map = getmap();
		extract_mapinfo(map, mapinfo);
	}
	return 0;
}

int rendermap_impl::addr_avlcompare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(rendermap_impl, _avlnode, a);
	auto* bb = AVLNODE_ENTRY(rendermap_impl, _avlnode, b);

	size_t aa_start = (size_t)aa->_buffer;
	size_t bb_start = (size_t)bb->_buffer;

	if (aa_start >= bb_start &&
		aa->_buffer_end <= bb->_buffer_end) return 0;
	else if (aa_start >= bb->_buffer_end) return 1;
	else if (aa->_buffer_end <= bb_start) return -1;
	else assert(0);
}

rendermap::rendermap()
: _data(nullptr)
{
	auto* map = new rendermap_impl();
	if (nullptr == map) return;
	_data = reinterpret_cast<void*>(map);
}

rendermap::~rendermap()
{
	if (_data) {
		auto* map = reinterpret_cast<rendermap_impl*>(_data);
		delete map;
		_data = nullptr;
	}
}

int rendermap::load_fromfile(const char* filename)
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* map = reinterpret_cast<rendermap_impl*>(_data);
	return map->load_fromfile(filename);
}

int rendermap::extract_blocks(const set<uint32_t>& blkset,
	const set<uint32_t>& shrblkset, string& ret) const
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* map = reinterpret_cast<rendermap_impl*>(_data);
	if (!map->valid()) {
		return -EINVALID;
	}
	return map->extract_blocks(blkset, shrblkset, ret);
}

int rendermap::get_block_dependencies(
	const set<uint32_t>& input, set<uint32_t>& output) const
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* map = reinterpret_cast<rendermap_impl*>(_data);
	if (!map->valid()) {
		return -EINVALID;
	}
	return map->get_block_dependencies(input, output);
}

int rendermap::load_frombuffer(const void* buffer, size_t sz)
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* map = reinterpret_cast<rendermap_impl*>(_data);
	if (!map->valid()) {
		return -EINVALID;
	}
	return map->load_frombuffer(buffer, sz);
}

int rendermap::load_frombuffer(const string& buffer)
{
	return load_frombuffer(buffer.data(), buffer.size());
}

int rendermap::extract_roadobjects(int blkid,
	const set<uint32_t>& categories,
	set<const rendermap_roadobject*>& objects, bool reset) const
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* map = reinterpret_cast<rendermap_impl*>(_data);
	if (!map->valid()) {
		return -EINVALID;
	}
	return map->extract_roadobjects(blkid, categories, objects, reset);
}

int rendermap::find_junctions(double x, double y, int count,
	vector<rendermap_nearest_junctions>& set) const
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* map = reinterpret_cast<rendermap_impl*>(_data);
	if (!map->valid()) {
		return -EINVALID;
	}
	return map->find_junctions(x, y, count, set);
}

const char* rendermap::get_georef(void) const
{
	if (nullptr == _data) {
		return nullptr;
	}
	auto* map = reinterpret_cast<rendermap_impl*>(_data);
	if (!map->valid()) {
		return nullptr;
	}
	return map->get_georef();
}

int rendermap::get_offset(double& dx, double& dy) const
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* map = reinterpret_cast<rendermap_impl*>(_data);
	if (!map->valid()) {
		return -EINVALID;
	}
	return map->get_offset(dx, dy);
}

int rendermap::get_mapinfo(rendermap_info& mapinfo) const
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* map = reinterpret_cast<rendermap_impl*>(_data);
	if (!map->valid()) {
		return -EINVALID;
	}
	return map->get_mapinfo(mapinfo);
}

const rendermap_junction* rendermap::get_junction(uint32_t juncid) const
{
	if (nullptr == _data) {
		return nullptr;
	}
	auto* map = reinterpret_cast<rendermap_impl*>(_data);
	if (!map->valid()) {
		return nullptr;
	}
	return reinterpret_cast<const rendermap_junction*>(map->get_junction(juncid));
}

const rendermap_junction* rendermap::get_junction_by_index(uint32_t idx) const
{
	if (nullptr == _data) {
		return nullptr;
	}
	auto* map = reinterpret_cast<rendermap_impl*>(_data);
	if (!map->valid()) {
		return nullptr;
	}
	return reinterpret_cast
		<const rendermap_junction*>
		(map->get_junction_by_index(idx));
}


int rendermap_junction::center_point(double& x, double& y, double& radius) const
{
	auto* junction = reinterpret_cast<const junction_v1*>(this);
	if (nullptr == junction) {
		return -EINVALID;
	}
	x = junction->center_x;
	y = junction->center_y;
	radius = junction->radius;
	return 0;
}

uint32_t rendermap_junction::getid(void) const
{
	auto* junction = reinterpret_cast<const junction_v1*>(this);
	if (nullptr == junction) {
		return 0;
	}
	return junction->extinfo.id;
}

int rendermap_junction::extract_desc(const sdmap* smap,
	const rendermap* rmap, vector<const char*>& names) const
{
	if (!smap || !rmap || !smap->valid()) {
		return -EBADPARM;
	}

	auto* junc = reinterpret_cast<const junction_v1*>(this);
	if (nullptr == junc) {
		return -EINVALID;
	}
	
	// get the wgs84 projection
	auto* rmapimpl = reinterpret_cast<rendermap_impl*>((void*)rmap->_data);
	if (nullptr == rmapimpl) { return -EINVALID; }
	auto& prj = rmapimpl->get_wgs84proj();
	if (!prj.valid()) { return -EINVALID; }

	// convert the junction center point to WGS84
	double dx, dy;
	rmap->get_offset(dx, dy);
	point3d pt = {junc->center_x + dx, junc->center_y + dy};
	prj.inv_transform_point(pt);
	
	// find nearest nodes from the SDMap
	sdmap_nearest_node nodes[JUNC_SDMAP_NODES_MAX_SEARCH];
	if (smap->search_nearest_nodes(pt.llh.lat, pt.llh.lon,
		JUNC_SDMAP_NODES_MAX_SEARCH, nodes)) {
		return -ENOTFOUND;
	}

	// find the nearest node that is a key node and
	// has 2 intersection road name
	vector<const char*> tmp_names;
	for (int i = 0; i < JUNC_SDMAP_NODES_MAX_SEARCH; ++i) {
		assert(nullptr != nodes[i].node);
		tmp_names.clear();
		if (!nodes[i].node->get_intersection_roads_name(tmp_names)
			|| !tmp_names.size()) {
			continue;
		}
		if (tmp_names.size() >= 2) {
			names = tmp_names;
			return 0;
		}
		else if (!names.size()) {
			names = tmp_names;
		}
	}
	// in worst cases, the "names" will be empty
	return 0;
}

hdrmap_roadobject_type rendermap_roadobject::gettype(void) const
{
	if (nullptr == this) {
		return hdrm_robjtype_unknown;
	}
	auto ro = reinterpret_cast<const road_object_v1*>(this);
	if (ro->type < hdrm_robjtype_unknown
		|| ro->type > hdrm_robjtype_max) {
		return hdrm_robjtype_unknown;
	}
	return (hdrmap_roadobject_type)ro->type;
}

const hdrmap_ro_signal_light* rendermap_roadobject::as_siglight(void) const
{
	if (nullptr == this) {
		return nullptr;
	}
	auto ro = reinterpret_cast<const road_object_v1*>(this);
	if (ro->type != hdrm_robjtype_signal_light) {
		return nullptr;
	}
	return reinterpret_cast
		<const hdrmap_ro_signal_light*>(ro->extinfo.siginfo.ptr);
}

const hdrmap_ro_roadside_device* rendermap_roadobject::as_rsd(void) const
{
	if (nullptr == this) {
		return nullptr;
	}
	auto ro = reinterpret_cast<const road_object_v1*>(this);
	if (ro->type != hdrm_robjtype_rsd) {
		return nullptr;
	}
	return reinterpret_cast
		<const hdrmap_ro_roadside_device*>(ro->extinfo.rsdinfo.ptr);
}

uint64_t hdrmap_ro_roadside_device::get_spotid(void) const
{
	if (nullptr == this) {
		return 0;
	}
	auto rsdinfo = reinterpret_cast<const roadside_device_info_v1*>(this);
	return rsdinfo->spotid;
}

uint64_t hdrmap_ro_signal_light::getuid(void) const
{
	if (nullptr == this) {
		return 0;
	}
	auto siginfo = reinterpret_cast<const siglight_info_v1*>(this);
	return siginfo->siguid;
}

const point3d& hdrmap_ro_signal_light::getpos(void) const
{
	const static point3d zero;
	if (nullptr == this) {
		return zero;
	}
	auto siginfo = reinterpret_cast<const siglight_info_v1*>(this);
	return siginfo->base_xyz;
}

void hdrmap_ro_signal_light::getpos(point3d& pt) const
{
	if (nullptr == this) {
		return;
	}
	auto siginfo = reinterpret_cast<const siglight_info_v1*>(this);
	pt = siginfo->base_xyz;
}

const double* hdrmap_ro_signal_light::get_rotate_matrix(void) const
{
	if (nullptr == this) {
		return nullptr;
	}
	auto siginfo = reinterpret_cast<const siglight_info_v1*>(this);
	return siginfo->matrix;
}

void hdrmap_ro_signal_light::get_rotate_matrix(double* mtx) const
{
	if (nullptr == this || nullptr == mtx) {
		return;
	}
	auto siginfo = reinterpret_cast<const siglight_info_v1*>(this);
	for (int i = 0; i < 9; ++i) {
		mtx[i] = siginfo->matrix[i];
	}
}

uint64_t hdrmap_ro_signal_board::getuid(void) const
{
	if (nullptr == this) {
		return 0;
	}
	auto sigboard = reinterpret_cast<const sigboard_info_v1*>(this);
	return sigboard->sigboard_uid;
}

uint32_t hdrmap_ro_signal_board::get_type(void) const
{
	if (nullptr == this) {
		return 0;
	}
	auto sigboard = reinterpret_cast<const sigboard_info_v1*>(this);
	return sigboard->type;
}

uint32_t hdrmap_ro_signal_board::get_major_type(void) const
{
	if (nullptr == this) {
		return 0;
	}
	auto sigboard = reinterpret_cast<const sigboard_info_v1*>(this);
	return sigboard->t.major;
}

uint32_t hdrmap_ro_signal_board::get_sub_type(void) const
{
	if (nullptr == this) {
		return 0;
	}
	auto sigboard = reinterpret_cast<const sigboard_info_v1*>(this);
	return sigboard->t.sub;
}

}} // end of namespace zas::mapcore

namespace osm_parser1 {

int hdrmap_file_junction_v1::kdtree_x_compare(const void* a, const void* b)
{
	auto* aa = *((hdrmap_file_junction_v1**)a);
	auto* bb = *((hdrmap_file_junction_v1**)b);
	if (aa->center_x > bb->center_x) {
		return 1;
	} else if (aa->center_x < bb->center_x) {
		return -1;
	} else return 0;
}

int hdrmap_file_junction_v1::kdtree_y_compare(const void* a, const void* b)
{
	auto* aa = *((hdrmap_file_junction_v1**)a);
	auto* bb = *((hdrmap_file_junction_v1**)b);
	if (aa->center_y > bb->center_y) {
		return 1;
	} else if (aa->center_y < bb->center_y) {
		return -1;
	} else return 0;
}

} // end of namespace osm_parser1
/* EOF */
