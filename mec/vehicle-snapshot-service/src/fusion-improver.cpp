#include "fusion-improver.h"
#include "vehicle-planning.h"
#include <sys/time.h>
#include <Eigen/Dense>

#include "device-snapshot.h"
#include "webcore/logger.h"

#include "core/src/modules/imgprocessor/imgprocessor.h"


namespace zas {
namespace vehicle_snapshot_service {

#define PLANNING_MAX_TIME (5000)
#define PLANNING_MIN_SPEED (1)

using VehicleState = coop::v2x::VehicleState;
using IMGPROCESSOR = coop::v2x::IMGPROCESSOR;
using VehicleObs = coop::v2x::VehicleObs;
using spVehicleState = coop::v2x::spVehicleState;
using Earth = coop::v2x::Earth;
using Vec2d = coop::common::math::Vec2d;

using namespace zas::utils;
using namespace zas::mapcore;
using namespace zas::webcore;
using namespace jos;

std::shared_ptr<IMGPROCESSOR> sp_img_processor_;
double NormalizeRad(const double angle) {
  double a = std::fmod(angle, 360.0);
  if (a < 0.0) {
    a += 360;
  }
  return a;
}
fusion_veh_item::fusion_veh_item()
{
	listnode_init(ownerlist);
}

int fusion_veh_item::set_target(target &src)
{
	veh_item = src;
	orgin_pt.set(veh_item.lon(), veh_item.lat());
	vehicle_planning::inst()->change_gps_to_utm(orgin_pt, pt.pt);
	hdg = veh_item.hdg();
	speed = veh_item.speed();
	if (speed < 10e-15) {
		speed = 0.0;
	}
	bad_item = false;
	return 0;
}

int fusion_veh_item::get_target(target &dst)
{
	if (veh_item.id() == DEBUG_TEST_ID) {
		DEBUG_OUTPUT("targe %u: (%f, %f) s: %f, speed %f, hdg %f, timestamp %lu\n", DEBUG_TEST_ID, pt.pt.xyz.x, pt.pt.xyz.y, pt.s, speed, hdg, timestamp);
	}
	point3d lla;
	vehicle_planning::inst()->change_utm_to_gps(pt.pt, lla);
	veh_item.set_lon(lla.v[0]);
	veh_item.set_lat(lla.v[1]);
	veh_item.set_hdg(NormalizeRad(hdg));
	dst = veh_item;
	dst.set_speed(speed);
	return 0;
}

void lane_change_info::clear(void)
{
	dst_lane = nullptr;
	croute.clear();
	droute.clear();
	right = 0;
	step1_acc = 0;
	step2_acc = 0;
	step1_totaltime = 0;
	step2_totaltime = 0;
	step1_time = 0;
	step2_time = 0;
}

fusion_improver::fusion_improver(void)
{
	coop::v2x::Earth::SetOrigin(Eigen::Vector3d(31.284156453, 121.170937985, 0), true);
	sp_img_processor_ = std::make_shared<IMGPROCESSOR>();
	std::string img_fd = std::string(PROJECT_ROOT_PATH) + "/img_result";
	sp_img_processor_->set_img_folder(img_fd);
	_fn_planning.clear();
	_fj_planning.clear();
	_fm_planning.clear();
	_light_info.clear();
	_frame_count = 0;
}

fusion_improver::~fusion_improver()
{

}

int fusion_improver::init_junction_info(spfusion_juncion_info info)
{
	if (nullptr == info) {
		return -EBADPARM;
	}
	int ret = vehicle_planning::inst()->init_junciton(info);
	if (ret < 0) {
		return ret;
	}
	_fusion_jinfo = info;
	for (auto const &incom_rd : info->road_info) {
		for (auto const &lane : incom_rd.second->lane_info) {
			for (auto const &nlane : lane.second->next_lanes) {
				vss_light_mgr::inst()->add_light(info->junction_id, nlane.second->light_id);
			}
		}
	}
	return 0;
}

int fusion_improver::improve_fusion_target_to_junction(
	device_junction_item* item, fusion_service_pkg &pkg)
{
	assert(nullptr != item);
	auto fpkg = pkg.fus_pkg();
	item->info[item->index].clear_targets();
#ifdef IMAGE_DEBUG
	std::map<int, spVehicleState> orign_v;
	std::map<int, spVehicleState> tran_v;
	double x = 0.0, y = 0.0;
	cv::Mat combine;
#endif
	int index = 0;
	uint64_t pkgtime = pkg.timestamp();
	uint32_t dtime = get_process_time(pkgtime);

	reset_status();
	junction_idm_process(dtime);
	
	// for debug
	if (_frame_count == IMAGE_DEBUG_FRAME) {
		DEBUG_OUTPUT("framecoung %d for test\n", _frame_count);
	}

	for (uint32_t i = 0; i < fpkg.targets_size(); i++) {
		auto dev_tg = fpkg.targets(i);
#ifdef IMAGE_DEBUG
		VehicleObs tvo(&dev_tg);
		spVehicleState tvs= std::make_shared<VehicleState>();
		tvs->Init(tvo);
		// tvs->set_id(i);
		tvs->set_id(dev_tg.id());
		orign_v[i] = tvs;
#endif
		//非机动车辆，组做地图匹配
		if (dev_tg.class_() < target_type_car) {
			continue;
		}
		if (dev_tg.id() == DEBUG_TEST_ID) {
			DEBUG_OUTPUT("target %d, frame %d\n", dev_tg.id(), _frame_count);
		}

		auto vitem = std::make_shared<fusion_veh_item>();
		vitem->set_target(dev_tg);
		vitem->timestamp = pkgtime;
		if (_fj_planning.count(dev_tg.id()) > 0) {
			junction_process(vitem);
		} else if (_fm_planning.count(dev_tg.id()) > 0){
			_fm_planning[dev_tg.id()]->veh_item = vitem->veh_item;
			_fm_planning[dev_tg.id()]->timestamp = 0;
		}
		else {
			normal_process(vitem);
		}
	}

	junction_process(dtime);
	missing_process(dtime);
	check_set_status_result();
	set_targets(&item->info[item->index]);

#ifdef IMAGE_DEBUG
	if (_frame_count >0) {
	for (int i = 0 ; i < item->info[item->index].targets_size(); i++) {
		auto& tgt = item->info[item->index].targets(i);
		VehicleObs tvf(&tgt);
		spVehicleState tvsf= std::make_shared<VehicleState>();
		tvsf->Init(tvf);
		// tvsf->set_id(i);
		tvsf->set_id(tgt.id());
		tran_v[i] = tvsf;
		if (tgt.id() == DEBUG_TEST_ID) {
			if (_fj_planning.count(tgt.id())) {
				x = _fj_planning[tgt.id()]->pt.pt.xyz.x;
				y = _fj_planning[tgt.id()]->pt.pt.xyz.y;
			} else if (_fn_planning.count(tgt.id())) {
				x = _fn_planning[tgt.id()]->pt.pt.xyz.x;
				y = _fn_planning[tgt.id()]->pt.pt.xyz.y;
			}
		}
	}
	long cur_time = gettick_nanosecond();
	sp_img_processor_->PlotVehicleSnapshot(&combine, orign_v, tran_v, cur_time, cur_time - 5, x, y);
	for (auto &&fvitem : _fn_planning) {
		if (fvitem.second->line.size() > 0) {
			std::vector<Vec2d> line_enu;
			for (auto& linei : fvitem.second->line){
				point3d lla;
				vehicle_planning::inst()->change_utm_to_gps(linei, lla);
				auto ella = Eigen::Vector3d{lla.v[1], lla.v[0], 0};
				Eigen::Vector3d pos_enu = Earth::LLH2ENU(ella, true);
				Vec2d pcen_enu{pos_enu[0], pos_enu[1]};
				line_enu.push_back(pcen_enu);
			}
			sp_img_processor_->PlotCenterLine(&combine, line_enu, cv::Scalar(0, 255, 0));
		}
	}

	sp_img_processor_->SaveImage(&combine, cur_time);
	}
#endif

	_frame_count++;
	return 0;
}

int fusion_improver::reset_status(void)
{
	// 如果本次处理过程中缺失target，则认为是不可用target
	// 即处理完成后仍为false的，作为消失的target
	for (auto && veh : _fn_planning) {
		veh.second->bused = false;
	}
	for (auto && veh : _fj_planning) {
		veh.second->bused = false;
	}

	if (nullptr == _fusion_jinfo) {
		return -ENOTAVAIL;
	}

	//更新信号灯状态
	vss_light_mgr::inst()->get_light_state(_fusion_jinfo->junction_id, &_light_info);

	return 0;
}

uint32_t fusion_improver::get_process_time(uint64_t pkgtime)
{
	uint32_t dtime = 50;
	if (!_fn_planning.empty()) {
		dtime = pkgtime - _fn_planning.begin()->second->timestamp;
		if (dtime > 10000 || dtime == 0) {
			dtime = 50;
		}
	}
	else if (!_fj_planning.empty()) {
		dtime = pkgtime - _fj_planning.begin()->second->timestamp;
		if (dtime > 10000 || dtime == 0) {
			dtime = 50;
		}
	}
	return dtime;
}

int fusion_improver::check_set_status_result(void)
{
	for (auto it = _fm_planning.begin(); it != _fm_planning.end();) {
		if (it->second->timestamp == 0
			|| it->second->timestamp > PLANNING_MAX_TIME) {
			DEBUG_OUTPUT("+++++++++++remove car %u, remove reason %lu\n", it->first, it->second->timestamp);
			it = _fm_planning.erase(it);
			continue;
		}
		it++;
	}

	for (auto it = _fj_planning.begin(); it != _fj_planning.end();) {
		if (!it->second->bused) {
			_fm_planning[it->first] = it->second;
			auto veh = it->second;
			listnode_del(veh->ownerlist);
			veh->timestamp = 0;
			veh->cinfo = nullptr;
			it = _fj_planning.erase(it);
			continue;
		}
		it++;
	}

	for (auto it = _fn_planning.begin(); it != _fn_planning.end();) {
		if (it->second->bused) {
			if (is_in_junction(it->second)) {
				_fj_planning[it->first] = it->second;
				// 0 未进入路中， 1进入路中
				_fj_planning[it->first]->status = 0;
				_fj_planning[it->first]->a = 0;
				_fj_planning[it->first]->count_incomm = false;
				_fj_planning[it->first]->count_outgoing = false;
				_fj_planning[it->first]->cinfo = nullptr;
				it = _fn_planning.erase(it);
				continue;
			}
		} else {
			it = _fn_planning.erase(it);
			continue;
		}
		it++;
	}
	return 0;
}

bool fusion_improver::is_missed(std::map<uint32_t, spfusion_veh_item> &jun_items, spfusion_veh_item curr)
{
	uint32_t id = curr->veh_item.id();
	if (jun_items.count(id) <= 0) {
		return true;
	}
	double distance = VEH_DISTANCE(jun_items[id]->pt.pt.xyz.x - curr->pt.pt.xyz.x, jun_items[id]->pt.pt.xyz.y - curr->pt.pt.xyz.y);

	if (distance < LANE_CHANGE_MAX_DISTANCE) {
		printf("[%u]jucntion change to normal\n", id);
		return false;
	}
	printf("[%u]jucntion change to missng\n", id);
	return true;
}

bool fusion_improver::is_in_junction(spfusion_veh_item item)
{
	if(nullptr == _fusion_jinfo) {
		return false;
	}
	for (auto const &incom_rd : _fusion_jinfo->road_info) {
		for (auto const &lane : incom_rd.second->lane_info) {
			if (item->laneinfo.cur_lane == lane.second->curr_last_ls) {
				auto rd = lane.second->curr_last_ls->get_lane()->get_road();
				auto len = rd->get_length();
				if (item->pt.s > len || (len - item->pt.s) < 0.5) {
					return true;
				} else {
					return false;
				}
			}
		}
	}
	return false;
}

int fusion_improver::normal_process(spfusion_veh_item item)
{
	int iret = vehicle_planning::inst()->normal_planning(_fn_planning, _fusion_jinfo, item);
	return 0;
}

int fusion_improver::junction_process(spfusion_veh_item item)
{
	int iret = vehicle_planning::inst()->junction_planning(_fj_planning, _fusion_jinfo, &_light_info, item);
	return 0;
}

int fusion_improver::junction_process(uint32_t dtime)
{
	int iret = vehicle_planning::inst()->junction_planning(_fj_planning, _fusion_jinfo, &_light_info, dtime);
	return 0;
}

int fusion_improver::junction_idm_process(uint32_t dtime)
{
	int iret = vehicle_planning::inst()->junction_idm_planning(_fusion_jinfo, dtime);
	return 0;
}

int fusion_improver::missing_process(uint32_t dtime)
{
	int iret = vehicle_planning::inst()->missing_planning(_fm_planning, _fusion_jinfo, dtime);
	return 0;
}

int fusion_improver::set_targets(fusion_package* fpkg)
{
	for (auto const &veh : _fn_planning) {
		if (veh.second->bad_item) {
			continue;
		}
		auto* tgt = fpkg->add_targets();
		veh.second->get_target(*tgt);
		if(tgt->speed() < 0.0) {
			printf("fusion normal speed %lf\n", tgt->speed());
		}
	}
	for (auto const &veh : _fj_planning) {
		if (veh.second->bad_item) {
			continue;
		}
		auto* tgt = fpkg->add_targets();
		veh.second->get_target(*tgt);
		if(tgt->speed() < 0.0) {
			printf("fusion junction speed %lf\n", tgt->speed());
		}
	}
	for (auto const &veh : _fm_planning) {
		if (veh.second->bad_item) {
			continue;
		}
		auto* tgt = fpkg->add_targets();
		veh.second->get_target(*tgt);
		if(tgt->speed() < 0.0) {
			printf("fusion missing speed %lf\n", tgt->speed());
		}
	}
	return 0;
}


}}	//zas::vehicle_snapshot_service




