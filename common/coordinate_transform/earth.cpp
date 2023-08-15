#include "earth.hpp"
#include "units.hpp"

namespace civ {
namespace common {
namespace coord_transform {
Eigen::Vector3d Earth::_origin = Eigen::Vector3d::Zero();   // ECEF
Eigen::Matrix3d Earth::_cne = Eigen::Matrix3d::Identity();  //
bool Earth::_origin_setted = false;  // 是否设置过圆心

Eigen::Quaterniond a2qua(double pitch, double roll, double yaw) {
  pitch /= 2.0, roll /= 2.0, yaw /= 2.0;
  double sp = sin(pitch), sr = sin(roll), sy = sin(yaw), cp = cos(pitch),
         cr = cos(roll), cy = cos(yaw);
  Eigen::Quaterniond qnb;
  qnb.w() = cp * cr * cy - sp * sr * sy;
  qnb.x() = sp * cr * cy - cp * sr * sy;
  qnb.y() = cp * sr * cy + sp * cr * sy;
  qnb.z() = cp * cr * sy + sp * sr * cy;
  return qnb;
}

Eigen::Quaterniond a2qua(Eigen::Vector3d const &att) {
  return a2qua(att[0], att[1], att[2]);
}
Eigen::Quaterniond pra2qua(Eigen::Vector3d const &att) {
  return a2qua(att[0], att[1], azimuthToYaw(att[2]));
}

Eigen::Vector3d q2att(const Eigen::Quaterniond &qnb) {
  double q11 = qnb.w() * qnb.w(), q12 = qnb.w() * qnb.x(),
         q13 = qnb.w() * qnb.y(), q14 = qnb.w() * qnb.z(),
         q22 = qnb.x() * qnb.x(), q23 = qnb.x() * qnb.y(),
         q24 = qnb.x() * qnb.z(), q33 = qnb.y() * qnb.y(),
         q34 = qnb.y() * qnb.z(), q44 = qnb.z() * qnb.z();
  Eigen::Vector3d att;
  att(0) = asin(2 * (q34 + q12));                           // pit
  att(1) = atan2(-2 * (q24 - q13), q11 - q22 - q33 + q44);  // rol
  att(2) = atan2(-2 * (q23 - q14), q11 - q22 + q33 - q44);  // yaw
  att(2) =
      (fabs(att(2) - 360.0_deg) <= 0.005_deg ? att(2) - 360.0_deg : att(2));
  return att;
}
Eigen::Vector3d q2pra(const Eigen::Quaterniond &qnb) {
  Eigen::Vector3d att = q2att(qnb);
  att[2] = yawToAzimuth(att[2]);
  return att;
}

Eigen::Matrix3d askew(Eigen::Vector3d const &v) {
  Eigen::Matrix3d result;
  result << 0, -v(2), v(1), v(2), 0.0, -v(0), -v(1), v(0), 0;
  return result;
}

Eigen::Quaterniond rv2q(const Eigen::Vector3d &rv) {
#define F1 (2 * 1)  // define: Fk=2^k*k!
#define F2 (F1 * 2 * 2)
#define F3 (F2 * 2 * 3)
#define F4 (F3 * 2 * 4)
#define F5 (F3 * 2 * 5)
  // float n2 = rv.i*rv.i + rv.j*rv.j + rv.k*rv.k, c, f;
  double n2 = rv.squaredNorm(), c, f;

  if (n2 < (M_PI / 180.0 * M_PI / 180.0))  // 0.017^2
  {
    double n4 = n2 * n2;
    c = 1.0 - n2 * (1.0 / F2) + n4 * (1.0 / F4);
    f = 0.5 - n2 * (1.0 / F3) + n4 * (1.0 / F5);
  } else {
    double n_2 = sqrt(n2) / 2.0;
    c = cos(n_2);
    f = sin(n_2) / n_2 * 0.5;
  }
  return Eigen::Quaterniond(c, f * rv(0), f * rv(1), f * rv(2));
}

Eigen::Vector3d q2rv(const Eigen::Quaterniond &q) {
  Eigen::AngleAxisd angle_axis(q);

  return angle_axis.axis() * angle_axis.angle();
}

}  // namespace coord_transform
}  // namespace civmap
}  // namespace civ
