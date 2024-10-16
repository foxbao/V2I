#pragma once
#include <vector>
#include "modules/dataType/vehicle_obs.hpp"

namespace coop {
namespace v2x {
class Association {
 public:
  Association() = default;
  Eigen::MatrixXf CalCostMatrix(const std::vector<VehicleObs> &targets_cam0,
                                const std::vector<VehicleObs> &targets_cam1);

 private:
};
}  // namespace v2x
}  // namespace coop
