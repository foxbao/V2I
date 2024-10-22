/** @file hdmap.h
 *  definition of HD Map
 */

#ifndef __CXX_ZAS_MAPCORE_HDMAP_H__
#define __CXX_ZAS_MAPCORE_HDMAP_H__

#include <set>
#include <vector>
#include <string>
#include <Eigen/Dense>
#include "mapcore/mapcore.h"
#include "Viewer/RoadNetworkMesh.h"

namespace zas {
namespace mapcore {

using namespace std;

class sdmap;
class rendermap;

// ro = road object
class hdrmap_ro_signal_light;
class hdrmap_ro_roadside_device;

class MAPCORE_EXPORT rendermap_junction
{
public:
	rendermap_junction() = delete;
	~rendermap_junction() = delete;

	/*
	 * get the center point of the junction
	 * @param [out] x of center point in UTM
	 * @param [out] y of center point in UTM
	 * @param [out] radius of the junction based
	 * 		the center point
	 */
	int center_point(double& x, double& y, double& radius) const;

	/*
	 * get the junction unique ID
	 * @return the junction ID
	 */
	uint32_t getid(void) const;

	/*
	 * get the junction description (intersect street names)
	 * @param map the SDMap used to extract the description
	 * @param rendermap the HD Rendermap that the junction get from
	 * @param names the description (street names) extracted
	 * 		and ordered [a-z]
	 * @return 0 for success
	 */
	int extract_desc(const sdmap* smap, const rendermap* rmap,
		vector<const char*>& names) const;

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(rendermap_junction);
};

struct rendermap_nearest_junctions
{
	rendermap_junction* junction;
	double distance;
};

class MAPCORE_EXPORT rendermap_roadobject
{
public:
	rendermap_roadobject() = delete;
	~rendermap_roadobject() = delete;

	/**
	 * get the road object type
	 * @return the type of road object
	 */
	hdrmap_roadobject_type gettype(void) const;

	/**
	 * get the signal light info
	 * note: the type of road object must be "siglight"
	 * otherwise it will return nullptr
	 * @return the pointer to the signal light object
	 */
	const hdrmap_ro_signal_light* as_siglight(void) const;

	/**
	 * get the road side device info
	 * note: the type of road object must be "road side device"
	 * otherwise it will return nullptr
	 * @return the pointer to the road side device object
	 */
	const hdrmap_ro_roadside_device* as_rsd(void) const;

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(rendermap_roadobject);
};

class MAPCORE_EXPORT hdrmap_ro_signal_light
{
public:
	hdrmap_ro_signal_light() = delete;
	~hdrmap_ro_signal_light() = delete;

	/**
	 * get the signal light unique ID
	 * @return uid
	 */
	uint64_t getuid(void) const;

	/**
	 * get the base point of the signal
	 * @return point
	 */
	const point3d& getpos(void) const;

	/**
	 * get the base point of the signal
	 */
	void getpos(point3d&) const;

	/**
	 * get the rotate matrix of signal light
	 * @return matrix
	 */
	const double* get_rotate_matrix(void) const;

	/**
	 * get the rotate matrix of signal light
	 */
	void get_rotate_matrix(double* mtx) const;

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(hdrmap_ro_signal_light);
};

class MAPCORE_EXPORT hdrmap_ro_roadside_device
{
public:
	hdrmap_ro_roadside_device() = delete;
	~hdrmap_ro_roadside_device() = delete;

	/**
	 * get the road side device position ID
	 * @return uid
	 */
	uint64_t get_spotid(void) const;

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(hdrmap_ro_roadside_device);
};

class MAPCORE_EXPORT hdrmap_ro_signal_board
{
public:
	hdrmap_ro_signal_board() = delete;
	~hdrmap_ro_signal_board() = delete;

	/**
	 * get the signal board unique ID
	 * @return uid
	 */
	uint64_t getuid(void) const;

	/**
	 * get the signal board type
	 * this includes major type and sub type
	 * @return the type
	 */
	uint32_t get_type(void) const;

	/**
	 * get the signal board major type
	 * @return major type
	 */
	uint32_t get_major_type(void) const;

	/**
	 * get the signal board sub type
	 * @return sub type
	 */
	uint32_t get_sub_type(void) const;
};

struct rendermap_info
{
	// the unique ID of this rendermap
	uint128_t uuid;

	// block rows and cols
	// rows * cols = total block count
	uint32_t rows, cols;

	// count of junctions
	uint32_t junction_count;

	// the bounding box of the rendermap
	double xmin, ymin, xmax, ymax;
};

class MAPCORE_EXPORT rendermap
{
	friend class rendermap_junction;
public:
	rendermap();
	~rendermap();

	/*
	 * load the rendermap from a file
	 * @param filename the file to be loaded
	 * @return 0 for success
	 */
	int load_fromfile(const char* filename);

	/*
	 * load the rendermap from the buffer
	 * @param buffer the memory buffer
	 * @return 0 for success
	 */
	int load_frombuffer(const void* buffer, size_t sz);

	/*
	 * load the rendermap from the string buffer
	 * @param buffer the string buffer
	 * @return 0 for success 
	 */
	int load_frombuffer(const string& buffer);

	/*
	 * load the specific blocks from the rendermap
	 * @param blkset the block number set of blocks to be extracted
	 * @param shrblkset don't extract the referred render objects
	 * 		from the specified blocks since it is already downloaded
	 * @param ret the binary data extracted
	 * @return 0 for success
	 */
	int extract_blocks(const set<uint32_t>& blkset,
		const set<uint32_t>& shrblkset,
		string& ret) const;

	/*
	 * get the dependencies (also the block IDs) of specified blocks
	 * @param input the specified blocks (in ID form)
	 * @parma output the dependent blocks (in ID form)
	 * @return 0 for success
	 */
	int get_block_dependencies(
		const set<uint32_t>& input, set<uint32_t>& output) const;

	/*
	 * find the nearest junctions
	 * @param x the x coordination
	 * @param y the y coordination
	 * @param count how many nearest items are there in
	 * 		this rendermap
	 * @param set the buffer to room the items
	 * @return 0 for success
	 */
	int find_junctions(double x, double y, int count,
		vector<rendermap_nearest_junctions>& set) const;

	/*
	 * get junction by ID
	 * @param juncid
	 * @return the junction object 
	 */
	const rendermap_junction* get_junction(uint32_t juncid) const;
	
	/*
	 * get junction by index
	 * @param index
	 * @return the junction object
	 */
	const rendermap_junction* get_junction_by_index(uint32_t idx) const;

	/*
	 * extract road objects from the block by the category
	 * @param blkid the block to be extracted for road objects
	 * @param the categories to be extracted
	 * @param objects the set of objects extracted
	 * @param reset reset the object set before add extracted objects to
	 * @return 0 for success
	 */
	int extract_roadobjects(int blkid, const set<uint32_t>& categories,
		set<const rendermap_roadobject*>& objects, bool reset = false) const;

	/*
	 * get the geographic reference string that could be used
	 * for projection
	 * @return the georef string, nullptr for error
	 */
	const char* get_georef(void) const;

	/*
	 * get the map offset against the UTM projection
	 * @param xoffs, yoffs the offset
	 * @return 0 for success
	 */
	int get_offset(double& dx, double& dy) const;

	/*
	 * get the rendermap information
	 * @return 0 for success
	 */
	int get_mapinfo(rendermap_info& mapinfo) const;

private:
	void* _data;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(rendermap);
};

class MAPCORE_EXPORT tilecache
{
public:
	tilecache();
	~tilecache();

	/*
	 * bind map using the mapinfo
	 * @param mapinfo the buffer of mapinfo downloaded
	 * 		from the map server
	 * @param sz the size of mapinfo buffer
	 * @return 0 for success
	 */
	int bind_map(const void* mapinfo, size_t sz);

	/*
	 * bind map using the mapinfo
	 * @param mapinfo the string form of mapinfo
	 * 		downloaded from the map server
	 * @return 0 for success
	 */
	int bind_map(const string& mapinfo);

	/*
	 * add a tile to the tile cache
	 * @param buffer the raw render tile downlaoded
	 * 		from the map server
	 * @param sz the size of the raw render map
	 * @return >= 0 success and the return value is the
	 * 		count of blocks in raw data
	 * 		< 0 means error
	 */
	int add_tile(const void* buffer, size_t sz);

	/*
	 * add a tile to the tile cache
	 * @param buffer the string form raw render tile
	 * 		buffer downloaded from map server
	 * @return >= 0 success and the return value is the
	 * 		count of blocks in raw data
	 * 		< 0 means error
	 */
	int add_tile(const string& buffer);

	/*
	 * get all mesh of a block
	 * @param blkid the id of the block
	 * @param regen indicate if this call generate the
	 * 		3DMesh or just returned previouslyu generated one
	 * @return the road network mesh for this block
	 */
	shared_ptr<const odr::RoadNetworkMesh> get_mesh(uint32_t blkid, bool* gen = nullptr);

	/*
	 * release the generated mesh of a block
	 * this is used to release memory when the block is
	 * no longer to be rendered
	 * @param blkid the id of the block
	 * @return 0 for success
	 */
	int release_mesh(uint32_t blkid);

	/*
	 * search all unloaded blocks covering the polygon
	 * the block IDs could be used to request raw rendering
	 * map data from the map server
	 * @param pts the polygon requested
	 * @param blkids all unloaded block IDs
	 */
	int query_unloaded_blocks(const vector<point2d>& pts,
		set<uint32_t>& blkids) const;

	/*
	 * search all loaded blocks covering the polygon
	 * @param pts the polygon requested
	 * @param blkids all loaded block IDs
	 */
	int query_loaded_blocks(const vector<point2d>& pts,
		set<uint32_t>& blkids) const;

	/*
	 * extract specified road objects from the specified block
	 * @param blkid the ID of the specified block
	 * @param categories the categories of road objects to be extracted
	 * @param object store all extracted road objects
	 * @return 0 for success
	 */
	int extract_roadobjects(int blkid, const set<uint32_t>& categories,
		set<const rendermap_roadobject*>& objects) const;

	/*
	 * extract specified road objects from blocks in specified area
	 * (in polygon form)
	 * @param pts specify area as a polygon (multiple point2ds)
	 * @param categories the categories of road objects to be extracted
	 * @param object store all extracted road objects
	 * @return 0 for success
	 */
	int extract_roadobjects(const vector<point2d>& pts,
		const set<uint32_t>& categories, set<const rendermap_roadobject*>& objects) const;

	/*
	 * get the map unique id that current tilecache
	 * is caching
	 * @param id the returned map id
	 * @return true for success
	 */
	bool get_mapid(uint128_t& id) const;

	/*
	 * check all block IDs in the set and remove all
	 * IDs that is not loaded in the tilecache
	 * @param [in/out] the IDs not loaded in this tile
	 * 		cache will be removed from this set after this call
	 * @param lock lock all remaining IDs (loaded in this
	 * 		tilecache) so that it could not be released
	 * 		before unlock is called
	 * @return 0 for success
	 */
	int filter_loaded_blocks(set<uint32_t>& blkids, bool lock);

	/*
	 * lock all blocks in the set if it exists in this
	 * tilecache. these blocks could not be released until
	 * it is unlocked
	 * @param blkids the block set
	 * @return 0 for success
	 */
	int lock_blocks(const set<uint32_t>& blkids);

	/*
	 * unlock all blocks in the set if it exists in this
	 * tilecache. these blocks could be released thereafter
	 * @param blkids the block set
	 * @return 0 for success
	 */
	int unlock_blocks(const set<uint32_t>& blkids);

	/*
	 * release blocks in the set
	 * @param blkids the block set
	 * @return 0 for success
	 */
	int release_blocks(const set<uint32_t>& blkids);

	/*
	 * get the map projection string (UTM projection)
	 * @return the string or nullptr if map is not binded
	 */
	const char* get_projection(void) const;

	/*
	 * get the map offset in UTM projection
	 * @return the point3d of the offset
	 */
	const point3d& get_mapoffset(void) const;

private:
	void* _data;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(tilecache);
};

struct hdmap_info
{
	// the unique ID of this rendermap
	uint128_t uuid;

	// block rows and cols
	// rows * cols = total block count
	uint32_t rows, cols;

	// count of junctions
	uint32_t road_count;
	uint32_t junction_count;

	// the bounding box of the rendermap
	double xmin, ymin, xmax, ymax;
};

struct Pose
{
    double angle;
    Eigen::Vector3d ENU;
};

struct Curve
{
    uint64_t id;
    std::vector<Eigen::Vector3d> p3d;
};
// struct central_curve:Curve
// {};




struct hdmap_segment_point
{
	hdmap_segment_point() {s = 0.0; }
	
	hdmap_segment_point(const hdmap_segment_point& hbpt) {
		pt = hbpt.pt; s = hbpt.s;
	}
	hdmap_segment_point& operator=(const hdmap_segment_point& o) {
		pt = o.pt; s = o.s;
		return *this;
	}
	point3d pt;
	double s;
};

class MAPCORE_EXPORT hdmap_junction
{
public:
	hdmap_junction() = delete;
	~hdmap_junction() = delete;

	/*
	 * get the center point of the junction
	 * @param [out] x of center point in UTM
	 * @param [out] y of center point in UTM
	 * @param [out] radius of the junction based
	 * 		the center point
	 */
	int center_point(double& x, double& y, double& radius) const;

	/*
	 * get the junction unique ID
	 * @return the junction ID
	 */
	uint32_t getid(void) const;

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(hdmap_junction);
};

class MAPCORE_EXPORT hdmap_refrenceline
{
public:
	hdmap_refrenceline() = delete;
	~hdmap_refrenceline() = delete;
	double get_length(void) const;
private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(hdmap_refrenceline);
};

class MAPCORE_EXPORT hdmap_geo_spiral
{
public:
	hdmap_geo_spiral() = delete;
	~hdmap_geo_spiral() = delete;

	int get_curv(double &start, double &end) const;
};

class MAPCORE_EXPORT hdmap_geo_arc
{
public:
	hdmap_geo_arc() = delete;
	~hdmap_geo_arc() = delete;

	int get_curvature(double &curvature) const;
};

class MAPCORE_EXPORT hdmap_geo_param_poly3
{
public:
	hdmap_geo_param_poly3() = delete;
	~hdmap_geo_param_poly3() = delete;

	int get_param(double &aU, double &bU, double &cU, double &dU,
		double &aV, double &bV, double &cV, double &dV,
		hdmap_poly3_range &range) const;
};

class MAPCORE_EXPORT hdmap_geometry
{
public:
	hdmap_geometry() = delete;
	~hdmap_geometry() = delete;

	int get_geo(double &s, double &x, double &y,
		double &hdg, double &length) const;
	hdmap_geometry_type get_type(void) const;
	hdmap_geo_spiral* get_spiral(void) const;
	hdmap_geo_arc* get_arc(void) const;
	hdmap_geo_param_poly3* get_param_poly3(void) const;
};

class MAPCORE_EXPORT hdmap_poly3
{
public:
	hdmap_poly3() = delete;
	~hdmap_poly3() = delete;
	int get_poly(double &a, double &b, double &c, double &d) const;
};

class hdmap_lane;
class hdmap_road;
class MAPCORE_EXPORT hdmap_laneseg
{
public:
	hdmap_laneseg() = delete;
	~hdmap_laneseg() = delete;
	
	int get_id() const;
	int get_endpoint(point3d& start, point3d& end) const;
	int get_insider_point(std::map<double, hdmap_segment_point>& ipoints) const;
	int get_center_line(std::vector<point3d>& line) const;
	int get_outsider_point(std::map<double, hdmap_segment_point>& opoints) const;
	const hdmap_laneseg* get_adc_and_point(double s, hdmap_segment_point& spt, hdmap_segment_point& nextpt, double &hdg, double& w, std::vector<const hdmap_laneseg*> &route) const;
	hdmap_laneseg* get_next_laneseg_by_hdg(const hdmap_laneseg* ls, double hdg) const;
	// 0 : lane self next, 1: outsider, -1 insider
	const hdmap_laneseg* get_next_laneseg(const hdmap_laneseg* ls, int direct) const;
	hdmap_laneseg* get_insider(void) const;
	hdmap_laneseg* get_outsider(void) const;
	hdmap_lane* get_lane(void) const;

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(hdmap_laneseg);
};

class MAPCORE_EXPORT hdmap_lane
{
public:
	hdmap_lane() = delete;
	~hdmap_lane() = delete;

	int get_predecessors(vector<hdmap_lane*>& set) const;
	int get_successors(vector<hdmap_lane*>& set) const;
	int get_lanesegs(vector<hdmap_laneseg*>& set) const;
	const hdmap_laneseg* get_lane_first_segment(void) const;
	const hdmap_laneseg* get_lane_last_segment(void) const;
	hdmap_road* get_road(void) const;
	int get_id(void) const;
	double get_length(void) const;

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(hdmap_lane);
};

class MAPCORE_EXPORT hdmap_road
{
public:
	hdmap_road() = delete;
	~hdmap_road() = delete;

	int get_lanes(vector<hdmap_lane*>& set) const;
	const hdmap_lane* get_lane_by_id(int laneid) const;
	double get_length(void) const;
	int get_junctionid(void) const;
	uint32_t get_road_id(void) const;

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(hdmap_road);
};

struct hdmap_nearest_junctions
{
	hdmap_junction* junction;
	double distance;
};

struct hdmap_nearest_laneseg
{
	hdmap_laneseg* laneseg;
	double distance;
};

class MAPCORE_EXPORT hdmap
{
	friend class hdmap_junction;
public:
	hdmap();
	~hdmap();

	/*
	 * load the hdmap from a file
	 * @param filename the file to be loaded
	 * @return 0 for success
	 */
	int load_fromfile(const char* filename);

	// /*
	//  * load the hdmap from the buffer
	//  * @param buffer the memory buffer
	//  * @return 0 for success
	//  */
	// int load_frombuffer(const void* buffer, size_t sz);

	// /*
	//  * load the hdmap from the string buffer
	//  * @param buffer the string buffer
	//  * @return 0 for success 
	//  */
	// int load_frombuffer(const string& buffer);

	/*
	 * find the nearest laneseg
	 * @param x the x coordination
	 * @param y the y coordination
	 * @param count how many nearest items are there in
	 * 		this hdmap
	 * @param set the buffer to room the items
	 * @return 0 for success
	 */
	int find_laneseg(double x, double y, int count,
		vector<hdmap_nearest_laneseg>& set) const;

	/*
	 * find the laneseg
	 * @param point3d (utm)
	 * @param hdg (heading)
	 * @param point in laneseg center
	 * @return point of laneseg
	 */
	const hdmap_laneseg* find_laneseg(point3d& src, double &hdg,
		hdmap_segment_point& centet_line_pt, std::vector<point3d> *line) const;

	int get_laneseg_width(const hdmap_laneseg* from, const hdmap_laneseg *to, double s, double& fwidth, double& twidth, double& bwidth) const;

	const hdmap_road* get_road_by_id(uint32_t rid) const;

	bool is_in_lane(const hdmap_laneseg *ls, double s, point3d &pt) const;
	
	bool is_same_road(const hdmap_laneseg *lss, const hdmap_laneseg *lsd) const;

	int get_neareast_points(point3d p3d, double hdg, std::map<uint64_t, std::shared_ptr<hdmap_segment_point>> &points, double distance) const;

	int get_neareast_laneses(point3d p3d, double hdg, std::map<uint64_t, std::shared_ptr<hdmap_segment_point>> points, std::map<uint64_t, double> &lanesegs) const;

	int generate_lanesect_transition(const char* filepath);
	// /*
	//  * get junction by ID
	//  * @param juncid
	//  * @return the junction object 
	//  */
	// const hd_junction* get_junction(uint32_t juncid) const;
	
	// /*
	//  * get junction by index
	//  * @param index
	//  * @return the junction object
	//  */
	// const hdmap_junction* get_junction_by_index(uint32_t idx) const;

	/*
	 * get the geographic reference string that could be used
	 * for projection
	 * @return the georef string, nullptr for error
	 */
	const char* get_georef(void) const;

	/*
	 * get the map offset against the UTM projection
	 * @param xoffs, yoffs the offset
	 * @return 0 for success
	 */
	int get_offset(double& dx, double& dy) const;

	/*
	 * get the rendermap information
	 * @return 0 for success
	 */
	int get_mapinfo(hdmap_info& mapinfo) const;

	std::vector<uint64_t> get_lanes_near_pt_enu(Eigen::Vector3d p3d, double radius = 20);

	std::vector<uint64_t> get_lanes_near_pt_enu(std::vector<Eigen::Vector3d> trajectory, double radius = 20);

	std::vector<uint64_t> get_lanes_near_pt_enu(std::vector<Pose> trajectory, double radius = 20);

	Eigen::MatrixXd CalculateTransitionalProbability(std::vector<uint64_t> lanes_id);

	Eigen::MatrixXd CalculateTransitionalProbability(std::vector<uint64_t> lanes_id_1,std::vector<uint64_t> lanes_id_2);

	double get_distance_pt_curve_enu(const Eigen::Vector3d &pt_enu, uint64_t lane_id, Eigen::Vector3d &cross_pt_curve);

	std::vector<Curve> get_central_curves_enu(void);

	std::vector<Curve> get_boundary_curves_enu(void);

	std::vector<Eigen::Vector3d> get_curve(uint64_t lane_id);

	double get_distance_pt_closest_central_line_enu(const Eigen::Vector3d &pt_enu, Eigen::Vector3d &cross_pt_map_enu);

private:
	void* _data;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(hdmap);
};

}} // end of namespace zas::mapcore
#endif // __CXX_ZAS_MAPCORE_HDMAP_H__
/* EOF */