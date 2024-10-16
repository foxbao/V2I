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
#include "src/modules/dataType/vehicle_state.h"
#include "modules/common/earth.h"
#include "src/modules/common/util/util.h"
namespace coop {
namespace v2x {
VehicleState::VehicleState() {
  ttl_ = ttl_limit_;
  lla_(0) = 0;
  lla_(1) = 0;
  lla_(2) = 0;
  spFilter_ = std::make_shared<ExtendedKalmanFilter>();
  inited_ = false;
  type_ = VehicleType::State;
}

void VehicleState::Init(const VehicleObs& obs) {
  lla_ = Eigen::Vector3d{obs.lat(), obs.lon(), 0};

  Eigen::Vector3d pos_enu = Earth::LLH2ENU(lla_, true);
  obj_class_ = obs.obj_class();
  cog_ = obs.cog();
  speed_scale_ = kmph2mps(obs.sog());
  // Eigen::Vector3d filter_init_state{pos_enu(0), pos_enu(1),
  // DEG2RAD*(-obs.cog()+90)};
  Eigen::Vector4d filter_init_state{pos_enu(0), pos_enu(1), speed_scale_,
                                    DEG2RAD * (-obs.cog() + 90)};
  spFilter_->Init(filter_init_state);

  obj_length_ = obs.length();
  obj_height_ = obs.height();
  obj_width_ = obs.width();
  sog_ = obs.sog();
  G_vel_ = Eigen::Vector3d{cos(HeadingENU() * DEG2RAD) * speed_scale_,
                           sin(HeadingENU() * DEG2RAD) * speed_scale_, 0};
  ttl_ = ttl_limit_;
  inited_ = true;
}

void VehicleState::PropagateVehicle2(OCLong current_time) {
  OCLong dt_ms = current_time - timestamp_;
  spFilter_->Predict(dt_ms / 1000.0);
  Eigen::Vector4d state_predicted = spFilter_->get_state();

  Eigen::Vector3d state_predicted_enu{state_predicted(0), state_predicted(1),
                                      0};
  lla_ = Earth::ENU2LLH(state_predicted_enu, true);
  timestamp_ = current_time;
  ttl_ -= dt_ms / 1000.0;
}

void VehicleState::Update2(const VehicleObs& obs) {
  ttl_ = ttl_limit_;
  // Eigen::Vector3d filter_obs{obs.pos_enu()(0), obs.pos_enu()(1),
  // DEG2RAD*(90-obs.cog())}; spFilter_->Correct(filter_obs);
  Eigen::Vector4d filter_obs{obs.pos_enu()(0), obs.pos_enu()(1),
                             obs.velocity_scale(), DEG2RAD * (90 - obs.cog())};
  spFilter_->Correct(filter_obs);

  Eigen::Vector4d filter_result = spFilter_->get_state();
  Eigen::Vector3d state_updated_enu{filter_result(0), filter_result(1), 0};
  lla_ = Earth::ENU2LLH(state_updated_enu, true);
  speed_scale_ = filter_result(2);
  cog_ = 90 - RAD2DEG * (filter_result(3));
  set_velocity(speed_scale_);
  obj_length_ = obs.length();
  obj_height_ = obs.height();
  obj_width_ = obs.width();
  G_vel_ = Eigen::Vector3d{cos(HeadingENU() * DEG2RAD) * speed_scale_,
                           sin(HeadingENU() * DEG2RAD) * speed_scale_, 0};
  AddTrajectoryPoint(state_updated_enu);
}

}  // namespace v2x
}  // namespace coop
