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
#include "cacher.hpp"
#include "proto/config.pb.h"
namespace coop {
namespace v2x {

bool frameCompare(sp_cRaysunFrame frame0, sp_cRaysunFrame frame1) {
  return (frame0->head().timestamp < frame1->head().timestamp);
}

Cacher::Cacher() {
  auto const& cfg = g_fuserConfig->cacher_cfg();
  duration_s_ = cfg.duration_s();
  max_delay_ms_ = cfg.max_delay_ms();
}

// Get the last value of the specific device <= time of main device
sp_cRaysunFrame Cacher::GetNearestFrame(const OCLong timestamp,
                                        const OCLong devid) {
  // std::lock_guard<std::mutex> lock(mtx_named_buff_);
  auto& dev_map = named_buff_[devid];
  if (dev_map.size() == 0) {
    return nullptr;
  }
  auto it = dev_map.upper_bound(timestamp);

  if (it != dev_map.begin()) {
    --it;
    return it->second;
  } else {
    return nullptr;
  }

}

void Cacher::SortCache() {
  // std::sort(buffer_.begin(), buffer_.end(), frameCompare);

  // for (auto &&map : named_buff_)
  // {
  //     int bbb=2;
  //     // std::sort(que.second.begin(), que.second.end(), frameCompare);
  // }
}

}  // namespace v2x
}  // namespace coop
