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

#include "modules/vehicle/vehicle_factory.h"
#include <memory>
namespace coop {
namespace v2x {

VehicleFactory::VehicleFactory() {}

std::shared_ptr<vehicle_info> VehicleFactory::CreateVehicleData(
    sp_cVehicleState vehicle_state) {
  vehicle_info vehicle;
  vehicle.set_id(std::to_string(vehicle_state->id()));
  vehicle.set_lat(vehicle_state->lat());
  vehicle.set_lon(vehicle_state->lon());
  vehicle.set_timestamp(vehicle_state->timestamp());
  vehicle.set_heading(vehicle_state->HeadingENU());
  vehicle.set_speed(kmph2mps(vehicle_state->cog()));
  return std::make_shared<vehicle_info>(vehicle);
}
}  // namespace v2x
}  // namespace coop
