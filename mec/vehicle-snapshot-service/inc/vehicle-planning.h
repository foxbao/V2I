#ifndef __CXX_SNAPSHOT_SERVICE_VEHICLE_PLAINNING_H__
#define __CXX_SNAPSHOT_SERVICE_VEHICLE_PLAINNING_H__

#include <string.h>
#include "snapshot-service-def.h"
#include "fusion-improver.h"

#include "mapcore/mapcore.h"
#include "mapcore/hdmap.h"

namespace zas {
namespace vehicle_snapshot_service {

#define DEBUG_TEST_ID	(52)
#define IMAGE_DEBUG_FRAME (431)
#define PLANNING_NORMAL_SPEED (20.0)
#define DEBUG_OUTPUT 
#define PLANNING_INITIAL_SPEED (30.0)
#define LANE_CHANGE_MAX_DISTANCE (3.5)
#define VEH_DISTANCE(x, y) (sqrt((x)*(x) + (y)*(y)))
// #define IMAGE_DEBUG

using namespace zas::mapcore;

class motion_planning
{

};

class vehicle_planning : public motion_planning
{
public:
	vehicle_planning();
	~vehicle_planning();

	static vehicle_planning* inst();
	int init_junciton(spfusion_juncion_info info);
	int change_gps_to_utm(const point3d &src, point3d &dst);
	int change_utm_to_gps(const point3d &src, point3d &dst);
	int normal_planning(std::map<uint32_t, spfusion_veh_item> &status, spfusion_juncion_info info, spfusion_veh_item veh);
	int junction_idm_planning(spfusion_juncion_info info, uint32_t dtime);
	int junction_planning(std::map<uint32_t, spfusion_veh_item> &status, spfusion_juncion_info info, std::map<int, int> *lights, spfusion_veh_item veh);
	int junction_planning(std::map<uint32_t, spfusion_veh_item> &veh, spfusion_juncion_info info, std::map<int, int> *lights, uint32_t dtime);
	int missing_planning(std::map<uint32_t, spfusion_veh_item> &veh, spfusion_juncion_info info, uint32_t dtime);

private:
	const hdmap_laneseg* get_laneseg_by_vehicle(spfusion_veh_item &veh, hdmap_segment_point& center, std::vector<point3d> *line);
	int update_status(spfusion_veh_item &veh_status, spfusion_veh_item& veh, const hdmap_laneseg* ls);
	int normal_process(spfusion_veh_item &veh_status, spfusion_veh_item& veh, bool bused = true);
	int status_process(spfusion_veh_item &veh_status, spfusion_juncion_info info, double len, double dtime);
	int change_process(spfusion_veh_item &veh_status, spfusion_veh_item& veh);
	int init_lane_change_info(spfusion_veh_item &veh_status, spfusion_veh_item& veh, const hdmap_laneseg* ls);
	int change_lane_info(spfusion_veh_item &veh_status, spfusion_veh_item& veh, const hdmap_laneseg* ls);
	bool is_in_route(std::vector<const hdmap_laneseg*> &route, const hdmap_laneseg* ls);
	bool is_in_lane(const hdmap_laneseg *ls, double s, point3d &pt);
	bool is_same_road(const hdmap_laneseg *lss, const hdmap_laneseg *lsd);
	int refresh_mid_point(spfusion_veh_item &veh_status, spfusion_veh_item& veh, const hdmap_laneseg* ls);
	bool is_close_end_lane(spfusion_veh_item &veh_status, spfusion_veh_item& veh, const hdmap_laneseg* endls);
	
	spfusion_junc_lane_info get_next_enable_laneseg(spincomming_road info,
	std::map<int, int> *lights, int laneid);
	int junction_change_lane(spfusion_veh_item status, spfusion_junc_lane_info laneinfo, double len, bool self = true);
	int calc_acceleration(fusion_veh_item *prev, fusion_veh_item *curr, double max_speed);
	int junction_process(spfusion_veh_item status, spfusion_juncion_info info, std::map<int, int> *lights, uint32_t dtime, bool real = true);
	double get_vehcle_distance(spfusion_veh_item status, uint32_t dtime);
	int junction_idm_process(fusion_veh_item *root, uint32_t dtime);

	int init_junciton_info(spfusion_juncion_info info);
	int init_junciton_count(spcount_junction info);
	int update_incomming_vehicle(spfusion_veh_item &status,  spfusion_juncion_info info, const hdmap_laneseg* next);
	int update_outgoing_vehicle(spfusion_veh_item &status,  spfusion_juncion_info info, const hdmap_laneseg* next);

private:
	hdmap* _hdmap;
	proj* _proj;
	point3d 	_projoffset;
};



}}	//zas::vehicle_snapshot_service

#endif /* __CXX_SNAPSHOT_SERVICE_DEVICE_SNAPSHOT_H__*/