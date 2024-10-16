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
#include <map>
#include "core/src/modules/common/util/inner_types.hpp"
#include "core/src/modules/dataType/raysunframe.hpp"
#include "core/src/modules/dataType/vehicle_state.h"
namespace coop {
namespace v2x {
class Predictor {
 public:
  Predictor() {}
  ~Predictor() {}
  /**
   * @brief propagate the vehicle states to current time with previous time and
   * position
   * @param current_time the given time until when the system is propagated
   */
  void PropagateVehicles(std::map<int, spVehicleState> &vehicle_states,std::map<int, spVehicleState> &removed_vehicle_states,
                         OCLong current_time);
  /**
   * @brief use the speed and orientation to predict the observation frame in
   * time dt
   *
   * @param ori_frame Original frame
   * @param dt Delta time
   * @return spRaysunFrame Propagated frame
   */
  spRaysunFrame PropagateFrame(sp_cRaysunFrame ori_frame, OCLong dt);

 private:
};
}  // namespace v2x
}  // namespace coop
