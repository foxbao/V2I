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
#include <vector>
#include <string>
#include "modules/common/util/inner_types.hpp"
namespace coop {
namespace v2x {
class CivMap {
 public:
  CivMap();
  ~CivMap();
  std::vector<sp_cZMapLineSegment> get_lines() { return lines_; }
  std::vector<sp_cZMapLineSegment> get_lines_enu();
  void ReadData(std::string file_path);

 private:
  std::vector<sp_cZMapLineSegment> lines_;  // llh
};
DEFINE_EXTEND_TYPE(CivMap);
}  // namespace v2x
}  // namespace coop
