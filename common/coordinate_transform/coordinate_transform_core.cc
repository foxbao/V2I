#include "coordinate_transform_core.h"
#include <eigen3/Eigen/Geometry>

namespace civ {
namespace common {
namespace coord_transform {

using namespace Eigen;

static Vector3d kEnuOriginXYZ = {0, 0, 0};

Vector3d LLH2XYZ(Vector3d const& p_llh) {
  Vector3d pos_xyz = {0, 0, 0};
  pos2ecef(p_llh.data(), pos_xyz.data());
  return pos_xyz;
}

Matrix3d Pos2Cne(Vector3d const& p_llh) {
  return (AngleAxisd(-(M_PI_2 - p_llh[0]), Vector3d::UnitX()) * AngleAxisd(-(M_PI_2 + p_llh[1]), Vector3d::UnitZ())).toRotationMatrix();
}

Vector3d get_enu_pos(Vector3d const& p_llh) {
  // 计算ecef 中心点
  if (kEnuOriginXYZ.norm() == 0) {
    pos2ecef(kEnuOriginLLH.data(), kEnuOriginXYZ.data());
  }
  // 计算ecef向量
  Vector3d p_xyz = LLH2XYZ(p_llh);
  // 旋转至圆心点enu
  Vector3d p_enu = Pos2Cne(kEnuOriginLLH) * (p_xyz - kEnuOriginXYZ);

  return p_enu;
}

Eigen::Vector3d get_llh_pos(Eigen::Vector3d const& p_enu) {
  // 计算ecef 中心点
  if (kEnuOriginXYZ.norm() == 0) {
    pos2ecef(kEnuOriginLLH.data(), kEnuOriginXYZ.data());
  }
  // 转换至ecef
  Vector3d p_xyz = kEnuOriginXYZ + Pos2Cne(kEnuOriginLLH).transpose() * p_enu;
  // 转换至llh
  Vector3d p_llh = XYZ2LLh(p_xyz);

  return p_llh;
}

Eigen::Vector3d XYZ2LLh(Eigen::Vector3d const& p_xyz) {
  Vector3d pos_llh = {0, 0, 0};
  ecef2pos(p_xyz.data(), pos_llh.data());
  return pos_llh;
}

double UTC2GPST(double const utc, int* week) {
  gtime_t time{static_cast<time_t>(utc), utc - static_cast<time_t>(utc)};
  return time2gpst(utc2gpst(time), week);
}

double GPST2UTC(int week, double sec_s) {
  gtime_t utc_time = gpst2utc(gpst2time(week, sec_s));
  return utc_time.time + utc_time.sec;
}
}  // namespace coord_transform

}  // namespace common

}  // namespace civ
