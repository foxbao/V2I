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
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include "modules/common/util/inner_types.hpp"
// #include "proto/config.pb.h"

namespace coop {
namespace v2x {
#define DEG2RAD M_PI / 180
#define RAD2DEG 180 / M_PI
#define MPS2KPH 3.6
#define KPH2MPS 1 / 3.6

#ifdef USED_BY_MEC
#define PROJECT_ROOT_PATH "/zassys/sysapp/others/fusion-service"
#else
#define PROJECT_ROOT_PATH "/home/baojiali/Downloads/fusion"
#endif

// class Cacher;
class FuserConfig;
extern std::unique_ptr<FuserConfig> g_fuserConfig;
extern std::vector<std::string> split(const std::string &str,
                                      const std::string &pattern);
extern std::string trimstr(std::string s);
extern const double g_ori_pos_deg[3];

extern OCLong hms2unix(const std::string &data);

inline size_t s2us(const double t_s) {
  return static_cast<size_t>(round(t_s * 1.0e6));
}
inline size_t s2ms(const double t_s) {
  return static_cast<size_t>(round(t_s * 1.0e3));
}
// convert speed from km/h to m/s
inline double kmph2mps(const double kmh)
{
  return static_cast<double>(kmh*1000/3600);
}


inline double normalizeRad(const double ori_rad)
{
  if(ori_rad<=-M_PI)
  {
    return ori_rad+2*M_PI;
  }
  else if(ori_rad>M_PI)
  {
    return ori_rad-2*M_PI;
  }
  else
  {
    return ori_rad;
  }
}
inline double normalizeAngle(const double ori_angle)
{
  if(ori_angle<=-180)
  {
    return ori_angle+360;
  }
  else if(ori_angle>180)
  {
    return ori_angle-360;
  }
  else
  {
    return ori_angle;
  }
}


}  // namespace v2x
}  // namespace coop
