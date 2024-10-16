#include<stdio.h>
#include <math.h>
#include <iostream>
#include <algorithm>
#include <stack>
#include<queue>

#include "planning.h"
#include "generator.h"
#include "osm-map.h"

namespace osm_parser1 {


#define EARTH_RADIUS 6378.137
#define PI 3.1415926535898

planning_map_node::planning_map_node():_edgesize(0)
{
	listnode_init(_edgelist);
}
planning_map_node::~planning_map_node()
{
	while (!listnode_isempty(_edgelist))
	{
		listnode_t* node = _edgelist.next;
		listnode_del(*node);
	}
}

planning_map_block::planning_map_block():id(0),minlat(0.),minlon(0.),maxlat(0.),maxlon(0.),_nodecnt(0),_kdtree(nullptr)
{
	listnode_init(_nodelist);
}

planning_map_block::~planning_map_block()
{
	while (!listnode_isempty(_nodelist))
	{
		listnode_t* node = _nodelist.next;
		listnode_del(*node);
	}
}

void dijgraph::addedge(int64_t s, int64_t e, double w) {
	// printf("s = %ld, e = %ld, w = %f.\n", s, e, w);
	// if (s < v_count_ && e < v_count_) {
	// 	adj_[s].emplace_back(s, e, w) ;
	// }
	std::map<int64_t, std::vector<edge>>::iterator it = adj_.find(s);
	if (it != adj_.end()) {
		adj_[s].emplace_back(s, e, w);
	}
	else {
		std::vector<edge> edges;
		edges.emplace_back(s, e, w);
		adj_[s] = edges;
	}
}

void dijgraph::dijkstra(int64_t s, int64_t e, std::vector<int64_t> &plannodes) {
	size_t sz = adj_.size();
	std::map<int64_t, int64_t> parent;
	// std::vector<int64_t> parent(sz); // 用于还原最短路径, 当前点最短路径的父节点
	std::vector<vertex> vertexes;   // 用于记录s到当前点的最短距离
	std::map<int64_t, bool> shortest;
	// for (int i = 0; i < v_count_; ++i) {
	// 	vertexes[i] = vertex(i, numeric_limits<int>::max());
	// }
	std::map<int64_t, std::vector<edge>>::iterator iter;
	iter = adj_.begin();
	while (iter != adj_.end())
	{
		int64_t s = iter->first;
		vertexes.emplace_back(s, numeric_limits<double>::max());
		shortest[s] = false;
		iter++;
	}
	
	struct cmp {
		bool operator() (const vertex &v1, const vertex &v2) { return v1.dist_ > v2.dist_;}
	};
	// 小顶堆
	priority_queue<vertex, std::vector<vertex>, cmp> queue;
	// 标记是否已找到最短距离
	// std::vector<bool> shortest(v_count_, false);

	// vertexes[s].dist_ = 0;
	// queue.push(vertexes[s]);
	for (int i = 0; i < sz; ++i) {
		if (vertexes[i].id_ == s) {
			vertexes[i].dist_ = 0;
			queue.push(vertexes[i]);
			break;
		}
	}
	while (!queue.empty()) {
		vertex minVertex = queue.top(); // 保证小顶堆出来的点一定是最小的。
		queue.pop();
		if (minVertex.id_ == e) { break; }
		if (shortest[minVertex.id_]) { continue; } // 之前更新过，是冗余备份
		shortest[minVertex.id_] = true;
		for (int i = 0; i < adj_[minVertex.id_].size(); ++i) { // 遍历节点连通的边
			edge cur_edge = adj_[minVertex.id_].at(i); // 取出当前连通的边
			uint64_t next_vid = cur_edge.eid_;
			int verindex = 0;
			for (int i = 0; i < sz; ++i) {
				if (vertexes[i].id_ == next_vid) {
					verindex = i;
					break;
				}
			}
			if (minVertex.dist_ + cur_edge.w_ < vertexes[verindex].dist_) {
				vertexes[verindex].dist_ = minVertex.dist_ + cur_edge.w_;
				parent[next_vid] = minVertex.id_;
				queue.push(vertexes[verindex]);
			}
		}
	}
	// cout << s;
	print(s, e, parent, plannodes);
	if (!plannodes.empty()) {
		plannodes.insert(plannodes.begin(), s);
	}
}

void dijgraph::print(int64_t s, int64_t e, std::map<int64_t, int64_t> parent, std::vector<int64_t> &plannodes) {
	if (parent.count(e) == 0) return;
	if (s == e) return;
	print(s, parent[e], parent, plannodes);
	plannodes.push_back(e);
	// cout << "->" << e ;
}

planning_map::planning_map ()
{
}

planning_map::~planning_map ()
{
}

void planning_map_block::choose_split(planning_map_node** exm_set, size_t size, 
	int &split) 
{
	double tmp1 = 0., tmp2 = 0.;
	for (int i = 0; i < size; ++i) {
		auto* nd = exm_set[i];
		tmp1 += 1.0 / (double)size * nd->lat * nd->lat;
		tmp2 += 1.0 / (double)size * nd->lat;
    }
	double v1 = tmp1 - tmp2 * tmp2; //compute variance on the x dimension
	
	tmp1 = tmp2 = 0;
	for (int i = 0; i < size; ++i) {
		auto* nd = exm_set[i];
		tmp1 += 1.0 / (double)size * nd->lon * nd->lon;
		tmp2 += 1.0 / (double)size * nd->lon;
	}
    double v2 = tmp1 - tmp2 * tmp2; //compute variance on the y dimension
	
	split = (v1 > v2) ? 0 : 1; //set the split dimension
	if (split == 0) {
		qsort(exm_set, size, sizeof(planning_map_node*), cmp_lat);
	} else {
		qsort(exm_set, size, sizeof(planning_map_node*), cmp_lon);
	}
}

planning_map_kdtree* planning_map_block::build_kdtree(planning_map_node **exm_set, size_t size)
{
	if (size == 0){
		return nullptr;
	} 

	int split;
	planning_map_node** exm_set_left = nullptr;
	planning_map_node** exm_set_right = nullptr;

	choose_split(exm_set, size, split);

	int size_left = size / 2;
	int size_right = size - size_left - 1;

	if (size_left > 0) {
		exm_set_left = new planning_map_node* [size_left];
		assert(nullptr != exm_set_left);
		memcpy(exm_set_left, exm_set, sizeof(void*) * size_left);		
	}
	if (size_right > 0) {
		exm_set_right = new planning_map_node* [size_right];
		assert(nullptr != exm_set_right);
		memcpy(exm_set_right, exm_set
			+ size / 2 + 1, sizeof(void*) * size_right);
	}

	auto* ret = exm_set[size / 2];
	
	auto* kd = new planning_map_kdtree();
	kd->lat = ret->lat;
	kd->lon = ret->lon;
	kd->split = split;
	kd->node = ret;

	// build the sub kd-tree
	kd->leftnode = build_kdtree(exm_set_left, size_left);
	kd->rightnode = build_kdtree(exm_set_right, size_right);

	// free temp memory
	if (exm_set_left) delete [] exm_set_left;
	if (exm_set_right) delete [] exm_set_right;
	return kd;
}

int planning_map_block::cmp_lat(const void* node1, const void* node2)
{
	auto* n1 = *((planning_map_node**)node1);
	auto* n2 = *((planning_map_node**)node2);
	if(n1->lat < n2->lat) {
		return -1;
	} else if (n1->lat > n2->lat) {
		return 1;
	} else {
		return 0;
	}
}

int planning_map_block::cmp_lon(const void* node1, const void* node2)
{
	auto* n1 = *((planning_map_node**)node1);
	auto* n2 = *((planning_map_node**)node2);
	if (n1->lon < n2->lon) {
		return -1;
	} else if (n1->lon > n2->lon) {
		return 1;
	} else {
		return 0;
	}
}

bool planning_map_block::equal(planning_map_node* node1, planning_map_node* node2)
{
	auto* aa = *((fullmap_nodeinfo**)node1);
	auto* bb = *((fullmap_nodeinfo**)node2);
	if (node1->lat == node2->lat && node1->lon == node2->lon)
		return true;
	else
		return false;
}

double planning_map_block::rad(double angle)
{
	return angle * PI / 180.0;
}

double planning_map_block::distance(planning_map_node* data1, planning_map_node* data2)
{
	double radLat1 = rad(data1->lat);
	double radLat2 = rad(data2->lat);
	double a = radLat1 - radLat2;
	double b = rad(data1->lon) - rad(data2->lon);
	double s = 2 * asin(sqrt(pow(sin(a/2),2) + cos(radLat1)*cos(radLat2)*pow(sin(b/2),2)));
	s = s * EARTH_RADIUS;
	s = (int)(s * 10000000) / 10000;
	return s;
}

double planning_map::rad(double angle)
{
	return angle * PI / 180.0;
}

double planning_map::distance(planning_map_node* data1, planning_map_node* data2)
{
	double radLat1 = rad(data1->lat);
	double radLat2 = rad(data2->lat);
	double a = radLat1 - radLat2;
	double b = rad(data1->lon) - rad(data2->lon);
	double s = 2 * asin(sqrt(pow(sin(a/2),2) + cos(radLat1)*cos(radLat2)*pow(sin(b/2),2)));
	s = s * EARTH_RADIUS;
	s = (int)(s * 10000000) / 10000;
	return s;
}

int planning_map::get_level(planning_map_level** plvl, int lvlid) {
	if (_level.empty()) return -1;
	for (size_t i = 0; i < _level.size(); i++) {
		planning_map_level* lvl = _level[i];
		if (lvl->level == lvlid) {
			*plvl = lvl;
		} 
	}
	return 0;
}

void planning_map::generate_level(map_scope scope, map_level* level, int lvlid)
{
	planning_map_level* plvl = new planning_map_level();
	plvl->level = lvlid;
	plvl->row = level->row;
	plvl->col = level->col;

	int total = level->col * level->row;

	double latstep, lonstep;
	plvl->minlat = scope.minlat;
	plvl->maxlat = scope.maxlat;
	plvl->minlon = scope.minlon;
	plvl->maxlon = scope.maxlon;

	plvl->latstep = latstep = fabs(plvl->maxlat - plvl->minlat) / level->row;
	plvl->lonstep = lonstep = fabs(plvl->maxlon - plvl->minlon) / level->col;

	int index = 0;
	for(int i=0; i < level->row; i++) {
		for(int j=0; j < level->col; j++) {
			planning_map_block* pblock = new planning_map_block();
			pblock->id = index;

			pblock->minlat = i * latstep + plvl->minlat;
			pblock->maxlat = (i + 1) * latstep + plvl->minlat;
			pblock->minlon = j * lonstep + plvl->minlon;
			pblock->maxlon = (j + 1) * lonstep + plvl->minlon;

			if(pblock->maxlat > scope.maxlat)
				pblock->maxlat = scope.maxlat;
			if(pblock->maxlon > scope.maxlon)
				pblock->maxlon = scope.maxlon;

			plvl->_blocks.push_back(pblock);

			index++;
		}
	}
	_level.push_back(plvl);
}

int planning_map::find_block(planning_map_level* plvl, double lat, double lon, planning_map_block** pblk)
{
	if(lat>plvl->maxlat || lon > plvl->maxlon || 
		lat < plvl->minlat || lon < plvl->minlon) {
		return -1;
	} 
	if (plvl->_blocks.empty()) return -2;
	for (size_t blksz = 0; blksz < plvl->_blocks.size(); blksz++) {
		planning_map_block* blk = plvl->_blocks[blksz];
		if (lat <= blk->maxlat && lat >= blk->minlat && 
			lon <= blk->maxlon && lon >= blk->minlon) {
			*pblk = blk;
			break;
		}
	}
	return 0;
}

// int planning_map::find_node_block_index(planning_map_block* pblk, planning_map_node* pnode, size_t &index) 
// {
// 	if (!pblk) return -1;
// 	if (!pnode) return -2;
// 	listnode_t* nodes = pblk->get_nodes();
// 	auto* item = nodes->next;
// 	for (; item != nodes; item = item->next) {
// 		auto* pmnode = list_entry(planning_map_node, ownerlist, item);
// 		if (pmnode->uid == pnode->uid) {
// 			return 0;
// 		}
// 		++index;
// 	}
// 	return -3;
// }

void planning_map::generate_node(planning_map_level* plvl, osm_node* node) 
{
	if(1 == node->attr.is_keynode) {
		planning_map_block* pmblock = nullptr;
		if(!find_block(plvl, node->lat, node->lon, &pmblock)) {
			planning_map_node *pmnode = new planning_map_node();
			pmnode->uid = node->id;
			pmnode->lat = node->lat;
			pmnode->lon = node->lon;
			pmnode->blockid = pmblock->id;
			pmnode->inblockindex = pmblock->get_node_count();
			pmblock->add_node(pmnode);
			avl_insert(&plvl->_gbl_avlnode, &pmnode->avlnode, 	
				planning_map::uid_avl_comp);
		}
	}
}

void planning_map::build_block_kdtree() {
	if (_level.empty()) return;
	for (size_t lvlsz = 0; lvlsz < _level.size(); lvlsz++) {
		planning_map_level* plvl = _level[lvlsz];
		if (plvl->_blocks.empty()) continue;
		for (size_t bksz = 0; bksz < plvl->_blocks.size(); bksz++) {
			planning_map_block* pblk = plvl->_blocks[bksz];
			pblk->create_kdtree();
		}
	}
}

int planning_map::uid_avl_comp(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(planning_map_node, avlnode, a);
	auto* bb = AVLNODE_ENTRY(planning_map_node, avlnode, b);
	if (aa->uid > bb->uid) return 1;
	else if (aa->uid < bb->uid) return -1;
	else return 0;
}

void planning_map::generate_edge(osm_node* node)
{
	// avl_node_t* pmitem = avl_first(_gbl_avlnode);
	// for (int i = 0; pmitem; pmitem = avl_next(pmitem), ++i) {
	for (size_t lvlsz = 0; lvlsz < _level.size(); lvlsz++) {
		planning_map_level* plvl = _level[lvlsz];
		auto* pnode = avl_find(plvl->_gbl_avlnode,
					MAKE_FIND_OBJECT(node->id, planning_map_node, uid, avlnode), planning_map::uid_avl_comp);
		if (pnode) {
			auto* pmnode = AVLNODE_ENTRY(planning_map_node, avlnode, pnode);
			auto* item = node->wayref_list.next;
			for (; item != &node->wayref_list;
				item = item->next) {
				auto* wref = list_entry(osm_wayref, node_ownerlist, item);
				uint32_t ndindex = wref->ndref_index;
				size_t nodesize = wref->way->node_set.getsize();
				for(int j = ndindex + 1; j < nodesize; j++) {
					auto* nextnode = (osm_node*)wref->way->node_set.buffer()[j];
					if(nextnode ->attr.is_keynode == 1) {
						int64_t nextuid = nextnode->id;
						auto* retnode = avl_find(plvl->_gbl_avlnode,
						MAKE_FIND_OBJECT(nextnode->id, planning_map_node, uid, avlnode), planning_map::uid_avl_comp);
						if(retnode) {
							auto* nextpmnode = AVLNODE_ENTRY(planning_map_node, avlnode, retnode);
							if (pmnode->uid == nextpmnode->uid) {
								continue;
							}
							planning_map_edge* edge = new planning_map_edge();
							edge->start_node = pmnode;
							edge->end_node = nextpmnode;
							edge->distance = distance(pmnode, nextpmnode);
							edge->weight = edge->distance;
							pmnode->add_edge(edge);
							if(1 != wref->way->attr.is_oneway) {
								edge = new planning_map_edge();
								edge->start_node = nextpmnode;
								edge->end_node = pmnode;
								edge->distance = distance(pmnode, nextpmnode);
								edge->weight = edge->distance;
								nextpmnode->add_edge(edge);
							}
						}
						break;
					}
				}
			}
		}
	}
}

void planning_map::show_block_info()
{
	// for(int i= 0;i< _blocks.size(); i++) {
	// 	printf("block %ld node count is %d.\n", _blocks[i]->id, _blocks[i]->get_node_count());
	// }
}

size_t planning_map::get_node_size() {
	size_t nodesz = 0;
	for (size_t lvlsz = 0; lvlsz < _level.size(); lvlsz++) {
		planning_map_level* plvl = _level[lvlsz];
		for (size_t blksz = 0; blksz < plvl->_blocks.size(); blksz++) {
			planning_map_block* pblk = plvl->_blocks[blksz];
			nodesz += sizeof(planning_map_file_nodeinfo) * pblk->get_node_count();
		}
	}
	return nodesz;
}

size_t planning_map::get_kdnode_size()
{
	size_t kdnodesz = 0;
	for (size_t lvlsz = 0; lvlsz < _level.size(); lvlsz++) {
		planning_map_level* plvl = _level[lvlsz];
		for (size_t blksz = 0; blksz < plvl->_blocks.size(); blksz++) {
			planning_map_block* pblk = plvl->_blocks[blksz];
			kdnodesz += sizeof(planning_map_file_node_kdtree) * pblk->get_node_count();
		}
	}
	return kdnodesz;
}

void planning_map::generate_maplevel(planning_map_file_level* file_level, planning_map_level* plvl)
{
	plvl->level = file_level->level;
	plvl->row = file_level->row;
	plvl->col = file_level->col;
	plvl->latstep = file_level->latstep;
	plvl->lonstep = file_level->lonstep;
	plvl->minlat = file_level->minlat;
	plvl->maxlat = file_level->maxlat;
	plvl->minlon = file_level->minlon;
	plvl->maxlon = file_level->maxlon;
}

void planning_map::generate_mapblock(planning_map_file_blockinfo* pfblk, planning_map_block* pblk)
{
	pblk->id = pfblk->id;
	pblk->maxlat = pfblk->maxlat;
	pblk->maxlon = pfblk->maxlon;
	pblk->minlat = pfblk->minlat;
	pblk->minlon = pfblk->minlon;
}

void planning_map::generate_mapnode(planning_map_file_nodeinfo* pfnode, planning_map_node* pnode)
{
	pnode->uid = pfnode->uid;
	pnode->blockid = pfnode->blockid;
	pnode->lat = pfnode->data.lat;
	pnode->lon = pfnode->data.lon;
	// avl_insert(&_gbl_avlnode, &pnode->avlnode, planning_map::uid_avl_comp);
}

void planning_map::generate_mapedge(planning_map_file_edge* pfedge, planning_map_edge* pedge)
{
	pedge->uid = pfedge->uid;
	pedge->distance = pfedge->distance;
	pedge->weight = pfedge->weight;
	planning_map_node* startNode = new planning_map_node();
	startNode->blockid = pfedge->start_node.u.blkid;
	startNode->inblockindex = pfedge->start_node.u.index;
	pedge->start_node = startNode;
	planning_map_node* endNode = new planning_map_node();
	endNode->blockid = pfedge->end_node.u.blkid;
	endNode->inblockindex = pfedge->end_node.u.index;
	pedge->end_node = endNode;
}

int planning_map::generate_filelevel(planning_map_level* plvl, planning_map_file_level* file_level)
{
	file_level->level = plvl->level;
	file_level->row = plvl->row;
	file_level->col = plvl->col;
	file_level->latstep = plvl->latstep;
	file_level->lonstep = plvl->lonstep;
	file_level->minlat = plvl->minlat;
	file_level->maxlat = plvl->maxlat;
	file_level->minlon = plvl->minlon;
	file_level->maxlon = plvl->maxlon;
	file_level->block_count = plvl->_blocks.size();
	return 0;
}

void planning_map::deserial_planningmap()
{
	_level.clear();

	_planning_map_file = fopen("/home/coder/map/planning_map", "r+");
	fseek (_planning_map_file , 0 , SEEK_END);  
    size_t fsz = ftell (_planning_map_file);  
    rewind (_planning_map_file);
	char* buffer = (char*) malloc (sizeof(char)*fsz);  
	fread (buffer, 1, fsz, _planning_map_file);  

	planning_map_fileheader* fheader = nullptr;
	size_t addr = deserial_header(buffer, &fheader);
	addr = deserial_level(buffer, fheader);

	free(buffer);
	fclose(_planning_map_file);


	// for (size_t lvlsz = 0; lvlsz < _level.size(); lvlsz++) {
	// 	size_t blkcnt = 0;
	// 	size_t blknodecnt = 0;
	// 	planning_map_level* plvl = _level[lvlsz];
	// 	for (size_t blksz = 0; blksz < plvl->_blocks.size(); blksz++) {
	// 		planning_map_block* pblk = plvl->_blocks[blksz];
	// 		blkcnt++;
	// 		blknodecnt += pblk->get_node_count();
	// 	}
	// 	printf("level %d, block count %ld, node count %ld.\n", plvl->level, blkcnt, blknodecnt);
	// 	blkcnt = 0;
	// 	blknodecnt = 0;
	// }
}

size_t planning_map::deserial_header(char* buffer, planning_map_fileheader** header)
{
	*header = reinterpret_cast<planning_map_fileheader*>(buffer);
	size_t hsz = sizeof(planning_map_fileheader);
	return hsz;
}

size_t planning_map::deserial_level(char* buffer, planning_map_fileheader* header)
{
	int lvlsz = header->level_count;
	pos_t start_post = header->start_of_level_table;
	for (int i = 0; i < lvlsz; i++) {
		size_t lvladdr = start_post + sizeof(planning_map_file_level) * i;
		planning_map_file_level* pflvl = reinterpret_cast<planning_map_file_level*>(buffer + lvladdr);
		planning_map_level* plvl = new planning_map_level();
		generate_maplevel(pflvl, plvl);
		deserial_block(buffer, pflvl, plvl);
		_level.push_back(plvl);

		create_planning_map_edge(plvl);
	}
	return start_post;
}

void planning_map::create_planning_map_edge(planning_map_level* plvl)
{
	for(int i = 0; i < plvl->_blocks.size(); i++) {
		listnode_t* nodes = plvl->_blocks[i]->get_nodes();
		auto* item = nodes->next;
		for (; item != nodes; item = item->next) {
			auto* pmnode = list_entry(planning_map_node, ownerlist, item);
			find_edge_node(pmnode, plvl);
		}
	}
}

void planning_map::find_edge_node(planning_map_node* pmnode, planning_map_level* plvl)
{
	listnode_t* edgelist = pmnode->get_edges();
	auto* edgeitem = edgelist->next;
	for(;edgeitem != edgelist; edgeitem = edgeitem->next) {
		auto* pmedge = list_entry(planning_map_edge, ownerlist, edgeitem);
		int startblk = pmedge->start_node->blockid;
		int startindex = pmedge->start_node->inblockindex;
		planning_map_node* startnode = nullptr;
		find_edge_block_node(startblk, startindex, plvl, &startnode);
		int endblk = pmedge->end_node->blockid;
		int endindex = pmedge->end_node->inblockindex;
		planning_map_node* endnode = nullptr;
		find_edge_block_node(endblk, endindex, plvl, &endnode);
		if (startnode) {
			delete pmedge->start_node;
			pmedge->start_node = nullptr;
			pmedge->start_node = startnode;
		}
		if (endnode) {
			delete pmedge->end_node;
			pmedge->end_node = nullptr;
			pmedge->end_node = endnode;
		}
	}
}

void planning_map::find_edge_block_node(int blkid, int nodeindex, planning_map_level* plvl, planning_map_node** node)
{
	if (!plvl->_blocks.size()) return;
	if (blkid > plvl->_blocks.size()-1) return;
	for(int i = 0; i < plvl->_blocks.size(); i++) {
		planning_map_block* pblk = plvl->_blocks[i];
		if (pblk->id == blkid) {
			int index = 0;
			listnode_t* nodes = plvl->_blocks[i]->get_nodes();
			auto* item = nodes->next;
			for (; item != nodes; item = item->next) {
				auto* pmnode = list_entry(planning_map_node, ownerlist, item);
				if (nodeindex == index) {
					*node = pmnode;
					break;
				}
				index++;
			}
			break;
		}
	}
}

size_t planning_map::deserial_block(char* buffer, planning_map_file_level* pflvl, planning_map_level* plvl)
{
	int blkcnt = pflvl->block_count;
	pos_t start_post = pflvl->start_of_block_table;
	for (int i = 0; i < blkcnt; i++) {
		planning_map_file_blockinfo* pfblk = reinterpret_cast<planning_map_file_blockinfo*>(buffer + start_post);
		planning_map_block* pblk = new planning_map_block();
		generate_mapblock(pfblk, pblk);
		deserial_nodes(buffer, pfblk, pblk);
		plvl->_blocks.push_back(pblk);
		start_post += sizeof(planning_map_file_blockinfo);
	}
	return 0;
}

size_t planning_map::deserial_nodes(char* buffer, planning_map_file_blockinfo* pfblk, planning_map_block* pblk)
{
	int nodecnt = pfblk->node_count;
	pos_t start_post = pfblk->start_of_node_table;
	for (int i = 0; i < nodecnt; i++) {
		planning_map_file_nodeinfo* pfnode = reinterpret_cast<planning_map_file_nodeinfo*>(buffer + start_post);
		planning_map_node* pnode = new planning_map_node();
		generate_mapnode(pfnode, pnode);
		deserial_edges(buffer, pfnode, pnode);
		pblk->add_node(pnode);
		pnode->inblockindex = pblk->get_node_count();
		start_post += sizeof(planning_map_file_nodeinfo);
	}

	// int kdnodecnt = pfblk->kdtree_count;
	// pos_t kdstart_post = pfblk->start_of_kdtree_table;
	// planning_map_kdtree* pkdtree = nullptr;
	// for (int i = 0; i < kdnodecnt; i++) {
	// 	planning_map_file_node_kdtree* pfkdnode = reinterpret_cast<planning_map_file_node_kdtree*>(buffer + kdstart_post);
	// 	if ()

	// }
	// pblk->set_kdtree(pkdtree);
	pblk->create_kdtree();
	return 0;
}

size_t planning_map::deserial_edges(char* buffer, planning_map_file_nodeinfo* pfnode, planning_map_node* pnode)
{
	int edgecnt = pfnode->edge_count;
	pos_t start_post = pfnode->start_of_edge_table;
	for (int i = 0; i < edgecnt; i++) {
		planning_map_file_edge* pfedge = reinterpret_cast<planning_map_file_edge*>(buffer + start_post);
		planning_map_edge* pedge = new planning_map_edge();
		generate_mapedge(pfedge, pedge);
		pnode->add_edge(pedge);
		start_post += sizeof(planning_map_file_edge);
	}

	return 0;
}

int planning_map::get_nearby_node(planning_map_block* pblk, planning_map_node** pnode, planning_map_node* target, double &d)
{
	if (!pblk->get_kdtree()) return -1;

	//1. 如果Kd是空的，则设dist为无穷大返回
	//2. 向下搜索直到叶子结点
	stack<planning_map_kdtree*> search_path;
	planning_map_kdtree* pSearch = pblk->get_kdtree();
	planning_map_node* nearest = nullptr;
	double dist;
 
	while (pSearch != nullptr) {
		//pSearch加入到search_path中;
		search_path.push(pSearch);
 
		if (pSearch->split == 0) {
			if (target->lat <= pSearch->lat) { /* 如果小于就进入左子树 */
				pSearch = pSearch->leftnode;
			}
			else {
				pSearch = pSearch->rightnode;
			}
		}
		else {
			if (target->lon <= pSearch->lon) { /* 如果小于就进入左子树 */
				pSearch = pSearch->leftnode;
			}
			else {
				pSearch = pSearch->rightnode;
			}
		}
	}
	//取出search_path最后一个赋给nearest
	// nearest.x = search_path.top()->dom_elt.x;
	// nearest.y = search_path.top()->dom_elt.y;
	nearest = search_path.top()->node;
	search_path.pop();
 
	dist = distance(nearest, target);
	//3. 回溯搜索路径
	planning_map_kdtree* pBack;
	while (search_path.empty()) {
		//取出search_path最后一个结点赋给pBack
		pBack = search_path.top();
		search_path.pop();


 
		if (pBack->leftnode == nullptr && pBack->rightnode == nullptr) { /* 如果pBack为叶子结点 */
			if (distance(nearest, target) > distance(pBack->node, target)) {
				nearest = pBack->node;
				dist = distance(pBack->node, target);
			}
			continue;
		}
		if (pBack->split == 0) {
			if (fabs(pBack->lat - target->lat) < dist) { /* 如果以target为中心的圆（球或超球），半径为dist的圆与分割超平面相交， 那么就要跳到另一边的子空间去搜索 */
				if (distance(nearest, target) > distance(pBack->node, target)) {
					nearest = pBack->node;
					dist = distance(pBack->node, target);
				}
				if (target->lat <= pBack->lat) /* 如果target位于pBack的左子空间，那么就要跳到右子空间去搜索 */
					pSearch = pBack->rightnode;
				else
					pSearch = pBack->leftnode; /* 如果target位于pBack的右子空间，那么就要跳到左子空间去搜索 */
				if (pSearch != nullptr)
					//pSearch加入到search_path中
					search_path.push(pSearch);
			}
		}
		else {
			if (fabs(pBack->lon - target->lon) < dist) { /* 如果以target为中心的圆（球或超球），半径为dist的圆与分割超平面相交， 那么就要跳到另一边的子空间去搜索 */
				if (distance(nearest, target) > distance(pBack->node, target)) {
					nearest = pBack->node;
					dist = distance(pBack->node, target);
				}
				if (target->lon <= pBack->lon) /* 如果target位于pBack的左子空间，那么就要跳到右子空间去搜索 */
					pSearch = pBack->rightnode;
				else
					pSearch = pBack->leftnode; /* 如果target位于pBack的右子空间，那么就要跳到左子空间去搜索 */
				if (pSearch != nullptr)
					// pSearch加入到search_path中
					search_path.push(pSearch);
			}
		}
	}
 
	// nearestpoint.x = nearest.x;
	// nearestpoint.y = nearest.y;
	*pnode = nearest;
	d = dist;
	return 0;
}

void planning_map::planning_road(double lat1, double lon1, double lat2, double lon2, std::vector<int64_t> &plannodes)
{
	if (_level.empty()) return;
	int64_t lvlidx = 0;
	planning_map_node* n1 = nullptr;
	planning_map_node* n2 = nullptr;
	planning_map_node** pnode1 = &n1;
	planning_map_node** pnode2 = &n2;

	while (lvlidx < _level.size())
	{
		planning_nodes(lvlidx, pnode1, pnode2, lat1, lon1, lat2, lon2, plannodes);
	}
}

int planning_map::calc_dependblk_nodes(planning_map_level* plvl, double lat1, double lon1, double lat2, double lon2, int64_t nodeuid1, int64_t nodeuid2, std::vector<int64_t> &nodes)
{
	if (!plvl) return -1;

	dijgraph g{1000};

	std::vector<size_t> indexes;
	get_calc_block(plvl, lat1, lon1, lat2, lon2, indexes);
	for (size_t blksz = 0; blksz < indexes.size(); blksz++) {
		// printf("%ld, ", indexes[blksz]);
		planning_map_block* pblk = plvl->_blocks[indexes[blksz]];
		listnode_t* nodes = pblk->get_nodes();
		auto* item = nodes->next;
		for (; item != nodes; item = item->next) {
			auto* pmnode = list_entry(planning_map_node, ownerlist, item);
			listnode_t* edgelist = pmnode->get_edges();
			auto* edgeitem = edgelist->next;
			for(;edgeitem != edgelist; edgeitem = edgeitem->next) {
				auto* pmedge = list_entry(planning_map_edge, ownerlist, edgeitem);
				g.addedge(pmedge->start_node->uid, pmedge->end_node->uid, pmedge->distance);
			}
		}
	}

	g.dijkstra(nodeuid1, nodeuid2, nodes);
	return 0;
}

int planning_map::planning_nodes(int64_t &lvlidx, planning_map_node** pnode1, planning_map_node** pnode2, double lat1, double lon1, double lat2, double lon2, std::vector<int64_t> &nodes)
{
	planning_map_level* plvl = _level[lvlidx];

	planning_map_block* pblk1 = nullptr;
	planning_map_block* pblk2 = nullptr;

	find_block(plvl, lat1, lon1, &pblk1);
	find_block(plvl, lat2, lon2, &pblk2);

	planning_map_node target1;
	target1.lat = lat1;
	target1.lon = lon1;
	planning_map_node* near1;
	double distance1 = 0;
	get_nearby_node(pblk1, &near1, &target1, distance1);
	
	planning_map_node target2;
	target2.lat = lat2;
	target2.lon = lon2;
	planning_map_node* near2;
	double distance2 = 0;
	get_nearby_node(pblk2, &near2, &target2, distance2);

	if (*pnode1 == nullptr && *pnode2 == nullptr) {
		calc_dependblk_nodes(plvl, lat1, lon1, lat2, lon2, near1->uid, near2->uid, nodes);
		if (!nodes.empty()) {
			*pnode1 = near1;
			*pnode2 = near2;
		} 
	} else {
		if (*pnode1 && (*pnode1)->uid != near1->uid) {
			std::vector<int64_t> nearnodes;
			calc_dependblk_nodes(plvl, near1->lat, near1->lon, (*pnode1)->lat, (*pnode1)->lon, near1->uid, (*pnode1)->uid, nearnodes);
			if (!nearnodes.empty()) {
				nodes.insert(nodes.begin(), nearnodes.begin(), nearnodes.end());
				*pnode1 = near1;
			}
		}
		if (*pnode2 && (*pnode2)->uid != near2->uid) {
			std::vector<int64_t> nearnodes;
			calc_dependblk_nodes(plvl, near2->lat, near2->lon, (*pnode2)->lat, (*pnode2)->lon, near1->uid, (*pnode2)->uid, nearnodes);
			nodes.insert(nodes.end(), nearnodes.begin(), nearnodes.end());
			if (!nearnodes.empty()) {
				nodes.insert(nodes.begin(), nearnodes.begin(), nearnodes.end());
				*pnode2 = near2;
			}
		}
	}

	lvlidx++;
	return 0;
}

int planning_map::get_calc_block(planning_map_level* plvl, double lat1, double lon1, double lat2, double lon2, std::vector<size_t> &indexes)
{
	if (!plvl) return -1;
	if (plvl->_blocks.empty()) return -2;

	double distance = sqrt(pow(plvl->latstep, 2) + pow(plvl->lonstep, 2));

	double a = 0.,b = 0.;
	calc_line_paraemeter(a, b, lat1, lon1, lat2, lon2);
	
	double nlat1 = 0, nlon1 = 0, nlat2 = 0, nlon2 = 0;
	calc_new_point(lat1, lon1, lat2, lon2, a, b, nlat1, nlon1, nlat2, nlon2, distance);

	rectangle rect;
	pos p1;
	p1.lat = nlat1;
	p1.lon = nlon1;
	pos p2;
	p2.lat = nlat2;
	p2.lon = nlon2;
	calc_vertical_point(p1, p2, rect, distance);

	for(size_t sz = 0; sz < plvl->_blocks.size(); sz++) {
		planning_map_block* pblk = plvl->_blocks[sz];
		pos p1,p2,p3,p4;
		get_point_by_block(pblk, p1, p2, p3, p4);
		if (point_in_block(p1, rect)) {
			// block->show_block();
			indexes.push_back(sz);
			continue;
		}
		if (point_in_block(p2, rect)) {
			// block->show_block();
			indexes.push_back(sz);
			continue;
		}
		if (point_in_block(p3, rect)) {
			// block->show_block();
			indexes.push_back(sz);
			continue;
		}
		if (point_in_block(p4, rect)) {
			// block->show_block();
			indexes.push_back(sz);
			continue;
		}
	}
	return 0;
}

int planning_map::calc_line_paraemeter(double &a, double &b, double lat1, double lon1, double lat2, double lon2)
{
	if (lat1 == lat2 && lon1 == lon2) return -1;
	a = (lon2 - lon1) / (lat2 - lat1);
	b = lon1 - a*lat1;
	return 0;
}

int planning_map::calc_new_point(double lat1, double lon1, double lat2, double lon2, double a, double b, 
	double &nlat1, double &nlon1, double &nlat2, double &nlon2, double distance)
{
	if (lat1 == lat2 && lon1 == lon2) return -1;
	double x1 = 0., x2 = 0., x3 = 0., x4 = 0.;
	double y1 = 0., y2 = 0., y3 = 0., y4 = 0.;
	double delta = 0.;

	double pa = 0., pb = 0., pc = 0.;
	pa = pow(a,2) + 1;
	pb = 0 - (2 * lat1) - (2* a * (lon1 - b));
	pc = pow(lat1, 2) + pow((lon1-b),2) - pow(distance,2);
	delta = pb*pb - (4*pa*pc) ;
	// printf("delta = %f.\n", delta);
	if ( delta > 0 ) {
		x1 = (-pb-sqrt(delta)) / (2*pa);
		x2 = (-pb+sqrt(delta)) / (2*pa);
		if (lat1 > lat2) {
			nlat1 = x2;
		}
		else {
			nlat1 = x1;
		}
		// printf ("该方程有两解，x1 = %f\n,x2 = %f\n",x1,x2);
	}
	else if (delta == 0) {
		x1 = (-pb) / (2*pa);
		x2 = x1;
		nlat1 = x1;
		// printf ("该方程有唯一解，x1 = x2 = %f\n",x1 = x2);
	}
	else if (delta < 0) {
		// printf ("该方程无解\n");
		return -3;
	}
	nlon1 = a*nlat1 + b;

	pa = pow(a,2) + 1;
	pb = 0 - (2 * lat2) - (2* a * (lon2 - b));
	pc = pow(lat2, 2) + pow((lon2-b),2) - pow(distance,2);
	delta = pb*pb - (4*pa*pc) ;
	// printf("delta = %f.\n", delta);
	if ( delta > 0 ) {
		x1 = (-pb-sqrt(delta)) / (2*pa);
		x2 = (-pb+sqrt(delta)) / (2*pa);
		if (lat1 > lat2) {
			nlat2 = x1;
		}
		else {
			nlat2 = x2;
		}
		// printf ("该方程有两解，x1 = %f\n,x2 = %f\n",x1,x2);
	}
	else if (delta == 0) {
		x1 = (-pb) / (2*pa);
		x2 = x1;
		nlat2 = x1;
		// printf ("该方程有唯一解，x1 = x2 = %f\n",x1 = x2);
	}
	else if (delta < 0) {
		// printf ("该方程无解\n");
		return -3;
	}
	nlon2 = a*nlat2 + b;
	return 0;
}

int planning_map::calc_vertical_point(pos p1, pos p2, rectangle &rect, double distance)
{
	if (p1.lat == p2.lat && p1.lon == p2.lon) return -1;
	
	double lat = 0.;
	double lon = 0.;

	if (p1.lat > p2.lat) {
		lat = p1.lat - p2.lat;
		lon = p1.lon - p2.lon;
	}
	else {
		lat = p2.lat - p1.lat;
		lon = p2.lon - p1.lon;
	}

	double delta = 0.;
	double pa = 0., pb = 0., pc = 0.;
	pa = pow(lat,2) / pow(lon,2) + 1;
	pb = 0;
	pc = 0 - distance*distance;
	delta = pb*pb - (4*pa*pc) ;
	// printf("delta = %f.\n", delta);

	double lat1 = 0., lat2 = 0., lat3 = 0., lat4 = 0.;
	double lon1 = 0., lon2 = 0., lon3 = 0., lon4 = 0.;
	if ( delta > 0 ) {
		lat1 = (-pb-sqrt(delta)) / (2*pa);
		double yy1 = (0 - (lat * lat1)) / lon;
		lat2 = (-pb+sqrt(delta)) / (2*pa);
		double yy2 = (0 - (lat * lat2)) / lon;
		// printf ("该方程有两解，x1 = %f\n,x2 = %f\n",x1,x2);
		rect.lb.lat = lat1 + p1.lat;
		rect.lb.lon = yy1 + p1.lon;
		rect.lt.lat = lat1 + p2.lat;
		rect.lt.lon = yy1 + p2.lon;
		rect.rt.lat = lat2 + p2.lat;
		rect.rt.lon = yy2 + p2.lon;
		rect.rb.lat = lat2 + p1.lat;
		rect.rb.lon = yy2 + p1.lon;
	}
	else if (delta == 0) {
		lat1 = (-pb) / (2*pa);
		lat2 = lat1;
		// printf ("该方程有唯一解，x1 = x2 = %f\n",x1 = x2);
		return -4;
	}
	else if (delta < 0) {
		// printf ("该方程无解\n");
		return -3;
	}
	return 0;
}

void planning_map::get_point_by_block(planning_map_block* pblk, pos &np1, pos &np2, pos &np3, pos &np4)
{
	np1.lat = pblk->minlat;
	np1.lon = pblk->minlon;
	np2.lat = pblk->minlat;
	np2.lon = pblk->maxlon;
	np3.lat = pblk->maxlat;
	np3.lon = pblk->minlon;
	np4.lat = pblk->maxlat;
	np4.lon = pblk->maxlon;
}

bool planning_map::point_in_block(pos p, rectangle rect)
{
	double a = (rect.lt.lat - rect.lb.lat) * (p.lon - rect.lb.lon) - (rect.lt.lon - rect.lb.lon) * (p.lat - rect.lb.lat);
	double b = (rect.rt.lat - rect.lt.lat) * (p.lon - rect.lt.lon) - (rect.rt.lon - rect.lt.lon) * (p.lat - rect.lt.lat);
	double c = (rect.rb.lat - rect.rt.lat) * (p.lon - rect.rt.lon) - (rect.rb.lon - rect.rt.lon) * (p.lat - rect.rt.lat);
	double d = (rect.lb.lat - rect.rb.lat) * (p.lon - rect.rb.lon) - (rect.lb.lon - rect.rb.lon) * (p.lat - rect.rb.lat);
	// printf("point x = %f, y = %f.\n", p.x, p.y);
	// printf("a = %f, b = %f, c = %f, d = %f.\n", a, b, c, d);
	return (a > 0 && b > 0 && c > 0 && d > 0) || (a < 0 && b < 0 && c < 0 && d < 0);
}

void planning_map::serial_planningmap()
{
	_planning_map_file = fopen("/home/coder/map/planning_map", "w+");
	size_t start_addr = serial_header();
	start_addr = serial_level(start_addr);
	start_addr = serial_block(start_addr);
	start_addr = serial_nodes(start_addr);
	// start_addr = serial_kdtree(start_addr);
	start_addr = serial_edges(start_addr);
	fclose(_planning_map_file);
}

size_t planning_map::serial_kdtree(size_t start_addr)
{
	for (size_t lvlsz = 0; lvlsz < _level.size(); lvlsz++) {
		planning_map_level* plvl = _level[lvlsz];
		for(int i = 0; i < plvl->_blocks.size(); i++) {
			// printf("kdtree block id is %ld.\n", _blocks[i]->id);
			planning_map_kdtree* kdtree = plvl->_blocks[i]->get_kdtree();
			std::stack<planning_map_kdtree*> kdtree_stack;
			while (nullptr != kdtree || !kdtree_stack.empty())
			{
				planning_map_file_node_kdtree filekdtree;
				if (kdtree != nullptr) {
					filekdtree.id = kdtree->id;
					filekdtree.split = kdtree->split;
					filekdtree.nodeid.all = kdtree->node->uid;
					filekdtree.nodeid.u.blkid = kdtree->node->blockid;
					filekdtree.nodeid.u.index = kdtree->node->inblockindex;
					if(nullptr != kdtree->leftnode) {
						filekdtree.leftnode.all = kdtree->leftnode->node->allindex;
						filekdtree.leftnode.u.blkid = kdtree->leftnode->node->blockid;
						filekdtree.leftnode.u.index = kdtree->leftnode->node->inblockindex;
					}
					if(nullptr != kdtree->rightnode) {
						filekdtree.rightnode.all = kdtree->rightnode->node->allindex;
						filekdtree.rightnode.u.blkid = kdtree->rightnode->node->blockid;
						filekdtree.rightnode.u.blkid = kdtree->rightnode->node->inblockindex;
					}
					kdtree_stack.push(kdtree);
					kdtree = kdtree->leftnode;
				} else {
					filekdtree.id = -1;

					kdtree = kdtree_stack.top();
					kdtree_stack.pop();
					kdtree = kdtree->rightnode;
				}
				fwrite(&filekdtree, sizeof(planning_map_file_node_kdtree), 1, 
					_planning_map_file);
			}
		}
	}
	return start_addr + get_kdnode_size();
}

size_t planning_map::serial_edges(size_t start_addr)
{
	for (size_t lvlsz = 0; lvlsz < _level.size(); lvlsz++) {
		planning_map_level* plvl = _level[lvlsz];
		for(int i = 0; i < plvl->_blocks.size(); i++) {
			planning_map_block* pblk = plvl->_blocks[i];
			listnode_t* nodes = pblk->get_nodes();
			auto* item = nodes->next;
			for (; item != nodes; item = item->next) {
				auto* pmnode = list_entry(planning_map_node, ownerlist, item);
				listnode_t* edgelist = pmnode->get_edges();
				auto* edgeitem = edgelist->next;
				for(;edgeitem != edgelist; edgeitem = edgeitem->next) {
					start_addr += sizeof(planning_map_file_edge);
					auto* pmedge = list_entry(planning_map_edge, ownerlist, 
						edgeitem);
					// if (pmedge->start_node->uid == pmedge->end_node->uid) {
					// 	printf("start node id = %ld, end node id = %ld.\n", pmedge->start_node->uid, pmedge->end_node->uid);
					// }
					planning_map_file_edge edge = pmnode->generte_file_edge(pmedge);
					planning_map_node* startnode = pmedge->start_node;
					planning_map_node* endnode = pmedge->end_node;
					// planning_map_block* edgeblk = nullptr;
					// size_t index = 0;
					// find_block(plvl, startnode->lat, startnode->lon, &edgeblk);
					// find_node_block_index(edgeblk, startnode, index);
					// edge.start_node.u.blkid = edgeblk->id;
					// edge.start_node.u.index = index;
					// index = 0;
					// find_block(plvl, endnode->lat, endnode->lon, &edgeblk);
					// find_node_block_index(edgeblk, endnode, index);
					// edge.end_node.u.blkid = edgeblk->id;
					// edge.end_node.u.index = index;
					fwrite(&edge, sizeof(planning_map_file_edge), 1, _planning_map_file);
				}
			}
		}
	}
	return start_addr;
}

size_t planning_map::serial_nodes(size_t start_addr)
{
	size_t nodesz = get_node_size();
	// size_t kdnodesz = get_kdnode_size();
	size_t edgestartsz = start_addr + nodesz;
	for (size_t lvlsz = 0; lvlsz < _level.size(); lvlsz++) {
		planning_map_level* plvl = _level[lvlsz];
		for(int i = 0; i < plvl->_blocks.size(); i++) {
			start_addr += sizeof(planning_map_file_nodeinfo) * plvl->_blocks[i]->get_node_count();
			listnode_t* nodes = plvl->_blocks[i]->get_nodes();
			auto* item = nodes->next;
			for (; item != nodes; item = item->next) {
				auto* pmnode = list_entry(planning_map_node, ownerlist, item);
				planning_map_file_nodeinfo filenode;
				pmnode->generte_file_node(&filenode, pmnode);
				filenode.start_of_edge_table = edgestartsz;
				edgestartsz += sizeof(planning_map_file_edge) * pmnode->get_edgesize();
				fwrite(&filenode, sizeof(planning_map_file_nodeinfo), 1, _planning_map_file);
			}
		}
	}
	return start_addr;
}

size_t planning_map::serial_block(size_t start_addr) 
{
	for (size_t lvlsz = 0; lvlsz < _level.size(); lvlsz++) {
		planning_map_level* plvl = _level[lvlsz];
		start_addr += (sizeof(planning_map_file_blockinfo) * plvl->_blocks.size());
	}

	size_t nodecountsz = 0;
	for (size_t lvlsz = 0; lvlsz < _level.size(); lvlsz++) {
		planning_map_level* plvl = _level[lvlsz];

		// size_t nodecountsz = 0;
		// size_t blknodesz[plvl->_blocks.size()];
		// size_t blkkdnodesz[plvl->_blocks.size()];

		// for (size_t blksz = 0; blksz < plvl->_blocks.size(); blksz++) {
		// 	planning_map_block* pblk = plvl->_blocks[blksz];
		// 	blknodesz[blksz] = sizeof(planning_map_file_nodeinfo) * pblk->get_node_count();
		// 	nodecountsz += blknodesz[blksz];
		// 	blkkdnodesz[blksz] = sizeof(planning_map_file_node_kdtree) * pblk->get_node_count();
		// }
		// size_t nodetablesz = start_addr;
		// size_t kdnodetablesz = start_addr + nodecountsz;
		for (size_t blksz = 0; blksz < plvl->_blocks.size(); blksz++) {
			planning_map_block* pblk = plvl->_blocks[blksz];
			planning_map_file_blockinfo file_block;
			generate_file_block(&file_block, pblk);
			// nodetablesz += blksz==0 ? 0 : blknodesz[blksz-1];
			// kdnodetablesz += blksz==0 ? 0 : blkkdnodesz[blksz-1];
			file_block.start_of_node_table = sizeof(planning_map_file_nodeinfo) * nodecountsz + start_addr;
			file_block.start_of_kdtree_table = sizeof(planning_map_file_node_kdtree) * nodecountsz + start_addr;
			nodecountsz += pblk->get_node_count();
			fwrite(&file_block, sizeof(planning_map_file_blockinfo), 1, _planning_map_file);
		}
	}

	return start_addr;
}

size_t planning_map::serial_header() {
	if(nullptr != _planning_map_file) {
		planning_map_fileheader fileheader;
		for (int i = 0; i < 12; i++) {
			fileheader.magic[i] = 'x';
		}
		fileheader.version = 1;
		fileheader.level_count = _level.size();
		fileheader.start_of_level_table = sizeof(planning_map_fileheader);
		fwrite(&fileheader, sizeof(planning_map_fileheader), 1, _planning_map_file);
	}
	return sizeof(planning_map_fileheader);
}

size_t planning_map::serial_level(size_t start_addr)
{
	if (_level.empty()) return 0;

	start_addr += (sizeof(planning_map_file_level) * _level.size());

	size_t blksz = start_addr;
	for (size_t sz = 0; sz < _level.size(); sz++) {
		planning_map_level* plvl = _level[sz];
		planning_map_file_level file_level;
		generate_filelevel(plvl, &file_level);
		file_level.start_of_block_table = blksz;
		blksz += sizeof(planning_map_file_blockinfo) * plvl->_blocks.size();
		fwrite(&file_level, sizeof(planning_map_file_level), 1, _planning_map_file);
	}

	return start_addr;
}

void planning_map::generate_file_block(planning_map_file_blockinfo* file_block, planning_map_block* block) 
{
	file_block->id = block->id;
	file_block->minlat = block->minlat;
	file_block->maxlat = block->maxlat;
	file_block->minlon = block->minlon;
	file_block->maxlon = block->maxlon;
	file_block->node_count = block->get_node_count();
	file_block->kdtree_count = block->get_node_count();
}

planning_map_file_node_kdtree planning_map::generate_file_kdtree(planning_map_kdtree* kdtree)
{
	planning_map_file_node_kdtree filekdtree;
	if (kdtree != nullptr) {
		filekdtree.id = kdtree->id;
		filekdtree.split = kdtree->split;
		filekdtree.nodeid.all = kdtree->node->uid;
		filekdtree.nodeid.u.blkid = kdtree->node->blockid;
		filekdtree.nodeid.u.index = kdtree->node->inblockindex;
		if(nullptr != kdtree->leftnode) {
			filekdtree.leftnode.all = kdtree->leftnode->node->allindex;
			filekdtree.leftnode.u.blkid = kdtree->leftnode->node->blockid;
			filekdtree.leftnode.u.index = kdtree->leftnode->node->inblockindex;
		}
		if(nullptr != kdtree->rightnode) {
			filekdtree.rightnode.all = kdtree->rightnode->node->allindex;
			filekdtree.rightnode.u.blkid = kdtree->rightnode->node->blockid;
			filekdtree.rightnode.u.blkid = kdtree->rightnode->node->inblockindex;
		}
	} else {
		filekdtree.id = -1;
	}
	return filekdtree;
}

void planning_map_block::add_node(planning_map_node *pmnode)
{
	_nodecnt++;
	listnode_add(_nodelist, pmnode->ownerlist);
}

int planning_map_block::create_kdtree()
{
	if (_nodecnt == 0) {
		return -1;
	}
	auto** exm_set = new planning_map_node* [_nodecnt];

	uint32_t index = 0;
	auto* item = _nodelist.next;
	for (; item != &_nodelist; item = item->next) {
		auto* node = list_entry(planning_map_node, ownerlist, \
			item);
		exm_set[index] = node;
		index++;
	}
	// printf("block id is %ld\n", id);
	_kdtree = build_kdtree(exm_set, _nodecnt);
	return 0;
}

void planning_map_node::add_edge(planning_map_edge* edge)
{
	_edgesize++;
	listnode_add(_edgelist, edge->ownerlist);
}

size_t planning_map_node::get_edgesize()
{
	return _edgesize;
}

listnode_t* planning_map_node::get_edges()
{
	return &_edgelist;
}

void planning_map_node::generte_file_node(planning_map_file_nodeinfo* file_node, planning_map_node* pmnode)
{
	file_node->uid = pmnode->uid;
	file_node->blockid = pmnode->blockid;
	file_node->data.lat = pmnode->lat;
	file_node->data.lon = pmnode->lon;
	file_node->edge_count = pmnode->get_edgesize();
}

planning_map_file_edge planning_map_node::generte_file_edge 
	(planning_map_edge* edge)
{
	planning_map_file_edge fileedge;
	fileedge.uid = edge->uid;
	fileedge.start_node.all = edge->start_node->allindex;
	fileedge.start_node.u.blkid = edge->start_node->blockid;
	fileedge.start_node.u.index = edge->start_node->inblockindex;
	fileedge.end_node.all = edge->end_node->allindex;
	fileedge.end_node.u.blkid = edge->end_node->blockid;
	fileedge.end_node.u.index = edge->end_node->inblockindex;
	fileedge.distance = edge->distance;
	fileedge.weight = edge->weight;
	return fileedge;
}

} // end of namespace osm_planning
/* EOF */