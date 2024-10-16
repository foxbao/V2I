
#pragma once
#include <vector>
#include "modules/common/earth.h"
#include "modules/common/math/vec2d.h"
#include "modules/common/util/inner_types.hpp"
namespace coop {
namespace v2x {
using Line = std::vector<common::math::Vec2d>;

enum class VehicleType { None = 0, State = 1, Obs = 2 };
class VehicleBase {
 public:
  OCInt id() const { return id_; }
  OCFloat length() const { return obj_length_; }
  OCFloat width() const { return obj_width_; }
  OCFloat height() const { return obj_height_; }
  OCByte obj_class() const { return obj_class_; }
  // km/h
  OCFloat sog() const { return sog_; }
  OCFloat cog() const {
    return cog_;
  }  //航向角, 单位 度, WGS84/CGCS2000 坐标系时的航向角,from north,
  OCDouble lat() const { return lla_(0); }
  OCDouble lon() const { return lla_(1); }
  VehicleType type() { return type_; }
  OCLong timestamp() const { return timestamp_; }
  OCFloat velocity_scale() const { return sog_ / 3.6; }
  Eigen::Vector3d pos_enu() const { return Earth::LLH2ENU(lla_, true); }
  std::vector<common::math::Vec2d> GetTriangleContourENU() const;
  std::vector<Eigen::Vector3d> trajectory_enu() const {
    return trajectory_enu_;
  }
  Line GetVelocityArrowENU() const;
  virtual void PropagateVehicle(OCLong current_time);
  void AddTrajectoryPoint(const Eigen::Vector3d& pt_enu);

 protected:
  OCInt id_;
  OCLong timestamp_;
  OCByte obj_class_;    // object class
  OCFloat obj_length_;  // 长度, 单位 m
  OCFloat obj_width_;   // 宽度, 单位 m
  OCFloat obj_height_;  // 高度, 单位 m
  OCFloat sog_;         // 速度, 单位 km/h
  OCFloat cog_;  // 航向角, 单位 度, WGS84/CGCS2000 坐标系时的航向角,from north,
  Eigen::Vector3d lla_;  // WGS84 position 纬度，经度，高度 [deg deg m]
  Eigen::Vector3d G_vel_{0, 0, 0};  // Speed in global frame
  std::vector<Eigen::Vector3d> trajectory_enu_;
  VehicleType type_;
};
DEFINE_EXTEND_TYPE(VehicleBase);
}  // namespace v2x
}  // namespace coop
