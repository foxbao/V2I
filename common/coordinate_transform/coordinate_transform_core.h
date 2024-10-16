#pragma once

// #include "common/cyber_interface/logger/log_interface.h"
#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/Geometry>
#include "gnss_third_party/rtklib.h"
#include "util/util.h"
namespace civ {
namespace common {
namespace coord_transform {

int aaa=civ::common::util::g_ori_pos_deg[0];
Eigen::Vector3d const kEnuOriginLLH = {civ::common::util::g_ori_pos_deg[0] * D2R, civ::common::util::g_ori_pos_deg[1] * D2R,
                                       16.504};
Eigen::Vector3d get_enu_pos(Eigen::Vector3d const& p_llh);  // 经纬度转东北天
Eigen::Vector3d get_llh_pos(Eigen::Vector3d const& p_enu);  // 东北天转经纬度
Eigen::Vector3d LLH2XYZ(Eigen::Vector3d const& p_llh);      // 经纬高转ecef
Eigen::Vector3d XYZ2LLh(Eigen::Vector3d const& p_xyz);      // ecef转经纬高
Eigen::Matrix3d Pos2Cne(Eigen::Vector3d const& p_llh);

template <typename _Scalar>
inline Eigen::Matrix<_Scalar, 3, 1> ConvertVectorInA(
    Eigen::Quaterniond const& q_a_b, Eigen::Vector3d const& t_a_b,
    Eigen::Matrix<_Scalar, 3, 1> const& l_b) {
  using namespace Eigen;
  Isometry3d T_a_b = Isometry3d::Identity();
  T_a_b.linear() = q_a_b.toRotationMatrix();
  T_a_b.translation() = t_a_b;
  return T_a_b.cast<_Scalar>() * l_b;
}

template <typename _Scalar>
inline Eigen::Matrix<_Scalar, 3, 1> ConvertVectorInB(
    Eigen::Quaterniond const& q_a_b, Eigen::Vector3d const& t_a_b,
    Eigen::Matrix<_Scalar, 3, 1> const& l_a) {
  using namespace Eigen;
  Isometry3d T_b_a = Isometry3d::Identity();
  T_b_a.linear() = q_a_b.toRotationMatrix().transpose();
  T_b_a.translation() = -t_a_b;
  return T_b_a.cast<_Scalar>() * l_a;
}

// 时间转换, unit second [seconds, week]
double UTC2GPST(double const utc, int* week = nullptr);
double GPST2UTC(int week, double sec_s);
}  // namespace coord_transform

}  // namespace common

}  // namespace civ
