#ifndef __CXX_OSM_PLANNING_MAP_H__
#define __CXX_OSM_PLANNING_MAP_H__
#include <map>
#include <vector>
#include <math.h>

#include "sdmap-format.h"
#include "std/list.h"
#include "utils/avltree.h"

namespace osm_parser1 {

using namespace zas::utils;

struct planning_map_edge;
class planning_map_block;

struct pos {
	double lat;
	double lon;
};

struct rectangle {
	pos lb; // 左下
	pos lt; // 左上
	pos rt; // 右上
	pos rb; // 右下
};

class dijgraph {
public:
	dijgraph(int v_count)
		: v_count_(v_count) {
		// adj_ = std::vector<std::vector<edge>>(v_count, std::vector<edge>{});
	}
	void addedge(int64_t s, int64_t e, double w);

	void dijkstra(int64_t s, int64_t e, std::vector<int64_t> &plannodes);

	void print(int64_t s, int64_t e, std::map<int64_t, int64_t> parent, std::vector<int64_t> &plannodes);

	private:
	struct edge { // 表示边
		int64_t sid_; // 边的起始节点
		int64_t eid_; // 边的结束节点
		double w_;   // 边的权重
		edge() = default;
		edge(int64_t s, int64_t e, double w)
			: sid_(s), eid_(e), w_(w) {}
	};
	struct vertex { // 算法实现中，记录第一个节点到这个节点的距离
		int64_t id_;
		double dist_;
		vertex() = default;
		vertex(int64_t id, double dist)
			: id_(id), dist_(dist) {}
	};
	std::map<int64_t, std::vector<edge>> adj_; // 邻接表
	int v_count_;           // 顶点数
};

class planning_map_node {
public:
	planning_map_node();
	~planning_map_node();
	void add_edge(planning_map_edge* edge);
	size_t get_edgesize();
	listnode_t* get_edges();
	void generte_file_node(planning_map_file_nodeinfo* file_node, planning_map_node* pmnode);
	planning_map_file_edge generte_file_edge(planning_map_edge* pmedge);

public:
	listnode_t ownerlist;
	avl_node_t avlnode;
	int64_t uid;
	double lat, lon;
	uid_t blockid;
	uint32_t inblockindex;
	uint32_t allindex;

private:
	size_t _edgesize;
	listnode_t _edgelist;
};

struct planning_map_edge {
	listnode_t ownerlist;

	uid_t uid;

	planning_map_node* start_node;
	planning_map_node* end_node;

	double distance;
	double weight;
};

struct planning_map_kdtree {
	uid_t id;
	double lat, lon;

	int split;// 0 - lat; 1 - lon;

	planning_map_node* node;
	planning_map_kdtree* leftnode;
	planning_map_kdtree* rightnode;
};

struct planning_map_level {
	uint8_t level;// 0 ~ 5

	int row, col;
	double latstep, lonstep;
	double minlat, maxlat, minlon, maxlon;

	avl_node_t* _gbl_avlnode = nullptr;

	std::vector<planning_map_block*> _blocks;
	std::vector<planning_map_node*> _nodes;
};

class planning_map_block {
public:
	planning_map_block();
	~planning_map_block();

	void searchNearestNeighbor(double x, double y, planning_map_kdtree &kdnode);
	void add_node(planning_map_node* node);
	listnode_t* get_nodes() {
		return &_nodelist;
	}
	int create_kdtree(void);
	void setting_kdtree(planning_map_kdtree* kdtree) {
		_kdtree = kdtree;
	}
	planning_map_kdtree* get_kdtree() {
		return _kdtree;
	}
	int get_node_count() {
		return _nodecnt;
	}

public:
	uid_t id;
	double minlat, minlon;
	double maxlat, maxlon;

private:
	void choose_split(planning_map_node **exm_set, size_t size, int &split);
	planning_map_kdtree* build_kdtree(planning_map_node **exm_set, size_t size);
	static int cmp_lat(const void* node1, const void* node2);
	static int cmp_lon(const void* node1, const void* node2);
	bool equal(planning_map_node* node1, planning_map_node* node2);
	double rad(double angle);
	double distance(planning_map_node* data1, planning_map_node* data2);

private:
	size_t _nodecnt;
	listnode_t _nodelist;
	planning_map_kdtree* _kdtree;
};

class osm_node;
struct map_scope;
struct map_level;

class planning_map 
{
public:
	planning_map();
	~planning_map();
	int get_level(planning_map_level** plvl, int lvlid);
	void generate_level(map_scope scope, map_level* level, int lvlid);
	void generate_node(planning_map_level* plvl, osm_node* node);
	void generate_edge(osm_node* node);
	void show_block_info();
	void build_block_kdtree();
	void serial_planningmap();
	void deserial_planningmap();
	void planning_road(double lat1, double lon1, double lat2, double lon2, std::vector<int64_t> &plannodes);

private:
	double rad(double angle);
	double distance(planning_map_node* data1, planning_map_node* data2);
	int find_block(planning_map_level* plvl, double lat, double lon, planning_map_block** pblk);	
	// int find_node_block_index(planning_map_block* pblk, planning_map_node* pnode, size_t &indexinblk);
	void find_edge_node(planning_map_node* pnode, planning_map_level* plvl);
	void find_edge_block_node(int blkid, int index, planning_map_level* plvl, planning_map_node** node);
	static int uid_avl_comp(avl_node_t* a, avl_node_t* b);
	void generate_maplevel(planning_map_file_level* file_level, planning_map_level* plvl);
	void generate_mapblock(planning_map_file_blockinfo* pfblk, planning_map_block* pblk);
	void generate_mapnode(planning_map_file_nodeinfo* pfnode, planning_map_node* pnode);
	void generate_mapkdnode(planning_map_file_node_kdtree* pfkdnode, planning_map_kdtree* pkdnode);
	void generate_mapedge(planning_map_file_edge* pfedge, planning_map_edge* pedge);
	int generate_filelevel(planning_map_level* plvl, planning_map_file_level* file_level);
	void generate_file_block(planning_map_file_blockinfo* file_block, planning_map_block* block);
	void create_planning_map_edge(planning_map_level* plvl);
	size_t get_node_size(void);
	size_t get_kdnode_size(void);
	planning_map_file_node_kdtree generate_file_kdtree(planning_map_kdtree* pmkdtree);
	size_t serial_header(void);
	size_t serial_level(size_t start_addr);
	size_t serial_block(size_t start_addr);
	size_t serial_nodes(size_t start_addr);
	size_t serial_edges(size_t start_addr);
	size_t serial_kdtree(size_t start_addr);
	size_t deserial_header(char* buffer, planning_map_fileheader** header);
	size_t deserial_level(char* buffer, planning_map_fileheader* fheader);
	size_t deserial_block(char* buffer, planning_map_file_level* pflvl, planning_map_level* plvl);
	size_t deserial_nodes(char* buffer, planning_map_file_blockinfo* pfblk, planning_map_block* pblk);
	size_t deserial_edges(char* buffer, planning_map_file_nodeinfo* pfnode, planning_map_node* pnode);
	size_t deserial_kdtree(char* buffer);

	int get_nearby_node(planning_map_block* pblk, planning_map_node** pnode, planning_map_node* target, double &distance);

	int get_calc_block(planning_map_level* plvl, double lat1, double lon1, double lat2, double lon2, std::vector<size_t> &indexes);
	int calc_line_paraemeter(double &a, double &b, double lat1, double lon1, double lat2, double lon2);
	int calc_new_point(double lat1, double lon1, double lat2, double lon2, double a, double b, double &nlat1, double &nlon1, double &nlat2, double &nlon2, double distance);
	int calc_vertical_point(pos p1, pos p2, rectangle &rect, double distance);
	void get_point_by_block(planning_map_block* pblk, pos &np1, pos &np2, pos &np3, pos &np4);
	bool point_in_block(pos p, rectangle b);
	int planning_nodes(int64_t &lvlidx, planning_map_node** pnode1, planning_map_node** pnode2, double lat1, double lon1, double lat2, double lon2, std::vector<int64_t> &nodes);
	int calc_dependblk_nodes(planning_map_level* plvl, double lat1, double lon1, double lat2, double lon2, int64_t nodeuid1, int64_t nodeuid2, std::vector<int64_t> &nodes);

private:
	std::vector<planning_map_level*> _level;
	FILE *_planning_map_file;
};

} // namespace osm_planning
#endif // __CXX_OSM_PLANNING_MAP_H__
/* EOF */