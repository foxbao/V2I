/******************************************************************************
 * Copyright 2022 The CIV Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/
#include "interface/fuser.h"
#include <sys/time.h>
#include <eigen3/Eigen/Dense>
#include <iostream>
#include <string>
#include <vector>
#include "common/file.h"
#include "common/util/util.h"
#include "modules/common/earth.h"
#include "modules/common/time_system.h"
#include "modules/common/util/util.h"
#include "modules/common/util/vehicle_id.h"
#include "modules/fusion/fusion.h"

namespace coop {
namespace v2x {
using namespace jos;
// Definition and initialization of global variables

std::unique_ptr<FuserConfig> g_fuserConfig =
    common::util::make_unique<FuserConfig>();

Fuser::Fuser() {
  Earth::SetOrigin(
      Eigen::Vector3d(g_ori_pos_deg[0], g_ori_pos_deg[1], g_ori_pos_deg[2]),
      true);
  sp_fusion_ = std::make_shared<Fusion>();
  sp_img_processor_ = std::make_shared<IMGPROCESSOR>();
  // sp_viz_processor_ = std::make_shared<VizProcessor>();
  sp_predictor_ = std::make_shared<Predictor>();
  sp_vehicle_factory_ = std::make_shared<VehicleFactory>();
  std::cout << "root path:" << PROJECT_ROOT_PATH << std::endl;
  std::string raysun_system_cfg_pth =
      std::string(PROJECT_ROOT_PATH) +
      "/middle_ware/cyber/conf/systemConfig.pb.txt";
  raysunSystem_config_ = std::make_shared<RaysunSystemConfig>();
  apollo::cyber::common::GetProtoFromFile(raysun_system_cfg_pth,
                                          raysunSystem_config_.get());
  std::cout << raysunSystem_config_->max_match_distance() << std::endl;
  std::string cfg_pth =
      std::string(PROJECT_ROOT_PATH) + "/middle_ware/cyber/conf/config.pb.txt";
  apollo::cyber::common::GetProtoFromFile(cfg_pth, g_fuserConfig.get());

  g_cacher_ = std::make_shared<Cacher>();
  sp_fusion_->set_max_distance_match(
      raysunSystem_config_->max_match_distance());
  std::string img_fd = std::string(PROJECT_ROOT_PATH) + "/img_result";
  sp_img_processor_->set_img_folder(img_fd);
}

Fuser::~Fuser() {}

void Fuser::Fuse() {
  ReadData();
  ProcessData();
}

void Fuser::ReadData() {
  spDataReader sp_dataReader = std::make_shared<DataReader>();
  std::vector<std::string> raysun_files;
  for (const auto &filePath : raysunSystem_config_->rayunfilepaths()) {
    raysun_files.push_back(filePath);
  }
  sp_dataReader->ReadRaysunFiles(raysun_files, data_buffer_);
}

void Fuser::ProcessData() {
  int frame_idx = 0;
  for (const auto &frame : data_buffer_) {
    frame_idx++;
    if (frame_idx < 1150) {
      continue;
    }
    ProcessOneFrame(frame, true);
  }
}

void Fuser::ProcessOneFrame(sp_cRaysunFrame frame, bool plot) {
  g_cacher_->push_back(frame);
  // std::cout << "vehicles size" << vehicle_states_.size() << std::endl;
  std::shared_ptr<std::vector<spRaysunFrame>> syncFrames =
      std::make_shared<std::vector<spRaysunFrame>>();
  // std::cout << "frame devid:" << frame->head().devid
  //           << " utc time:" << frame->head().timestamp << std::endl;

  // If the time of coming frame is earlier than current time, only save it in
  // cache, but do not use it to update time
  if (frame->head().timestamp <= current_time_) {
    return;
  }
  last_time_ = current_time_;
  current_time_ = frame->head().timestamp;
  // repeated frame from different sensors
  if (current_time_ == last_time_) {
    return;
  }
  // get sync frames from different cameras
  GetSyncFrames(frame, syncFrames);

  PropagateVehicles(vehicle_states_, frame->head().timestamp);
  UpdateVehicleStates(vehicle_states_, *syncFrames);
  UpdateVehicleObsTraj(syncFrames);

  if (plot) {
    PlotVehicleStatesAndFrames(vehicle_states_, *syncFrames, vehicle_obsers_,
                               current_time_, last_time_);
  }
}

void Fuser::PropagateVehicles(std::map<int, spVehicleState> &vehicle_states,
                              OCLong current_time) {
  removed_vehicle_states_.clear();
  sp_predictor_->PropagateVehicles(vehicle_states, removed_vehicle_states_,current_time);
}
void Fuser::UpdateVehicleObsTraj(
    const std::shared_ptr<std::vector<spRaysunFrame>> syncFrames) {
  std::map<int, spVehicleObs> current_vehicle_obs;
  // get observed vehicles from all the synced devices
  for (const auto &frame : *syncFrames) {
    for (auto &&obs : frame->targets()) {
      current_vehicle_obs[obs.id()] = std::make_shared<VehicleObs>(obs);
      current_vehicle_obs[obs.id()]->set_devid(frame->head().devid);
    }
  }
  // if a vehicle observation trajectory cannot be found in current frames,
  // remove it
  for (auto it = vehicle_obsers_.begin(); it != vehicle_obsers_.end();) {
    if (!current_vehicle_obs.count(it->second->id())) {
      vehicle_obsers_.erase(it++);
    } else {
      it++;
    }
  }

  // if one of the target in frame is not found in observation trajectory, add
  // it if found, add trajectory

  for (auto const &obs : current_vehicle_obs) {
    if (!vehicle_obsers_.count(obs.second->id())) {
      vehicle_obsers_[obs.second->id()] =
          std::make_shared<VehicleObs>(*(obs.second));
      vehicle_obsers_[obs.second->id()]->AddTrajectoryPoint(
          obs.second->pos_enu());
    } else {
      vehicle_obsers_[obs.second->id()]->AddTrajectoryPoint(
          obs.second->pos_enu());
    }
    vehicle_obsers_[obs.second->id()]->trajectory_enu();
    obs.second->SetTrajectoryData(
        vehicle_obsers_[obs.second->id()]->trajectory_enu());
  }
}

void Fuser::UpdateVehicleStates(std::map<int, spVehicleState> &vehicles,
                                const std::vector<spRaysunFrame> syncFrames) {
  for (const auto &frame : syncFrames) {
    for (const auto &obs : frame->targets()) {
      int vehicle_state_id;  // id of vehicle tracked of associated
      // if the observation is already tracked by one vehicle state
      // update the state with observation
      if (isTracked(frame->head().devid, obs.id(), vehicles,
                    &vehicle_state_id)) {
        vehicles[vehicle_state_id]->set_timestamp(frame->head().timestamp);
        vehicles[vehicle_state_id]->Update2(obs);

        continue;
      }

      // if the observation is not tracked but can be associated with a close
      // vehicle
      if (isAssociated(frame->head().devid, obs, vehicles, &vehicle_state_id)) {
        vehicles[vehicle_state_id]->obs_dev_target_id[frame->head().devid] =
            obs.id();
        vehicles[vehicle_state_id]->set_timestamp(frame->head().timestamp);
        vehicles[vehicle_state_id]->Update2(obs);
        continue;
      }
      // Add a new vehicle state if it is not in current states
      AddVehicleState(vehicles, frame, obs);
      // UpdateVehicleObsTraj(obs)
    }
  }
}

bool Fuser::AddVehicleState(std::map<int, spVehicleState> &vehicles,
                            sp_cRaysunFrame frame, const VehicleObs &target) {
  int new_vehicle_id = VehicleID::getInstance()->get_new_id();
  vehicles[new_vehicle_id] = std::make_shared<VehicleState>();
  auto new_vehicle = vehicles[new_vehicle_id];
  new_vehicle->Init(target);
  new_vehicle->set_id(new_vehicle_id);
  new_vehicle->set_timestamp(frame->head().timestamp);
  new_vehicle->obs_dev_target_id[frame->head().devid] = target.id();
}

bool Fuser::isAssociated(
    const OCLong dev_id, const VehicleObs &target_obs,
    std::map<int, coop::v2x::spVehicleState> &vehicle_states,
    int *vehicle_state_id) {
  for (const auto &vehicle : vehicle_states) {
    if (target_obs.obj_class() == RAYSUN_TARGET_TYPE_PEDESTRIAN &&
        vehicle.second->obj_class() != RAYSUN_TARGET_TYPE_PEDESTRIAN) {
      continue;
    }
    if (target_obs.obj_class() != RAYSUN_TARGET_TYPE_PEDESTRIAN &&
        vehicle.second->obj_class() == RAYSUN_TARGET_TYPE_PEDESTRIAN) {
      continue;
    }

    if (vehicle.second->obs_dev_target_id.count(dev_id)) {
      continue;
    }
    // if the observed vehicle is close to a vehicle state, we associate them
    Eigen::Vector3d dis_3d = target_obs.pos_enu() - vehicle.second->pos_enu();
    Eigen::Vector2d dis_2d_ENU{dis_3d(0),
                               dis_3d(1)};  // difference in ENU coordinate
    double heading_enu = vehicle.second->HeadingENU();
    Eigen::Matrix2d rotation;
    rotation << cos(heading_enu * DEG2RAD), -sin(heading_enu * DEG2RAD),
        sin(heading_enu * DEG2RAD), cos(heading_enu * DEG2RAD);
    Eigen::Vector2d dis_2d_vehicle =
        rotation.transpose() * dis_2d_ENU;  // difference in vehicle coordinate

    if (abs(dis_2d_vehicle(0)) < 4 && abs(dis_2d_vehicle(1)) < 2.5) {
      *vehicle_state_id = vehicle.second->id();
      return true;
    }
  }
  return false;
}
bool Fuser::isTracked(const OCLong devid, const OCInt target_id,
                      std::map<int, coop::v2x::spVehicleState> &vehicle_states,
                      int *vehicle_state_id) {
  for (auto &&vehicle : vehicle_states) {
    // if the vehicle state is already tracked by a specific dev by target id
    // return true and give back the target id
    if (vehicle.second->obs_dev_target_id.count(devid)) {
      if (target_id == vehicle.second->obs_dev_target_id[devid]) {
        *vehicle_state_id = vehicle.second->id();
        return true;
      }
    }
  }
  return false;
}

void Fuser::PlotVehicleStatesAndFrames(
    std::map<int, spVehicleState> vehicles,
    std::vector<spRaysunFrame> syncFrames,
    std::map<int, spVehicleObs> vehicle_obsers, OCLong current_time,
    OCLong last_time) {
  cv::Mat combine;
  sp_img_processor_->PlotVehicleStatesAndFrames(
      &combine, vehicles, syncFrames, vehicle_obsers, current_time, last_time);
  sp_img_processor_->SaveImage(&combine, current_time);
}

void Fuser::GetSyncFrames(
    sp_cRaysunFrame frame,
    std::shared_ptr<std::vector<spRaysunFrame>> syncFrames) {
  for (auto &&cam_deque : g_cacher_->named_buff()) {
    sp_cRaysunFrame sp_cMatchedFrame =
        g_cacher_->GetNearestFrame(frame->head().timestamp, cam_deque.first);
    if (!sp_cMatchedFrame) {
      continue;
    }
    OCLong dt_ms =
        frame->head().timestamp - sp_cMatchedFrame->head().timestamp;  // ms
    if (dt_ms < 1000) {
      // if the matched frame in name_buff is too old, do not merge
      // interpolate the frame to sync
      spRaysunFrame syncSubFrame =
          sp_predictor_->PropagateFrame(sp_cMatchedFrame, dt_ms);
      syncFrames->push_back(syncSubFrame);
    }
  }
}

spVehicleState Fuser::AssociateADVehicle(
    std::shared_ptr<vss::vehicle_info> sp_adVehicle) {
  spVehicleState associated_vehicle = nullptr;
  return nullptr;
}

int Fuser::FuseVehicleData(spRaysunFrame frame, const KITTI_RAW &vehicle) {
  return sp_fusion_->FuseVehicleData(frame, vehicle);
}

spRaysunFrame Fuser::FuseRaysunData(
    const std::vector<spRaysunFrame> &syncFrames) {
  std::vector<VehicleObs> fused_objects;
  std::vector<std::vector<VehicleObs>> fusion_result;
  auto const &cfg = g_fuserConfig->cacher_cfg();
  spRaysunFrame spFusedResult = std::make_shared<RaysunFrame>();
  for (const auto &frame : syncFrames) {
    sp_fusion_->CombineNewResource(frame->targets(), &fused_objects);
  }
  spFusedResult->AddTargets(fused_objects);
  // std::cout<<"fused result vehicle number:"<<
  // spFusedResult->TargetsNum()<<std::endl;
  return spFusedResult;
}

int Fuser::set_fusion(std::map<int, spVehicleState> vehicles,
                      fusion_package &outpkg) {
  // std::cout << "fusion result number " << vehicles.size() <<std::endl;
  timeval tv;
  gettimeofday(&tv, nullptr);
  uint64_t timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000;
  outpkg.set_timestamp(timestamp);
  for (const auto &veh : vehicles) {
    auto target = veh.second;
    auto *tg = outpkg.add_targets();
    set_fusion_target(target, *tg);
  }
  return 0;
}

int Fuser::set_fusion_target(spVehicleState &status, jos::target &tgt) {

  tgt.set_id((uint32_t)status->id());
  tgt.set_class_((uint32_t)status->obj_class());
  tgt.set_hdg(status->cog());
  tgt.set_lon(status->lon());
  tgt.set_lat(status->lat());
  tgt.set_speed(status->sog());
  tgt.set_length(status->length());
  tgt.set_width(status->width());
  tgt.set_height(status->height());
  return 0;
}

int Fuser::fuse_frame(junction_fusion_package &inpkg, fusion_service_pkg &outpkg) {
  if (inpkg.jun_pkg().radar_info_size() <= 0) {
    std::cout << "fusion packge no radar date" << std::endl;
    return -1;
  }
  uint64_t endtms = inpkg.fusion_endtime();
  if (endtms == 0) {
    std::cout << "fusion packge no radar date" << std::endl;
    return -2;
  }
  // std::cout << "fusion id " << inpkg.jun_pkg().radar_info(0).id() << " endtime" << endtms << std::endl;
  auto &jun_pkg = inpkg.jun_pkg();
  if (jun_pkg.radar_info_size() > 0) {
    for (int i = 0; i < jun_pkg.radar_info_size(); i++) {
      auto &rinfo = jun_pkg.radar_info(i);
      sp_cRaysunFrame frame = std::make_shared<RaysunFrame>(rinfo);
      ProcessOneFrame(frame);
      for (auto it = removed_vehicle_states_.begin();
        it != removed_vehicle_states_.end();it++) {
          auto tgt = outpkg.add_rm_item();
          set_fusion_target(it->second, *tgt);
      }
      removed_vehicle_states_.clear();
    }
  }

  auto *fs_tgt = outpkg.mutable_fus_pkg();
  set_fusion(vehicle_states_, *fs_tgt);

  auto *timeinfo = fs_tgt->mutable_timeinfo();
  auto mfinfo = jun_pkg.radar_info(jun_pkg.radar_info_size() - 1);
  *timeinfo = mfinfo.timeinfo();

	// timeval tv;
	// gettimeofday(&tv, nullptr);
  // timeinfo->set_erecv_timestamp_sec(tv.tv_sec);
  // timeinfo->set_erecv_timestamp_usec(tv.tv_usec);
  uint64_t tsr = timeinfo->erecv_timestamp_sec() * 1000 + timeinfo->erecv_timestamp_usec() /1000;

  uint64_t tsc = tsr - timeinfo->collection_timestamp_sec() / 1000;
  timeinfo->set_collection_timestamp_sec(tsc / 1000);
  timeinfo->set_collection_timestamp_usec((tsc % 1000) * 1000);
  
  uint64_t dtsm = timeinfo->timestamp_sec();
  if (dtsm > 100000) {
    dtsm = 25000;
  }
  uint64_t tst = tsc - dtsm / 1000;
  timeinfo->set_timestamp_sec(tst / 1000);
  timeinfo->set_timestamp_usec((tst % 1000) * 1000);

  uint64_t tsra = tst - 45;
  tsra += random() % 8;
  fs_tgt->set_timestamp(tsra);
  // printf("radar %d, transf %d, collect %d. er %ld\n", tst - tsra, tsc - tst, tsr-tsc, tsr);
  
  if (vehicle_states_.size() > 0 && vehicle_states_.begin()->second) {
    outpkg.set_timestamp(vehicle_states_.begin()->second->timestamp());
  } else {
    outpkg.set_timestamp(tst);
  }

  if (vehicle_states_.size() == 0) {
    return 0;
  }
  //   int veh_sz = inpkg.vehicle_states_size();
  //   int target_sz = outpkg.targets_size();
  //   for (int i = 0; i < veh_sz; i++) {
  //     auto veh_info = inpkg.vehicles(i);
  //     KITTI_RAW vehicle;
  //     vehicle.id = 100;
  //     vehicle.timestamp = veh_info.timestamp();
  //     vehicle.lon = veh_info.lon();
  //     vehicle.lat = veh_info.lat();
  //     vehicle.yaw = veh_info.heading();
  //     vehicle.speed = veh_info.speed();
  //     int matching_idx = FuseVehicleData(spFusedResult, vehicle);
  //     if (matching_idx < 0 || matching_idx >= target_sz) {
  //       continue;
  //     }
  //     auto *jf_target = outpkg.mutable_targets(matching_idx);
  //     jf_target->set_associate_vehicle_id(veh_info.id());
  //   }
  return 0;
}

}  // namespace v2x
}  // namespace coop
