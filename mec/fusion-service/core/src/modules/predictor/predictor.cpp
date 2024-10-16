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
#include "core/src/modules/predictor/predictor.h"
#include <memory>
#include "modules/common/earth.h"
namespace coop {
namespace v2x {
spRaysunFrame Predictor::PropagateFrame(sp_cRaysunFrame ori_frame,
                                        OCLong dt_ms) {
  using Eigen::AngleAxisd;
  using Eigen::Isometry3d;
  using Eigen::Matrix3d;
  using Eigen::Vector3d;
  spRaysunFrame syncSubFrame = std::make_shared<RaysunFrame>();
  syncSubFrame->set_head(ori_frame->head());
  syncSubFrame->set_timestamp(ori_frame->head().timestamp + dt_ms);
  for (auto target : ori_frame->targets()) {

    target.Clone();
    
    OCFloat speed_mps = target.sog() * KPH2MPS;
    OCFloat yaw_rad = (-target.cog() + 90) * DEG2RAD;
    Vector3d center_enu = target.pos_enu();
    Vector3d t1(center_enu[0], center_enu[1], center_enu[2]);
    // use speed and dt to calculate the movement along y-axis
    Vector3d longitudinal_move(speed_mps * dt_ms / 1000, 0, 0);

    AngleAxisd rotation_vector(yaw_rad, Vector3d::UnitZ());
    Matrix3d rotate_mat = Matrix3d::Identity();
    rotate_mat = rotation_vector.toRotationMatrix();
    Isometry3d T1 = Isometry3d::Identity();
    T1.rotate(rotate_mat);
    // T1.pretranslate(t1);
    Vector3d translation = T1 * longitudinal_move;
    Vector3d center_enu_propagated = t1 + translation;

    Vector3d center_llh_propagated =
        Earth::ENU2LLH(center_enu_propagated, true);
    if(center_llh_propagated(1)==0)
    {
      int dddd=2;
    }
    VehicleObs propagated_target;
    propagated_target = target;
    propagated_target.set_llh(center_llh_propagated[0],
                              center_llh_propagated[1]);

    syncSubFrame->AddTarget(propagated_target);
  }
  return syncSubFrame;
}

void Predictor::PropagateVehicles(std::map<int, spVehicleState> &vehicle_states,std::map<int, spVehicleState> &removed_vehicle_states,
                                  OCLong current_time) {
  // remove the vehicle state if time to live is <0
  // otherwise, use the speed to propagate to current time
  for (auto it = vehicle_states.begin(); it != vehicle_states.end();) {
    if (it->second->ttl() < 0) {
      removed_vehicle_states[it->first]=it->second;
      it = vehicle_states.erase(it);
    } else {
      it->second->PropagateVehicle2(current_time);
      it++;
    }
  }
}
}  // namespace v2x
}  // namespace coop
