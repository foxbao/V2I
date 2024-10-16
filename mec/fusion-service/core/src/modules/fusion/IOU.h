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
#include <algorithm>
#include <eigen3/Eigen/Core>

namespace coop {
namespace v2x {

template <typename Type>
Type CalculateIou2DXY(const Eigen::Matrix<Type, 3, 1> &center0,
                      const Eigen::Matrix<Type, 3, 1> &size0,
                      const Eigen::Matrix<Type, 3, 1> &center1,
                      const Eigen::Matrix<Type, 3, 1> &size1) {
  Type min_x_bbox_0 = center0(0) - size0(0) * static_cast<Type>(0.5);
  Type min_x_bbox_1 = center1(0) - size1(0) * static_cast<Type>(0.5);
  Type max_x_bbox_0 = center0(0) + size0(0) * static_cast<Type>(0.5);
  Type max_x_bbox_1 = center1(0) + size1(0) * static_cast<Type>(0.5);
  Type start_x = std::max(min_x_bbox_0, min_x_bbox_1);
  Type end_x = std::min(max_x_bbox_0, max_x_bbox_1);
  Type length_x = end_x - start_x;
  if (length_x <= 0) {
    return 0;
  }
  Type min_y_bbox_0 = center0(1) - size0(1) * static_cast<Type>(0.5);
  Type min_y_bbox_1 = center1(1) - size1(1) * static_cast<Type>(0.5);
  Type max_y_bbox_0 = center0(1) + size0(1) * static_cast<Type>(0.5);
  Type max_y_bbox_1 = center1(1) + size1(1) * static_cast<Type>(0.5);
  Type start_y = std::max(min_y_bbox_0, min_y_bbox_1);
  Type end_y = std::min(max_y_bbox_0, max_y_bbox_1);
  Type length_y = end_y - start_y;
  if (length_y <= 0) {
    return 0;
  }
  Type intersection_area = length_x * length_y;
  Type bbox_0_area = size0(0) * size0(1);
  Type bbox_1_area = size1(0) * size1(1);
  Type iou =
      intersection_area / (bbox_0_area + bbox_1_area - intersection_area);
  return iou;
}
}  // namespace v2x
}  // namespace coop
