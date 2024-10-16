#include "distance_collection.h"
#include <eigen3/Eigen/Core>
namespace coop {
namespace v2x {
float LocationDistance(const VehicleObs &obj0, const VehicleObs &obj1) {
  Eigen::Vector3f obj0_anchor_point = (obj0.pos_enu()).cast<float>();
  Eigen::Vector3f obj1_anchor_point = (obj1.pos_enu()).cast<float>();

  Eigen::Vector3f ojb0_obj1_diff = obj1_anchor_point - obj0_anchor_point;
  float location_dist = static_cast<float>(sqrt(
      (ojb0_obj1_diff.head(2).cwiseProduct(ojb0_obj1_diff.head(2))).sum()));
  return location_dist;
}
}  // namespace v2x
}  // namespace coop
