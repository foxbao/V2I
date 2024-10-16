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
#include <eigen3/Eigen/Core>
#include <map>
#include <memory>
#include <vector>
#include "modules/common/util/inner_types.hpp"
#include "modules/dataType/vehicle_base.hpp"
#include "modules/dataType/vehicle_obs.hpp"
#include "modules/filter/kalman_filter.h"
namespace coop {
namespace v2x {

class VehicleState : public VehicleBase {
 public:
  VehicleState();
  ~VehicleState() {}
  void Init(const VehicleObs& obs);
  double HeadingENU() const { return -cog_ + 90; }  // degree
  double ttl() { return ttl_; }
  void set_id(int id) { id_ = id; }
  void set_timestamp(OCLong timestamp) { timestamp_ = timestamp; }
  void set_velocity(float velocity) { sog_ = velocity * 3.6; }
  // void PropagateVehicle(OCLong current_time);
  void PropagateVehicle2(OCLong current_time);
 public:
  // void Update(const VehicleObs& target);
  void Update2(const VehicleObs& target);
  std::map<OCLong, OCInt> obs_dev_target_id;  // map<camera_id, vehicle_id>

 protected:
  std::shared_ptr<ExtendedKalmanFilter> spFilter_;
  double speed_scale_=0.0;     // m/s

  double ttl_;  // time to live,
  const double ttl_limit_ = 1.0;
  bool inited_;
  // Covariance.
  // Eigen::Matrix<double, 15, 15> cov;
};
DEFINE_EXTEND_TYPE(VehicleState);

}  // namespace v2x
}  // namespace coop
