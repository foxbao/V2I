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
#include <vector>
#include "modules/dataType/vehicle_obs.hpp"
#include "modules/dataType/raysunframe.hpp"
#include "modules/fusion/km.h"

namespace coop {
namespace v2x {
// using namespace common::util;
struct ScoreParams {
  double max_match_distance_ = 2.5;
  void set_max_match_distance(double max_match_distance) {
    max_match_distance_ = max_match_distance;
  }
  double max_match_distance() { return max_match_distance_; }
  bool check_type() { return true; }

  double min_score() { return 0; }
};

class Fusion {
 public:
  Fusion();
  ~Fusion() {}


  int FuseVehicleData(spRaysunFrame frame, const KITTI_RAW &vehicle);
  /**
   * @brief use the speed and orientation to predict the state of frame in time
   * dt
   *
   * @param ori_frame
   * @param dt
   * @return spRaysunFrame
   */
  bool CombineNewResource(const std::vector<VehicleObs> &new_objects,
                          std::vector<VehicleObs> *fused_objects);

  /**
   * @brief calculate the association between two groups of objects
   *
   * @param in1_objects
   * @param in2_objects
   * @return association_mat
   */
  bool ComputeAssociateMatrix(const std::vector<VehicleObs> &in1_objects,
                              const std::vector<VehicleObs> &in2_objects,
                              Eigen::MatrixXf *association_mat);
  void set_max_distance_match(double distance);

 private:
  bool CheckDisScore(const VehicleObs &in1_ptr, const VehicleObs &in2_ptr,
                     double *score);

  double CheckOdistance(const VehicleObs &in1_ptr, const VehicleObs &in2_ptr);
  ScoreParams score_params_;
  KMkernal km_matcher_;
};

DEFINE_EXTEND_TYPE(Fusion);
}  // namespace v2x
}  // namespace coop
