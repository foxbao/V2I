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
#include "modules/dataType/vehicle_obs.hpp"
#include "modules/common/earth.h"
#include "modules/common/math/vec2d.h"
#include "proto/junction_package.pb.h"
#include "proto/fusion_package.pb.h"

namespace coop {
namespace v2x {

using namespace jos;

VehicleObs::VehicleObs() { type_ = VehicleType::Obs; }

VehicleObs::VehicleObs(const RaysunTarget &target) {
  VehicleObs();
  SetVehicleObsData(target);
}

VehicleObs::VehicleObs(const radar_target *target) {
  VehicleObs();
  id_ = target->id();           // ID
  obj_class_ = target->type();  // object class
  lla_(1) = target->lon();  // 经度, 原始坐标为雷达坐标系,障碍物中心点 XY WGS84
                            /// CGCS2000 坐标系, (物体中心点经纬度)
  lla_(0) = target->lat();  // 纬度, 原始坐标为雷达坐标系,障碍物中心点 XY WGS84
                            /// CGCS2000 坐标系, (物体中心点经纬度)
  lla_(2) = 0;
  obj_length_ = target->length();  // 长度, 单位 m
  obj_width_ = target->width();    // 宽度, 单位 m
  obj_height_ = target->height();  // 高度, 单位 m
  sog_ = target->speed();          // 速度, 单位 km/h
  cog_ = target->heading();  // 航向角, 单位 度, WGS84/CGCS2000 坐标系时的航向角
  dist_ = target->dist();        // 相对距离,单位 m
  bearing_ = target->bearing();  // 相对角度,单位 度
  objpos_ =
      target
          ->pos();  // 目标所处区域,
                    // 障碍物为机动车或非机动车时,判断所处第几车道,以雷达观测区域从左到右依次
                    // 为 1 –9 车道;
                    // 目标为行人时,判断是否在人行道上,是为 10,否为 0
  stable_ = target->stable();  // 稳定度
}

VehicleObs::VehicleObs(const target *target) {
  VehicleObs();
  id_ = target->id();           // ID
  obj_class_ = target->class_();  // object class
  lla_(1) = target->lon();  // 经度, 原始坐标为雷达坐标系,障碍物中心点 XY WGS84
                            /// CGCS2000 坐标系, (物体中心点经纬度)
  lla_(0) = target->lat();  // 纬度, 原始坐标为雷达坐标系,障碍物中心点 XY WGS84
                            /// CGCS2000 坐标系, (物体中心点经纬度)
  lla_(2) = 0;
  obj_length_ = target->length();  // 长度, 单位 m
  obj_width_ = target->width();    // 宽度, 单位 m
  obj_height_ = target->height();  // 高度, 单位 m
  sog_ = target->speed();          // 速度, 单位 km/h
  cog_ = target->hdg();  // 航向角, 单位 度, WGS84/CGCS2000 坐标系时的航向角
}

void VehicleObs::SetVehicleObsData(const RaysunTarget &target) {
  id_ = target.id;                // ID
  obj_class_ = target.obj_class;  // object class

  lla_(0) = target.lat;  // 纬度, 原始坐标为雷达坐标系,障碍物中心点 XY WGS84
                         // CGCS2000 坐标系, (物体中心点经纬度)
  lla_(1) = target.lon;  // 经度, 原始坐标为雷达坐标系,障碍物中心点 XY WGS84
  lla_(2) = 0;
  // CGCS2000 坐标系, (物体中心点经纬度)
  obj_length_ = target.obj_length;  // 长度, 单位 m
  obj_width_ = target.obj_width;    // 宽度, 单位 m
  obj_height_ = target.obj_height;  // 高度, 单位 m
  sog_ = target.sog;                // 速度, 单位 km/h
  cog_ = target.cog;  // 航向角, 单位 度, WGS84/CGCS2000 坐标系时的航向角 from
                      //  north, counterclockwise
  dist_ = target.dist;        // 相对距离,单位 m
  bearing_ = target.bearing;  // 相对角度,单位 度
  objpos_ =
      target
          .objpos;  // 目标所处区域,
                    // 障碍物为机动车或非机动车时,判断所处第几车道,以雷达观测区域从左到右依次
                    // 为 1 –9 车道;
                    // 目标为行人时,判断是否在人行道上,是为 10,否为 0
  stable_ = target.stable;  // 稳定度
}

void VehicleObs::SetTrajectoryData(const std::vector<Eigen::Vector3d> &traj) {
  trajectory_enu_ = traj;
}

void VehicleObs::GetCorners(const double *pos_ori_deg,
                            std::vector<common::math::Vec2d> *corners) {
  Eigen::Vector3d center_enu = pos_enu();
  common::math::Vec2d center_enu_2d(center_enu[0], center_enu[1]);
  sp_box_ = std::make_shared<common::math::Box2d>(
      center_enu_2d, (-cog_ + 90) * DEG2RAD, obj_length_, obj_width_);
  sp_box_->InitCorners();
  sp_box_->GetAllCorners(corners);
}

void VehicleObs::set_llh(OCDouble lat, OCDouble lon, OCDouble height) {
  lla_(0) = lat;
  lla_(1) = lon;
  lla_(2) = height;
}

VehicleObs VehicleObs::Clone() {
  VehicleObs clone_object = *this;
  return clone_object;
}

void VehicleObs::PropagateVehicle(OCLong current_time) {
  OCLong dt_ms = current_time - timestamp_;
  Eigen::Vector3d pos_enu_current = pos_enu() + G_vel_ * dt_ms / 1000.0;
  lla_ = Earth::ENU2LLH(pos_enu_current, true);
  if (lla_(1) == 0) {
    int aaa = 1;
  }
  timestamp_ = current_time;
}

}  // namespace v2x
}  // namespace coop
