/** @file sdmap.cpp
 */

#include "inc/kdtree.h"
#include "inc/sdmap-impl.h"

namespace zas {
namespace mapcore {

using namespace std;

sdmap_impl::sdmap_impl()
: _buffer(nullptr)
, _flags(0)
{
}

sdmap_impl::~sdmap_impl()
{
	reset();
}

void sdmap_impl::reset(void)
{
	if (nullptr != _buffer) {
		delete [] _buffer;
		_buffer = nullptr;
	}
	_f.valid = 0;
}

int sdmap_impl::load_fromfile(const char* filename)
{
	if (!filename || !*filename) {
		return -ENOTFOUND;
	}

	if (nullptr != _buffer) {
		return -ENOTALLOWED;
	}

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

	// set success
	_f.valid = 1;
	return 0;
}

int sdmap_impl::loadfile(const char* filename)
{
	FILE* fp = fopen(filename, "rb");
	if (nullptr == fp) {
		return -EFAILTOLOAD;
	}

	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);
	if (!sz) {
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

int sdmap_impl::check_validity(void)
{
	auto* hdr = header();
	if (memcmp(hdr->magic, FULLMAP_MAGIC, sizeof(hdr->magic))) {
		return -1;
	}
	// check version
	if (hdr->version > FULLMAP_VERSION) {
		return -2;
	}
	return 0;
}

int sdmap_impl::fix_ref(void)
{
	auto* keyinfo = file_keyinfo();
	if (!keyinfo->block_cols || !keyinfo->block_rows) {
		return -1;
	}
	if (keyinfo->block_count !=
		keyinfo->block_cols * keyinfo->block_rows) {
		return -2;
	}
	if (fix_ref_blocks(keyinfo)) {
		return -3;
	}
	return 0;
}

int sdmap_impl::fix_ref_blocks(fullmap_file_keyinfo_v1* keyinfo)
{
	// fix blocks information
	auto* blocks = (fullmap_file_blockinfo_v1*)(keyinfo + 1);
	for (int i = 0; i < keyinfo->block_count; ++i) {
		auto* block = &blocks[i];
		auto* memblk = (fullmap_blockinfo_v1*)block;

		memblk->start_of_blockdata = (size_t)_buffer + block->start_of_blockdata;
		memblk->node_table = (fullmap_nodeinfo_v1*)(
			memblk->start_of_blockdata + block->start_of_node_table);
		memblk->link_table = (fullmap_linkinfo_v1*)(
			memblk->start_of_blockdata + block->start_of_link_table);
		memblk->way_table = (fullmap_wayinfo_v1*)(
			memblk->start_of_blockdata + block->start_of_way_table);
		memblk->string_table = (const char*)(
			memblk->start_of_blockdata + block->start_of_string_table);		
	}

	for (int i = 0; i < keyinfo->block_count; ++i)
	{
		auto* memblk = (fullmap_blockinfo_v1*)&blocks[i];
		if (fix_ref_nodeinfo(keyinfo, memblk)) {
			return -1;
		}
		if (fix_ref_linkinfo(keyinfo, memblk)) {
			return -2;
		}
		if (fix_ref_wayinfo(keyinfo, memblk)) {
			return -3;
		}
	}
	// clear all data field in links
	reset_linkdata(keyinfo);
	return 0;
}

void sdmap_impl::reset_linkdata(fullmap_file_keyinfo_v1* keyinfo)
{
	auto* blocks = (fullmap_file_blockinfo_v1*)(keyinfo + 1);
	for (int i = 0; i < keyinfo->block_count; ++i)
	{
		auto* blkinfo = (fullmap_blockinfo_v1*)&blocks[i];
		int linkcnt = blkinfo->link_count;
		if (!linkcnt) {
			continue;
		}

		// initialize all ext-data field as null
		for (int j = 0; j < linkcnt; ++j) {
			auto* linkinfo = &blkinfo->link_table[j];
			linkinfo->data = nullptr;
		}
	}
}

int sdmap_impl::fix_ref_wayinfo(
	fullmap_file_keyinfo_v1* keyinfo, fullmap_blockinfo_v1* blkinfo)
{
	int waycnt = blkinfo->way_count;
	if (!waycnt) {
		return 0;
	}

	for (int i = 0; i < waycnt; ++i) {
		auto* mem_wayinfo = &blkinfo->way_table[i];
		auto* wayinfo = (fullmap_file_wayinfo_v1*)mem_wayinfo;

		// fix the string
		if (!wayinfo->way_name) {
			mem_wayinfo->name = nullptr;
		}
		else mem_wayinfo->name = blkinfo->string_table
			+ wayinfo->way_name;
	}
	return 0;
}

int sdmap_impl::fix_ref_linkinfo(
	fullmap_file_keyinfo_v1* keyinfo, fullmap_blockinfo_v1* blkinfo)
{
	int linkcnt = blkinfo->link_count;
	if (!linkcnt) {
		return 0;
	}

	for (int i = 0; i < linkcnt; ++i)
	{
		auto* mem_linkinfo = &blkinfo->link_table[i];
		auto* linkinfo = (fullmap_file_linkinfo_v1*)mem_linkinfo;

		// fix the way_id
		auto wayid = linkinfo->way_id;
		assert(wayid.u.blkid == block_id(blkinfo));
		assert(wayid.u.index < blkinfo->way_count);
		mem_linkinfo->wayinfo = &blkinfo->way_table[wayid.u.index];

		// fix link node1 and node2
		mem_linkinfo->node1 = get_linknode(linkinfo->node1_id);
		if (nullptr == mem_linkinfo->node1) {
			return -1;
		}
		mem_linkinfo->node2 = get_linknode(linkinfo->node2_id);
		if (nullptr == mem_linkinfo->node2) {
			return -2;
		}

		// fix prev link and next link
		if (linkinfo->prev_link) {
			mem_linkinfo->prev = get_link(linkinfo->prev_link_blockid,
				linkinfo->prev_link);
			if (nullptr == mem_linkinfo->prev) {
				return -3;
			}
		}
		else mem_linkinfo->prev = nullptr;

		if (linkinfo->next_link) {
			mem_linkinfo->next = get_link(linkinfo->next_link_blockid,
				linkinfo->next_link);
			if (nullptr == mem_linkinfo->next) {
				return -4;
			}
		}
		else mem_linkinfo->next = nullptr;

	}
	return 0;
}

fullmap_linkinfo_v1* sdmap_impl::get_link(size_t blkid, size_t linkid)
{
	auto* keyinfo = file_keyinfo();
	if (blkid >= keyinfo->block_count) {
		return nullptr;
	}
	auto* blktbl = (fullmap_blockinfo_v1*)(keyinfo + 1);
	auto* linktbl = blktbl[blkid].link_table;

	// search for linkid
	int mid;
	int start = 0;
	int end = blktbl[blkid].link_count - 1;
	if (end < 0) return nullptr;

	// find the item
	while (start <= end)
	{
		mid = (start + end) / 2;
		auto* link = (fullmap_file_linkinfo_v1*)&linktbl[mid];
		size_t val = link->uid;

		if (linkid == val) {
			return &linktbl[mid];
		}
		else if (linkid > val) start = mid + 1;
		else end = mid - 1;
	}
	return nullptr;
}

fullmap_nodeinfo_v1* sdmap_impl::get_linknode(osm_parser1::arridx_t id)
{
	auto* keyinfo = file_keyinfo();
	auto* blkinfo = (fullmap_blockinfo_v1*)(keyinfo + 1);

	if (id.u.blkid >= keyinfo->block_count) {
		return nullptr;
	}
	auto* block = &blkinfo[id.u.blkid];
	if (id.u.index >= block->node_count) {
		return nullptr;
	}
	return &block->node_table[id.u.index];
}

int sdmap_impl::fix_kdtree(fullmap_file_keyinfo_v1* keyinfo,
	arridx_t ref, fullmap_nodeinfo_v1** node)
{
	if (ref.all == UINT64_MAX) {
		*node = nullptr;
		return 0;
	}

	auto* block_tbl = (fullmap_blockinfo_v1*)(keyinfo + 1);
	int blkid = ref.u.blkid;
	if (blkid >= keyinfo->block_count) {
		return -1;
	}
	auto& block = block_tbl[blkid];
	int nodeid = ref.u.index;
	if (nodeid >= block.node_count) {
		return -2;
	}
	*node = &block.node_table[nodeid];
	return 0;
}

int sdmap_impl::fix_ref_nodeinfo(
	fullmap_file_keyinfo_v1* keyinfo, fullmap_blockinfo_v1* blkinfo)
{
	int nodecnt = blkinfo->node_count;
	if (!nodecnt) return 0;

	// fix all extra info first
	int extinfo_cnt = blkinfo->node_extdata_count;
	auto* extinfo_tbl = (fullmap_file_node_extra_info_v1*)(
		blkinfo->start_of_blockdata +
		sizeof(fullmap_file_nodeinfo_v1) * nodecnt);
	
	// note: there is an empty extinfo in the very beginning,
	// so there is an + 1 here
	size_t* link_idxtbl = (size_t*)(&extinfo_tbl[extinfo_cnt + 1]);

	for (int i = 1; i <= extinfo_cnt; ++i) {
		auto* extinfo = &extinfo_tbl[i];
		assert(extinfo->intersect_name_count);
		
		auto* mem_extinfo = (fullmap_node_extra_info_v1*)extinfo;
		mem_extinfo->first = blkinfo->string_table + extinfo->first;
	}

	// fix node info
	auto* nodeinfo_tbl = (fullmap_file_nodeinfo_v1*)(
		blkinfo->start_of_blockdata);

	for (int i = 0; i < nodecnt; ++i) {
		auto* node = &nodeinfo_tbl[i];
		auto* mem_node = (fullmap_nodeinfo_v1*)node;

		// fix left and right
		if (fix_kdtree(keyinfo, node->left_id, &mem_node->left)) {
			return -1;
		}
		if (fix_kdtree(keyinfo, node->right_id, &mem_node->right)) {
			return -2;
		}

		// fix extra info pointer
		if (node->extra_info) {
			if (node->extra_info > extinfo_cnt) {
				return -3;
			}
			mem_node->extra_info = (fullmap_node_extra_info_v1*)
				&extinfo_tbl[node->extra_info];
		}

		// fix link_start
		mem_node->links = (fullmap_linkinfo_v1**)&link_idxtbl[node->link_start];
	}

	// fix node ref
	for (int i = 0; i < blkinfo->ref_link_count; ++i) {
		arridx_t idx; idx.all = link_idxtbl[i];

		// sanity check
		if (idx.u.blkid >= keyinfo->block_count) {
			return -4;
		}

		// get the specific block
		auto* blk_start = (fullmap_blockinfo_v1*)(
			((fullmap_file_keyinfo_v1*)(header() + 1)) + 1);
		auto& block = blk_start[idx.u.blkid];
			
		// check if it exceeds link count
		if (idx.u.index >= block.link_count) {
			return -5;
		}
		link_idxtbl[i] = (size_t)&block.link_table[idx.u.index];

	}
	return 0;
}

static int sdmap_nearest_link_compare(const void* a, const void* b)
{
	auto* aa = *((sdmap_nearest_link_impl**)a);
	auto* bb = *((sdmap_nearest_link_impl**)b);
	if (aa->distance > bb->distance) {
		return 1;
	} else if (aa->distance < bb->distance) {
		return -1;
	} else return 0;
}

int sdmap_impl::search_nearest_links(double lat, double lon,
	sdmap_nearest_link_impl* links,
	int node_range, int link_count,
	fullmap_nodeinfo_v1** nearest_node)
{
	if (link_count <= 0
		|| link_count > MAX_SEARCH_LINKS) {
		return -EBADPARM;
	}

	if (!node_range) {
		node_range = DEF_SEARCH_NODES;
	} else if (node_range > MAX_SEARCH_NODES) {
		node_range = MAX_SEARCH_NODES;
	}
	if (node_range < link_count) {
		node_range = link_count;
	}

	auto* nodeset = (sdmap_nearest_node_impl*)
		alloca(node_range * sizeof(sdmap_nearest_node_impl));
	auto** linkset = (sdmap_nearest_link_impl**)
		alloca(node_range * sizeof(sdmap_nearest_link_impl*));
	assert(nullptr != nodeset && nullptr != linkset);

	// search nearest nodes
	if (search_nearest_nodes(lat, lon, node_range, nodeset)) {
		return -EINVALID;
	}

	// search nearest link for each nodes
	for (int i = 0; i < node_range; ++i) {
		auto* nl = (sdmap_nearest_link_impl*)
			alloca(sizeof(sdmap_nearest_link_impl));
		assert(nullptr != nl);

		double plat, plon, pdist;
		nl->node = nodeset[i].node;
		nl->link = get_node_nearest_link(nl->node,
			lat, lon, plat, plon, pdist);
		
		if (nl->link) {
			nl->projection.set(plat, plon);
			nl->distance = pdist;
		} else {
			nl->projection.set(0., 0.);
			nl->distance = -1;
		}
		linkset[i] = nl;
	}
	
	qsort(linkset, node_range, sizeof(sdmap_nearest_link_impl*),
		sdmap_nearest_link_compare);

	// copy necessary links
	for (int i = 0; i < link_count; ++i) {
		links[i] = *linkset[i];
	}

	if (nearest_node) {
		*nearest_node = nodeset->node;
	}
	return 0;
}

fullmap_linkinfo_v1* sdmap_impl::get_node_nearest_link(
	fullmap_nodeinfo_v1* node,
	double srclat, double srclon,
	double& dstlat, double& dstlon, double& dist)
{
	dist = DBL_MAX;
	double lon, lat;
	fullmap_linkinfo_v1* ret = nullptr;
	for (int i = 0; i < node->link_count; ++i) {
		auto* link = node->links[i];
		double tmp = point_to_link(
			link, srclon, srclat, lon, lat);
		
		if (tmp < dist) {
			ret = link, dist = tmp;
			dstlat = lat, dstlon = lon;
		}
	}
	return ret;
}

bool sdmap_impl::point_in_link(fullmap_linkinfo_v1* link,
	double lon, double lat)
{
	double a, b, c;
	a = link->node1->lat, b = link->node2->lat;
	if (a > b) {
		c = a, a = b, b = c;
	}
	if (lat < a || lat >= b) {
		return false;
	}

	a = link->node1->lon, b = link->node2->lon;
	if (a > b) {
		c = a, a = b, b = c;
	}
	if (lon < a || lon >= b) {
		return false;
	}
	return true;
}

double sdmap_impl::point_to_link(fullmap_linkinfo_v1* link,
	double srclon, double srclat,
	double& dstlon, double& dstlat)
{
	/*
	 * k = (lat1 - lat2) / (lon1 - lon2)
	 * the line equation is:
	 * kx - y + lat1 - k * lon1 = 0
	 * A = k, B = -1, C = lat1 - k * lon1
	 * then, the point in the line will be:
	 * ((B^2x₀-ABy₀-AC)/(A^2+B^2), (A^2y₀-ABx₀-BC)/(A^2+B^2))
	*/
	double k = (link->node1->lat - link->node2->lat)
		/ (link->node1->lon - link->node2->lon);
	double C = link->node1->lat - k * link->node1->lon;
	double A2 = k * k;
	double A2B2 = A2 + 1.;

	// dstlon = (srclon + A * srclat - A * C)
	// since A = k, so it becomes:
	dstlon = (srclon + k * srclat - k * C) / A2B2;
	// dstlat = (A2 * srclat + A * srclon + C);
	// since A = k, so it becomes:
	dstlat = (A2 * srclat + k * srclon + C) / A2B2;

	// check if the point is in the link range
	if (point_in_link(link, dstlon, dstlat)) {
		return distance(srclon, srclat, dstlon, dstlat);
	}

	double dist1 = distance(srclon, srclat, link->node1->lon, link->node1->lat);
	double dist2 = distance(srclon, srclat, link->node2->lon, link->node2->lat);
	if (dist1 < dist2) {
		dstlon = link->node1->lon;
		dstlat = link->node1->lat;
		return dist1;
	} else {
		dstlon = link->node2->lon;
		dstlat = link->node2->lat;
		return dist2;
	}
	// shall never be here
}

int sdmap_impl::search_nearest_nodes(double lat, double lon,
	int count, sdmap_nearest_node_impl* set)
{
	if (count <= 0 || !set) {
		return -EBADPARM;
	}
	auto* keyinfo = file_keyinfo();
	if (!in_scope(lon, lat, keyinfo->thisfile_scope.minlon,
		keyinfo->thisfile_scope.minlat,
		keyinfo->thisfile_scope.maxlon,
		keyinfo->thisfile_scope.maxlat)) {
		return -EOUTOFOSCOPE;
	}

	auto* blocks = (fullmap_blockinfo_v1*)(keyinfo + 1);

	int blkid = keyinfo->node_kdtree_root.u.blkid;
	if (blkid >= keyinfo->block_count) {
		return -EINVALID;
	}
	auto* block = &blocks[blkid];
	int index = keyinfo->node_kdtree_root.u.index;
	if (index >= block->node_count) {
		return EINVALID;
	}
	auto* node = &block->node_table[index];

	for (int i = 0; i < count; ++i) {
		auto& item = set[i];
		// x = lat, y = lon
		item.node = kdtree2d_search(node, lat, lon,
			item.distance);
	}

	// restore "selected" flags
	for (int i = 0; i < count; ++i) {
		auto& item = set[i];
		if (nullptr == item.node) {
			continue;
		}
		item.node->attr.KNN_selected = 0;
	}
	return 0;
}

sdmap::sdmap()
: _data(nullptr) {
	_data = reinterpret_cast<void*>(new sdmap_impl());
}

sdmap::~sdmap()
{
	if (nullptr != _data) {
		auto* obj = reinterpret_cast<sdmap_impl*>(_data);
		delete obj;
		_data = nullptr;
	}
}

int sdmap::load_fromfile(const char* filename)
{
	if (nullptr == _data) {
		return -EINVALID;
	}

	auto* map = reinterpret_cast<sdmap_impl*>(_data);
	return map->load_fromfile(filename);
}

const sdmap_node* sdmap::nearest_node(double lat, double lon,
	double& distance) const
{
	sdmap_nearest_node_impl node;
	if (nullptr == _data) {
		return nullptr;
	}

	auto* map = reinterpret_cast<sdmap_impl*>(_data);
	if (!map->valid()) {
		return nullptr;
	}

	if (map->search_nearest_nodes(lat, lon, 1, &node)) {
		return nullptr;
	}
	distance = node.distance;
	return reinterpret_cast<sdmap_node*>(node.node);
}

const sdmap_node* sdmap::nearest_node(const coord_t& c,
	double& distance) const
{
	return nearest_node(c.lat(), c.lon(), distance);
}

int sdmap::search_nearest_nodes(double lat, double lon,
	int count, sdmap_nearest_node* nodeset) const
{
	if (nullptr == _data) {
		return -EINVALID;
	}

	auto* map = reinterpret_cast<sdmap_impl*>(_data);
	if (!map->valid()) {
		return -EINVALID;
	}

	return map->search_nearest_nodes(lat, lon,
		count, (sdmap_nearest_node_impl*)nodeset);
}

int sdmap::search_nearest_nodes(const coord_t& c,
	int count, sdmap_nearest_node* nodeset) const
{
	return search_nearest_nodes(c.lat(), c.lon(),
		count, nodeset);
}

int sdmap::search_nearest_links(double lat,
	double lon, sdmap_nearest_link* links,
	int node_range, int link_count,
	sdmap_node** nearest_node) const
{
	if (nullptr == links) {
		return -EBADPARM;
	} 
	if (nullptr == _data) {
		return -EINVALID;
	}

	auto* map = reinterpret_cast<sdmap_impl*>(_data);
	if (!map->valid()) {
		return -EINVALID;
	}

	fullmap_nodeinfo_v1* nodeinfo;
	int ret = map->search_nearest_links(
		lat, lon, (sdmap_nearest_link_impl*)links,
		node_range, link_count, &nodeinfo);
	
	if (nearest_node) {
		*nearest_node = reinterpret_cast<sdmap_node*>(nodeinfo);
	}
	return ret;
}

bool sdmap::valid(void) const
{
	if (nullptr == _data) {
		return false;
	}
	auto* map = reinterpret_cast<sdmap_impl*>(_data);
	return map->valid();
}

uint64_t sdmap_node::getuid(void) const
{
	if (nullptr == this) {
		return 0;
	}
	auto* node = reinterpret_cast<const fullmap_nodeinfo_v1*>(this);
	return node->uid;
}

double sdmap_node::lat(void) const
{
	if (nullptr == this) {
		return 0.;
	}
	auto* node = reinterpret_cast<const fullmap_nodeinfo_v1*>(this);
	return node->lat;
}

double sdmap_node::lon(void) const
{
	if (nullptr == this) {
		return 0.;
	}
	auto* node = reinterpret_cast<const fullmap_nodeinfo_v1*>(this);
	return node->lon;
}

bool sdmap_node::get_intersection_roads_name(
	vector<const char*>& names) const
{
	if (nullptr == this) {
		return false;
	}
	auto* node = reinterpret_cast<const fullmap_nodeinfo_v1*>(this);
	if (!node->attr.keynode) {
		return false;
	}
	if (node->extra_info == nullptr) {
		return true;
	}
	const char* start = node->extra_info->first;
	for (int i = 0; i < node->extra_info->intersect_name_count; ++i) {
		names.push_back(start);
		start += strlen(start) + 1;
	}
	return true;
}

int sdmap_node::link_count(void) const
{
	if (nullptr == this) {
		return 0;
	}

	auto* node = reinterpret_cast<const fullmap_nodeinfo_v1*>(this);
	return node->link_count;
}

sdmap_link* sdmap_node::get_link(int index) const
{
	if (nullptr == this) {
		return nullptr;
	}

	auto* node = reinterpret_cast<const fullmap_nodeinfo_v1*>(this);
	if (index >= node->link_count) {
		return nullptr;
	}
	return reinterpret_cast<sdmap_link*>(node->links[index]);
}

void* sdmap_link::get_userdata(void) const
{
	if (nullptr == this) {
		return nullptr;
	}

	auto* link = reinterpret_cast<const fullmap_linkinfo_v1*>(this);
	return link->data;
}

void* sdmap_link::set_userdata(void* data)
{
	if (nullptr == this) {
		return nullptr;
	}

	auto* link = reinterpret_cast<fullmap_linkinfo_v1*>(this);
	auto ret = link->data;
	link->data = data;
	return ret;
}

const sdmap_way* sdmap_link::get_way(void) const
{
	if (nullptr == this) {
		return nullptr;
	}

	auto* link = reinterpret_cast<const fullmap_linkinfo_v1*>(this);
	return reinterpret_cast<const sdmap_way*>(link->wayinfo);
}

const sdmap_node* sdmap_link::get_node1(void) const
{
	if (nullptr == this) {
		return nullptr;
	}

	auto* link = reinterpret_cast<const fullmap_linkinfo_v1*>(this);
	return reinterpret_cast<const sdmap_node*>(link->node1);
}

const sdmap_node* sdmap_link::get_node2(void) const
{
	if (nullptr == this) {
		return nullptr;
	}

	auto* link = reinterpret_cast<const fullmap_linkinfo_v1*>(this);
	return reinterpret_cast<const sdmap_node*>(link->node2);
}

const sdmap_link* sdmap_link::prev(void) const
{
	if (nullptr == this) {
		return nullptr;
	}

	auto* link = reinterpret_cast<const fullmap_linkinfo_v1*>(this);
	return reinterpret_cast<const sdmap_link*>(link->prev);
}

const sdmap_link* sdmap_link::next(void) const
{
	if (nullptr == this) {
		return nullptr;
	}

	auto* link = reinterpret_cast<const fullmap_linkinfo_v1*>(this);
	return reinterpret_cast<const sdmap_link*>(link->next);
}

const char* sdmap_way::get_name(void) const
{
	if (nullptr == this) {
		return nullptr;
	}

	auto* way = reinterpret_cast<const fullmap_wayinfo_v1*>(this);
	return way->name;
}

int sdmap_way::get_type(void) const
{
	if (nullptr == this) {
		return -1;
	}

	auto* way = reinterpret_cast<const fullmap_wayinfo_v1*>(this);
	return way->attr.highway_type;
}


}} // end of namespace zas::mapcore