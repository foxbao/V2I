#include "vehicle-planning.h"
#include <sys/time.h>
#include <Eigen/Dense>

#include "device-snapshot.h"
#include "webcore/sysconfig.h"
#include "webcore/logger.h"

namespace zas {
namespace vehicle_snapshot_service {

#define LANE_CHANGE_MAX_TIME (4000)
#define LANE_MAX_STOP_SPEED (10)
#define LANE_MAX_INIT_SPEED (5)
#define LANE_MAX_NUMBER_IN_ROAD (4)
#define VEH_PLANNING_IN_JUNCTION_MAX_TIME (5000)
#define CROSS_MULTI(x1, y1, x2, y2) ((x1)*(y2) - (x2)*(y1))
#define LINE_SLOPE(x1, y1, x2, y2) (((y2)-(y1))/((x2)-(x1)))
#define SQUARE_POINT(x) ((x)*(x))
#define NORMAL_SPEED(x) ((x) / 3.6)
#define TO_KM_SPEED(x) ((x) * 3.6)
#define MSTOS(x) ((x) / 1000.0)
#define COG2DEG(x) ((90 - (x)) * M_PI / 180)

#define DESIRED_MAX_VELOCITY (12.0)
#define DESIRED_MIN_VELOCITY (5.0)
#define SATE_TIME_HEADWAY (0.25)
#define MAX_ACC (3.0)
#define COMFOR_DEC (1.67)
#define ACC_EXPONENT(x) ((x)*(x)*(x)*(x))
#define MIN_DISTANCE (2)
#define VEHICLE_LENGTH (5)
#define VEHICLE_MAX_SPEED (1.0)


using namespace zas::utils;
using namespace zas::webcore;
using namespace zas::mapcore;
using namespace jos;

double NormalizeAngle(const double angle) {
  double a = std::fmod(angle + M_PI, 2.0 * M_PI);
  if (a < 0.0) {
    a += (2.0 * M_PI);
  }
  return a - M_PI;
}

double AngleDiff(const double from, const double to) {
  return NormalizeAngle(to - from);
}

vehicle_planning* vehicle_planning::inst()
{
	static vehicle_planning* _inst = NULL;
	if (_inst) return _inst;
	_inst = new vehicle_planning();
	assert(NULL != _inst);
	return _inst;
}

vehicle_planning::vehicle_planning()
: _hdmap(nullptr)
, _proj(nullptr)
{
	std::string hdmap_file = "/zassys/sysapp/others/hdmap/hdmap";
	hdmap_file = get_sysconfig("snapshot-service.hdmap",
		hdmap_file.c_str());
	_hdmap = new hdmap();
	int ret = _hdmap->load_fromfile(hdmap_file.c_str());
	if (!ret) {
		_proj = new proj(GREF_WGS84, _hdmap->get_georef());
		double x, y;
		_hdmap->get_offset(x, y);
		_projoffset.set(x,y);
	} else {
		printf("load hdmap file error:[%d], file: %s\n",
			ret, hdmap_file.c_str());
	}
}

vehicle_planning::~vehicle_planning()
{
	if (_proj) {
		delete _proj;
	}
	if (_hdmap) {
		delete _hdmap;
	}
}

int vehicle_planning::change_gps_to_utm(const point3d &src, point3d &dst)
{
	dst = _proj->transform(src);
	if (isnan(dst.xyz.x)) {
		printf("trafs error\n");
	}
	dst -= _projoffset;
	return 0;
}

int vehicle_planning::change_utm_to_gps(const point3d &src, point3d &dst)
{
	auto spt = src;
	spt += _projoffset;
	dst = _proj->transform(spt, true);
	return 0;
}

int vehicle_planning::init_junciton(spfusion_juncion_info info)
{
	int ret = 0;
	ret = init_junciton_info(info);
	if (ret < 0) {
		return ret;
	}
	return init_junciton_count(info->count_info);
}

int vehicle_planning::init_junciton_info(spfusion_juncion_info info)
{
	for (auto const &rd : info->road_info) {
		auto hdrd = _hdmap->get_road_by_id(rd.first);
		if (nullptr == hdrd) {
			log.e(SNAPSHOT_SNAPSHOT_TAG, "jucntion not find road[%d]\n", rd.first);
			return -ENOTFOUND;
		}
		
		info->road_info[rd.first]->road = hdrd;
		auto& inrd = info->road_info[rd.first];
		inrd->s = hdrd->get_length();
		for (auto const &lane : inrd->lane_info) {
			auto hdlane = hdrd->get_lane_by_id(lane.first);
			if (nullptr == hdlane) {
				log.e(SNAPSHOT_SNAPSHOT_TAG, "jucntion not find road[%d], lane [%d]\n", rd.first, lane.first);
				return -ENOTFOUND;
			}
			auto& curlane = inrd->lane_info[lane.first];
			curlane->curr_lane = hdlane;
			curlane->curr_last_ls = hdlane->get_lane_last_segment();
			if (nullptr == curlane->curr_last_ls) {
				log.e(SNAPSHOT_SNAPSHOT_TAG, "jucntion not find road[%d], lane [%d], last segment\n", rd.first, lane.first);
				return -ENOTFOUND;
			}
			for (auto const &nlane : curlane->next_lanes) {
				nlane.second->next_road = _hdmap->get_road_by_id(nlane.second->next_road_id);
				if (nullptr == nlane.second->next_road) {
					log.e(SNAPSHOT_SNAPSHOT_TAG, "jucntion not find next road[%d]\n", nlane.second->next_road_id);
					return -ENOTFOUND;
				}

				auto next_lane = nlane.second->next_road->get_lane_by_id(nlane.second->next_lane_id);
				if (nullptr == next_lane) {
					log.e(SNAPSHOT_SNAPSHOT_TAG, "jucntion not find next road[%d] lane [%d]\n", nlane.second->next_road_id, nlane.second->next_lane_id);
					return -ENOTFOUND;
				}
				nlane.second->next_laneseg = next_lane->get_lane_first_segment();
				if (nullptr == nlane.second->next_laneseg) {
					log.e(SNAPSHOT_SNAPSHOT_TAG, "jucntion not find next road[%d], lane [%d], last segment\n", nlane.second->next_road_id, nlane.second->next_lane_id);
					return -ENOTFOUND;
				}
			}
		}
	}
	return 0;
}

int vehicle_planning::init_junciton_count(spcount_junction info)
{
	if (nullptr == info) {
		return -EBADPARM;
	}
	for (auto const &rd : info->incom_info) {
		for (auto const &mmt : rd.second->movemts) {
			for (auto const &lane : mmt.second->lanes) {
				auto hdrd = _hdmap->get_road_by_id(lane.second->rdid);
				if (nullptr == hdrd) {
					log.e(SNAPSHOT_SNAPSHOT_TAG, "jucntion not find road[%d]\n", lane.second->rdid);
					return -ENOTFOUND;
				}
				auto hdlane = hdrd->get_lane_by_id(lane.second->laneid);
				if (nullptr == hdlane) {
					log.e(SNAPSHOT_SNAPSHOT_TAG, "jucntion not find road[%d], lane [%d]\n", lane.second->rdid, lane.second->laneid);
					return -ENOTFOUND;
				}
				lane.second->hdlane = hdlane;
				info->incom_lanes[lane.second->hdlane] = lane.second;
			}
		}
	}
	for (auto const &rd : info->outgoing_info) {
		auto hdrd = _hdmap->get_road_by_id(rd.second->rdid);
		if (nullptr == hdrd) {
			log.e(SNAPSHOT_SNAPSHOT_TAG, "jucntion not find road[%d]\n", rd.second->rdid);
			return -ENOTFOUND;
		}
		rd.second->road = hdrd;
		info->outgoing_roads[hdrd] = rd.second;
	}
	return 0;
}

int vehicle_planning::normal_planning(
	std::map<uint32_t, spfusion_veh_item> &status,
	spfusion_juncion_info info,
	spfusion_veh_item veh)
{
	uint32_t index = veh->veh_item.id();
	hdmap_segment_point center;
	std::vector<point3d> line;
	if (index == DEBUG_TEST_ID) {
 		DEBUG_OUTPUT("id is %u\n", index);
	} 
	// 需要忽略路口中间的不合规感知目标
	if (status.count(index) > 0 && status[index]->bad_item){
		status[index]->timestamp = veh->timestamp;
		status[index]->veh_item = veh->veh_item;
		status[index]->bused = true;
		return 0;
	}

#ifdef IMAGE_DEBUG
	auto* laneseg = get_laneseg_by_vehicle(veh, center, &line);
#else 
	auto* laneseg = get_laneseg_by_vehicle(veh, center, nullptr);
#endif

	// 如果没有找到langseg，用之前的laneseg进行推算
	if (nullptr == laneseg) {
		auto it = status.find(index);
		if (it == status.end()) {
			return -ENOTFOUND;
		}
#ifdef IMAGE_DEBUG
		if (status[index]->laneinfo.cur_lane) {
			line.clear();
			status[index]->laneinfo.cur_lane->get_center_line(line);
		}
#endif
		status[index]->line = line;
		status[index]->bused = true;
		update_status(status[index], veh, nullptr);
		return 0;
	}

	// 如果是新增感知目标，需要判断是否在路口中
	if (status.count(index) <= 0) {
		double dg = AngleDiff(NormalizeAngle(COG2DEG(veh->veh_item.hdg())), NormalizeAngle(COG2DEG(veh->hdg)));
		if (dg > M_PI_4 || dg < -M_PI_4) {
			DEBUG_OUTPUT("target %d, diff angle %lf\n", index, dg);
			return 0;
		}
		auto* rd = laneseg->get_lane()->get_road();
		veh->status = 0;
		veh->laneinfo.cur_lane = laneseg;
		veh->pt = center;
		status[index] = veh;
		status[index]->line = line;
		status[index]->bused = true;
		int jun_id = rd->get_junctionid();
		if (nullptr != info && info->junction_id == jun_id) {
			double len = rd->get_length();
			status[index]->bad_item = true;
			DEBUG_OUTPUT("bad targe id %d, juntion id %d\n", index, rd->get_junctionid());
		} else {
			if (info) {
				DEBUG_OUTPUT("junction id is %d, jun id %d,\n", jun_id, info->junction_id);
			} else {
				DEBUG_OUTPUT("junction id is %d, target id %d,\n", jun_id, index);
			}
		}
		return 0;
	}
	// status位置出错，或者，当前laneseg与status相同且status为0， 直接使用检索的位置
	else if (isnan(status[index]->pt.s) || ((status[index]->laneinfo.cur_lane == laneseg) && (status[index]->status == 0))) {
		veh->status = 0;
		veh->laneinfo.cur_lane = laneseg;
		veh->pt = center;
		if (center.s > status[index]->pt.s) {
			status[index] = veh;
		} else {
			status[index]->veh_item = veh->veh_item;
			status[index]->speed = veh->speed;
			status[index]->timestamp = veh->timestamp;
		}
		status[index]->line = line;
		status[index]->bused = true;;
		return 0;
	}
	veh->laneinfo.cur_lane = laneseg;
	veh->pt = center;
	veh->line = line;
	if (status[index]->laneinfo.cur_lane) {
		line.clear();
		status[index]->laneinfo.cur_lane->get_center_line(line);
	}
	status[index]->line = line;
	status[index]->bused = true;
	return update_status(status[index], veh, laneseg);
}

int vehicle_planning::junction_idm_planning(spfusion_juncion_info info, uint32_t dtime)
{
	if (nullptr == info) {
		return 0;
	}
	for (auto const &incom_rd : info->road_info) {
		for (auto const &lane : incom_rd.second->lane_info) {
			junction_idm_process(&(lane.second->root), dtime);
		}
	}
	return 0;
}

int vehicle_planning::junction_planning(
	std::map<uint32_t, spfusion_veh_item> &vehs,
	spfusion_juncion_info info,
	std::map<int, int> *lights,
	spfusion_veh_item veh)
{
	uint32_t index = veh->veh_item.id();

	auto& status = vehs[index];
	uint32_t dtime = veh->timestamp - status->timestamp;
	if (dtime > 1000) {
		printf("time [%u] is long between frames\n", dtime);
		dtime = 100;
	}
	if (index == DEBUG_TEST_ID) {
		DEBUG_OUTPUT("test car [%d], speed %f\n", index, veh->speed);
	}
	status->bused = true;
	status->veh_item = veh->veh_item;
	status->timestamp = veh->timestamp;
	if (status->status == 0) {
		status->speed = veh->speed;
	}
	return junction_process(status, info, lights, dtime);
}

int vehicle_planning::junction_process(spfusion_veh_item status, spfusion_juncion_info info, std::map<int, int> *lights, uint32_t dtime, bool real)
{
	// 未进入路口
	if (status->status == 0) {
		double len = NORMAL_SPEED(status->speed) * MSTOS(dtime);
		spfusion_junc_lane_info inlane_info;
		spincomming_road rd_incom;
		if (info) {
			for (auto const &incom_rd : info->road_info) {
				for (auto const &lane : incom_rd.second->lane_info) {
					if (status->laneinfo.cur_lane == lane.second->curr_last_ls) {
						rd_incom = incom_rd.second;
						inlane_info = lane.second;
						break;
					}
				}
			}
		}
		if (inlane_info && rd_incom) {
			bool enable_run = false;
			int light_id = inlane_info->next_lanes[0]->light_id;
			if (light_id < 0 || 
				(lights->count(light_id) > 0 && (*lights)[light_id] < 3)) {
				enable_run = true;
				if (status->speed < LANE_MAX_INIT_SPEED && !real) {
					status->speed = LANE_MAX_STOP_SPEED - 1;
					len = NORMAL_SPEED(status->speed) * MSTOS(dtime);
				}
			}
			double ts = len + status->pt.s;
			if (ts < rd_incom->s) {
				return status_process(status, info, len, dtime);
			}
			if (enable_run) {
				junction_change_lane(status, inlane_info, (ts - rd_incom->s));
				len = get_vehcle_distance(status, dtime);
				return status_process(status, info, (ts - rd_incom->s), dtime);
			}
			if (status->speed < LANE_MAX_STOP_SPEED) {
				status->speed = 0;
				if ((status->pt.s + 1.0) < rd_incom->s) {
					return status_process(status, info, (rd_incom->s - status->pt.s -0.2), dtime);
				}
				return 0;
			} else {
				inlane_info = get_next_enable_laneseg(rd_incom, lights, inlane_info->lane_id);
				if (inlane_info) {
					junction_change_lane(status, inlane_info, (ts - rd_incom->s), false);
					len = get_vehcle_distance(status, dtime);
					return status_process(status, info, (ts - rd_incom->s), dtime);
				} else {
					if ((status->pt.s + 1.0) < rd_incom->s) {
						return status_process(status, info, (rd_incom->s - status->pt.s -0.2), dtime);
					} else {
						status->speed = 0;
					}
					return 0;
				}
			} 
		}else {
			// error
			status->bused = false;
		}
	} else {
		double len = get_vehcle_distance(status, dtime);
		status_process(status, info, len, dtime);
		if (status->laneinfo.cur_lane == nullptr) {
			status->bused = false;
		} else {
			uint32_t junid = status->laneinfo.cur_lane->get_lane()->get_road()->get_junctionid();
			if (junid != info->junction_id) {
				if (status->veh_item.id() == DEBUG_TEST_ID) {
					DEBUG_OUTPUT("test\n");
				}
				status->bused = false;
			}
		}
	}
	return 0;
}

double vehicle_planning::get_vehcle_distance(spfusion_veh_item status, uint32_t dtime)
{
	double len = NORMAL_SPEED(status->speed) * MSTOS(dtime) + status->a * SQUARE_POINT(MSTOS(dtime)) / 2;
	status->speed += TO_KM_SPEED(status->a*MSTOS(dtime));
	if (status->speed < 0.1) {
		status->speed = LANE_MAX_INIT_SPEED;
		len = NORMAL_SPEED(status->speed) * MSTOS(dtime);
	}
	return len;
}

int vehicle_planning::calc_acceleration(fusion_veh_item *prev, fusion_veh_item *curr, double max_speed)
{
	double curr_speed = NORMAL_SPEED(curr->speed);
	double accexp = ACC_EXPONENT(curr_speed / max_speed);
	double va_free = MAX_ACC * ( 1 - accexp);
	double va_init = 0.0;
	if (prev != nullptr) {
		double delta_speed = curr_speed - NORMAL_SPEED(prev->speed);
		double fveh_dis = VEH_DISTANCE(prev->pt.pt.xyz.x - curr->pt.pt.xyz.x, prev->pt.pt.xyz.y - curr->pt.pt.xyz.y) - VEHICLE_LENGTH;
		if (fveh_dis < 0) {
			fveh_dis = 0.1;
		}
		double va_dv = (curr_speed * delta_speed) / (sqrt(MAX_ACC*COMFOR_DEC)*2);
		double s_va_dv = MIN_DISTANCE + curr_speed*SATE_TIME_HEADWAY + va_dv;
		va_init = -1 * MAX_ACC * SQUARE_POINT(s_va_dv / fveh_dis);
	}
	curr->a = va_free + va_init;
	return 0;
}

int vehicle_planning::junction_idm_process(fusion_veh_item *root, uint32_t dtime)
{
	if (nullptr == root) {
		return -EBADPARM;
	}
	if (listnode_isempty(root->ownerlist)) {
		return 0;
	}
	auto* rootnd = &(root->ownerlist);
	auto* itemnode = rootnd->next;
	fusion_veh_item *previtem = nullptr;
	for (; itemnode != rootnd; itemnode = itemnode->next) {
		auto* item = LIST_ENTRY(fusion_veh_item, ownerlist, itemnode);
		double rspeed = NORMAL_SPEED(item->veh_item.speed());
		if (rspeed > DESIRED_MAX_VELOCITY) {
			rspeed = DESIRED_MAX_VELOCITY;
		} else if (rspeed < DESIRED_MIN_VELOCITY) {
			rspeed = DESIRED_MIN_VELOCITY;
		}
		rspeed = rspeed* VEHICLE_MAX_SPEED;
		calc_acceleration(previtem, item, rspeed);
		previtem = item;
	}
	return 0;
}

int vehicle_planning::junction_change_lane(spfusion_veh_item status, spfusion_junc_lane_info laneinfo, double len, bool self)
{
	status->status = 1;
	status->pt.s = 0;
	auto ls = laneinfo->next_lanes[0]->next_laneseg;

	listnode_add(laneinfo->root.ownerlist, status->ownerlist);
	if (self) {
		status->laneinfo.cur_lane = ls;
		return 0;
	}
	std::vector<const hdmap_laneseg*> route;
	double nhdg = status->hdg;
	auto destpt= status->pt;
	auto width = 0.0;
	auto * nlaneseg = ls->get_adc_and_point(len, status->pt, destpt, nhdg, width, route);

	double cross_m = CROSS_MULTI((destpt.pt.xyz.x - status->pt.pt.xyz.x), (destpt.pt.xyz.y - status->pt.pt.xyz.y), cos(COG2DEG(status->hdg)), sin(COG2DEG(status->hdg)));
	int right;
	if (cross_m > 10e-15) {
		right = 1;
	} else {
		right = -1;
	}

	status->cinfo = std::make_shared<lane_change_info>();
	auto vl_cinfo = status->cinfo;
	vl_cinfo->start_pos = (status->laneinfo.width + width) / 2 * right * -1;
	vl_cinfo->v = (vl_cinfo->start_pos * -1) / MSTOS(LANE_CHANGE_MAX_TIME / 2);
	vl_cinfo->dst_ds = 0.0;
	vl_cinfo->dst_w = 0.0;
	vl_cinfo->step1_totaltime = LANE_CHANGE_MAX_TIME / 2;
	vl_cinfo->step1_time = 0;
	vl_cinfo->step1_acc = 0;
	vl_cinfo->step2_totaltime = 0;
	vl_cinfo->step2_time = 0;
	vl_cinfo->step2_acc = 0;
	vl_cinfo->dst_lane = nullptr;
	status->laneinfo.cur_lane = ls;
	status->laneinfo.width = width;
	status->pt.s = 0;
	return 0;
}

spfusion_junc_lane_info
vehicle_planning::get_next_enable_laneseg(spincomming_road info,
	std::map<int, int> *lights, int laneid)
{
	for (int i = 0; i < LANE_MAX_NUMBER_IN_ROAD; i++) {
		int next_lane_id = laneid - i - 1;
		if (info->lane_info.count(next_lane_id)) {
			int light_id = info->lane_info[next_lane_id]->next_lanes[0]->light_id;
			if (light_id < 0 || 
				(lights->count(light_id) > 0 && (*lights)[light_id] < 3)) {
				return info->lane_info[next_lane_id];
			}
			break;
		}
	}
	for (int i = 0; i < LANE_MAX_NUMBER_IN_ROAD; i++) {
		int next_lane_id = laneid + i + 1;
		if (info->lane_info.count(next_lane_id)) {
			int light_id = info->lane_info[next_lane_id]->next_lanes[0]->light_id;
			if (light_id < 0 || 
				(lights->count(light_id) > 0 && (*lights)[light_id] < 3)) {
				return info->lane_info[next_lane_id];
			}
			break;
		}
	}

	// for (const auto& lane : info->lane_info) {
	// 	int light_id = lane.second->light_id;
	// 	if (light_id < 0 || 
	// 		(lights->count(light_id) > 0 && (*lights)[light_id] < 3)) {
	// 		return lane.second;
	// 	}
	// }
	return nullptr;
}

int vehicle_planning::status_process(spfusion_veh_item &status,  spfusion_juncion_info info, double len, double dtime)
{
	std::vector<const hdmap_laneseg*> route;
	double nhdg = status->hdg;
	auto destpt= status->pt;

	auto * nlaneseg = status->laneinfo.cur_lane->get_adc_and_point(len, status->pt, destpt, nhdg, status->laneinfo.width, route);
	if (status->veh_item.id() == DEBUG_TEST_ID) {
		DEBUG_OUTPUT("test, curr %p, next %p, curhdg %f, nhdg %lf\n", status->laneinfo.cur_lane, nlaneseg, status->hdg, nhdg);
	}
	if (!status->count_incomm && info && info->count_info) {
		update_incomming_vehicle(status, info, nlaneseg);
	}
	if (!status->count_outgoing && info && info->count_info) {
		update_outgoing_vehicle(status, info, nlaneseg);
	}
	status->laneinfo.cur_lane = nlaneseg;
	status->pt = destpt;
	status->hdg = nhdg;
	status->bused = true;
	if (status->cinfo) {
		auto vl_info = status->cinfo;
		vl_info->step1_time += dtime;
		double w = vl_info->v * MSTOS(vl_info->step1_time);
		w = vl_info->start_pos + w;
		double dif_hdg = 90.0;
		if (w < 0) {
			w = w * -1;
			dif_hdg = dif_hdg * -1;
		}

		double x = w * cos(COG2DEG(status->hdg + dif_hdg));
		double y = w * sin(COG2DEG(status->hdg + dif_hdg));
		point3d dpt(x, y);
		status->pt.pt += dpt;
		if (vl_info->step1_time >= vl_info->step1_totaltime) {
			status->cinfo = nullptr;
		}	
	}
	return 0;
}

int vehicle_planning::update_incomming_vehicle(spfusion_veh_item &status,  spfusion_juncion_info info, const hdmap_laneseg* nlaneseg)
{
	auto cnt_lanes = info->count_info->incom_lanes;
	auto curr_lane = status->laneinfo.cur_lane->get_lane();
	auto next_lane = nlaneseg->get_lane();
	spcount_lane in_cnt_lane = nullptr;
	if (cnt_lanes.count(curr_lane) > 0) {
		in_cnt_lane = cnt_lanes[curr_lane];
		status->count_incomm = true;
	} else if (cnt_lanes.count(next_lane) > 0) {
		in_cnt_lane = cnt_lanes[next_lane];
		status->count_incomm = true;
	}
	if (status->count_incomm) {
		auto* incomm_info = info->junc_count.add_invs();
		incomm_info->set_approachid(in_cnt_lane->id);
		incomm_info->set_name(in_cnt_lane->name);
		incomm_info->set_type((movement_type)in_cnt_lane->type);
		incomm_info->set_laneid(in_cnt_lane->in_laneid);
		auto* vehinfo = incomm_info->mutable_veh();
		vehinfo->set_id(status->veh_item.id());
		vehinfo->set_speed(status->speed);
		status->incomm_info = std::make_shared<count_incomming_info>();
		status->incomm_info->approadid = in_cnt_lane->id;
		status->incomm_info->type = in_cnt_lane->type;
		status->incomm_info->laneid = in_cnt_lane->in_laneid;
		auto& incomm_tm = info->count_info->incomm_time;
		if (incomm_tm.count(in_cnt_lane->in_rdid) > 0 
			&& incomm_tm[in_cnt_lane->in_rdid].count(in_cnt_lane->in_laneid) > 0) {
			timeval tv;
			gettimeofday(&tv, nullptr);
			uint64_t times = tv.tv_sec * 1000 + tv.tv_usec / 1000;
			if (0 == incomm_tm[in_cnt_lane->in_rdid][in_cnt_lane->in_laneid]) {
				incomm_info->set_time_interval(0);
			} else {
				incomm_info->set_time_interval(times - incomm_tm[in_cnt_lane->in_rdid][in_cnt_lane->in_laneid]);
			}
			incomm_tm[in_cnt_lane->in_rdid][in_cnt_lane->in_laneid] = times;
		}
	}
	return 0;
}

int vehicle_planning::update_outgoing_vehicle(spfusion_veh_item &status,  spfusion_juncion_info info, const hdmap_laneseg* nlaneseg)
{
	auto cnt_roads = info->count_info->outgoing_roads;
	auto curr_roads = status->laneinfo.cur_lane->get_lane()->get_road();
	auto next_roads = nlaneseg->get_lane()->get_road();
	hdmap_lane* outgo_cnt_lane = nullptr;
	spcount_outgoing_road outgroad = nullptr;
	if (cnt_roads.count(curr_roads) > 0) {
		outgo_cnt_lane = status->laneinfo.cur_lane->get_lane();
		outgroad = cnt_roads[curr_roads];
		status->count_outgoing = true;
	} else if (cnt_roads.count(next_roads) > 0) {
		outgo_cnt_lane = nlaneseg->get_lane();
		outgroad = cnt_roads[next_roads];
		status->count_outgoing = true;
	}
	if (status->count_outgoing) {
		auto* outg_info = info->junc_count.add_outvs();
		outg_info->set_outgoingid(outgroad->id);
		outg_info->set_name(outgroad->name);
		if (status->count_incomm && nullptr != status->incomm_info) {
			outg_info->set_incommingid(status->incomm_info->approadid);
			outg_info->set_incomming_laneid(status->incomm_info->laneid);
			outg_info->set_type((movement_type)status->incomm_info->type);
		}
		outg_info->set_outgoing_laneid(outgo_cnt_lane->get_id());
		auto vehinfo = outg_info->mutable_veh();
		vehinfo->set_id(status->veh_item.id());
		vehinfo->set_speed(status->speed);
	}
	return 0;
}

int vehicle_planning::junction_planning(
	std::map<uint32_t, spfusion_veh_item> &vehs,
	spfusion_juncion_info info,
	std::map<int, int> *lights,
	uint32_t dtime)
{
	for (auto const& veh : vehs) {
		auto status = veh.second;
		if (status->bused) {
			continue;
		}
		if (status->veh_item.id() == DEBUG_TEST_ID) {
			DEBUG_OUTPUT("test\n");
		}
		status->bused = true;
		status->timestamp += dtime;
		junction_process(status, info, lights, dtime, false);
	}
	return 0;
}

int vehicle_planning::missing_planning(
	std::map<uint32_t, spfusion_veh_item> &vehs,
	spfusion_juncion_info info,
	uint32_t dtime)
{
	for (auto it = vehs.begin(); it != vehs.end();) {
		auto veh = it->second;
		auto id = veh->veh_item.id();
		if (id == DEBUG_TEST_ID) {
			DEBUG_OUTPUT("id is %u\n", id);
		}

		if (veh->bad_item) {
			it = vehs.erase(it);
			continue;
		}
		if (veh->speed < 5) {
			veh->speed = 20;
		}
		double len = NORMAL_SPEED(veh->speed) * MSTOS(dtime);
		std::vector<const hdmap_laneseg*> route;
		uint64_t beforetime = veh->timestamp;
		veh->timestamp += dtime;
		double nhdg = veh->hdg;
		auto destpt= veh->pt;

		auto * nlaneseg = veh->laneinfo.cur_lane->get_adc_and_point(len, veh->pt, destpt, nhdg, veh->laneinfo.width, route);
		veh->hdg = nhdg;
		if (isnan(veh->pt.pt.xyz.x) || isnan(veh->pt.pt.xyz.y)) {
			printf("++++++++remove planning date %d by error\n", id);
			it = vehs.erase(it);
			continue;
		}
		if (nullptr == nlaneseg) {
			printf("+++++++++remove planning date %d by no route\n", id);
			it = vehs.erase(it);;
			continue;
		}
		if (veh->pt.pt.xyz.x == destpt.pt.xyz.x && veh->pt.pt.xyz.y == destpt.pt.xyz.y) {
			it = vehs.erase(it);
			continue;
		}
		veh->pt = destpt;
		veh->laneinfo.cur_lane = nlaneseg;
		// if (veh->timestamp > VEH_PLANNING_IN_JUNCTION_MAX_TIME) {
		// 	auto* rd = nlaneseg->get_lane()->get_road();	
		// 	if (rd && rd->get_junctionid() > 0) {
		// 		veh->timestamp = VEH_PLANNING_IN_JUNCTION_MAX_TIME;
		// 	}
		// }
#ifdef IMAGE_DEBUG
		nlaneseg->get_center_line(veh->line);
#endif

		it++;
	}
	return 0;
}

const hdmap_laneseg* vehicle_planning::get_laneseg_by_vehicle(spfusion_veh_item &veh, hdmap_segment_point& center, std::vector<point3d> *line)
{
	auto* laneseg = _hdmap->find_laneseg(veh->pt.pt, veh->hdg, center, line);
	return laneseg;
}

int vehicle_planning::update_status(spfusion_veh_item &veh_status, spfusion_veh_item& veh, const hdmap_laneseg* ls)
{
	if (nullptr == ls) {
		if (veh_status->status == 0) {
			return normal_process(veh_status, veh);
		} else if (veh_status->status == 1) {
			return change_process(veh_status, veh);
		} else {
			printf("vehicle planning update status error\n");
			return -ENOTALLOWED;
		}
	}

	if (veh_status->status == 0) {
		int ret = init_lane_change_info(veh_status, veh, ls);
		if (ret > 0) {
			return 0;
		}
		if (!ret) {
			ret = change_process(veh_status, veh);
		}
		return ret;
	} else if (veh_status->status == 1) {
		auto vl_cinfo = veh_status->cinfo;
		//存在第二段切换车道的情况
		if (vl_cinfo->step2_time != vl_cinfo->step2_totaltime) {
			// check 是不是目标车道
			// if ((!is_in_route(vl_cinfo->croute, ls) && is_in_route(vl_cinfo->droute, ls)) && (ls = vl_cinfo->dst_lane)) {
			if (is_in_route(vl_cinfo->droute, ls)) {
				// 是目标车道中的位置，推算
				return change_process(veh_status, veh);
			}
		} else {
			if (is_in_route(vl_cinfo->croute, ls)) {
				// 是目标车道中的位置，推算
				return change_process(veh_status, veh);
			}
		}
		// 切换车道
		int ret = change_lane_info(veh_status, veh, ls);
		if (!ret) {
			return change_process(veh_status, veh);
		} 
		else if (ret > 0){
			return 0;
		} else {
			printf("vehicle planning change lane error\n");
			return change_process(veh_status, veh);
		}
	}

	return 0;
}

bool vehicle_planning::is_in_route(std::vector<const hdmap_laneseg*> &route, const hdmap_laneseg* ls)
{
	for (auto& rlane : route) {
		if (rlane == ls) {
			return true;
		}
	}
	return false;
}

bool vehicle_planning::is_in_lane(const hdmap_laneseg *ls, double s, point3d &pt)
{
	return _hdmap->is_in_lane(ls, s, pt);
}

bool vehicle_planning::is_same_road(const hdmap_laneseg *lss, const hdmap_laneseg *lsd)
{
	return _hdmap->is_same_road(lss, lsd);
}

int vehicle_planning::change_lane_info(spfusion_veh_item &veh_status, spfusion_veh_item& veh, const hdmap_laneseg* ls)
{
	// 计算点在目标路径中的lane，按照现有路径计算，不予以切换考虑
	if ((veh_status->cinfo->dst_lane == nullptr && is_in_lane(veh_status->laneinfo.cur_lane, veh_status->pt.s, veh->pt.pt)) || (veh_status->cinfo->dst_lane && is_in_lane(veh_status->cinfo->dst_lane, veh->pt.s, veh->pt.pt))){
		change_process(veh_status, veh);
		return 1;
	}
	//计算点与当前点位置距离相差3.5m以上，不予以切换考虑
	if (VEH_DISTANCE(veh_status->pt.pt.xyz.x - veh->pt.pt.xyz.x, veh_status->pt.pt.xyz.y - veh->pt.pt.xyz.y) > LANE_CHANGE_MAX_DISTANCE) {
		change_process(veh_status, veh);
		return 1;
	}

	//计算点与当前点角度相差正负45度， 不予以切换考虑
	double dg = AngleDiff(NormalizeAngle(COG2DEG(veh_status->hdg)), NormalizeAngle(COG2DEG(veh->hdg)));
	if (dg > M_PI_4 || dg < -M_PI_4) {
		change_process(veh_status, veh);
		return 1;
	}
	
	//是否是同一条路， 如果不是按照当前路来推算
	if (!is_same_road(veh_status->laneinfo.cur_lane, ls)) {
		// if (is_close_end_lane(veh_status, veh, ls)) {
		// 	veh_status->laneinfo.cur_lane = ls;
		// 	veh_status->hdg = veh->hdg;
		// 	veh_status->pt = veh->pt;
		// 	veh_status->speed = veh->speed;
		// 	veh_status->veh_item = veh->veh_item;
		// 	veh_status->status = 0;
		// 	veh_status->timestamp = veh->timestamp;
		// 	veh_status->cinfo = nullptr;
		// 	return 1;
		// }
		change_process(veh_status, veh);
		return 1;
	}
	//计算点，在当前车道的左侧或右侧
	double cross_m = CROSS_MULTI((veh->pt.pt.xyz.x - veh_status->pt.pt.xyz.x), (veh->pt.pt.xyz.y - veh_status->pt.pt.xyz.y), cos(COG2DEG(veh_status->hdg)), sin(COG2DEG(veh_status->hdg)));
	int right;
	if (cross_m > 10e-15) {
		right = 1;
	} else {
		right = -1;
	}
	int curr_id = veh_status->laneinfo.cur_lane->get_id();
	int next_id = ls->get_id();
	auto vl_cinfo = veh_status->cinfo;
	uint32_t id = veh->veh_item.id();
	if (id == DEBUG_TEST_ID) {
		DEBUG_OUTPUT("id is %u\n", id);
	}

	double move_dist = vl_cinfo->v * MSTOS(vl_cinfo->step1_time) + vl_cinfo->step1_acc * SQUARE_POINT(MSTOS(vl_cinfo->step1_time))/2;

	// 目标点
	double len = vl_cinfo->start_speed * MSTOS(LANE_CHANGE_MAX_TIME);
	hdmap_segment_point	dest_pt;
	double dest_w = 0.0;
	double dest_hdg = veh->hdg;
	std::vector<const hdmap_laneseg*> dest_route;
	auto* nlangseg = ls->get_adc_and_point(len, veh->pt, dest_pt, dest_hdg, dest_w, dest_route);
	if (nullptr == nlangseg) {
		printf("vehicle planning next lane segment is null\n");
		return -ELOGIC;
	}

	// hdmap_segment_point	src_final_pt;
	// double srcf_w = 0.0;
	// double srcf_hdg = veh_status->hdg;
	// std::vector<const hdmap_laneseg*> srcfroute;
	// auto* lsrcf = veh_status->laneinfo.cur_lane->get_adc_and_point(len, veh_status->pt, src_final_pt, srcf_hdg, srcf_w, srcfroute);
	// double dangle = AngleDiff(NormalizeAngle(COG2DEG(srcf_hdg)), NormalizeAngle(COG2DEG(dest_hdg)));
	// if (id == DEBUG_TEST_ID) {
	// 	DEBUG_OUTPUT("id is %u\n", id);
	// }
	// // 最终计算落点角度与当前角度差距正负90度，忽略
	// if (dangle > M_PI_2 || dangle < -M_PI_2) {
	// 	normal_process(veh_status, veh);
	// 	return 1;
	// }

	//当前路径变化处于第二阶段
	if (vl_cinfo->step2_time == 0 && vl_cinfo->step2_totaltime == 0)
	{
		// 同向切换
		if (right == vl_cinfo->right) {
			double lane_pos = vl_cinfo->start_pos + move_dist;
			double lane_dis = veh_status->laneinfo.width * right / 2 - lane_pos;
			if (std::abs(lane_dis) > 0.3) {
				vl_cinfo->start_pos = lane_pos;
				double curr_lanew = lane_dis;
				double dst_lanew = dest_w * right / 2;
				vl_cinfo->dst_lane = nlangseg;
				vl_cinfo->dst_ds = 0;
				vl_cinfo->dst_w = dest_w;
				vl_cinfo->start_speed = std::abs(NORMAL_SPEED(PLANNING_INITIAL_SPEED));
				vl_cinfo->droute = dest_route;
				double t1 = curr_lanew * LANE_CHANGE_MAX_TIME / (curr_lanew + dst_lanew);
				double t2 = dst_lanew * LANE_CHANGE_MAX_TIME / (curr_lanew + dst_lanew);
				vl_cinfo->step1_totaltime = t1;
				vl_cinfo->step2_totaltime = t2;
				vl_cinfo->step1_time = 0;
				vl_cinfo->step2_time = 0;
				vl_cinfo->v = 0;
				vl_cinfo->right = right;
				double dst_s_len = curr_lanew + dst_lanew;
				vl_cinfo->step1_acc = dst_s_len * 2 / (MSTOS(t1) * MSTOS(LANE_CHANGE_MAX_TIME));
				vl_cinfo->step2_acc = dst_s_len * 2 * -1 / (MSTOS(t2) * MSTOS(LANE_CHANGE_MAX_TIME));
				if (isnan(vl_cinfo->step1_acc) || isnan(vl_cinfo->step2_acc)) {
					printf("lane change, acc error\n");
				}
				veh_status->status = 1;
				refresh_mid_point(veh_status, veh, ls);
			} else {
				vl_cinfo->start_pos = lane_dis + dest_w * right * -1 / 2;
				veh_status->laneinfo.cur_lane = ls;
				veh_status->laneinfo.width = dest_w;
				veh_status->pt = veh->pt;
				vl_cinfo->dst_lane = nullptr;
				vl_cinfo->dst_ds = 0;
				vl_cinfo->dst_w = 0;
				vl_cinfo->start_speed = std::abs(NORMAL_SPEED(PLANNING_INITIAL_SPEED));
				vl_cinfo->croute = dest_route;
				double t1 = LANE_CHANGE_MAX_TIME / 2;
				vl_cinfo->step1_totaltime = t1;
				vl_cinfo->step2_totaltime = 0;
				vl_cinfo->step1_time = 0;
				vl_cinfo->step2_time = 0;
				vl_cinfo->right = right * -1;
				double dst_s_len = vl_cinfo->start_pos;
				vl_cinfo->v = dst_s_len * -1 / MSTOS(t1);
				vl_cinfo->step1_acc = 0;
				vl_cinfo->step2_acc = 0;
				veh_status->status = 1;
			}
		} else {
			// 反向切换
			double curr_lanew = veh_status->laneinfo.width * right - move_dist;
			double dst_lanew = dest_w / 2 * right;
			vl_cinfo->start_pos = veh_status->laneinfo.width * vl_cinfo->right / 2 + move_dist;
			vl_cinfo->dst_lane = nlangseg;
			vl_cinfo->dst_ds = 0;
			vl_cinfo->dst_w = dest_w;
			vl_cinfo->start_speed = std::abs(NORMAL_SPEED(PLANNING_INITIAL_SPEED));
			vl_cinfo->droute = dest_route;
			double t1 = curr_lanew * LANE_CHANGE_MAX_TIME / (curr_lanew + dst_lanew);
			double t2 = dst_lanew * LANE_CHANGE_MAX_TIME / (curr_lanew + dst_lanew);
			vl_cinfo->step1_totaltime = t1;
			vl_cinfo->step2_totaltime = t2;
			vl_cinfo->step1_time = 0;
			vl_cinfo->step2_time = 0;
			vl_cinfo->v = 0;
			vl_cinfo->right = right;
			double dst_s_len = curr_lanew + dst_lanew;
			vl_cinfo->step1_acc = dst_s_len * 2 / (MSTOS(t1) * MSTOS(LANE_CHANGE_MAX_TIME));
			vl_cinfo->step2_acc = dst_s_len * 2 * -1 / (MSTOS(t2) * MSTOS(LANE_CHANGE_MAX_TIME));
			if (isnan(vl_cinfo->step1_acc) || isnan(vl_cinfo->step2_acc)) {
				printf("lane change, acc error\n");
			}
			veh_status->status = 1;
			refresh_mid_point(veh_status, veh, ls);
		}
	} else {
		// 切换回当前路径
		if (is_in_route(veh_status->cinfo->croute, ls) || is_in_lane(veh_status->laneinfo.cur_lane, veh->pt.s, veh->pt.pt)) {
			vl_cinfo->start_pos = vl_cinfo->start_pos + move_dist;
			vl_cinfo->v = (vl_cinfo->start_pos * -1) / MSTOS(LANE_CHANGE_MAX_TIME / 2);
			vl_cinfo->dst_ds = 0.0;
			vl_cinfo->dst_w = 0.0;
			vl_cinfo->step1_totaltime = LANE_CHANGE_MAX_TIME / 2;
			vl_cinfo->step1_time = 0;
			vl_cinfo->step1_acc = 0;
			vl_cinfo->step2_totaltime = 0;
			vl_cinfo->step2_time = 0;
			vl_cinfo->step2_acc = 0;
			vl_cinfo->dst_lane = nullptr;
		}
		// 同向切换
		else if (right == vl_cinfo->right) {
			change_process(veh_status, veh);
			printf("change_lane_info, current is right error\n");
		} else {
			double curr_lanew = veh_status->laneinfo.width * right / 2 - move_dist;
			double dst_lanew = dest_w / 2 * right;
			vl_cinfo->start_pos = move_dist;
			vl_cinfo->dst_lane = nlangseg;
			vl_cinfo->dst_ds = 0;
			vl_cinfo->dst_w = dest_w;
			vl_cinfo->start_speed = std::abs(NORMAL_SPEED(PLANNING_INITIAL_SPEED));
			vl_cinfo->droute = dest_route;
			double t1 = curr_lanew * LANE_CHANGE_MAX_TIME / (curr_lanew + dst_lanew);
			double t2 = dst_lanew * LANE_CHANGE_MAX_TIME / (curr_lanew + dst_lanew);
			vl_cinfo->step1_totaltime = t1;
			vl_cinfo->step2_totaltime = t2;
			vl_cinfo->step1_time = 0;
			vl_cinfo->step2_time = 0;
			vl_cinfo->v = 0;
			vl_cinfo->right = right;
			double dst_s_len = curr_lanew + dst_lanew;
			vl_cinfo->step1_acc = dst_s_len * 2 / (MSTOS(t1) * MSTOS(LANE_CHANGE_MAX_TIME));
			vl_cinfo->step2_acc = dst_s_len * 2 * -1 / (MSTOS(t2) * MSTOS(LANE_CHANGE_MAX_TIME));
			if (isnan(vl_cinfo->step1_acc) || isnan(vl_cinfo->step2_acc)) {
				printf("lane change, acc error\n");
			}
			veh_status->status = 1;
			refresh_mid_point(veh_status, veh, ls);
		}
	}
	return 0;
}

int vehicle_planning::init_lane_change_info(spfusion_veh_item &veh_status, spfusion_veh_item& veh, const hdmap_laneseg* ls)
{
	if (veh_status->status == 0) {
		if (ls == veh_status->laneinfo.cur_lane) {
			normal_process(veh_status, veh);
			return 1;
		}
		uint32_t id = veh->veh_item.id();
		if (id == DEBUG_TEST_ID) {
			DEBUG_OUTPUT("id is %u\n", id);
		}

		double dangle = AngleDiff(NormalizeAngle(COG2DEG(veh_status->hdg)), NormalizeAngle(COG2DEG(veh->hdg)));
		//与车道方向偏差超过正负45度，不予与考虑切换
		if (dangle > M_PI_4 || dangle < -M_PI_4) {
			normal_process(veh_status, veh, false);
			return 1;
		}

		//直线距离超过3.5m的，不考虑
		if (VEH_DISTANCE(veh_status->pt.pt.xyz.x - veh->pt.pt.xyz.x, veh_status->pt.pt.xyz.y - veh->pt.pt.xyz.y) > LANE_CHANGE_MAX_DISTANCE) {
			normal_process(veh_status, veh, false);
			return 1;
		}

		// 当前点仍然在车道内，则不进行车道变更。应对多车道区域重叠
		if (is_in_lane(veh_status->laneinfo.cur_lane, veh_status->pt.s, veh->pt.pt)) {
			normal_process(veh_status, veh);
			return 1;
		}

		//是否是同一条路， 如果不是按照当前路来推算
		if (!is_same_road(veh_status->laneinfo.cur_lane, ls)) {
			// if (is_close_end_lane(veh_status, veh, ls)) {
			// 	veh_status->laneinfo.cur_lane = ls;
			// 	veh_status->hdg = veh->hdg;
			// 	veh_status->pt = veh->pt;
			// 	veh_status->speed = veh->speed;
			// 	veh_status->veh_item = veh->veh_item;
			// 	veh_status->status = 0;
			// 	veh_status->timestamp = veh->timestamp;
			// 	veh_status->cinfo = nullptr;
			// 	return 1;
			// }
			normal_process(veh_status, veh, false);
			return 1;
		}

		//计算当前在指定时间内，形式路线是否有重叠
		double dst_len = std::abs(NORMAL_SPEED(PLANNING_INITIAL_SPEED) * MSTOS(LANE_CHANGE_MAX_TIME));
		hdmap_segment_point	dst_curr_pt;
		hdmap_segment_point	dst_next_pt;
		double dst_curr_w = 0.0;
		double dst_next_w = 0.0;
		double dst_curr_hdg = veh_status->hdg;
		double dst_next_hdg = veh->hdg;
		std::vector<const hdmap_laneseg*> dst_curr_route;
		std::vector<const hdmap_laneseg*> dst_next_route;
		auto * clangseg = veh_status->laneinfo.cur_lane->get_adc_and_point(dst_len, veh_status->pt, dst_curr_pt, dst_curr_hdg, dst_curr_w, dst_curr_route);
		auto * nlangseg = ls->get_adc_and_point(dst_len, veh->pt, dst_next_pt, dst_next_hdg, dst_next_w, dst_next_route);
		for (auto &rlane : dst_curr_route) {
			if (rlane == ls) {
				normal_process(veh_status, veh);
				return 1;
			}
			for (auto &dlane : dst_next_route) {
				if (dlane == rlane) {
					normal_process(veh_status, veh);
					return 1;
				}
			}
		}

		//计算在当前道路的左侧还是右侧
 		double cross_m = CROSS_MULTI((veh->pt.pt.xyz.x - veh_status->pt.pt.xyz.x), (veh->pt.pt.xyz.y - veh_status->pt.pt.xyz.y), cos(COG2DEG(veh_status->hdg)), sin(COG2DEG(veh_status->hdg)));
		int right = 0;
		if (cross_m > 10e-15) {
			right = 1;
		} else {
			right = -1;
		}

		int curr_id = veh_status->laneinfo.cur_lane->get_id();
		int next_id = ls->get_id();

		//计算中间点
		double mid_len = std::abs(dst_len / 2);
		hdmap_segment_point	mid_curr_pt;
		double mid_curr_w = 0.0;
		double mid_curr_hdg = veh_status->hdg;
		std::vector<const hdmap_laneseg*> mid_curr_route;
		hdmap_segment_point	mid_next_pt;
		double mid_next_w = 0.0;
		double mid_next_hdg = veh->hdg;
		std::vector<const hdmap_laneseg*> mid_next_route;
		auto * mclangseg = veh_status->laneinfo.cur_lane->get_adc_and_point(mid_len, veh_status->pt, mid_curr_pt, mid_curr_hdg, mid_curr_w, mid_curr_route);
		auto * mnlangseg = ls->get_adc_and_point(mid_len, veh->pt, mid_next_pt, mid_next_hdg, mid_next_w, mid_next_route);

		//中间点角度偏差超过正负90度，不予以考虑
		dangle = AngleDiff(NormalizeAngle(COG2DEG(mid_curr_hdg)), NormalizeAngle(COG2DEG(mid_next_hdg)));
		if (dangle > M_PI_2 || dangle < -M_PI_2) {
			normal_process(veh_status, veh, false);
			return 1;
			// veh_status->laneinfo.cur_lane = ls;
			// veh_status->hdg = veh->hdg;
			// veh_status->pt = veh->pt;
			// veh_status->veh_item = veh->veh_item;
			// veh_status->status = 0;
			// veh_status->timestamp = veh->timestamp;
			// veh_status->cinfo = nullptr;
			// return 1;
		}		
		if (nullptr == nlangseg || nullptr == mnlangseg) {
			printf("next lane segment not find dest laneseg\n");
			normal_process(veh_status, veh);
			return 1;
		}
		if (dst_next_w < 0.1) {
			normal_process(veh_status, veh);
			return 1;
		}	
		if (mid_curr_w < 0.1) {
			veh_status->laneinfo.cur_lane = ls;
			veh_status->hdg = veh->hdg;
			veh_status->pt = veh->pt;
			veh_status->veh_item = veh->veh_item;
			veh_status->status = 0;
			veh_status->timestamp = veh->timestamp;
			veh_status->cinfo = nullptr;
			return 1;
		}

		veh_status->cinfo = std::make_shared<lane_change_info>();
		veh_status->cinfo->v = 0;

		if (id == DEBUG_TEST_ID) {
			DEBUG_OUTPUT("id is %u\n", id);
		}
		veh_status->cinfo->start_pos = 0;
		veh_status->cinfo->dst_lane = nlangseg;
		veh_status->cinfo->dst_ds = mid_next_pt.s - mid_curr_pt.s;
		veh_status->cinfo->dst_w = dst_next_w;
		veh_status->cinfo->start_speed = std::abs(NORMAL_SPEED(PLANNING_INITIAL_SPEED));
		veh_status->cinfo->croute = dst_curr_route;
		veh_status->cinfo->droute = dst_next_route;
		double t1 = mid_curr_w * LANE_CHANGE_MAX_TIME / (mid_curr_w + dst_next_w);
		double t2 = dst_next_w * LANE_CHANGE_MAX_TIME / (mid_curr_w + dst_next_w);
		veh_status->cinfo->step1_totaltime = t1;
		veh_status->cinfo->step2_totaltime = t2;
		veh_status->laneinfo.width = mid_curr_w;
		veh_status->cinfo->step1_time = 0;
		veh_status->cinfo->step2_time = 0;
		veh_status->cinfo->right = right;
		double dst_s_len = (mid_curr_w + dst_next_w) * right / 2;
		veh_status->cinfo->step1_acc = dst_s_len * 2 / (MSTOS(t1) * MSTOS(LANE_CHANGE_MAX_TIME));
		veh_status->cinfo->step2_acc = dst_s_len * 2 * -1 / (MSTOS(t2) * MSTOS(LANE_CHANGE_MAX_TIME));
		if (isnan(veh_status->cinfo->step1_acc) || isnan(veh_status->cinfo->step2_acc)) {
			printf("lane change init, acc error\n");
		}
		veh_status->status = 1;

		refresh_mid_point(veh_status, veh, ls);
		return 0;
	}
	return -ENOTAVAIL;
}

int vehicle_planning::refresh_mid_point(spfusion_veh_item &veh_status, spfusion_veh_item& veh, const hdmap_laneseg* ls)
{
	auto vl_cinfo = veh_status->cinfo;
	double mid_len = std::abs(vl_cinfo->start_speed * MSTOS(vl_cinfo->step1_totaltime - vl_cinfo->step1_time));
	hdmap_segment_point	dest_mid_pt;
	double dest_mid_w = 0.0;
	double dest_mid_hdg = veh->hdg;
	std::vector<const hdmap_laneseg*> dest_mid_route;
	auto* dmidlane = ls->get_adc_and_point(mid_len, veh->pt, dest_mid_pt, dest_mid_hdg, dest_mid_w, dest_mid_route);
	if (nullptr == dmidlane) {
		printf("vehicle planning find mid point error\n");
		return 0;
	}

	hdmap_segment_point	src_mid_pt;
	double src_mid_w = 0.0;
	double src_mid_hdg = veh_status->hdg;
	std::vector<const hdmap_laneseg*> src_mroute;
	auto* midlane = veh_status->laneinfo.cur_lane->get_adc_and_point(mid_len, veh_status->pt, src_mid_pt, src_mid_hdg, src_mid_w, src_mroute);
	vl_cinfo->dst_ds = dest_mid_pt.s - src_mid_pt.s;
	vl_cinfo->dst_lane = dmidlane;
	return 0;
}

bool vehicle_planning::is_close_end_lane(spfusion_veh_item &veh_status, spfusion_veh_item& veh, const hdmap_laneseg* endls)
{
	const hdmap_laneseg* curls = veh_status->laneinfo.cur_lane;
	if (nullptr == curls) { return true;}
	if (nullptr == endls) { return false;}
	bool bincur = is_in_lane(curls, veh_status->pt.s, veh->orgin_pt);
	bool binend = is_in_lane(endls, veh_status->pt.s, veh->orgin_pt);
	if (!binend) { return false;}
	if (!bincur) { return true;}

	double diff_cur = NormalizeAngle(COG2DEG(veh_status->hdg) - COG2DEG(veh->veh_item.hdg()));
	double diff_end = NormalizeAngle(COG2DEG(veh->hdg) - COG2DEG(veh->veh_item.hdg()));

	if (std::abs(diff_cur) < std::abs(diff_end)) {
		return true;
	}
	return false;
}

int vehicle_planning::normal_process(spfusion_veh_item &veh_status, spfusion_veh_item& veh, bool bused)
{
	double len = std::abs(NORMAL_SPEED(veh_status->speed) * MSTOS(veh->timestamp - veh_status->timestamp));
	if (bused) {
		double diff_angle = NormalizeAngle(COG2DEG(veh->veh_item.hdg()) - COG2DEG(veh_status->hdg));
		double diff_cos = std::abs(cos(diff_angle));
		if (diff_cos < 0.3) {
			diff_cos = 1;
		}
		len = len * diff_cos;
	}
	std::vector<const hdmap_laneseg*> route;
	veh_status->timestamp = veh->timestamp;
	auto * nlangseg = veh_status->laneinfo.cur_lane->get_adc_and_point(len, veh_status->pt, veh->pt, veh->hdg, veh_status->laneinfo.width, route);
	if (nullptr == nlangseg) {
		printf("vehicle planning not find next lanesection\n");
		return -ENOTFOUND;
	}
	veh_status->laneinfo.cur_lane = nlangseg;
	veh_status->hdg = veh->hdg;
	veh_status->pt = veh->pt;
	if (bused) {
		veh_status->speed = veh->speed;
	} else {
		if (veh_status->speed < LANE_MAX_STOP_SPEED - 1){
			veh_status->speed = LANE_MAX_STOP_SPEED - 1 ;
		}
	}
	veh_status->veh_item = veh->veh_item;
	veh_status->status = 0;
	return 0;
}

int vehicle_planning::change_process(spfusion_veh_item &veh_status, spfusion_veh_item& veh)
{
	uint32_t id = veh->veh_item.id();
	double test_hdg = veh_status->hdg;
	if (veh_status->timestamp > veh->timestamp) {
		printf("time error status %lu, current %lu\n", veh_status->timestamp, veh->timestamp);
		return 0;
	}
	double d_tm = veh->timestamp - veh_status->timestamp;
	if (d_tm > 1000 || d_tm < 0) {
		printf("error:process time is long %f\n", d_tm);
		d_tm = 200;
	}
	auto vl_info = veh_status->cinfo;
	if (id == DEBUG_TEST_ID) {
		DEBUG_OUTPUT("id is %u\n", id);
		DEBUG_OUTPUT("status s %f, veh %f\n", veh_status->pt.s, veh->pt.s);
	}
	veh_status->veh_item = veh->veh_item;
	double diff_angle = NormalizeAngle(COG2DEG(veh->veh_item.hdg()) - COG2DEG(veh_status->hdg));
	double diff_cos = std::abs(cos(diff_angle));
	if (diff_cos < 0.3) {
		diff_cos = 1;
	}
	double len = std::abs(NORMAL_SPEED(veh_status->speed) * diff_cos * MSTOS(d_tm));
	d_tm = len * 1000 / vl_info->start_speed;
	if (d_tm > 1000) {
		printf("process time is long %f\n", d_tm);
	}
	vl_info->step1_time += d_tm;
	if (vl_info->step1_time < vl_info->step1_totaltime ||
		(vl_info->step2_time == 0 && vl_info->step2_totaltime == 0)) {
		std::vector<const hdmap_laneseg*> lsroute;
		veh_status->timestamp = veh->timestamp;
		double next_hdg = veh_status->hdg;
		double width = veh_status->laneinfo.width;
		hdmap_segment_point nextpt;
		auto * nlangseg = veh_status->laneinfo.cur_lane->get_adc_and_point(len, veh_status->pt, nextpt, next_hdg, width, lsroute);
		if (nullptr == nlangseg) {
			printf("error: get adc and point return nullptr\n");
			return 0;
		}
		veh_status->hdg = next_hdg;
		veh_status->laneinfo.width = width;
		veh_status->pt = nextpt;
		veh_status->laneinfo.cur_lane = nlangseg;
		double w = vl_info->step1_acc * SQUARE_POINT(MSTOS(vl_info->step1_time))/2 + vl_info->v * MSTOS(vl_info->step1_time);
		w = vl_info->start_pos + w;
		double dif_hdg = 90.0;
		if (w < 0) {
			w = w * -1;
			dif_hdg = dif_hdg * -1;
		}

		double x = w * cos(COG2DEG(veh_status->hdg + dif_hdg));
		double y = w * sin(COG2DEG(veh_status->hdg + dif_hdg));
		point3d dpt(x, y);
		veh_status->pt.pt += dpt;
		veh_status->speed = veh->speed;
		if (vl_info->step1_time >= vl_info->step1_totaltime) {
			veh_status->cinfo = nullptr;
			veh_status->status = 0;
		}
	} else {
		if (id == DEBUG_TEST_ID) {
			DEBUG_OUTPUT("id is %u\n", id);
		}
		double ddt2 = vl_info->step1_time - vl_info->step1_totaltime;
		double v_max = vl_info->step1_acc * MSTOS(vl_info->step1_totaltime);
		vl_info->v = v_max;
		vl_info->right *= -1;
		std::vector<const hdmap_laneseg*> lsroute;
		veh_status->pt.s += vl_info->dst_ds;
		vl_info->dst_ds = 0;
		veh_status->timestamp = veh->timestamp;
		auto * nlangseg = vl_info->dst_lane->get_adc_and_point(len, veh_status->pt, veh_status->pt, veh_status->hdg, veh_status->laneinfo.width, lsroute);
		double w = vl_info->step2_acc * SQUARE_POINT(MSTOS(ddt2))/2 + v_max * MSTOS(ddt2);
		vl_info->start_pos = veh_status->laneinfo.width / 2 * vl_info->right;
		w = vl_info->start_pos + w;
		double dif_hdg = 90.0;
		if (w < 0) {
			w = w * -1;
			dif_hdg = dif_hdg * -1;
		}
		double x = w * cos(COG2DEG(veh_status->hdg + dif_hdg));
		double y = w * sin(COG2DEG(veh_status->hdg + dif_hdg));
		point3d dpt(x, y);
		vl_info->step1_acc = vl_info->step2_acc;
		if (isnan(vl_info->step1_acc )) {
			printf("process change step1 acc error\n");
		}
		vl_info->step2_acc = 0;
		if (ddt2 < 0) {
			vl_info->step1_time = 0;
		} else {
			vl_info->step1_time = ddt2;
		}
		vl_info->step2_time = 0;
		vl_info->step1_totaltime = vl_info->step2_totaltime;
		vl_info->step2_totaltime = 0;
		vl_info->croute = vl_info->droute;
		vl_info->droute.clear();
		veh_status->laneinfo.cur_lane = vl_info->dst_lane;
		vl_info->dst_lane = nullptr;
		veh_status->pt.pt += dpt;
		veh_status->speed = veh->speed;
		veh_status->line = veh->line;
	}
	return 0;
}

}}	//zas::vehicle_snapshot_service




