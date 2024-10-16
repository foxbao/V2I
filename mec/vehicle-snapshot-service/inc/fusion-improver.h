#ifndef __CXX_SNAPSHOT_SERVICE_FUSION_IMPROVER_H__
#define __CXX_SNAPSHOT_SERVICE_FUSION_IMPROVER_H__

#include <string.h>
#include <map>
#include "snapshot-service-def.h"

#include "std/list.h"
#include "mapcore/mapcore.h"
#include "mapcore/hdmap.h"
#include "proto/fusion_service_pkg.pb.h"
#include "proto/center_junction_count.pb.h"

namespace zas {
namespace vehicle_snapshot_service {

using namespace zas::mapcore;
using namespace jos;


struct fusion_lane_info
{
	const hdmap_laneseg	*cur_lane;
	double width;
};

DEFINE_ZAS_EXTEND_TYPE(fusion_lane_info);

struct lane_change_info
{
	const hdmap_laneseg	*dst_lane;
	double dst_w;
	double dst_ds;
	double start_speed;
	double start_pos;
	double v;
	std::vector<const hdmap_laneseg*> croute;
	std::vector<const hdmap_laneseg*> droute;
	int		right;
	double	step1_acc;
	double	step2_acc;
	int		step1_totaltime;
	int		step2_totaltime;
	int 	step1_time;
	int 	step2_time;
	void clear(void);
};
DEFINE_ZAS_EXTEND_TYPE(lane_change_info);

struct count_incomming_info
{
	string approadid;
	int type;
	int laneid;
};
DEFINE_ZAS_EXTEND_TYPE(count_incomming_info);

struct fusion_veh_item
{
	fusion_veh_item();
	int set_target(target &src);
	int get_target(target &dst);
	target veh_item;
	hdmap_segment_point	pt;
	point3d orgin_pt;
	double hdg;		//	target 航向角
	double speed;	//	target 速度
	double a;
	uint64_t timestamp;
	//	target 状态：0为正常车道形式，1为初始化车道切换，2 车辆自己规划初始，3 车辆规划中
	int status;
	bool bused;
	bool bad_item;
	bool count_incomm;
	bool count_outgoing;
	spcount_incomming_info incomm_info;
	listnode_t ownerlist;
	fusion_lane_info laneinfo;
	splane_change_info cinfo;
	std::vector<point3d> line;
};
DEFINE_ZAS_EXTEND_TYPE(fusion_veh_item);

struct fusion_junc_next_lane {
	int light_id;
	uint32_t next_road_id;
	int next_lane_id;
	const hdmap_road *next_road;
	const hdmap_laneseg *next_laneseg;
};
DEFINE_ZAS_EXTEND_TYPE(fusion_junc_next_lane);

struct fusion_junc_lane_info {
	int lane_id;
	fusion_veh_item root;
	const hdmap_lane* curr_lane;
	const hdmap_laneseg* curr_last_ls;
	std::map<int, spfusion_junc_next_lane> next_lanes;
};
DEFINE_ZAS_EXTEND_TYPE(fusion_junc_lane_info);

struct incomming_road
{
	uint32_t road_id;
	std::map<int, spfusion_junc_lane_info> lane_info;
	const hdmap_road *road;
	double s;
};
DEFINE_ZAS_EXTEND_TYPE(incomming_road);

struct count_lane {
	std::string name;
	std::string id;
	int32_t type;
	int32_t in_rdid;
	int32_t in_laneid;
	int32_t rdid;
	int32_t laneid;
	const hdmap_lane* hdlane;
};
DEFINE_ZAS_EXTEND_TYPE(count_lane);

struct count_movement
{
	int32_t type;
	std::map<uint32_t, spcount_lane> lanes;
};
DEFINE_ZAS_EXTEND_TYPE(count_movement);

struct count_incomming_road {
	std::string name;
	std::string id;
	uint32_t orien;
	std::map<uint32_t, spcount_movement> movemts;
};
DEFINE_ZAS_EXTEND_TYPE(count_incomming_road);

struct count_outgoing_road {
	std::string name;
	std::string id;
	uint32_t orien;
	int32_t rdid;
	const hdmap_road *road;
};
DEFINE_ZAS_EXTEND_TYPE(count_outgoing_road);

struct count_junction {
	std::string name;
	uint32_t area;
	uint32_t id;
	std::map<uint32_t, spcount_incomming_road> incom_info;
	std::map<uint32_t, spcount_outgoing_road> outgoing_info;
	std::map<const hdmap_lane*, spcount_lane> incom_lanes;
	std::map<const hdmap_road*, spcount_outgoing_road> outgoing_roads;
	std::map<int, std::map<int, uint64_t>> incomm_time;
};
DEFINE_ZAS_EXTEND_TYPE(count_junction);

struct fusion_juncion_info {
	int junction_id;
	std::map<uint32_t, spincomming_road> road_info;
	spcount_junction count_info;
	cnt_junction junc_count;
};
DEFINE_ZAS_EXTEND_TYPE(fusion_juncion_info);

class device_junction_item;
class fusion_improver
{
public:
	fusion_improver();
	virtual ~fusion_improver();
	int init_junction_info(spfusion_juncion_info info);
	int improve_fusion_target_to_junction(device_junction_item* item, fusion_service_pkg &pkg);

private:
	int reset_status(void);
	uint32_t get_process_time(uint64_t pkgtime);
	int check_set_status_result(void);
	bool is_missed(std::map<uint32_t, spfusion_veh_item> &jun_items, spfusion_veh_item curr);
	bool is_in_junction(spfusion_veh_item item);
	int normal_process(spfusion_veh_item item);
	int junction_process(spfusion_veh_item item);
	int junction_idm_process(uint32_t dtime);
	int junction_process(uint32_t dtime);
	int missing_process(uint32_t dtime);
	int set_targets(fusion_package* fpkg);

private:
	std::map<uint32_t, spfusion_veh_item> _fn_planning;
	std::map<uint32_t, spfusion_veh_item> _fj_planning;
	std::map<uint32_t, spfusion_veh_item> _fm_planning;
	spfusion_juncion_info	_fusion_jinfo;
	std::map<int,int>		_light_info;
	int _frame_count;
};

DEFINE_ZAS_EXTEND_TYPE(fusion_improver);

}}	//zas::vehicle_snapshot_service

#endif /* __CXX_SNAPSHOT_SERVICE_DEVICE_SNAPSHOT_H__*/