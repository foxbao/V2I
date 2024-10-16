#include "utils/dir.h"
#include "generator.h"
#include "planning.h"

namespace osm_parser1 {

using namespace zas::utils;

bool map_level::allowed_type(int type)
{
	int mid;
	int start = 0;
	int* arr = types.buffer();
	int end = types.getsize() - 1;

	if (end < 0) {
		if (end == -1) {
			return (type >= 0 && type <= TAG_HIGHWAY_END_OF_OTHER_FEATURES)
			? true : false;
		}
		else return false;
	}

	// find the item
	while (start <= end)
	{
		mid = (start + end) / 2;
		int val = arr[mid];

		if (type == val) {
			return true;
		}
		else if (type > val) start = mid + 1;
		else end = mid - 1;
	}
	return false;
}

int osm_map::generate(void)
{
	// mkdir
	if (createdir(_psr.get_targetdir())) {
		return -1;
	}

	// calculate full map blocks
	_fullmap = new fullmap_level();
	assert(nullptr != _fullmap);

	if (calc_fullmap_row_col()) {
		return -2;
	}

	fprintf(stdout, "preparing all nodes and ways ...");
	fflush(stdout);

	if (fullmap_walk_all_nodes()) {
		return -3;
	}
	if (fullmap_walk_all_ways()) {
		return -4;
	}
	fprintf(stdout, "[OK]\n");

	if (_fullmap->writefile(this)) {
		return -5;
	}

	// finish generating the full map
	delete _fullmap;
	_fullmap = nullptr;
	fprintf(stdout, "full map generated successfully.\n\n");

	auto* level = (map_level*)_psr.get_maplevels();
	for (int i = 1; level->col; ++level, ++i) {
		fprintf(stderr, "pre-processing level %d %s...\n",
			i, (level->types.getsize()) ? "" : "(full layer) ");
		if (generate_level(level, i)) {
			return -4;
		}
	}

	auto* item = _highway_node_list.next;
	for (; item != &_highway_node_list; item = item->next) {
		auto* node = list_entry(osm_node, ownerlist, item);
		if (!node->attr.is_keynode) continue;
		_pmap->generate_edge(node);
	}

	_pmap->serial_planningmap();

	_pmap->deserial_planningmap();

	planning_map_node* pnode = nullptr;
	size_t sz = 0;
	double lat2 = 31.2689678;
	double lon2 = 121.2238481;
	double lat1 = 31.2905921;
	double lon1 = 121.1588100;
	std::vector<int64_t> plannodes;
	_pmap->planning_road(lat1, lon1, lat2, lon2, plannodes);

	return 0;
}

int osm_map::generate_level(map_level* level, int lvid)
{
	calc_row_col(level);
	auto* maplevel = new maplevel_data(level, lvid);
	if (nullptr == maplevel) {
		return -1;
	}

	_pmap->generate_level(_scope, level, lvid);
	// planning_map_level* plvl;
	// _pmap->get_level(plvl, lvid);

	if (maplevel_walk_all_keynodes(maplevel)) {
		return -2;
	}

	_pmap->build_block_kdtree();
	// pmap->serial_planningmap();
		
	return 0;
}

int osm_map::fullmap_walk_all_nodes(void)
{
	auto* item = _highway_node_list.next;
	for (; item != &_highway_node_list; item = item->next) {
		auto* node = list_entry(osm_node, ownerlist, item);
		if (_fullmap->add_node(node, _scope)) {
			return -1;
		}
	}

	// build the kdtree
	_fullmap->build_kdtree();
	return 0;
}

int osm_map::fullmap_walk_all_ways(void)
{
	auto* item = _highway_list.next;
	for (; item != &_highway_list; item = item->next) {
		auto* way = list_entry(osm_way, ownerlist, item);
		if (_fullmap->add_way(way)) {
			return -1;
		}
	}
	return 0;
}

int osm_map::maplevel_walk_all_keynodes(maplevel_data* data)
{
	planning_map_level* plvl;
	int lvid = data->get_level_id();
	_pmap->get_level(&plvl, lvid);
	auto* item = _highway_node_list.next;
	for (; item != &_highway_node_list; item = item->next) {
		auto* node = list_entry(osm_node, ownerlist, item);
		if (!node->attr.is_keynode) continue;
	
		// we handle key nodes
		if (data->add_keynode(_pmap, plvl, node)) {
			return -2;
		}
	}
	// data->get_planningmap()->show_block_info();

	
	printf("level %u: node count: %d\n",
		data->get_level_id(),
		data->get_node_count());
	return 0;
}

int osm_map::calc_fullmap_row_col(void)
{
	assert(nullptr != _fullmap && _fullmap->col <= 0 \
		&& _fullmap->row <= 0);

	if (!_fullmap->blk_max_nodes) {
		_fullmap->blk_max_nodes = MAPGEN_MAX_BLK_NODE_CNT;
	}

	int blkcnt = (_highway_node_count + _fullmap->blk_max_nodes
		- 1) / _fullmap->blk_max_nodes;

	int steps = 1;
	for (; steps * steps < blkcnt; ++steps);
	fprintf(stdout, "fullmap: block-max-nodes: %u\n", _fullmap->blk_max_nodes);
	fprintf(stdout, "fullmap: estimated total nodes: %lu -> grid: %d x %d\n",
		_highway_node_count, steps, steps);

	_fullmap->col = _fullmap->row = steps;
	return _fullmap->prepare_blocks();
}

int osm_map::calc_row_col(map_level* level)
{
	assert(level->col < 0 && level->row < 0);

	if (!level->blk_max_nodes) {
		level->blk_max_nodes = MAPGEN_MAX_BLK_KEY_NODE_CNT;
	}

	// calculate total key nodes
	size_t total_keynodes = 0;
	int type_cnt = level->types.getsize();
	if (!type_cnt) {
		for (int i = 0; i < TAG_HIGHWAY_END_OF_OTHER_FEATURES; ++i) {
			total_keynodes += _keynode_classified_count[i];
		}
	}
	else for (int i = 0; i < type_cnt; ++i) {
		int type = level->types.buffer()[i];
		assert(type >= 0 && type <= TAG_HIGHWAY_END_OF_OTHER_FEATURES);
		total_keynodes += _keynode_classified_count[type];
	}
	
	int blkcnt = (total_keynodes + level->blk_max_nodes
		- 1) / level->blk_max_nodes;

	int steps = 1;
	for (; steps * steps < blkcnt; ++steps);
	fprintf(stdout, "block-max-nodes: %u\n", level->blk_max_nodes);
	fprintf(stdout, "estimated total nodes: %lu -> grid: %d x %d\n",
		total_keynodes, steps, steps);

	level->col = level->row = steps;
	return 0;
}

maplevel_data::maplevel_data(map_level* level, int lvid)
: _level(level), _pblks(nullptr)
, _node_count(0), _level_id(lvid)
{
	memset(&_flags, 0, sizeof(_flags));

	// create the plandata blocks	
	assert(level->col && level->row);
	_pblks = new plandata_block [level->col * level->row];
	if (nullptr == _pblks) return;

	_flags.ready = 1;
}

maplevel_data::~maplevel_data()
{
	if (_pblks) {
		delete [] _pblks;
		_pblks = nullptr;
	}
}

plandata_block::plandata_block()
: node_count(0), lat_tree(nullptr)
, lon_tree(nullptr)
{
}

int maplevel_data::add_keynode(planning_map* pmap, planning_map_level* plvl, osm_node* node)
{
	// check if the node match with this map level
	if (!node_matched(node)) {
		return 0;
	}
	++_node_count;
	pmap->generate_node(plvl, node);
	return 0;
}

bool maplevel_data::node_matched(osm_node* node)
{
	auto* item = node->wayref_list.next;
	for (; item != &node->wayref_list; item = item->next) {
		auto* wref = list_entry(osm_wayref, node_ownerlist, item);
		auto* way = wref->way;
		if (_level->allowed_type(way->attr.highway_type)) {
			return true;
		}
	}
	return false;
}

fullmap_level::~fullmap_level()
{
	if (nullptr != blocks) {
		delete [] blocks;
		blocks = nullptr;
	}
	if (_fp) {
		fclose(_fp);
		_fp = nullptr;
	}
}

int fullmap_level::prepare_blocks(void)
{
	int total = col * row;
	if (!total) return -1;

	blocks = new fullmap_blockinfo [total];
	if (nullptr == blocks) {
		return -2;
	}
	for (int i = 0; i < total; ++i) {
		blocks[i].block_id = i;
	}
	return 0;
}

int fullmap_level::add_node(osm_node* node, map_scope& scope)
{
	int x = col, y = row;
	scope.in_scope(node->lat, node->lon, &x, &y);
	int idx = y * col + x;
	assert(idx < col * row);

	// create the fullmap node object
	auto* nd = new fullmap_nodeinfo;
	if (nullptr == nd) {
		return -1;
	}

	// kd-tree
	nd->left = nd->right = nullptr;

	nd->flags = 0;
	nd->fanin_link_count = 0;
	nd->fanout_link_count = 0;
	nd->fanin_links = nullptr;
	nd->fanout_links = nullptr;
	nd->uid = node->id;
	nd->node = node;
	nd->block = &blocks[idx];

	int ret = avl_insert(&nd->block->node_tree, &nd->avlnode,
		fullmap_nodeinfo::uid_avl_comp);
	if (ret) {
		return -2;
	}

	// add this to the fullmap global avltree
	ret = avl_insert(&node_tree, &nd->gbl_avlnode,
		fullmap_nodeinfo::uid_gblavl_comp);
	if (ret) {
		return -3;
	}
	++nd->block->node_count;	// blockinfo
	++node_count;				// fullmap
	return 0;
}

int fullmap_level::build_kdtree(void)
{
	if (node_count <= 0) {
		return -1;
	}

	// create the exam set table
	auto** exm_set = new fullmap_nodeinfo* [node_count];
	assert(nullptr != exm_set);

	// add all nodes to the array and build the kd-tree
	int cnt = 0;
	auto* node = avl_first(node_tree);
	for (; node; node = avl_next(node), ++cnt) {
		auto* nodeinfo = AVLNODE_ENTRY(fullmap_nodeinfo, \
			gbl_avlnode, node);
		exm_set[cnt] = nodeinfo;
	}

	assert(cnt == node_count);
	node_kdtree_root = do_build_kdtree(exm_set, node_count);
	assert(nullptr != node_kdtree_root);

	delete [] exm_set;
	return 0;
}

int fullmap_level::lat_compare(const void* a, const void* b)
{
	auto* aa = *((fullmap_nodeinfo**)a);
	auto* bb = *((fullmap_nodeinfo**)b);
	if (aa->node->lat > bb->node->lat) {
		return 1;
	} else if (aa->node->lat < bb->node->lat) {
		return -1;
	} else return 0;
}

int fullmap_level::lon_compare(const void* a, const void* b)
{
	auto* aa = *((fullmap_nodeinfo**)a);
	auto* bb = *((fullmap_nodeinfo**)b);
	if (aa->node->lon > bb->node->lon) {
		return 1;
	} else if (aa->node->lon < bb->node->lon) {
		return -1;
	} else return 0;
}

bool fullmap_nodeinfo::coord_equal(fullmap_nodeinfo* nd) {
	return (node->lat == nd->node->lat
		&& node->lon == nd->node->lon) ? true : false;
}

void fullmap_level::choose_split(
	fullmap_nodeinfo** exm_set, int size, int &split)
{
	double tmp1 = 0., tmp2 = 0.;
	for (int i = 0; i < size; ++i) {
		auto* nd = exm_set[i]->node;
		tmp1 += 1.0 / (double)size * nd->lat * nd->lat;
		tmp2 += 1.0 / (double)size * nd->lat;
    }
	double v1 = tmp1 - tmp2 * tmp2; //compute variance on the x dimension
	
	tmp1 = tmp2 = 0;
	for (int i = 0; i < size; ++i) {
		auto* nd = exm_set[i]->node;
		tmp1 += 1.0 / (double)size * nd->lon * nd->lon;
		tmp2 += 1.0 / (double)size * nd->lon;
	}
    double v2 = tmp1 - tmp2 * tmp2; //compute variance on the y dimension
	
	split = (v1 > v2) ? 0 : 1; //set the split dimension
	if (split == 0) {
		qsort(exm_set, size, sizeof(fullmap_nodeinfo*), lat_compare);
	} else{
		qsort(exm_set, size, sizeof(fullmap_nodeinfo*), lon_compare);
	}
}

fullmap_nodeinfo* fullmap_level::do_build_kdtree(
	fullmap_nodeinfo **exm_set, int size)
{
	if (size <= 0) {
		return nullptr;
	}

	int split;
	fullmap_nodeinfo** exm_set_left = nullptr;
	fullmap_nodeinfo** exm_set_right = nullptr;

	choose_split(exm_set, size, split);

	int size_left = size / 2;
	int size_right = size - size_left - 1;

	if (size_left > 0) {
		exm_set_left = new fullmap_nodeinfo* [size_left];
		assert(nullptr != exm_set_left);
		memcpy(exm_set_left, exm_set, sizeof(void*) * size_left);		
	}
	if (size_right > 0) {
		exm_set_right = new fullmap_nodeinfo* [size_right];
		assert(nullptr != exm_set_right);
		memcpy(exm_set_right, exm_set
			+ size / 2 + 1, sizeof(void*) * size_right);
	}

	auto* ret = exm_set[size / 2];
	ret->f.kdtree_split = split;
	
	// build the sub kd-tree
	ret->left = do_build_kdtree(exm_set_left, size_left);
	ret->right = do_build_kdtree(exm_set_right, size_right);

	// free temp memory
	if (exm_set_left) delete [] exm_set_left;
	if (exm_set_right) delete [] exm_set_right;
	return ret;
}

fullmap_nodeinfo* fullmap_level::get_node(osm_node* node)
{
	if (nullptr == node) {
		return nullptr;
	}
	auto* ret = avl_find(node_tree,
		MAKE_FIND_OBJECT(node->id, fullmap_nodeinfo, uid, gbl_avlnode),
		fullmap_nodeinfo::uid_gblavl_comp);
	if (nullptr == ret) {
		return nullptr;
	}
	return AVLNODE_ENTRY(fullmap_nodeinfo, gbl_avlnode, ret);
}

fullmap_wayinfo* fullmap_blockinfo::getway(osm_way* way)
{
	fullmap_wayinfo* wayinfo;
	avl_node_t* ret = avl_find(way_tree,
		MAKE_FIND_OBJECT(way->id, fullmap_wayinfo, uid, avlnode),
		fullmap_wayinfo::uid_avl_comp);
	
	if (nullptr == ret) {
		// create and insert a new wayinfo node to this block
		wayinfo = new fullmap_wayinfo();
		if (nullptr == wayinfo) {
			return nullptr;
		}
		wayinfo->uid = way->id;
		wayinfo->way = way;
		if (wayinfo->way_distance(way)) {
			delete wayinfo;
			return nullptr;
		}

		if (avl_insert(&way_tree, &wayinfo->avlnode,
			fullmap_wayinfo::uid_avl_comp)) {
			return nullptr;
		}
		++way_count;
	}
	else wayinfo = AVLNODE_ENTRY(fullmap_wayinfo, avlnode, ret);
	return wayinfo;
}

int fullmap_level::add_link(osm_way* way, osm_node* node1,
	osm_node* node2, fullmap_linkinfo** newlink)
{
	uid_t id1 = node1->id;
	uid_t id2 = node2->id;

	// get the fullmap_nodeinfo structure
	avl_node_t* ret = avl_find(node_tree,
		MAKE_FIND_OBJECT(id1, fullmap_nodeinfo, uid, gbl_avlnode),
		fullmap_nodeinfo::uid_gblavl_comp);
	if (nullptr == ret) {
		return -1;
	}

	// get block
	auto* fm_node = AVLNODE_ENTRY(fullmap_nodeinfo, gbl_avlnode, ret);
	auto* block = fm_node->block;

	fullmap_wayinfo* wayinfo = block->getway(way);
	if (nullptr == wayinfo) {
		return -2;
	}

	// add the link
	auto* link = new fullmap_linkinfo();
	if (nullptr == link) {
		return -3;
	}
	link->prev = link->next = nullptr;
	link->block_id = -1;
	// 0 is reserved for nullptr
	link->uid = link_count + 1;
	link->node1 = fm_node;

	// get link2
	ret = avl_find(node_tree,
		MAKE_FIND_OBJECT(id2, fullmap_nodeinfo, uid, gbl_avlnode),
		fullmap_nodeinfo::uid_gblavl_comp);
	if (nullptr == ret) {
		return -4;
	}
	link->node2 = AVLNODE_ENTRY(fullmap_nodeinfo, gbl_avlnode, ret);
	link->way = wayinfo;

	assert(link->block_id < 0);
	link->block_id = link->node1->block->block_id;

	// add to the first node (fanout)
	link->node_fanout_link = link->node1->fanout_links;
	link->node1->fanout_links = link;
	++link->node1->fanout_link_count;

	// add to the second node (fanin)
	link->node_fanin_link = link->node2->fanin_links;
	link->node2->fanin_links = link;
	++link->node2->fanin_link_count;

	// add to the current block
	if (avl_insert(&block->link_tree, &link->avlnode,
		fullmap_linkinfo::uid_avl_comp)) {
		return -5;
	}

	++block->link_count;
	++link_count;
		
	// set node referred
	link->node1->f.way_referred = 1;
	link->node2->f.way_referred = 1;
	if (newlink) *newlink = link;
	return 0;
}

int fullmap_level::add_way(osm_way* way)
{
	fullmap_linkinfo *link, *prev = nullptr;
	int ndcnt = way->node_set.getsize();
	assert(ndcnt > 1);

	--ndcnt; // omit the last node
	for (int i = 0; i < ndcnt; ++i) {
		// get node1 and node2 for the link
		auto* node1 = way->node_set.buffer()[i];
		auto* node2 = way->node_set.buffer()[i + 1];

		if (add_link(way, node1, node2, &link)) {
			return -1;
		}
		// finialize the link list
		assert(nullptr != link);
		link->prev = prev;
		if (prev) prev->next = link;
		prev = link;
	}

	// handle the last node
	auto* lastnode = way->node_set.buffer()[ndcnt];
	if (way->attr.close_loop) {
		// create a link from the last node to the first node
		auto* firstnode = way->node_set.buffer()[0];
		if (add_link(way, lastnode, firstnode, &link)) {
			return -2;
		}
		// finialize the link list
		assert(nullptr != link);
		link->prev = prev;
		if (prev) prev->next = link;
	}
	else {
		auto* node = get_node(lastnode);
		if (nullptr == node) {
			return -3;
		}
		node->f.endpoint = 1;
	}
	return 0;
}

int fullmap_level::writefile(osm_map* map)
{
	string path = map->get_parser().get_targetdir();
	path += "fullmap";

	_fp = fopen(path.c_str(), "wb");
	if (nullptr == _fp) return -1;

	if (write_header()) {
		return -1;
	}
	if (write_keyinfo(map)) {
		return -2;
	}
	if (write_blocks()) {
		return -3;
	}
	if (finalize_keyinfo()) {
		return -4;
	}
	return 0;
}

int fullmap_level::finalize_keyinfo(void)
{
	if (node_kdtree_root == nullptr) {
		return -1;
	}

	// kd-tree
	auto* block = node_kdtree_root->block;
	int blkid = block - blocks;
	if (blkid < 0) {
		return -2;
	}

	auto r = block->get_nodeid(node_kdtree_root->uid);
	if (r.u.blkid != blkid
		|| r.u.index >= block->node_count) {
		return -3;
	}
	_keyinfo.node_kdtree_root = r;
	
	fseek(_fp, sizeof(_header), SEEK_SET);
	if (fwrite(&_keyinfo, 1, sizeof(_keyinfo), _fp) != sizeof(_keyinfo)) {
		return -4;
	}
	return 0;
}

int fullmap_level::write_header(void)
{
	memcpy(_header.magic, FULLMAP_MAGIC,
		sizeof(_header.magic));
	_header.version = FULLMAP_VERSION;
	
	rewind(_fp);
	if (fwrite(&_header, 1, sizeof(_header), _fp) != sizeof(_header)) {
		return -1;
	}
	return 0;
}

int fullmap_level::write_keyinfo(osm_map* map)
{
	_keyinfo.block_rows = row;
	_keyinfo.block_cols = col;
	_keyinfo.block_count = row * col;
	if (!_keyinfo.block_count) {
		return -1;
	}

	const map_scope& scope = map->get_scope();
	_keyinfo.fullmap_scope.minlat = scope.minlat;
	_keyinfo.fullmap_scope.minlon = scope.minlon;
	_keyinfo.fullmap_scope.maxlat = scope.maxlat;
	_keyinfo.fullmap_scope.maxlon = scope.maxlon;
	_keyinfo.thisfile_scope.minlat = scope.minlat;
	_keyinfo.thisfile_scope.minlon = scope.minlon;
	_keyinfo.thisfile_scope.maxlat = scope.maxlat;
	_keyinfo.thisfile_scope.maxlon = scope.maxlon;
	_keyinfo.node_kdtree_root.all = 0;
	
	// write the keyinfo
	if (fwrite(&_keyinfo, 1, sizeof(_keyinfo), _fp) != sizeof(_keyinfo)) {
		return -1;
	}

	// write dummy blockinfo
	fullmap_file_blockinfo_v1 blkinfo;
	memset(&blkinfo, 0, sizeof(blkinfo));
	for (int i = 0; i < _keyinfo.block_count; ++i) {
		if (fwrite(&blkinfo, 1, sizeof(blkinfo), _fp)
			!= sizeof(blkinfo)) {
			return -1;
		}
	}
	return 0;
}

int fullmap_level::write_blocks(void)
{
	for (int i = 0; i < _keyinfo.block_count; ++i)
	{
		// output info
		fprintf(stdout, "\rwriting block %d of %d ...",
			i, _keyinfo.block_count);
		fflush(stdout);

		auto& block = blocks[i];
		int ret = block.write(_fp, this);
		if (ret) return ret;
	}

	fprintf(stdout, "\r%d blocks wrote.",
		_keyinfo.block_count);
	for (int i = 0; i < 16; fputc(' ', stdout), ++i);
	fputc('\n', stdout);
	return 0;
}

int fullmap_blockinfo::write_nodes(FILE* fp,
	fullmap_file_blockinfo_v1& blkinfo, fullmap_level* fm)
{
	if (!node_count) {
		return 0;
	}

	fullmap_file_nodeinfo_v1 nodeinfo;
	assert(nullptr != node_array);

	uint32_t link_start_idx = 0;
	blkinfo.start_of_node_table = 0;

	// write all nodes
	for (int i = 0; i < node_count; ++i) {	
		auto* node = node_array[i];
		assert(node->f.way_referred || node->f.endpoint);
		
		if (node->serialize(blkinfo, nodeinfo)) {
			return -1;
		}
		nodeinfo.link_start = link_start_idx;
		link_start_idx += nodeinfo.link_count;
		
		if (sizeof(nodeinfo) != fwrite(&nodeinfo, 1, sizeof(nodeinfo), fp)) {
			return -2;
		}
	}

	// write all extra data for thest nodes
	fullmap_file_node_extra_info_v1 extdata;
	memset(&extdata, 0, sizeof(extdata));

	// write an empty extra info first
	if (sizeof(extdata) != fwrite(&extdata, 1, sizeof(extdata), fp)) {
		return -3;
	}
	for (int i = 0; i < node_count; ++i) {
		auto* nodeinfo = node_array[i];
		auto* node = nodeinfo->node;
		if (!node->data) {
			continue;
		}

		extdata.first = 0;
		extdata.intersect_name_count = node->data->intersect_name_count;
		assert(extdata.intersect_name_count);

		for (int j = 0; j < extdata.intersect_name_count; ++j) {
			size_t pos;
			if (!strsect.alloc_copy_string(node->data->intersect_name[j], pos)) {
				return -4;
			}
			if (!extdata.first) extdata.first = pos;
		}

		if (sizeof(extdata) != fwrite(&extdata, 1, sizeof(extdata), fp)) {
			return -5;
		}
	}

	// write all links for these nodes
	for (int i = 0; i < node_count; ++i) {
		auto* node = node_array[i];
		if (node->fanin_link_count +
			node->fanout_link_count <= 0) {
			continue;
		}

		// handle fanout links
		for (auto* link = node->fanout_links; link;
			link = link->node_fanout_link,
			blkinfo.ref_link_count++) {
			arridx_t idx = get_linkid(link->uid);
			assert(idx.u.blkid == block_id);
			if (idx.u.index > link_count) {
				return -3;
			}
			if (sizeof(idx) != fwrite(&idx, 1, sizeof(idx), fp)) {
				return -4;
			}
		}

		// handle fanin links
		for (auto* link = node->fanin_links; link;
			link = link->node_fanin_link,
			blkinfo.ref_link_count++) {
			auto& block = fm->blocks[link->block_id];
			arridx_t idx = block.get_linkid(link->uid);
			if (idx.u.index > block.link_count) {
				return -5;
			}
			if (sizeof(idx) != fwrite(&idx, 1, sizeof(idx), fp)) {
				return -6;
			}
		}
	}
	assert(blkinfo.ref_link_count >= blkinfo.link_count);
	return 0;
}

arridx_t fullmap_blockinfo::get_linkid(uid_t uid)
{
	int mid;
	int start = 0;
	int end = link_count - 1;
	arridx_t ret;

	ret.all = link_count + 1;
	if (prepare_link_array()) {
		return ret;
	}

	// find the item
	while (start <= end)
	{
		mid = (start + end) / 2;
		uid_t val = link_array[mid]->uid;

		if (uid == val) {
			ret.u.index = mid;
			ret.u.blkid = block_id;
			return ret;
		}
		else if (uid > val) start = mid + 1;
		else end = mid - 1;
	}
	// error
	return ret;
}

arridx_t fullmap_blockinfo::get_wayid(uid_t uid)
{
	int mid;
	int start = 0;
	int end = way_count - 1;
	arridx_t ret;

	ret.all = way_count + 1;
	if (prepare_way_array()) {
		return ret;
	}

	// find the item
	while (start <= end)
	{
		mid = (start + end) / 2;
		uid_t val = way_array[mid]->uid;

		if (uid == val) {
			ret.u.index = mid;
			ret.u.blkid = block_id;
			return ret;
		}
		else if (uid > val) start = mid + 1;
		else end = mid - 1;
	}
	// error
	return ret;
}

arridx_t fullmap_blockinfo::get_nodeid(uid_t uid)
{
	int mid;
	int start = 0;
	int end = node_count - 1;
	arridx_t ret;

	ret.all = node_count + 1;
	if (prepare_node_array()) {
		return ret;
	}

	// find the item
	while (start <= end)
	{
		mid = (start + end) / 2;
		uid_t val = node_array[mid]->uid;

		if (uid == val) {
			ret.u.index = mid;
			ret.u.blkid = block_id;
			return ret;
		}
		else if (uid > val) start = mid + 1;
		else end = mid - 1;
	}
	// error
	return ret;
}

int fullmap_blockinfo::write_links(FILE* fp,
	fullmap_file_blockinfo_v1& blkinfo)
{
	if (!link_count) {
		return 0;
	}
	fullmap_file_linkinfo_v1 linkinfo;

	long tmp = ftell(fp);
	blkinfo.start_of_link_table = tmp - blkinfo.start_of_blockdata;
	for (int i = 0; i < link_count; ++i) {
		auto* link = link_array[i];
		if (link->serialize(blkinfo, linkinfo)) {
			return -1;
		}

		if (sizeof(linkinfo) != fwrite(&linkinfo, 1, sizeof(linkinfo), fp)) {
			return -2;
		}
	}
	return 0;
}

int fullmap_blockinfo::write_ways(FILE* fp,
	fullmap_file_blockinfo_v1& blkinfo)
{
	if (!way_count) {
		return 0;
	}
	fullmap_file_wayinfo_v1 wayinfo;

	long tmp = ftell(fp);
	blkinfo.start_of_way_table = tmp - blkinfo.start_of_blockdata;
	for (int i = 0; i < way_count; ++i) {
		auto* way = way_array[i];
		if (way->serialize(blkinfo, wayinfo, strsect)) {
			return -1;
		}
		if (sizeof(wayinfo) != fwrite(&wayinfo, 1, sizeof(wayinfo), fp)) {
			return -2;
		}
	}
	return 0;
}

int fullmap_blockinfo::write_string_table(FILE* fp, fullmap_file_blockinfo_v1& blkinfo)
{
	long pos = ftell(fp);
	blkinfo.start_of_string_table = pos32_t(pos
		- blkinfo.start_of_blockdata);

	// write data
	size_t size	= strsect.getsize();
	if (size != fwrite(strsect.getbuf(), 1, size, fp)) {
		return -1;
	}
	return 0;
}

int fullmap_blockinfo::finalize(FILE* fp, fullmap_file_blockinfo_v1& blkinfo)
{
	// save the file position temporary
	long tmpos = ftell(fp);
	long pos = sizeof(fullmap_fileheader_v1) + sizeof(fullmap_file_keyinfo_v1)
		+ sizeof(fullmap_file_blockinfo_v1) * block_id;
	fseek(fp, pos, SEEK_SET);
	if (sizeof(blkinfo) != fwrite(&blkinfo, 1, sizeof(blkinfo), fp)) {
		return -1;
	}

	// restore the position
	fseek(fp, tmpos, SEEK_SET);
	return 0;
}

int fullmap_blockinfo::prepare_way_array(void)
{
	if (way_array || !way_count) {
		return 0;
	}

	way_array = new fullmap_wayinfo* [way_count];
	if (nullptr == way_array) {
		return -1;
	}

	int i;
	auto* item = avl_first(way_tree);
	for (i = 0; item; item = avl_next(item), ++i) {
		auto* wayinfo = AVLNODE_ENTRY(fullmap_wayinfo, avlnode, item);
		way_array[i] = wayinfo;
	}
	if (i != way_count) {
		return -2;
	}
	return 0;
}

int fullmap_blockinfo::prepare_link_array(void)
{
	if (link_array || !link_count) {
		return 0;
	}

	link_array = new fullmap_linkinfo* [link_count];
	if (nullptr == link_array) {
		return -1;
	}

	int i;
	auto* item = avl_first(link_tree);
	for (i = 0; item; item = avl_next(item), ++i) {
		auto* linkinfo = AVLNODE_ENTRY(fullmap_linkinfo, avlnode, item);
		link_array[i] = linkinfo;
	}
	if (i != link_count) {
		return -2;
	}
	return 0;
}

int fullmap_blockinfo::prepare_node_array(void)
{
	if (node_array || !node_count) {
		return 0;
	}

	node_array = new fullmap_nodeinfo* [node_count];
	if (nullptr == node_array) {
		return -1;
	}

	int i;
	auto* item = avl_first(node_tree);
	for (i = 0; item; item = avl_next(item), ++i) {
		auto* nodeinfo = AVLNODE_ENTRY(fullmap_nodeinfo, avlnode, item);
		node_array[i] = nodeinfo;
	}
	if (i != node_count) {
		return -2;
	}
	return 0;
}

int fullmap_blockinfo::write(FILE* fp, fullmap_level* fm)
{
	long i, tmp;
	fullmap_file_blockinfo_v1 blkinfo;
	memset(&blkinfo, 0, sizeof(blkinfo));

	if (prepare_way_array()) {
		return -1;
	}
	if (prepare_node_array()) {
		return -2;
	}
	if (prepare_link_array()) {
		return -3;
	}

	blkinfo.node_count = node_count;
	blkinfo.link_count = link_count;
	blkinfo.way_count = way_count;
	blkinfo.start_of_blockdata = ftell(fp);

	// write nodes
	if (write_nodes(fp, blkinfo, fm)) {
		return -4;
	}

	// write links
	if (write_links(fp, blkinfo)) {
		return -5;
	}

	// write ways
	if (write_ways(fp, blkinfo)) {
		return -6;
	}

	// write string table
	if (write_string_table(fp, blkinfo)) {
		return -7;
	}

	// finish
	if (finalize(fp, blkinfo)) {
		return -8;
	}
	return 0;
}

fullmap_blockinfo::~fullmap_blockinfo()
{
	// release node tree
	// release way tree
	// release way array
	// release link array
}

int fullmap_nodeinfo::serialize(fullmap_file_blockinfo_v1& blkinfo,
	fullmap_file_nodeinfo_v1& nodeinfo)
{
	nodeinfo.uid = uid;
	nodeinfo.lat = node->lat;
	nodeinfo.lon = node->lon;
	nodeinfo.link_count = fanin_link_count + fanout_link_count;
	memset(&nodeinfo.attr, 0, sizeof(nodeinfo.attr));
	nodeinfo.extra_info = 0;

	// update kd-tree info: left
	nodeinfo.left_id.all = UINT64_MAX;
	nodeinfo.right_id.all = UINT64_MAX;
	if (left) {
		auto* block = left->block;
		if (nullptr == block) {
			return -1;
		}
		nodeinfo.left_id = block->get_nodeid(left->uid);
		if (nodeinfo.left_id.u.index >= block->node_count) {
			return -2;
		}
	}
	if (right) {
		auto* block = right->block;
		if (nullptr == block) {
			return -1;
		}
		nodeinfo.right_id = block->get_nodeid(right->uid);
		if (nodeinfo.right_id.u.index >= block->node_count) {
			return -3;
		}
	}

	nodeinfo.attr.kdtree_dimension = f.kdtree_split;
	
	if (node->attr.is_keynode) {
		nodeinfo.attr.keynode = 1;
		if (node->data) {
			nodeinfo.extra_info = ++blkinfo.node_extdata_count;
		}
	}
	return 0;
}

int fullmap_nodeinfo::uid_avl_comp(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(fullmap_nodeinfo, avlnode, a);
	auto* bb = AVLNODE_ENTRY(fullmap_nodeinfo, avlnode, b);
	if (aa->uid > bb->uid) return 1;
	else if (aa->uid < bb->uid) return -1;
	else return 0;
}

int fullmap_nodeinfo::uid_gblavl_comp(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(fullmap_nodeinfo, gbl_avlnode, a);
	auto* bb = AVLNODE_ENTRY(fullmap_nodeinfo, gbl_avlnode, b);
	if (aa->uid > bb->uid) return 1;
	else if (aa->uid < bb->uid) return -1;
	else return 0;
}

int fullmap_wayinfo::way_distance(osm_way* way)
{
	int cnt = way->node_set.getsize();
	if (cnt-- < 2) {
		return -1;
	}
	for (int i = 0; i < cnt; ++i) {
		auto* node1 = way->node_set.buffer()[i];
		auto* node2 = way->node_set.buffer()[i + 1];
		assert(nullptr != node1 && nullptr != node2);

		distance += osm_map::distance(node1->lon,
			node1->lat, node2->lon, node2->lat);
	}
	// see if the way is a close loop
	if (way->attr.close_loop) {
		auto* node1 = way->node_set.buffer()[cnt];
		auto* node2 = way->node_set.buffer()[0];
		distance += osm_map::distance(node1->lon,
			node1->lat, node2->lon, node2->lat);
	}
	return 0;
}

int fullmap_wayinfo::serialize(fullmap_file_blockinfo_v1&,
	fullmap_file_wayinfo_v1& wayinfo, string_section& ss)
{
	wayinfo.uid = uid;
	wayinfo.distance = distance;

	// serialize the way attributes
	wayinfo.attr.oneway			= way->attr.is_oneway;
	wayinfo.attr.highway		= way->attr.is_highway;
	wayinfo.attr.bridge			= way->attr.is_bridge;
	wayinfo.attr.close_loop		= way->attr.close_loop;
	wayinfo.attr.lanes			= way->attr.lanes;
	wayinfo.attr.maxspeed		= way->attr.maxspeed;
	wayinfo.attr.highway_type	= way->attr.highway_type;

	if (way->name) {
		size_t pos;
		if (!ss.alloc_copy_string(way->name, pos)) {
			return -1;
		}
		wayinfo.way_name = pos;
	}
	else wayinfo.way_name = 0;
	return 0;
}

int fullmap_wayinfo::uid_avl_comp(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(fullmap_wayinfo, avlnode, a);
	auto* bb = AVLNODE_ENTRY(fullmap_wayinfo, avlnode, b);
	if (aa->uid > bb->uid) return 1;
	else if (aa->uid < bb->uid) return -1;
	else return 0;
}

int fullmap_linkinfo::serialize(fullmap_file_blockinfo_v1&,
	fullmap_file_linkinfo_v1& linkinfo)
{
	linkinfo.uid = uid;
	linkinfo.distance = osm_map::distance(
		node1->node->lon, node1->node->lat,
		node2->node->lon, node2->node->lat);

	fullmap_blockinfo* block = node1->block;
	if (nullptr == block) return -1;

	linkinfo.way_id = block->get_wayid(way->uid);
	if (linkinfo.way_id.u.index >= block->way_count) {
		// way id not found
		return -2;
	}

	linkinfo.node1_id = block->get_nodeid(node1->uid);
	if (linkinfo.node1_id.u.index >= block->node_count) {
		return -3;
	}
	if (nullptr == node2->block) {
		return -4;
	}
	linkinfo.node2_id = node2->block->get_nodeid(node2->uid);
	if (linkinfo.node2_id.u.index
		>= node2->block->node_count) {
		return -5;
	}

	linkinfo.prev_link = linkinfo.next_link = 0;
	linkinfo.prev_link_blockid = linkinfo.next_link_blockid = 0;
	if (prev) {
		linkinfo.prev_link = prev->uid;
		assert(prev->block_id >= 0);
		linkinfo.prev_link_blockid = prev->block_id;
	}
	if (next) {
		linkinfo.next_link = next->uid;
		assert(next->block_id >= 0);
		linkinfo.next_link_blockid = next->block_id;
	}
	return 0;
}

int fullmap_linkinfo::uid_avl_comp(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(fullmap_linkinfo, avlnode, a);
	auto* bb = AVLNODE_ENTRY(fullmap_linkinfo, avlnode, b);
	if (aa->uid > bb->uid) return 1;
	else if (aa->uid < bb->uid) return -1;
	else return 0;
}

} // end of namespace osm_parser1
/* EOF */
