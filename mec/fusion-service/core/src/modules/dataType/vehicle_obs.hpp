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
#include <memory>
#include <string>
#include <vector>
#include "modules/common/math/box2d.h"
#include "modules/common/math/triangle2d.hpp"
#include "modules/common/math/vec2d.h"
#include "modules/common/util/util.h"
#include "modules/dataType/vehicle_base.hpp"
// #include "proto/config.pb.h"
// #include "proto/junction_package.pb.h"
namespace jos {
class radar_target;
class target;
};
namespace coop {
namespace v2x {
using Line = std::vector<common::math::Vec2d>;

class VehicleObs : public VehicleBase {
 public:
  VehicleObs();
  explicit VehicleObs(const RaysunTarget &target);
  explicit VehicleObs(const jos::radar_target *target);
  explicit VehicleObs(const jos::target *target);
  void SetVehicleObsData(const RaysunTarget &target);
  void SetTrajectoryData(const std::vector<Eigen::Vector3d> &traj);
  void GetCorners(const double *pos_ori_deg,
                  std::vector<common::math::Vec2d> *corners);
  bool is_fused() const { return is_fused_; }
  bool set_fused() { is_fused_ = true; }
  void set_llh(OCDouble lat, OCDouble lon, OCDouble height = 0.0);
  OCLong devid(){return devid_;}
  void set_devid(OCLong devid){devid_=devid;}
  VehicleObs Clone();
  void PropagateVehicle(OCLong current_time);
  std::shared_ptr<common::math::Box2d> sp_box_;

 protected:
  OCFloat dist_;     // 相对距离,单位 m
  OCFloat bearing_;  // 相对角度,单位 度
  OCByte
      objpos_;  // 目标所处区域,
                // 障碍物为机动车或非机动车时,判断所处第几车道,以雷达观测区域从左到右依次
                // 为 1 –9 车道;
                // 目标为行人时,判断是否在人行道上,是为 10,否为 0
  OCLong devid_;
  OCByte stable_;  // 稳定度
  bool is_fused_ = false;
};

DEFINE_EXTEND_TYPE(VehicleObs);

}  // namespace v2x
}  // namespace coop
