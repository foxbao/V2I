/** @file hdmap-impl.h
 */

#ifndef __CXX_ZAS_MAPCORE_HDMAP_IMPL_H__
#define __CXX_ZAS_MAPCORE_HDMAP_IMPL_H__

#include <map>
#include <set>

#include "inc/hdmap-format.h"
#include "mapcore/hdmap.h"
#include "std/list.h"
#include "utils/avltree.h"
#include "utils/json.h"

#include "OpenDriveMap.h"
#include "Road.h"
#include "Lanes.h"
#include "Viewer/RoadNetworkMesh.h"


namespace zas {
namespace mapcore {

#define CALC_NUM 3.;

using avl_node_t = zas::utils::avl_node_t;
using hdmap_fileheader_v1 = osm_parser1::hdmap_fileheader_v1;
using offset = osm_parser1::offset;
using hdmap_point_v1 = osm_parser1::hdmap_point_v1;
using hdmap_laneseg_point_v1 = osm_parser1::hdmap_laneseg_point_v1;
using hdmap_lane_boundary_point_v1 = osm_parser1::hdmap_lane_boundary_point_v1;
using hdmap_file_roadseg_v1 = osm_parser1::hdmap_file_roadseg_v1;
using hdmap_file_lane_v1 = osm_parser1::hdmap_file_lane_v1;
using hdmap_file_laneseg_v1 = osm_parser1::hdmap_file_laneseg_v1;
using hdmap_refline_point_v1 = osm_parser1::hdmap_refline_point_v1;
using point_hdmap_file_lane_v1 = osm_parser1::pointer<hdmap_file_lane_v1>;
using odr_lane = odr::Lane;

struct hdmap_lanesect_transition
{
	uint64_t lanesec_index;
	uint64_t road_id;
	double length;
	std::set<uint64_t> lsc_prev_index;
	std::set<uint64_t> lsc_next_index;
	std::map<uint64_t, uint64_t> lsc_left_index;
	std::map<uint64_t, uint64_t> lsc_right_index;
};

// 定义枚举类
enum class BoundaryType {
    center_curve,   // 中心曲线
    inner_boundary, // 内边界
    outer_boundary  // 外边界
};

class hdmap_impl
{
public:
	hdmap_impl();
	~hdmap_impl();

	// load from file
	int load_fromfile(const char *filename);

	int find_laneseg(double x, double y, int count,
		vector<hdmap_nearest_laneseg>& set);
	const hdmap_road* get_road_by_id(uint32_t rid);
	int get_laneseg_width(const hdmap_laneseg* from, const hdmap_laneseg *to, double s, double& fwidth, double& twidth, double& bwidth);
	bool is_in_lane(const hdmap_laneseg *ls, double s, point3d &pt);
	bool is_same_road(const hdmap_laneseg *lss, const hdmap_laneseg *lsd);
	const hdmap_laneseg* find_laneseg(point3d& src, double &hdg,
		hdmap_segment_point& centet_line_pt, std::vector<point3d> *line);
	// const int find_relate_lane_distance(point3d& src, double &hdg, std::map<int, double> &lanes);
	int get_neareast_points(point3d p3d, double hdg, std::map<uint64_t, std::shared_ptr<hdmap_segment_point>> &points, double distance);
	int get_neareast_laneses(point3d p3d, double hdg, std::map<uint64_t, std::shared_ptr<hdmap_segment_point>> points, std::map<uint64_t, double> &lanesegs);
	const char *get_georef(void);
	int get_offset(double &dx, double &dy);
	int get_mapinfo(hdmap_info &mapinfo);

	std::vector<uint64_t> get_lanes_near_pt_enu(Eigen::Vector3d p3d, double radius = 20);
	std::vector<uint64_t> get_lanes_near_pt_enu(std::vector<Eigen::Vector3d> trajectory, double radius = 20);
	std::vector<uint64_t> get_lanes_near_pt_enu(std::vector<Pose> trajectory, double radius = 20);
	Eigen::MatrixXd CalculateTransitionalProbability(std::vector<uint64_t> lanes_id);
	Eigen::MatrixXd CalculateTransitionalProbability(std::vector<uint64_t> lanes_id_1,std::vector<uint64_t> lanes_id_2);
	
	double get_distance_pt_curve_enu(const Eigen::Vector3d &pt_enu, uint64_t lane_id, Eigen::Vector3d &cross_pt_curve);
	std::vector<Curve> get_central_curves_enu(void);
	std::vector<Curve> get_boundary_curves_enu(void);
	std::vector<Curve> get_curves_enu(BoundaryType type=BoundaryType::center_curve);
	
	std::vector<Eigen::Vector3d> get_curve(uint64_t lane_id,BoundaryType type = BoundaryType::center_curve);
	double get_distance_pt_closest_central_line_enu(const Eigen::Vector3d &pt_enu, Eigen::Vector3d &cross_pt_map_enu);
	int	generate_lanesect_transition(const char* filepath);
	
	public:
	bool valid(void) { return (_f.valid) ? true : false; }

	int addref(void) { return __sync_add_and_fetch(&_refcnt, 1); }

	int decref(void) { return __sync_sub_and_fetch(&_refcnt, 1); }

	const proj &get_wgs84proj(void) const { return _wgs84prj; }

	

private:
	void release_all(void);
	int check_validity(void);
	int loadfile(const char *filename);
	int fix_ref(void);
	// int optimize_junctions(void);
	int update_hdmap_info(void);
	int update_hdmap_data(int blockid);
	int remove_hdmap_date(int blockid);
	int finalize(void);
	int drawmap(void);
	int get_refline_point(hdmap_file_roadseg_v1* rs, double s, point3d& pt);
	int get_center_point(hdmap_lane_boundary_point_v1* bpt,
		point3d& rpt, point3d& pt);
	double calcLaneProbability(double num, uint64_t lane_id1, uint64_t lane_id2);
	hdmap_file_laneseg_v1* get_laneseg(point3d& src, double &hdg, double &s);
	int get_neareast_laneseg(point3d &src,double &hdg, std::set<hdmap_file_laneseg_v1*> &lanesegs);
	const hdmap_file_laneseg_v1* get_laneseg_center_width(const hdmap_file_laneseg_v1* ls, double s, double& width);
	int find_boundary(hdmap_file_laneseg_v1* ls,
		hdmap_lane_boundary_point_v1* bpt, point3d& pt, int &right);
	int find_point_post_with_segment(hdmap_lane_boundary_point_v1* start, hdmap_lane_boundary_point_v1* end, point3d& pt, int &right);
	hdmap_file_laneseg_v1* find_laneseg_v1(point3d& src, double &hdg,
		hdmap_lane_boundary_point_v1** bpt);
	hdmap_file_laneseg_v1* get_prev_laneseg(hdmap_file_laneseg_v1* ls);
	const hdmap_file_laneseg_v1* get_next_laneseg(const hdmap_file_laneseg_v1* ls);
	const hdmap_file_laneseg_v1* get_laneseg_center_projection(
		const hdmap_file_laneseg_v1*ls, point3d& src,
		hdmap_segment_point& ppt, double &hdg, double s, std::vector<point3d>* line);
	const hdmap_file_laneseg_v1* get_laneseg_center_projection_distance(
		const hdmap_file_laneseg_v1*ls, point3d& src,
		hdmap_segment_point& ppt, double &hdg, double &s);
	int get_line_projection(point3d* start, point3d* end,
		point3d& pt, point3d& ppt, double &s);
	int get_line_projection_distance(point3d* start, point3d* end,
		point3d& pt, point3d& ppt, double &s);

private:
	hdmap_fileheader_v1 *header(void) {
		return reinterpret_cast<hdmap_fileheader_v1 *>(_buffer);
	}

	void ultoa(uint64_t v, string &str) {
		char buf[64];
		sprintf(buf, "%lu", v);
		str = buf;
	}

	void reset(void) { release_all(); }

private:
	int _refcnt;
	uint8_t *_buffer;
	size_t _buffer_end;
	union {
		uint32_t _flags;
		struct {
			uint32_t valid : 1;
		} _f;
	};

	listnode_t _blocklist;
	hdmap_file_roadseg_v1*	_roadsegs;
	hdmap_file_lane_v1*	_lanes;
	hdmap_file_laneseg_v1* _lanesegs;
	hdmap_laneseg_point_v1* _laneseg_kdtree;
	hdmap_lane_boundary_point_v1* _lanebdy_kdtree;
	std::map<int, std::map<double, point3d>*> _lanesegs_center;
	std::map<uint64_t, std::shared_ptr<hdmap_lanesect_transition>> _lsc_transitions;
	
	proj _wgs84prj;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(hdmap_impl);
};

}}  // namespace zas::mapcore
#endif  // __CXX_ZAS_MAPCORE_HDMAP_IMPL_H__
/* EOF */
