#ifndef __CXX_OSM_PARSER1_OSM_MAP_H__
#define __CXX_OSM_PARSER1_OSM_MAP_H__

#include "tinyxml2.h"
#include "std/list.h"
#include "utils/avltree.h"

// todo: re-arrange the following header
#include "./../hwgrph/inc/gbuf.h"

#include "parser.h"

#include "planning.h"

namespace osm_parser1 {

using namespace std;
using namespace tinyxml2;
using namespace zas::utils;

enum {
	TAG_HIGHWAY_START_OF_MAIN,
	TAG_HIGHWAY_MOTORWAY = 0,
	TAG_HIGHWAY_TRUNK,
	TAG_HIGHWAY_PRIMARY,
	TAG_HIGHWAY_SECONDARY,
	TAG_HIGHWAY_TERTIARY,
	TAG_HIGHWAY_UNCLASSIFIED,
	TAG_HIGHWAY_RESIDENTIAL,
	TAG_HIGHWAY_END_OF_MAIN = TAG_HIGHWAY_RESIDENTIAL,

	// link roads
	TAG_HIGHWAY_START_OF_LINK_ROADS,
	TAG_HIGHWAY_MOTORWAY_LINK = TAG_HIGHWAY_START_OF_LINK_ROADS,
	TAG_HIGHWAY_TRUNK_LINK,
	TAG_HIGHWAY_PRIMARY_LINK,
	TAG_HIGHWAY_SECONDARY_LINK,
	TAG_HIGHWAY_TERTIARY_LINK,
	TAG_HIGHWAY_END_OF_LINK_ROADS = TAG_HIGHWAY_TERTIARY_LINK,

	// Special road types
	TAG_HIGHWAY_START_OF_SPECIAL_ROAD_TYPES,
	TAG_HIGHWAY_LIVING_STREET = TAG_HIGHWAY_START_OF_SPECIAL_ROAD_TYPES,
	TAG_HIGHWAY_SERVICE,
	TAG_HIGHWAY_PEDESTRIAN,
	TAG_HIGHWAY_TRACK,
	TAG_HIGHWAY_BUS_GUIDEWAY,
	TAG_HIGHWAY_ESCAPE,
	TAG_HIGHWAY_RACEWAY,
	TAG_HIGHWAY_ROAD,
	TAG_HIGHWAY_BUSWAY,
	TAG_HIGHWAY_END_OF_SPECIAL_ROAD_TYPES = TAG_HIGHWAY_BUSWAY,

	// paths
	TAG_HIGHWAY_START_OF_PATH,
	TAG_HIGHWAY_FOOTWAY = TAG_HIGHWAY_START_OF_PATH,
	TAG_HIGHWAY_BRIDLEWAY,
	TAG_HIGHWAY_STEPS,
	TAG_HIGHWAY_CORRIDOR,
	TAG_HIGHWAY_PATH,
	TAG_HIGHWAY_CYCLEWAY,
	TAG_HIGHWAY_END_OF_PATH = TAG_HIGHWAY_CYCLEWAY,

	// Lifecycle
	TAG_HIGHWAY_START_OF_LIFECYCLE,
	TAG_HIGHWAY_PLANNED = TAG_HIGHWAY_START_OF_LIFECYCLE,
	TAG_HIGHWAY_PROPOSED,
	TAG_HIGHWAY_CONSTRUCTION,
	TAG_HIGHWAY_END_OF_LIFECYCLE = TAG_HIGHWAY_CONSTRUCTION,

	// Other highway features
	TAG_HIGHWAY_START_OF_OTHER_FEATURES,
	TAG_HIGHWAY_BUS_STOP = TAG_HIGHWAY_START_OF_OTHER_FEATURES,
	TAG_HIGHWAY_CROSSING,
	TAG_HIGHWAY_ELEVATOR,
	TAG_HIGHWAY_EMERGENCY_BAY,
	TAG_HIGHWAY_EMERGENCY_ACCESS_POINT,
	TAG_HIGHWAY_GIVE_WAY,
	TAG_HIGHWAY_MILESTONE,
	TAG_HIGHWAY_MINI_ROUNDABOUT,
	TAG_HIGHWAY_MOTORWAY_JUNCTION,
	TAG_HIGHWAY_PASSING_PLACE,
	TAG_HIGHWAY_PLATFORM,
	TAG_HIGHWAY_REST_AREA,
	TAG_HIGHWAY_SPEED_CAMERA,
	TAG_HIGHWAY_STREEP_LAMP,
	TAG_HIGHWAY_SERVICES,
	TAG_HIGHWAY_STOP,
	TAG_HIGHWAY_TRAFFIC_MIRROR,
	TAG_HIGHWAY_TRAFFIC_SIGNALS,
	TAG_HIGHWAY_TRAILHEAD,
	TAG_HIGHWAY_TURNING_CIRCLE,
	TAG_HIGHWAY_TURNING_LOOP,
	TAG_HIGHWAY_TOLL_GANTRY,
	TAG_HIGHWAY_END_OF_OTHER_FEATURES = TAG_HIGHWAY_TOLL_GANTRY,
};

struct map_scope
{
	double minlat, minlon;
	double maxlat, maxlon;
	map_scope(void) : minlat(0), minlon(0),
		maxlat(0), maxlon(0) {}
	
	bool in_scope(double lat, double lon,
		int* cols = nullptr,
		int* rows = nullptr);
};

struct osm_wayref;

struct osm_node_extra_data
{
	int intersect_name_count;
	char* intersect_name[0];
};

struct osm_node
{
	listnode_t ownerlist;
	avl_node_t avlnode;
	listnode_t wayref_list;
	int64_t id;
	double lat, lon;

	struct {
		uint32_t is_keynode : 1;
	} attr;
	osm_node_extra_data* data;

	osm_node();
	~osm_node();
	static int avl_compare(avl_node_t*, avl_node_t*);

	enum fan_inout { fan_in = 0, fan_out };

	// get the fan-in/fan-out wayref
	// @param wayref = nullptr means getting the
	// first fan-in/fan-out way
	osm_wayref* get_next_inout_wayref(fan_inout inout,
		osm_wayref* wayref = nullptr);

	// get the count of fan-in/fan-out wayref
	// @param wayref - the first fan-in/fan-out wayref
	int get_inout_wayref_count(fan_inout inout,
		osm_wayref** wayref = nullptr);

	int get_wayref_count(void);
};

typedef zas::hwgrph::gbuf_<osm_node*> osm_node_set;

struct osm_way
{
	listnode_t ownerlist;
	listnode_t wref_list;
	char* name;
	int64_t id;
	struct {
		uint32_t is_oneway : 1;
		uint32_t is_highway : 1;
		uint32_t is_bridge : 1;
		uint32_t close_loop : 1;
		uint32_t lanes : 5;
		uint32_t maxspeed : 8;
		uint32_t highway_type : 8;
		// same_type() shall be modified
		// if you add anything here

		uint32_t fullmap_referred : 1;
	} attr;

	osm_node_set node_set;

	osm_way();
	~osm_way();

	bool same_type(osm_way* way);
};

struct osm_wayref
{
	listnode_t node_ownerlist;
	listnode_t way_ownerlist;
	osm_way* way;
	uint32_t ndref_index;

	osm_wayref(osm_way* way);
};

class fullmap_level;
class maplevel_data;

class osm_map
{
public:
	osm_map(cmdline_parser& psr);
	~osm_map();

	int append(const XMLDocument& doc);
	int finalize(void);

	static const char* get_highway_type_name(int id);
	static int get_highway_type(const char* name);
	static double distance(double lon1, double lat1,
		double lon2, double lat2);

	int generate(void);

public:
	cmdline_parser& get_parser(void) {
		return _psr;
	}

	const map_scope& get_scope(void) {
		return _scope;
	}

private:
	// osm_map parser private methods
	int create_bounds(const XMLElement* e);
	int create_node(const XMLElement* e);
	int create_way(const XMLElement* e);
	osm_way* create_way_with_node_ref(const XMLElement* e);
	
	int handle_way_tags(osm_way* way, const XMLElement* e);
	int handle_highway(osm_way* way, const char* v);
	int handle_key_attributes(const char* k,
		const char* v, osm_way* way);

	static int inlist(const char** list, const char* k);

	int finalize_highway(void);
	int wayinfo_statistics(osm_way* way);
	int nodeinfo_statistics(osm_node* node);
	int associate_node(osm_way* way);

	int merge_ways(void);
	int mark_key_nodes(void);
	int mark_key_node(osm_node* node);

	bool way_could_merge(osm_way* way1, osm_way* way2);
	int merge_roads(osm_way* way1, osm_way* way2);

	bool node_in_bound(osm_node* node);
	void show_statistics(void);

private:
	// osm generator methods
	int generate_level(map_level* level, int lvid);
	int calc_row_col(map_level* level);
	int calc_fullmap_row_col(void);
	int fullmap_walk_all_nodes(void);
	int fullmap_walk_all_ways(void);
	int maplevel_walk_all_keynodes(maplevel_data* data);

private:
	static const char* _highway_type[];
	static int _highway_type_converter[];

private:
	size_t _node_count;
	size_t _way_count;
	size_t _highway_node_count;
	size_t _highway_count;

	// way satistics
	size_t _highway_classified_count[
		TAG_HIGHWAY_END_OF_OTHER_FEATURES + 1];
	size_t _allnode_classified_count[
		TAG_HIGHWAY_END_OF_OTHER_FEATURES + 1];
	size_t _keynode_classified_count[
		TAG_HIGHWAY_END_OF_OTHER_FEATURES + 1];

	listnode_t _highway_list;
	listnode_t _highway_node_list;
	listnode_t _otherway_list;
	avl_node_t* _node_tree;

	map_scope _scope;
	cmdline_parser& _psr;

	fullmap_level* _fullmap;

	planning_map* _pmap;
};

} // end of namespace osm_parser1
#endif // __CXX_OSM_PARSER1_OSM_MAP_H__
/* EOF */
