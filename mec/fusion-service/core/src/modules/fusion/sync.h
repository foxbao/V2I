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
#include <vector>
#include "modules/dataType/raysunframe.hpp"
// #include "raysunframehead.h"
namespace coop {
namespace v2x {
class SYNC {
 public:
  SYNC();
  bool FindMatchingFrame(const OCLong timestamp_mmsec,
                         const std::vector<RaysunFrame> &raysunFrames);
  bool ShiftFrame(const RaysunFrame &frame,
                  const std::vector<RaysunFrame> &raysunFrames,
                  RaysunFrame &frame_shift);

  bool ShiftFrame(const RaysunFrame &frame, const OCLong time_diff_msec,
                  RaysunFrame &frame_msg);
  template <class T>
  T InterpolateXYZ(const T &p1, const T &p2, const double frac1);
};
}  // namespace v2x
}  // namespace coop
