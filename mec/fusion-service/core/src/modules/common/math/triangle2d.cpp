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
#include "triangle2d.hpp"
#include <eigen3/Eigen/Dense>
#include <iostream>
namespace coop {
namespace common {
namespace math {
Triangle2d::Triangle2d(const Vec2d &center, const double heading,
                       const double length, const double width)
    : center_(center),
      length_(length),
      width_(width),
      half_length_(length / 2.0),
      half_width_(width / 2.0),
      heading_(heading),
      cos_heading_(cos(heading)),
      sin_heading_(sin(heading)) {
  // heading is in NED, which means counterclock from north
  // therefore, we have to convert NED to ENU to match with map
  Eigen::AngleAxisd rotation_vector_z(M_PI / 2, Eigen::Vector3d(0, 0, 1));
  Eigen::AngleAxisd rotation_vector_x(-M_PI, Eigen::Vector3d(1, 0, 0));
  Eigen::Matrix3d rotation_matrix_ENU_NED =
      rotation_vector_z.matrix() * rotation_vector_x.matrix();

  // convert from body from to NED
  Eigen::AngleAxisd yawAngle(heading, Eigen::Vector3d::UnitZ());
  Eigen::Matrix3d rotation_matrix_ned_b;
  rotation_matrix_ned_b = yawAngle.matrix();

  Eigen::Vector3d pt_0{half_length_, 0, 0};
  Eigen::Vector3d pt_1{-half_length_, -half_width_, 0};
  Eigen::Vector3d pt_2{-half_length_, half_width_, 0};

  Eigen::Vector3d pt_n_0 =
      rotation_matrix_ENU_NED * rotation_matrix_ned_b * pt_0 +
      Eigen::Vector3d{center_.x(), center_.y(), 0};

  Eigen::Vector3d pt_n_1 =
      rotation_matrix_ENU_NED * rotation_matrix_ned_b * pt_1 +
      Eigen::Vector3d{center_.x(), center_.y(), 0};
  Eigen::Vector3d pt_n_2 =
      rotation_matrix_ENU_NED * rotation_matrix_ned_b * pt_2 +
      Eigen::Vector3d{center_.x(), center_.y(), 0};

  corners_.clear();

  corners_.emplace_back(pt_n_0(0), pt_n_0(1));
  corners_.emplace_back(pt_n_1(0), pt_n_1(1));
  corners_.emplace_back(pt_n_2(0), pt_n_2(1));
}

void Triangle2d::InitCorners() {
  const double dx1 = cos_heading_ * half_length_;
  const double dy1 = sin_heading_ * half_length_;
  const double dx2 = sin_heading_ * half_width_;
  const double dy2 = -cos_heading_ * half_width_;
  corners_.clear();
  corners_.emplace_back(center_.x() + dx1 + dx2, center_.y() + dy1 + dy2);
  corners_.emplace_back(center_.x() + dx1 - dx2, center_.y() + dy1 - dy2);
  corners_.emplace_back(center_.x() - dx1 - dx2, center_.y() - dy1 - dy2);
  corners_.emplace_back(center_.x() - dx1 + dx2, center_.y() - dy1 + dy2);

  for (auto &corner : corners_) {
    max_x_ = std::fmax(corner.x(), max_x_);
    min_x_ = std::fmin(corner.x(), min_x_);
    max_y_ = std::fmax(corner.y(), max_y_);
    min_y_ = std::fmin(corner.y(), min_y_);
  }
}



std::vector<Vec2d> Triangle2d::GetAllCorners()const
{
  return corners_;
}
}  // namespace math
}  // namespace common
}  // namespace coop
