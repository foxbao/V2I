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

#include "modules/fusion/association.h"
#include "modules/fusion/distance_collection.h"
namespace coop {
namespace v2x {
Eigen::MatrixXf Association::CalCostMatrix(
    const std::vector<VehicleObs> &targets_cam0,
    const std::vector<VehicleObs> &targets_cam1) {
  Eigen::MatrixXf association_mat(targets_cam0.size(), targets_cam1.size());
  double distance;
  for (int i = 0; i < targets_cam0.size(); i++) {
    for (int j = 0; j < targets_cam1.size(); j++) {
      distance = LocationDistance(targets_cam0[i], targets_cam1[j]);
      association_mat(i, j) = distance;
    }
  }
  return association_mat;
}
}  // namespace v2x
}  // namespace coop
