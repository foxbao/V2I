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
#pragma once
#include <deque>
#include <future>
#include <map>
#include <memory>
#include <vector>
#include "modules/common/earth.h"
#include "modules/dataType/vehicle_state.h"
#include "modules/fusion/fusion.h"
#include "modules/imgprocessor/imgprocessor.h"
#include "modules/message_cache/cacher.hpp"
#include "modules/predictor/predictor.h"
#include "modules/preprocessor/preprocessor.h"
#include "modules/vehicle/vehicle_factory.h"
// #include "modules/viz/viz_processor.hpp"
#include "proto/config.pb.h"
#include "proto/fusion_service_pkg.pb.h"
#include "proto/junction_fusion_package.pb.h"
#include "proto/junction_package.pb.h"
#include "proto/vehicle_snapshot.pb.h"
namespace coop {
namespace v2x {

class Fuser {
 public:
  Fuser();
  ~Fuser();
  void Fuse();
  int fuse_frame(jos::junction_fusion_package &inpkg,
                 jos::fusion_service_pkg &outpkg);

 private:
  /**
   * @brief Add the a new vehicle state using the observation vehicle data. Used
   * in case that observation result cannot be tracked nor associated
   * @param frame Frame information as timestamp and device id will be added
   * @param target Observation to be added into vehicle states
   * @return
   */
  bool AddVehicleState(std::map<int, spVehicleState> &vehicles,
                       sp_cRaysunFrame frame, const VehicleObs &target);

  /**
   * @brief Fuse the frames from different radar equipment in the same time
   * interval into one frame, with depleting repetation
   * @param syncFrames
   * @return spRaysunFrame
   */
  spRaysunFrame FuseRaysunData(const std::vector<spRaysunFrame> &syncFrames);

  /**
   * @brief Decide if the observed vehicle can be physically associated with a
   * vehicle state
   * @param dev_id Id of observation camera
   * @param target_obs Observed vehicle to be associated
   * @param vehicle_state_id output, Id of vehicle state that is associated with
   * observed vehicle
   * @return If the given vehicle can be associated
   */
  bool isAssociated(const OCLong dev_id, const VehicleObs &target_obs,
                    std::map<int, coop::v2x::spVehicleState> &vehicle_states,
                    int *vehicle_state_id);

  /**
   * @brief Decide if the observed vehicle id can be tracked with a vehicle
   * state with id
   * @param dev_id Id of observation camera
   * @param target_id Observed vehicle id
   * @param vehicle_states
   * @param vehicle_state_id output, Id of vehicle state that is associated with
   * observed vehicle
   * @return if the given vehicle can be tracked
   */
  bool isTracked(const OCLong dev_id, const OCInt target_id,
                 std::map<int, coop::v2x::spVehicleState> &vehicle_states,
                 int *vehicle_state_id);

  /**
   * @brief propagate the vehicle states to current time with previous time and
   * position
   * @param vehicle_states
   * @param current_time the given time until when the system is propagated
   */
  void PropagateVehicles(std::map<int, spVehicleState> &vehicle_states,
                         OCLong current_time);
  /**
   * @brief Read in offline data .record
   */
  void ReadData();

  /**
   * @brief Start process the offline data readin
   */
  void ProcessData();

  /**
   * @brief process one observation frame to update the vehicles
   * @param frame observation frame from one camera
   * @param frame
   */
  void ProcessOneFrame(sp_cRaysunFrame frame, bool plot = false);

  void UpdateVehicleObsTraj(
      const std::shared_ptr<std::vector<spRaysunFrame>> syncFrames);
  /**
   * @brief Update the vehicle states with the synced observation frames
   * @param vehicles vehicle states
   * @param syncFrames synced latest observation frames from various cameras
   */
  void UpdateVehicleStates(std::map<int, spVehicleState> &vehicles,
                           const std::vector<spRaysunFrame> syncFrames);

  /**
   * @brief given frame t from camera k, get frames from other frames and sync
   * time
   * @param frame frame whose time will be used to sync other frames
   * @param syncFrames output value: the synced frames from different cameras
   */
  void GetSyncFrames(sp_cRaysunFrame frame,
                     std::shared_ptr<std::vector<spRaysunFrame>> syncFrames);

  /**
   * @brief visualize the fused vehicle states and observation frames in image
   * @param vehicles
   * @param syncFrames frame whose time will be used to sync other frames
   * @return
   */
  void PlotVehicleStatesAndFrames(std::map<int, spVehicleState> vehicles,
                                  std::vector<spRaysunFrame> syncFrames,std::map<int, spVehicleObs> vehicle_obsers,
                                  OCLong current_time, OCLong last_time);
  int FuseVehicleData(spRaysunFrame frame, const KITTI_RAW &vehicle);

  /**
   * @brief Associate
   * @param sp_adVehicle
   * @return Id of vehicle stat that can be fused with the AD vehicle
   */
  spVehicleState AssociateADVehicle(
      std::shared_ptr<vss::vehicle_info> sp_adVehicle);
  void viz_process();

 private:
  int set_fusion(std::map<int, spVehicleState>, jos::fusion_package &outpkg);
  int set_fusion_target(spVehicleState &status, jos::target &tgt);

 private:
  std::shared_ptr<IMGPROCESSOR> sp_img_processor_;
  std::shared_ptr<Predictor> sp_predictor_;
  // std::shared_ptr<VizProcessor> sp_viz_processor_;
  spFusion sp_fusion_;
  spVehicleFactory sp_vehicle_factory_;
  std::shared_ptr<RaysunSystemConfig> raysunSystem_config_ = nullptr;

  OCLong last_time_ = 0;
  OCLong current_time_ = 0;
  std::map<int, spVehicleState> vehicle_states_;
  std::map<int, spVehicleState> removed_vehicle_states_;
  std::map<int, spVehicleObs> vehicle_obsers_;
  std::vector<sp_cRaysunFrame> data_buffer_;
  std::shared_ptr<Cacher> g_cacher_;
  int idx=0;
};
DEFINE_EXTEND_TYPE(Fuser);
}  // namespace v2x
}  // namespace coop
