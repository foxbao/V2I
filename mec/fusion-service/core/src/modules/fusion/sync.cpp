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
#include "sync.h"
#include <iostream>
#include <limits>
#include <vector>

namespace coop {
namespace v2x {
// using namespace common;
SYNC::SYNC() {}

bool SYNC::ShiftFrame(const RaysunFrame &frame, const OCLong time_diff_msec,
                      RaysunFrame &frame_msg) {
  for (auto object : frame.targets()) {
    Eigen::Vector3d center_enu = object.pos_enu();
    center_enu += Eigen::Vector3d(1, 1, 1);
  }
  return true;
}

bool SYNC::ShiftFrame(const RaysunFrame &frame,
                      const std::vector<RaysunFrame> &raysunFrames,
                      RaysunFrame &frame_shift) {
  OCLong timestamp_msec = frame.head().timestamp;
  RaysunFrame frame_msg;
  auto frame_it = raysunFrames.begin();
  for (; frame_it != raysunFrames.end(); ++frame_it) {
    std::cout << "timestamp_msec:      " << timestamp_msec << std::endl;
    std::cout << "frame timestamp_msec:" << (*frame_it).head().timestamp
              << std::endl;
    if ((*frame_it).head().timestamp - timestamp_msec >
        std::numeric_limits<double>::min()) {
      int bbb = 1;
      break;
    }
  }
  // long long int timestamp_msec_sign=timestamp_msec;

  if (frame_it != raysunFrames.end()) {  // found one
    if (frame_it == raysunFrames.begin()) {
      std::cerr << " too short" << std::endl;
    } else {
      // here is the normal case
      auto frame_it_1 = frame_it;
      frame_it_1--;
      OCLong time_diff_msec = (*frame_it).head().timestamp - timestamp_msec;
      ShiftFrame(frame, time_diff_msec, frame_msg);
      int aaa = 1;
    }
  } else {
  }
  return true;
}

bool SYNC::FindMatchingFrame(const OCLong timestamp_msec,
                             const std::vector<RaysunFrame> &raysunFrames) {
  RaysunFrame frame_msg;
  auto frame_it = raysunFrames.begin();
  for (; frame_it != raysunFrames.end(); ++frame_it) {
    std::cout << "timestamp_msec:      " << timestamp_msec << std::endl;
    std::cout << "frame timestamp_msec:" << (*frame_it).head().timestamp
              << std::endl;
    if ((*frame_it).head().timestamp - timestamp_msec >
        std::numeric_limits<double>::min()) {
      int bbb = 1;
      break;
    }
  }
  // long long int timestamp_msec_sign=timestamp_msec;

  if (frame_it != raysunFrames.end()) {  // found one
    if (frame_it == raysunFrames.begin()) {
      std::cerr << " too short" << std::endl;
    } else {
      // here is the normal case
      auto frame_it_1 = frame_it;
      frame_it_1--;
      OCLong time_diff = (*frame_it).head().timestamp - timestamp_msec;
      int aaa = 1;
      // if (!ShiftFrame(*frame_it_1, *frame_it, timestamp_msec,frame_msg)) {
      //     std::cerr << "failed to interpolate IMU"<<std::endl;
      //     return false;
      // }
    }
  } else {
  }
  return true;
}

template <class T>
T SYNC::InterpolateXYZ(const T &p1, const T &p2, const double frac1) {
  T p;
  return p;
}
}  // namespace v2x
}  // namespace coop
