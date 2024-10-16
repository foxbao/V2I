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
#include "vehicle_id.h"

namespace coop {
namespace v2x {
VehicleID* VehicleID::vehicle_id_ = nullptr;
VehicleID* VehicleID::getInstance() {
  if (vehicle_id_ == nullptr) {
    vehicle_id_ = new VehicleID();
  }
  return vehicle_id_;
}

VehicleID::VehicleID(/* args */) { id_value_ = -1; }

VehicleID::~VehicleID() {}

int VehicleID::get_new_id() {
  id_value_ = id_value_ + 1;
  return id_value_;
}
}  // namespace v2x
}  // namespace coop
