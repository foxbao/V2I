/** @file sdmap.h
 */

#ifndef __CXX_ZAS_MAPCORE_SDMAP_H__
#define __CXX_ZAS_MAPCORE_SDMAP_H__

#include <vector>
#include "mapcore/shift.h"

namespace zas {
namespace mapcore {

class sdmap_node;
class sdmap_link;
class sdmap_way;

enum way_type
{
	WAY_TYPE_START_OF_MAIN,
	WAY_TYPE_MOTORWAY = 0,
	WAY_TYPE_TRUNK,
	WAY_TYPE_PRIMARY,
	WAY_TYPE_SECONDARY,
	WAY_TYPE_TERTIARY,
	WAY_TYPE_UNCLASSIFIED,
	WAY_TYPE_RESIDENTIAL,
	WAY_TYPE_END_OF_MAIN = WAY_TYPE_RESIDENTIAL,

	// link roads
	WAY_TYPE_START_OF_LINK_ROADS,
	WAY_TYPE_MOTORWAY_LINK = WAY_TYPE_START_OF_LINK_ROADS,
	WAY_TYPE_TRUNK_LINK,
	WAY_TYPE_PRIMARY_LINK,
	WAY_TYPE_SECONDARY_LINK,
	WAY_TYPE_TERTIARY_LINK,
	WAY_TYPE_END_OF_LINK_ROADS = WAY_TYPE_TERTIARY_LINK,

	// Special road types
	WAY_TYPE_START_OF_SPECIAL_ROAD_TYPES,
	WAY_TYPE_LIVING_STREET = WAY_TYPE_START_OF_SPECIAL_ROAD_TYPES,
	WAY_TYPE_SERVICE,
	WAY_TYPE_PEDESTRIAN,
	WAY_TYPE_TRACK,
	WAY_TYPE_BUS_GUIDEWAY,
	WAY_TYPE_ESCAPE,
	WAY_TYPE_RACEWAY,
	WAY_TYPE_ROAD,
	WAY_TYPE_BUSWAY,
	WAY_TYPE_END_OF_SPECIAL_ROAD_TYPES = WAY_TYPE_BUSWAY,

	// paths
	WAY_TYPE_START_OF_PATH,
	WAY_TYPE_FOOTWAY = WAY_TYPE_START_OF_PATH,
	WAY_TYPE_BRIDLEWAY,
	WAY_TYPE_STEPS,
	WAY_TYPE_CORRIDOR,
	WAY_TYPE_PATH,
	WAY_TYPE_CYCLEWAY,
	WAY_TYPE_END_OF_PATH = WAY_TYPE_CYCLEWAY,

	// Lifecycle
	WAY_TYPE_START_OF_LIFECYCLE,
	WAY_TYPE_PLANNED = WAY_TYPE_START_OF_LIFECYCLE,
	WAY_TYPE_PROPOSED,
	WAY_TYPE_CONSTRUCTION,
	WAY_TYPE_END_OF_LIFECYCLE = WAY_TYPE_CONSTRUCTION,

	// Other highway features
	WAY_TYPE_START_OF_OTHER_FEATURES,
	WAY_TYPE_BUS_STOP = WAY_TYPE_START_OF_OTHER_FEATURES,
	WAY_TYPE_CROSSING,
	WAY_TYPE_ELEVATOR,
	WAY_TYPE_EMERGENCY_BAY,
	WAY_TYPE_EMERGENCY_ACCESS_POINT,
	WAY_TYPE_GIVE_WAY,
	WAY_TYPE_MILESTONE,
	WAY_TYPE_MINI_ROUNDABOUT,
	WAY_TYPE_MOTORWAY_JUNCTION,
	WAY_TYPE_PASSING_PLACE,
	WAY_TYPE_PLATFORM,
	WAY_TYPE_REST_AREA,
	WAY_TYPE_SPEED_CAMERA,
	WAY_TYPE_STREEP_LAMP,
	WAY_TYPE_SERVICES,
	WAY_TYPE_STOP,
	WAY_TYPE_TRAFFIC_MIRROR,
	WAY_TYPE_TRAFFIC_SIGNALS,
	WAY_TYPE_TRAILHEAD,
	WAY_TYPE_TURNING_CIRCLE,
	WAY_TYPE_TURNING_LOOP,
	WAY_TYPE_TOLL_GANTRY,
	WAY_TYPE_END_OF_OTHER_FEATURES = WAY_TYPE_TOLL_GANTRY,
};

class MAPCORE_EXPORT sdmap_node
{
public:
	sdmap_node() = delete;
	~sdmap_node() = delete;

	/**
	 * Get the node unique ID
	 * note that it is an 64bit unsigned int
	 * @return the uid
	 */
	uint64_t getuid(void) const;

	/**
	 * Get the latitude
	 * @return latitude
	 */
	double lat(void) const;

	/**
	 * Get the longitude
	 * @return longitude
	 */
	double lon(void) const;

	/**
	 * Get the intersection road names
	 * @param names all names in string form
	 */
	bool get_intersection_roads_name(
		std::vector<const char*>& names) const;

	/**
	 * Get the link count
	 * @return the count
	 */
	int link_count(void) const;

	/*
	 * get the link node in array
	 * @return the link pointer
	 */
	sdmap_link* get_link(int index) const;
};

class MAPCORE_EXPORT sdmap_link
{
public:
	sdmap_link() = delete;
	~sdmap_link() = delete;

	/**
	 * Get the way the link belongs to
	 * @return the way
	 */
	const sdmap_way* get_way(void) const;

	/**
	 * Get the user data of the link
	 * @return the userid
	 */
	void* get_userdata(void) const;

	/**
	 * setter of user data field
	 * in the link object
	 * @param data the data to be set, nullptr means
	 * 		clear the data field
	 * @return void* the previous data that is set, or
	 * 		nullptr if nothing set before
	 * note: this function is not thread-safe, take
	 * 		care of by yourself
	 */
	void* set_userdata(void* data);

	/**
	 * Get the 2 nodes for the link
	 */
	const sdmap_node* get_node1(void) const;
	const sdmap_node* get_node2(void) const;

	const sdmap_link* prev(void) const;
	const sdmap_link* next(void) const;
};

class MAPCORE_EXPORT sdmap_way
{
public:
	sdmap_way() = delete;
	~sdmap_way() = delete;

	/**
	 * Get the name of the way
	 * @return name
	 */
	const char* get_name(void) const;

	/**
	 * Get the way type
	 * @return type
	 */
	int get_type(void) const;
};

struct sdmap_nearest_node
{
	sdmap_node* node;
	double distance;
};

struct sdmap_nearest_link
{
	sdmap_link* link;
	sdmap_node* node;
	coord_t projection;
	double distance;
};

class MAPCORE_EXPORT sdmap
{
public:
	sdmap();
	~sdmap();

	/*
	 * load the sdmap from a file
	 * @param filename the file to be loaded
	 * @return 0 for success
	 */
	int load_fromfile(const char* filename);

	/**
	 * search for the node in the sdmap that is
	 * the nearest one to the specific <lat,lon>
	 * @param c the specified <lat,lon>
	 * @param count the count of nearest nodes
	 * @param nodeset the set of nodes
	 * @return the nearest node
	 */
	int search_nearest_nodes(const coord_t& c,
		int count, sdmap_nearest_node* nodeset) const;

	int search_nearest_nodes(double lat, double lon,
		int count, sdmap_nearest_node* nodeset) const;

	/**
	 * search the nearest node in the sdmap
	 * @param c the specified <lat,lon>
	 * @param distance the distance calculated between
	 * 		the specific point and the nearest node
	 * @param return node the nearest node, nullptr
	 * 		returned if the specific point is out
	 * 		of the sdmap range
	 */
	const sdmap_node* nearest_node(const coord_t& c,
		double& distance) const;

	const sdmap_node* nearest_node(double lat, double lon,
		double& distance) const;

	/**
	 * search for the link in the sdmap that is
	 * the nearest one to the specific <lat,lon>
	 * @param c the specified <lat,lon>
	 * @param links the nearest links in <link_count>
	 * @param node_range how many nodes will it be handled
	 * 		to search for the nearest link
	 * @param link_count how many nearest links
	 * @param node the nearest node
	 * @return 0 for success
	 */
	int search_nearest_links(double lat, double lon, sdmap_nearest_link* links,
		int node_range = 0, int link_count = 1,
		sdmap_node** nearest_node = nullptr) const;

	/**
	 * check if the sdmap is loaded and valid
	 * @return true means loaded and valid
	 */
	bool valid(void) const;

private:
	void* _data;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(sdmap);
};

}} // end of namespace zas::mapcore
#endif /* __CXX_ZAS_MAPCORE_SDMAP_H__ */
/* EOF */
