#include <math.h>
#include "osm-map.h"
#include "mapcore/shift.h"

namespace osm_parser1 {

const double EARTH_RADIUS = 6378137;
const double pai = 3.1415926535;

inline static double rad(double d) {
	return d * pai / 180.0;
}

double osm_map::distance(double lon1,
	double lat1, double lon2, double lat2)
{
	double radlat1 = rad(lat1);  
	double radlat2 = rad(lat2);  
	double radlon1 = rad(lon1);  
	double radlon2 = rad(lon2);

	if (radlat1 < 0)
		radlat1 = pai / 2 + fabs(radlat1); // south  
	if (radlat1 > 0)
		radlat1 = pai / 2 - fabs(radlat1); // north  
	if (radlon1 < 0)
		radlon1 = pai * 2 - fabs(radlon1); // west  
	if (radlat2 < 0)
		radlat2 = pai / 2 + fabs(radlat2); // south
	if (radlat2 > 0)
		radlat2 = pai / 2 - fabs(radlat2); // north  
	if (radlon2 < 0)
		radlon2 = pai * 2 - fabs(radlon2); // west  

	double x1 = EARTH_RADIUS * cos(radlon1) * sin(radlat1);
	double y1 = EARTH_RADIUS * sin(radlon1) * sin(radlat1);
	double z1 = EARTH_RADIUS * cos(radlat1);

	double x2 = EARTH_RADIUS * cos(radlon2) * sin(radlat2);
	double y2 = EARTH_RADIUS * sin(radlon2) * sin(radlat2);
	double z2 = EARTH_RADIUS * cos(radlat2);

	double d = sqrt((x1 - x2) * (x1 - x2) + (y1 - y2)
		* (y1 - y2)+ (z1 - z2) * (z1 - z2));
	double theta = acos((EARTH_RADIUS * EARTH_RADIUS
		+ EARTH_RADIUS * EARTH_RADIUS - d * d)
		/ (2 * EARTH_RADIUS * EARTH_RADIUS));
	return theta * EARTH_RADIUS;
}

osm_map::osm_map(cmdline_parser& psr)
: _node_tree(nullptr)
, _node_count(0), _way_count(0)
, _highway_node_count(0), _highway_count(0)
, _psr(psr)
{
	listnode_init(_highway_list);
	listnode_init(_otherway_list);
	listnode_init(_highway_node_list);

	// statistics buffer
	memset(_highway_classified_count, 0,
		sizeof(_highway_classified_count));
	memset(_allnode_classified_count, 0,
		sizeof(_allnode_classified_count));
	memset(_keynode_classified_count, 0,
		sizeof(_keynode_classified_count));

	_pmap = new planning_map();
}

osm_map::~osm_map()
{
	// todo: release _fullmap
	// todo: release _node_tree
	delete _pmap;
	_pmap = nullptr;
}

int osm_map::append(const XMLDocument& doc)
{
	const XMLElement* e = doc.RootElement();
	for (e = e->FirstChildElement(); e; e = e->NextSiblingElement()) {
		const char* name = e->Name();
		if (!strcmp(name, "bounds")) {
			if (create_bounds(e)) return -1;
		}
		else if (!strcmp(name, "node")) {
			if (create_node(e)) return -2;
		}
		else if (!strcmp(name, "way")) {
			if (create_way(e)) return -3;
		}
	}
	return 0;
}

int osm_map::create_bounds(const XMLElement* e)
{
	double minlat, maxlat;
	double minlon, maxlon;
	if (e->QueryAttribute("minlat", &minlat)) {
		return -1;
	}
	if (e->QueryAttribute("minlon", &minlon)) {
		return -2;
	}
	if (e->QueryAttribute("maxlat", &maxlat)) {
		return -3;
	}
	if (e->QueryAttribute("maxlon", &maxlon)) {
		return -4;
	}

	if (_psr.use_wcj02()) {
		zas::mapcore::coord_t min_coord(minlat, minlon);
		min_coord.wgs84_to_gcj02();
		_scope.minlat = min_coord.lat();
		_scope.minlon = min_coord.lon();

		zas::mapcore::coord_t max_coord(maxlat, maxlon);
		max_coord.wgs84_to_gcj02();
		_scope.maxlat = max_coord.lat();
		_scope.maxlon = max_coord.lon();
	}
	else {
		_scope.minlat = minlat;
		_scope.maxlat = maxlat;
		_scope.minlon = minlon;
		_scope.maxlon = maxlon;
	}
	return 0;
}

bool osm_map::node_in_bound(osm_node* node)
{
	if (node->lat < _scope.minlat || node->lat > _scope.maxlat) {
		return false;
	}
	if (node->lon < _scope.minlon || node->lon > _scope.maxlon) {
		return false;
	}
	return true;
}

int osm_map::create_node(const XMLElement* e)
{
	if (!e->BoolAttribute("visible", true)) {
		return 0;
	}

	osm_node* node = new osm_node;
	if (nullptr == node) return -1;

	if (e->QueryAttribute("id", &node->id) || !node->id) {
		return -2;
	}
	if (e->QueryAttribute("lat", &node->lat)) {
		return -3;
	}
	if (e->QueryAttribute("lon", &node->lon)) {
		return -4;
	}

	// see if we need to convert it to WCJ02
	if (_psr.use_wcj02()) {
		zas::mapcore::coord_t co(node->lat, node->lon);
		co.wgs84_to_gcj02();
		node->lat = co.lat(); node->lon = co.lon();
	}

	// add object
	if (avl_insert(&_node_tree, &node->avlnode,
		osm_node::avl_compare)) {
		return -5;
	}
	++_node_count;
	return 0;
}

int osm_map::create_way(const XMLElement* e)
{
	if (!e->BoolAttribute("visible", true)) {
		return 0;
	}

	osm_way* way = create_way_with_node_ref(e);
	if (nullptr == way) {
		return -1;
	}
	if (handle_way_tags(way, e)) {
		return -2;
	}

	// add to list
	++_way_count;
	if (way->attr.is_highway) {
		listnode_add(_highway_list, way->ownerlist);
		++_highway_count;
	} else {
		listnode_add(_otherway_list, way->ownerlist);
	}
	return 0;
}

int osm_map::handle_way_tags(osm_way* way, const XMLElement* e)
{
	for (e = e->FirstChildElement(); e; e = e->NextSiblingElement()) {
		if (strcmp(e->Name(), "tag")) continue;
		const char* k = e->Attribute("k");
		const char* v = e->Attribute("v");

		// save the name
		if (!strcmp(k, "name")) {
			way->name = strdup(v);
			continue;
		}
		// parse the highway type
		if (!strcmp(k, "highway")) {
			handle_highway(way, v);
			continue;
		}

		// handle key attributes
		if (!handle_key_attributes(k, v, way)) {
			continue;
		}
	}
	return 0;
}

int osm_map::handle_key_attributes(const char* k,
	const char* v, osm_way* way)
{
	// set flag: oneway
	if (!strcmp(k, "oneway")) {
		if (!strcasecmp(v, "yes")) way->attr.is_oneway = 1;
		return 0;
	}

	// set the lanes info
	if (!strcmp(k, "lanes")) {
		int val = atoi(v);
		way->attr.lanes = val;
		return 0;
	}

	// set the max speed
	if (!strcmp(k, "maxspeed")) {
		int val = atoi(v);
		way->attr.maxspeed = val;
		return 0;
	}

	// set the bridge attribute
	if (!strcmp(k, "bridge")) {
		if (!strcasecmp(v, "yes"))
			way->attr.is_bridge = 1;
	}
	return 0;
}

// see https://wiki.openstreetmap.org/wiki/Map_features#Highway for details
const char* osm_map::_highway_type[] = {
	"motorway",
	"trunk", "primary", "secondary",
	"tertiary", "unclassified", "residential",
	"resdential", // handle spell mistake in OSM data

	// link roads
	"motorway_link", "trunk_link", "primary_link",
	"secondary_link", "tertiary_link",

	// Special road types
	"living_street", "service", "pedestrian",
	"track", "bus_guideway", "escape", "raceway",
	"road", "busway",

	// paths
	"footway", "bridleway", "steps", "corridor", "path",
	"cycleway",

	// Lifecycle
	"planned", "proposed", "construction",

	// Other highway features
	"bus_stop", "crossing", "elevator", "emergency_bay",
	"emergency_access_point", "give_way", "milestone",
	"mini_roundabout", "motorway_junction", "passing_place",
	"platform", "rest_area", "speed_camera", "street_lamp",
	"services", "stop", "traffic_mirror", "traffic_signals",
	"trailhead", "turning_circle", "turning_loop",
	"toll_gantry",

	// unknown - discovered in OSM data
	"cycleway",
	nullptr,
};

int osm_map::_highway_type_converter[] =
{
	TAG_HIGHWAY_MOTORWAY,
	TAG_HIGHWAY_TRUNK,
	TAG_HIGHWAY_PRIMARY,
	TAG_HIGHWAY_SECONDARY,
	TAG_HIGHWAY_TERTIARY,
	TAG_HIGHWAY_UNCLASSIFIED,
	TAG_HIGHWAY_RESIDENTIAL,
	TAG_HIGHWAY_RESIDENTIAL,	// handle OSM spell mistake

	// link roads
	TAG_HIGHWAY_MOTORWAY_LINK,
	TAG_HIGHWAY_TRUNK_LINK,
	TAG_HIGHWAY_PRIMARY_LINK,
	TAG_HIGHWAY_SECONDARY_LINK,
	TAG_HIGHWAY_TERTIARY_LINK,

	// Special road types
	TAG_HIGHWAY_LIVING_STREET,
	TAG_HIGHWAY_SERVICE,
	TAG_HIGHWAY_PEDESTRIAN,
	TAG_HIGHWAY_TRACK,
	TAG_HIGHWAY_BUS_GUIDEWAY,
	TAG_HIGHWAY_ESCAPE,
	TAG_HIGHWAY_RACEWAY,
	TAG_HIGHWAY_ROAD,
	TAG_HIGHWAY_BUSWAY,

	// paths
	TAG_HIGHWAY_FOOTWAY,
	TAG_HIGHWAY_BRIDLEWAY,
	TAG_HIGHWAY_STEPS,
	TAG_HIGHWAY_CORRIDOR,
	TAG_HIGHWAY_PATH,
	TAG_HIGHWAY_CYCLEWAY,

	// Lifecycle
	TAG_HIGHWAY_PLANNED,
	TAG_HIGHWAY_PROPOSED,
	TAG_HIGHWAY_CONSTRUCTION,

	// Other highway features
	TAG_HIGHWAY_BUS_STOP,
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
};

int osm_map::handle_highway(osm_way* way, const char* v)
{
	way->attr.is_highway = 1;
	int ret = get_highway_type(v);
	if (ret < 0) {
		fprintf(stderr, "warning: known highway tag: %s\n", v);
		return 0;
	}

	way->attr.highway_type = ret;
	return 0;
}

int osm_map::inlist(const char** list, const char* k)
{
	for (int i = 0; list[i]; ++i) {
		if (!strcmp(k, list[i])) {
			return i;
		}
	}
	return -1;
}

const char* osm_map::get_highway_type_name(int id)
{
	const char* unknown = "<unknown>";
	if (id < 0 || id > TAG_HIGHWAY_END_OF_OTHER_FEATURES) {
		return unknown;
	}
	int cnt = sizeof(_highway_type_converter) / sizeof(int);
	for (int i = 0; i < cnt; ++i) {
		if (_highway_type_converter[i] == id) {
			return _highway_type[i];
		}
	}
	return unknown;
}

int osm_map::get_highway_type(const char* name)
{
	if (!name || !*name) {
		return -1;
	}
	int id = inlist((const char**)_highway_type, name);
	if (id < 0) return id;

	return _highway_type_converter[id];
}

int osm_map::finalize(void)
{
	fputs("analyze relationship between ways"
		" and nodes ... ", stdout);
	if (finalize_highway()) {
		fputs("[fail]\n", stdout); return -1;
	}

	fputs("[OK]\nmerge same ways ... ", stdout);
	if (merge_ways()) {
		fputs("[fail]\n", stdout); return -2;
	}

	fputs("[OK]\nmark key nodes ... ", stdout);
	if (mark_key_nodes()) {
		fputs("[fail]\n", stdout); return -3;
	}

	fputs("[OK]\n", stdout);
	// finished, show statistics
	if (_psr.need_statistics()) {
		show_statistics();
	}
	return 0;
}

void osm_map::show_statistics(void)
{
	printf("node: %lu, way: %lu\n", _node_count, _way_count);
	printf("road node: %lu, road: %lu\n",
		_highway_node_count, _highway_count);
	printf("\n%-16s| %-8s| %-8s| %s\n", " road type",
		"roads", "nodes", "key nodes");
	printf("----------------+---------+---------+-------------\n");

	for (int i = 0; i <= TAG_HIGHWAY_END_OF_OTHER_FEATURES; ++i) {
		if (!_highway_classified_count[i]) continue;
		printf("%-16s| %-8lu| %-8lu| %lu\n",
			get_highway_type_name(i),
			_highway_classified_count[i],
			_allnode_classified_count[i],
			_keynode_classified_count[i]);
	}

	printf("\nnon-referred type(s):\n");
	for (int i = 0, j = 0; i <= TAG_HIGHWAY_END_OF_OTHER_FEATURES; ++i) {
		if (_highway_classified_count[i]) continue;
		printf("%-23s", get_highway_type_name(i));
		if (++j > 4) { printf("\n"); j = 0; }
	}
	printf("\n\n");
}

static int name_array_qsort_cmp(const void* a, const void* b)
{
	char* s1 = *((char**)a);
	char* s2 = *((char**)b);
	int ret = strcmp(s1, s2);
	if (ret < 0) return -1;
	else if (ret > 0) return 1;
	else return 0;
}

int osm_map::mark_key_nodes(void)
{
	auto* item = _highway_node_list.next;
	for (; item != &_highway_node_list; item = item->next) {
		auto* node = list_entry(osm_node, ownerlist, item);
		if (mark_key_node(node)) {
			return -1;
		}
		nodeinfo_statistics(node);
	}
	return 0;
}

int osm_map::mark_key_node(osm_node* node)
{
	int i, count = node->get_wayref_count();
	if (count < 1) {
		return 0;
	}
	else if (count == 1) {
		// the node is only belong to one road
		auto* wref = list_entry(osm_wayref, node_ownerlist,\
			node->wayref_list.next);
		auto& set = wref->way->node_set;

		if (set.buffer()[0] != node && node !=
			set.buffer()[set.getsize() - 1]) {
			// the node is not the first or the last node
			// in the way, it is not a key node
			return 0;
		}
	}
	node->attr.is_keynode = 1;

	char** name_array = (char**)alloca(sizeof(char**) * count);
	assert(nullptr != name_array);
	memset(name_array, 0, sizeof(char*) * count);

	auto* item = node->wayref_list.next;
	for (i = 0; item != &node->wayref_list;
		item = item->next) {
		auto* wref = list_entry(osm_wayref, node_ownerlist, item);
		auto* way = wref->way;

		if (way->name) {
			// discard the duplicate name
			int j; for (j = 0; j < i; ++j) {
				if (!strcmp(name_array[j], way->name)) {
					break;
				}
			}
			if (j != i) continue;
			name_array[i++] = strdup(way->name);
		}
	}
	if (i != 0) {
		qsort(name_array, i, sizeof(char*),
			name_array_qsort_cmp);
		node->data = (osm_node_extra_data*)malloc(
			sizeof(osm_node_extra_data) + i * sizeof(char*));
		if (nullptr == node->data) {
			return -1;
		}
		for (int j = 0; j < i; ++j) {
			node->data->intersect_name[j] = name_array[j];
		}
		node->data->intersect_name_count = i;
	}
	return 0;
}

int osm_map::finalize_highway(void)
{
	// first we finalize associating with nodes
	auto* item = _highway_list.next;
	for (; item != &_highway_list; item = item->next) {
		auto* way = list_entry(osm_way, ownerlist, item);
		if (associate_node(way)) return -1;
		if (wayinfo_statistics(way)) return -2;
	}
	return 0;
}

int osm_map::wayinfo_statistics(osm_way* way)
{
	if (!_psr.need_statistics()) {
		return 0;
	}
	int type = way->attr.highway_type;
	if (type < 0 || type > TAG_HIGHWAY_END_OF_OTHER_FEATURES) {
		return -1;
	}
	_highway_classified_count[type]++;
	return 0;
}

int osm_map::merge_ways(void)
{
	auto* item = _highway_node_list.next;
	for (; item != &_highway_node_list; item = item->next)
	{
		auto* node = list_entry(osm_node, ownerlist, item);

		osm_wayref* fan_in_wref =
			node->get_next_inout_wayref(osm_node::fan_in);

		while (nullptr != fan_in_wref)
		{
			osm_way* merge_target = nullptr;
			osm_wayref* fan_out_wref =
				node->get_next_inout_wayref(osm_node::fan_out);
			while (nullptr != fan_out_wref) {
				osm_way* way1 = fan_in_wref->way;
				osm_way* way2 = fan_out_wref->way;
				
				// check if  2 ways could be merged
				if (way_could_merge(way1, way2)) {
					// having multiple mergable way shall also
					// be regarded as "not mergable"
					if (!merge_target) {
						merge_target = way2;
					} else {
						merge_target = nullptr;
						break;
					}
				}

				fan_out_wref = node->get_next_inout_wayref(
					osm_node::fan_out, fan_out_wref);
			}

			// do merge if needed
			if (merge_target) {
				if (merge_roads(fan_in_wref->way, merge_target)) {
					return -1;
				}
			}

			fan_in_wref = node->get_next_inout_wayref(
				osm_node::fan_in, fan_in_wref);
		}
	}
	return 0;
}


int osm_map::nodeinfo_statistics(osm_node* node)
{
	if (!_psr.need_statistics()) {
		return 0;
	}

	int type = TAG_HIGHWAY_END_OF_OTHER_FEATURES + 1;
	auto* item = node->wayref_list.next;
	for (; item != &node->wayref_list; item = item->next) {
		auto* wref = list_entry(osm_wayref, node_ownerlist, item);
		auto* way = wref->way;
		if (way->attr.highway_type < type) {
			type = way->attr.highway_type;
		}
	}
	assert(type <= TAG_HIGHWAY_END_OF_OTHER_FEATURES);
	++_allnode_classified_count[type];
	if (node->attr.is_keynode) {
		++_keynode_classified_count[type];
	}
	return 0;
}

bool osm_way::same_type(osm_way* way)
{
	if (attr.is_oneway != way->attr.is_oneway) {
		return false;
	}
	if (attr.is_highway != way->attr.is_highway) {
		return false;
	}
	if (attr.is_bridge != way->attr.is_bridge) {
		return false;
	}
	if (attr.lanes != way->attr.lanes) {
		return false;
	}
	if (attr.maxspeed != way->attr.maxspeed) {
		return false;
	}
	if (attr.highway_type != way->attr.highway_type) {
		return false;
	}
	return true;
}

bool osm_map::way_could_merge(osm_way* way1, osm_way* way2)
{
	// a close loop could not be merged
	if (way1->attr.close_loop
		|| way2->attr.close_loop) {
		return false;
	}

	if (!way1->name && !way2->name) {
		if (way1->same_type(way2)) return true;
	}
	else if (way1->name && way2->name
		&& !strcmp(way1->name, way2->name)) {
		if (way1->same_type(way2)) return true;
	}
	return false;
}

int osm_map::merge_roads(osm_way* way1, osm_way* way2)
{
	assert(way1->node_set.buffer()[way1->node_set.getsize() - 1]\
		== way2->node_set.buffer()[0]);

	int way1_count = way1->node_set.getsize();
	int total_count = way1_count + way2->node_set.getsize();
	for (int i = way1_count, j = 0; i < total_count; ++i, ++j) {
		way1->node_set.add(way2->node_set.buffer()[j]);
	}
	// move all wayref object to way1
	while (!listnode_isempty(way2->wref_list))
	{
		auto* wref = list_entry(osm_wayref, way_ownerlist, \
			way2->wref_list.next);
		// reset the reference object to way1
		wref->way = way1;
		wref->ndref_index += way1->node_set.getsize();
		listnode_del(wref->way_ownerlist);
		listnode_add(way1->wref_list, wref->way_ownerlist);
	}

	// check if this become a close loop
	if (way1->node_set.buffer()[way1->node_set.getsize()
		 - 1] == way1->node_set.buffer()[0]) {
		way1->attr.close_loop = 1;
		way1->node_set.remove(1);
	}

	// release the way2 object
	listnode_del(way2->ownerlist);
	delete way2;

	// decrease the way count
	--_highway_count;
	return 0;
}


osm_wayref::osm_wayref(osm_way* _way)
{
	listnode_init(node_ownerlist);
	listnode_add(_way->wref_list, way_ownerlist);
	this->way = _way;
}

int osm_map::associate_node(osm_way* way)
{
	// check if this is a close loop
	int count = way->node_set.getsize();
	if (way->node_set.buffer()[0] == way->node_set.buffer()
		[count - 1] && count > 1) {
		way->attr.close_loop = 1;
		// omit the last node since this is
		// a close loop
		way->node_set.remove(1);
		--count;
	}

	for (int i = 0; i < count; ++i)
	{
		auto nid = (int64_t)way->node_set.buffer()[i];
		auto* item = avl_find(_node_tree,
			MAKE_FIND_OBJECT(nid, osm_node, id, avlnode),
			osm_node::avl_compare);

		if (nullptr == item) {
			fprintf(stderr, "error: node:%lu (in way:%lu) not found.\n",
				nid, way->id);
			continue;
		}

		// update the reference
		auto* node = AVLNODE_ENTRY(osm_node, avlnode, item);
		way->node_set.buffer()[i] = node;

		// create way reference object
		osm_wayref* wayref = new osm_wayref(way);
		if (nullptr == wayref) return -1;

		wayref->ndref_index = i;
		listnode_add(node->wayref_list, wayref->node_ownerlist);
		if (listnode_isempty(node->ownerlist)) {
			++_highway_node_count;
			listnode_add(_highway_node_list, node->ownerlist);
		}

		// create links
	}
	return 0;
}

osm_way* osm_map::create_way_with_node_ref(const XMLElement* e)
{
	auto* way = new osm_way();
	if (nullptr == way) {
		return way;
	}

	for (const XMLElement* tmp = e->FirstChildElement();
		tmp; tmp = tmp->NextSiblingElement()) {
		const char* name = tmp->Name();
		if (!strcmp(name, "nd")) {
			int64_t ref;
			if (tmp->QueryAttribute("ref", &ref)) {
				return nullptr;
			}
			way->node_set.add((osm_node*)ref);
		}
	}

	// set the wayid
	if (e->QueryAttribute("id", &way->id) || !way->id) {
		delete way;
		return nullptr;
	}
	return way;
}

osm_node::osm_node()
: data(nullptr)
{
	listnode_init(ownerlist);
	listnode_init(wayref_list);
	memset(&attr, 0, sizeof(attr));
}

osm_node::~osm_node()
{
	if (data) {
		free(data);
		data = nullptr;
	}
}

// one way direction:
// start node -> end node
osm_wayref* osm_node::get_next_inout_wayref(fan_inout inout,
	osm_wayref* wayref)
{
	auto* item = wayref_list.next;
	if (wayref) {
		item = wayref->node_ownerlist.next;
	}

	for (; item != &wayref_list; item = item->next) {
		auto* ref = list_entry(osm_wayref, node_ownerlist, item);
		osm_way* way = ref->way;
		
		// a close loop may neither be regarded
		// as fan-in nor as fan-out
		if (way->attr.close_loop) {
			continue;
		}

		// we could not determine fan-in/fan-out
		// for a non-oneway way, just omit it
		if (!way->attr.is_oneway) {
			continue;
		}

		int idx = ref->ndref_index;
		if (inout == fan_in) {
			if (idx == way->node_set.getsize() - 1) {
				return ref;
			}
		} else {
			if (!idx) {
				return ref;
			}
		}
	}
	return nullptr;
}

int osm_node::get_inout_wayref_count(fan_inout inout,
	osm_wayref** wayref)
{
	int count = 0;
	auto* node = get_next_inout_wayref(inout);
	if (wayref) *wayref = node;
	for (; node; ++count) {
		node = get_next_inout_wayref(inout, node);
	}
	return count;
}

int osm_node::get_wayref_count(void)
{
	int ret = 0;
	auto* item = wayref_list.next;
	for (; item != &wayref_list;
		item = item->next, ++ret);
	return ret;
}

int osm_node::avl_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(osm_node, avlnode, a);
	auto* bb = AVLNODE_ENTRY(osm_node, avlnode, b);
	if (aa->id > bb->id) return 1;
	else if (aa->id < bb->id) return -1;
	else return 0;
}

osm_way::osm_way()
: node_set(16)
, name(nullptr), id(0)
{
	listnode_init(ownerlist);
	listnode_init(wref_list);
	memset(&attr, 0, sizeof(attr));
}

osm_way::~osm_way()
{
	if (name) {
		free(name);
		name = nullptr;
	}

	// remove all wayref from the way
	while (!listnode_isempty(wref_list)) {
		auto* wref = list_entry(osm_wayref, way_ownerlist, \
			wref_list.next);
		listnode_del(wref->way_ownerlist);
	}
}

bool map_scope::in_scope(double lat, double lon, int* cols, int* rows)
{
	int ret = true;
	if (lat < minlat) { lat = minlat; ret = false; }
	if (lat > maxlat) { lat = maxlat; ret = false; }
	if (lon < minlon) { lon = minlon; ret = false; }
	if (lon > maxlon) { lon = maxlon; ret = false; }

	if (nullptr == cols || nullptr == rows) {
		return ret;
	}
	auto col = *cols, row = *rows;
	if (!col || !row) {
		return ret;
	}

	double delta_lat = (maxlat - minlat) / double(row);
	double delta_lon = (maxlon - minlon) / double(col);

	int x = int((lon - minlon) / delta_lon);
	int y = int((lat - minlat) / delta_lat);

	if (cols) *cols = (x < col) ? x : (col - 1);
	if (rows) *rows = (y < row) ? y : (row - 1);
	return ret;
}

} // end of namespace osm_parser1
/* EOF */
