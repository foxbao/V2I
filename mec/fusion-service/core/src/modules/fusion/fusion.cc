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
#include "fusion.h"
#include <algorithm>
#include <utility>
namespace coop {
namespace v2x {
Fusion::Fusion() {}

int Fusion::FuseVehicleData(spRaysunFrame frame, const KITTI_RAW &vehicle) {
  std::vector<VehicleObs> vec_vechile_data;

  VehicleObs vehicle_data;
  vehicle_data.set_llh(vehicle.lat, vehicle.lon);
  vec_vechile_data.push_back(vehicle_data);
  int u_num = vec_vechile_data.size();
  int v_num = frame->targets().size();
  Eigen::MatrixXf association_mat(u_num, v_num);
  this->ComputeAssociateMatrix(vec_vechile_data, frame->targets(),
                               &association_mat);
  // std::cout << "vehicle to raysun matching" << std::endl;
  Eigen::MatrixXd::Index maxRow, maxCol;
  // std::cout << association_mat << std::endl;
  double max = association_mat.maxCoeff(&maxRow, &maxCol);
  // std::cout << "max row:" << maxRow << std::endl;
  // std::cout << "max col:" << maxCol << std::endl;
  if (max < 10.0) {
    return -1;
  }
  return static_cast<int>(maxCol);
}

void Fusion::set_max_distance_match(double distance) {
  score_params_.set_max_match_distance(distance);
}

bool Fusion::CombineNewResource(const std::vector<VehicleObs> &new_objects,
                                std::vector<VehicleObs> *fused_objects) {
  // if there is no data from the new camera, just return
  if (new_objects.empty()) {
    return false;
  }
  // if there is not data in original fused data, copy the data from camra
  // to fused objects
  if (fused_objects->size() < 1) {
    fused_objects->assign(new_objects.begin(), new_objects.end());
    for (unsigned int j = 0; j < new_objects.size(); ++j) {
      std::vector<VehicleObs> matched_objects;
      matched_objects.push_back(new_objects[j]);
    }
    return true;
  }
  int u_num = fused_objects->size();
  int v_num = new_objects.size();
  Eigen::MatrixXf association_mat(u_num, v_num);
  ComputeAssociateMatrix(*fused_objects, new_objects, &association_mat);
  // size of association_mat is u*v
  // if i in original fused object matches j in new objects
  // mat(i,j) will be non-zero
  std::cout << "association mat" << std::endl;
  std::cout << association_mat << std::endl;
  // size of match_cps is v+u
  // for a pair(i,j), j represents the index of object in new objects
  // i represents the index of object in original fused objects
  // -1 means no matching
  std::vector<std::pair<int, int>> match_cps;
  if (u_num > v_num) {
    km_matcher_.GetKMResult(association_mat.transpose(), &match_cps, true);
  } else {
    km_matcher_.GetKMResult(association_mat, &match_cps, false);
  }

  std::vector<VehicleObs> matched_objects;
  matched_objects.push_back(fused_objects->back());
  for (auto it = match_cps.begin(); it != match_cps.end(); it++) {
    //
    if (it->second != -1) {
      if (it->first == -1) {
        fused_objects->push_back(new_objects[it->second]);

        // fusion_result->push_back(matched_objects);
      } else {
        int aaa = 1;
        // (*fusion_result)[it->first].push_back(new_objects[it->second]);
      }
    }
  }
  return true;
}

bool Fusion::ComputeAssociateMatrix(const std::vector<VehicleObs> &in1_objects,
                                    const std::vector<VehicleObs> &in2_objects,
                                    Eigen::MatrixXf *association_mat) {
  for (unsigned int i = 0; i < in1_objects.size(); ++i) {
    for (unsigned int j = 0; j < in2_objects.size(); ++j) {
      const VehicleObs &obj1_ptr = in1_objects[i];
      const VehicleObs &obj2_ptr = in2_objects[j];
      double score = 0;
      if (!CheckDisScore(obj1_ptr, obj2_ptr, &score)) {
        // AERROR << "V2X Fusion: check dis score failed";
      }
      // if (score_params_.check_type() &&
      //     !CheckTypeScore(obj1_ptr, obj2_ptr, &score))
      // {
      //     AERROR << "V2X Fusion: check type failed";
      // }
      (*association_mat)(i, j) =
          (score >= score_params_.min_score()) ? score : 0;
    }
  }
  return true;
}

double Fusion::CheckOdistance(const VehicleObs &in1_ptr, const VehicleObs &in2_ptr) {
  double xi = in1_ptr.pos_enu()[0];
  double yi = in1_ptr.pos_enu()[1];
  double xj = in2_ptr.pos_enu()[0];
  double yj = in2_ptr.pos_enu()[1];

  double distance = std::hypot(xi - xj, yi - yj);
  return distance;
}

bool Fusion::CheckDisScore(const VehicleObs &in1_ptr, const VehicleObs &in2_ptr,
                           double *score) {
  double dis = CheckOdistance(in1_ptr, in2_ptr);
  *score = 2.5 * std::max(0.0, score_params_.max_match_distance() - dis);
  return true;
}
}  // namespace v2x
}  // namespace coop
