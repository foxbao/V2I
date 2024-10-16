#ifndef __CXX_OSM_PARSER1_GENERATOR_H__
#define __CXX_OSM_PARSER1_GENERATOR_H__

#include <utils/avltree.h>
#include "parser.h"
#include "osm-map.h"
#include "planning.h"

// todo: re-arrange the following header
#include "./../hwgrph/inc/gbuf.h"

#include "sdmap-format.h"

namespace osm_parser1 {

class string_section
{
	const size_t min_size = 1024;
public:
	string_section()
	: _buf(nullptr), _total_sz(0)
	, _curr_sz(0) {
	}

	~string_section()
	{
		if (_buf) {
			free(_buf);
			_buf = nullptr;
		}
		_total_sz = _curr_sz = 0;
	}

	size_t getsize(void) {
		return _curr_sz;
	}

	void* getbuf(void) {
		return _buf;
	}

	char* alloc(int sz, size_t* delta_pos)
	{
		if (sz <= 0) {
			return nullptr;
		}
		int new_sz = _curr_sz + sz;
		int new_total_sz = _total_sz;
		for (; new_sz > new_total_sz; new_total_sz += min_size);
		if (new_total_sz != _total_sz) {
			_buf = (char*)realloc(_buf, new_total_sz);
			assert(NULL != _buf);
			_total_sz = new_total_sz;
		}
		char* ret = &_buf[_curr_sz];
		_curr_sz = new_sz;
		
		if (delta_pos) {
			*delta_pos = size_t(ret - _buf);
		}
		return ret;
	}

	const char* alloc_copy_string(const char* str, size_t& pos)
	{
		assert(NULL != str);
		if (nullptr == str) {
			return nullptr;
		}
		int len = strlen(str) + 1;
		char* ret = alloc(len, &pos);
		if (NULL == ret) return NULL;
		memcpy(ret, str, len);
		return (const char*)ret;
	}

	const char* to_string(size_t pos) {
		if (pos >= _curr_sz) return NULL;
		return (const char*)&_buf[pos];
	}

private:
	char* _buf;
	size_t _total_sz, _curr_sz;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(string_section);
};

#define MAPGEN_MAX_BLK_KEY_NODE_CNT		(30)
#define MAPGEN_MAX_BLK_NODE_CNT			(128)

typedef zas::hwgrph::gbuf_<int> type_set;

// full map generator structures
struct fullmap_nodeinfo;
struct fullmap_linkinfo;
struct fullmap_wayinfo;
struct fullmap_blockinfo;

struct fullmap_nodeinfo
{
	avl_node_t avlnode;
	avl_node_t gbl_avlnode;

	// kd-tree
	fullmap_nodeinfo *left, *right;

	uid_t uid;
	osm_node* node;
	fullmap_blockinfo* block;

	uint32_t fanin_link_count;
	uint32_t fanout_link_count;

	fullmap_linkinfo* fanin_links;
	fullmap_linkinfo* fanout_links;

	union {
		struct {
			uint32_t way_referred : 1;
			uint32_t endpoint : 1;
			uint32_t kdtree_split : 1;
		} f;
		uint32_t flags;
	};

	int serialize(fullmap_file_blockinfo_v1&,
		fullmap_file_nodeinfo_v1&);
	bool coord_equal(fullmap_nodeinfo* nd);

	static int uid_avl_comp(avl_node_t*, avl_node_t*);
	static int uid_gblavl_comp(avl_node_t*, avl_node_t*);
};

struct fullmap_linkinfo
{
	avl_node_t avlnode;
	fullmap_linkinfo *node_fanin_link;
	fullmap_linkinfo *node_fanout_link;
	fullmap_linkinfo *prev, *next;
	int block_id;

	uid_t uid;
	fullmap_nodeinfo* node1;
	fullmap_nodeinfo* node2;
	fullmap_wayinfo* way;

	int serialize(fullmap_file_blockinfo_v1&,
		fullmap_file_linkinfo_v1&);

	static int uid_avl_comp(avl_node_t*, avl_node_t*);
};

struct fullmap_wayinfo
{
	avl_node_t avlnode;

	uid_t uid;
	double distance;
	osm_way* way;

	int way_distance(osm_way* way);
	int serialize(fullmap_file_blockinfo_v1&,
		fullmap_file_wayinfo_v1&, string_section& ss);
	static int uid_avl_comp(avl_node_t*, avl_node_t*);
};

struct map_level
{
	int row, col;
	int blk_max_nodes;
	type_set types;

	map_level() : row(0), col(0)
	, blk_max_nodes(0), types(8) {
	}

	bool allowed_type(int type);
};

struct fullmap_blockinfo
{
	int block_id;
	int node_count;
	int link_count;
	int way_count;
	avl_node_t* node_tree;
	avl_node_t* link_tree;
	avl_node_t* way_tree;

	fullmap_wayinfo** way_array;
	fullmap_linkinfo** link_array;
	fullmap_nodeinfo** node_array;

	string_section strsect;

	fullmap_blockinfo()
	: node_count(0), link_count(0), way_count(0)
	, node_tree(nullptr)
	, link_tree(nullptr)
	, way_tree(nullptr)
	, way_array(nullptr)
	, link_array(nullptr)
	, node_array(nullptr) {
		size_t pos;
		strsect.alloc_copy_string(".str", pos);
	}

	~fullmap_blockinfo();

	fullmap_wayinfo* getway(osm_way* way);

	// generator
	int write(FILE *fp, fullmap_level* fm);
	int write_nodes(FILE* fp, fullmap_file_blockinfo_v1&,
		fullmap_level*);
	int write_links(FILE* fp, fullmap_file_blockinfo_v1&);
	int write_ways(FILE* fp, fullmap_file_blockinfo_v1&);
	int write_string_table(FILE* fp, fullmap_file_blockinfo_v1&);
	int finalize(FILE* fp, fullmap_file_blockinfo_v1&);

	arridx_t get_wayid(uid_t uid);
	arridx_t get_nodeid(uid_t uid);
	arridx_t get_linkid(uid_t uid);

private:
	int prepare_way_array(void);
	int prepare_link_array(void);
	int prepare_node_array(void);
};

struct fullmap_level
{
	int row, col;
	int blk_max_nodes;

	int node_count;
	int link_count;
	fullmap_blockinfo* blocks;

	avl_node_t* node_tree;
	fullmap_nodeinfo* node_kdtree_root;

	fullmap_level() : row(0), col(0)
	, blk_max_nodes(0), node_count(0)
	, link_count(0), _fp(nullptr)
	, blocks(nullptr), node_tree(nullptr)
	, node_kdtree_root(nullptr) {
	}

	~fullmap_level();

	int prepare_blocks(void);
	int add_node(osm_node* node, map_scope& scope);
	fullmap_nodeinfo* get_node(osm_node* node);
	int add_way(osm_way* way);
	int add_link(osm_way* way, osm_node* node1, osm_node* node2,
		fullmap_linkinfo** newlink);
	int writefile(osm_map* map);

	// build the kdtree
	int build_kdtree(void);

private:
	int write_header(void);
	int write_keyinfo(osm_map* map);
	int write_blocks(void);
	int finalize_keyinfo(void);

	// kd-tree methods
	void choose_split(fullmap_nodeinfo** exm_set,
		int size, int &split);
	fullmap_nodeinfo* do_build_kdtree(fullmap_nodeinfo** exm_set,
		int size);

	static int lat_compare(const void* a, const void* b);
	static int lon_compare(const void* a, const void* b);

private:
	FILE* _fp;
	fullmap_fileheader_v1 _header;
	fullmap_file_keyinfo_v1 _keyinfo;
};

struct plandata_block
{
	int node_count;
	avl_node_t* lat_tree;
	avl_node_t* lon_tree;

	plandata_block();
};

class maplevel_data
{
public:
	maplevel_data(map_level* level, int lvid);
	~maplevel_data();

	int add_keynode(planning_map* pmap, planning_map_level* plvl, osm_node* node);

public:
	int get_node_count(void) {
		return _node_count;
	}

	int get_level_id(void) {
		return _level_id;
	}

private:
	bool node_matched(osm_node* node);

private:
	plandata_block* _pblks;
	map_level* _level;

	int _node_count;
	int _level_id;

	struct {
		uint32_t ready : 1;
	} _flags;
};

} // end of namespace osm_parser1
#endif // __CXX_OSM_PARSER1_GENERATOR_H__
/* EOF */
