#pragma once

#include "../common/math/vec2d.h"
#include "modules/dataType/vehicle_obs.hpp"
namespace coop {
namespace v2x {
float LocationDistance(const VehicleObs &obj0, const VehicleObs &obj1);
}
}  // namespace coop
