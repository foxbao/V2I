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
#include <eigen3/Eigen/Core>
#include <limits>
#include <vector>
#include "modules/common/math/vec2d.h"
namespace coop {
namespace common {
namespace math {
class Triangle2d {
 public:
  Triangle2d() {}
  Triangle2d(const Vec2d &center, const double heading, const double length,
             const double width);

  ~Triangle2d() {}
  void InitCorners();

  /**
   * @brief Getter of the center of the box
   * @return The center of the box
   */
  const Vec2d &center() const { return center_; }

  /**
   * @brief Getter of the x-coordinate of the center of the box
   * @return The x-coordinate of the center of the box
   */
  double center_x() const { return center_.x(); }

  /**
   * @brief Getter of the y-coordinate of the center of the box
   * @return The y-coordinate of the center of the box
   */
  double center_y() const { return center_.y(); }

  /**
   * @brief Getter of the length
   * @return The length of the heading-axis
   */
  double length() const { return length_; }

  /**
   * @brief Getter of the width
   * @return The width of the box taken perpendicularly to the heading
   */
  double width() const { return width_; }

  /**
   * @brief Getter of half the length
   * @return Half the length of the heading-axis
   */
  double half_length() const { return half_length_; }

  /**
   * @brief Getter of half the width
   * @return Half the width of the box taken perpendicularly to the heading
   */
  double half_width() const { return half_width_; }

  /**
   * @brief Getter of the heading
   * @return The counter-clockwise angle between the x-axis and the heading-axis
   */
  double heading() const { return heading_; }

  /**
   * @brief Getter of the cosine of the heading
   * @return The cosine of the heading
   */
  double cos_heading() const { return cos_heading_; }

  /**
   * @brief Getter of the sine of the heading
   * @return The sine of the heading
   */
  double sin_heading() const { return sin_heading_; }

  /**
   * @brief Getter of the area of the box
   * @return The product of its length and width
   */
  double area() const { return length_ * width_; }

  /**
   * @brief Getter of the corners of the box
   * @return corners The vector where the corners are listed
   */
  std::vector<Vec2d> GetAllCorners() const;

 private:
  Vec2d center_;
  double length_ = 0.0;
  double width_ = 0.0;
  double half_length_ = 0.0;
  double half_width_ = 0.0;
  double heading_ = 0.0;
  double cos_heading_ = 1.0;
  double sin_heading_ = 0.0;

  std::vector<Vec2d> corners_;

  double max_x_ = std::numeric_limits<double>::lowest();
  double min_x_ = std::numeric_limits<double>::max();
  double max_y_ = std::numeric_limits<double>::lowest();
  double min_y_ = std::numeric_limits<double>::max();
};
}  // namespace math
}  // namespace common
}  // namespace coop
