#include "modules/dataType/vehicle_base.hpp"
#include <memory>
#include <vector>
#include "modules/common/math/triangle2d.hpp"
#include "modules/common/util/util.h"

namespace coop {
namespace v2x {

std::vector<common::math::Vec2d> VehicleBase::GetTriangleContourENU() const
{
  Eigen::Vector3d center_enu = pos_enu();
  common::math::Vec2d center_enu_2d(center_enu[0], center_enu[1]);
  std::shared_ptr<common::math::Triangle2d> sp_triangle2d =
      std::make_shared<common::math::Triangle2d>(center_enu_2d, (cog_)*DEG2RAD,
                                                 obj_length_, obj_width_);
  return sp_triangle2d->GetAllCorners();
}

Line VehicleBase::GetVelocityArrowENU() const {
  Eigen::AngleAxisd rotation_vector_z(M_PI / 2, Eigen::Vector3d(0, 0, 1));
  Eigen::AngleAxisd rotation_vector_x(-M_PI, Eigen::Vector3d(1, 0, 0));
  Eigen::Matrix3d rotation_matrix_ENU_NED =
      rotation_vector_z.matrix() * rotation_vector_x.matrix();

  double heading_NED = cog() * DEG2RAD;
  // convert from body from to NED
  Eigen::AngleAxisd yawAngle(heading_NED, Eigen::Vector3d::UnitZ());
  Eigen::Matrix3d rotation_matrix_ned_b;
  rotation_matrix_ned_b = yawAngle.matrix();
  double arrow_length = velocity_scale();

  Eigen::Vector3d velocity_start_body{0, 0, 0};
  Eigen::Vector3d velocity_end_body{arrow_length, 0, 0};

  Eigen::Vector3d velocity_start_ENU =
      rotation_matrix_ENU_NED * rotation_matrix_ned_b * velocity_start_body +
      Eigen::Vector3d{pos_enu()(0), pos_enu()(1), 0};

  Eigen::Vector3d velocity_end_ENU =
      rotation_matrix_ENU_NED * rotation_matrix_ned_b * velocity_end_body +
      Eigen::Vector3d{pos_enu()(0), pos_enu()(1), 0};
  Line Velocity_line;
  Velocity_line.emplace_back(velocity_start_ENU(0), velocity_start_ENU(1));
  Velocity_line.emplace_back(velocity_end_ENU(0), velocity_end_ENU(1));
  return Velocity_line;
}

void VehicleBase::PropagateVehicle(OCLong current_time) {
  OCLong dt_ms = current_time - timestamp_;
  Eigen::Vector3d pos_enu_current = pos_enu() + G_vel_ * dt_ms / 1000.0;
  lla_ = Earth::ENU2LLH(pos_enu_current, true);
  timestamp_ = current_time;
}

void VehicleBase::AddTrajectoryPoint(const Eigen::Vector3d& pt_enu)
{
  trajectory_enu_.push_back(pt_enu);
}

}  // namespace v2x
}  // namespace coop
