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
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "modules/dataType/raysunframe.hpp"
#include "modules/dataType/vehicle_obs.hpp"

namespace coop {
namespace v2x {
class DataReader {
 public:
  DataReader();
  ~DataReader() {}
  void ReadRaysunFrames(std::string file,
                        std::vector<RaysunFrame>* raysun_frames,
                        OCLong manual_devid = -1);
  void ReadRaysunFiles(std::vector<std::string> raysun_files,
                       std::vector<sp_cRaysunFrame>& data_buffer);

 private:
  int ReadRaysunTarget(FILE* fp, RaysunTarget* target);
  int ReadRaysunHead(FILE* fp, RaysunHead* head);
};

DEFINE_EXTEND_TYPE(DataReader);

}  // namespace v2x
}  // namespace coop
