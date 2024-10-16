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
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include "modules/common/util/inner_types.hpp"
#include "modules/dataType/raysunframe.hpp"

namespace coop {
namespace v2x {

class Cacher {
 public:
  Cacher();
  template <typename T>
  std::deque<sp_cRaysunFrame> push_back(sp(T) msg);
  void SortCache();
  std::deque<sp_cRaysunFrame> GetSampleTimeOrderedFrames();
  sp_cRaysunFrame GetNearestFrame(const OCLong timestamp, const OCLong devid);
  std::map<OCLong, std::map<size_t, sp_cRaysunFrame>> named_buff() {
    return named_buff_;
  }
  std::map<size_t, sp_cRaysunFrame> buffer() { return buffer_; }

 public:
  std::map<OCLong, std::map<size_t, sp_cRaysunFrame>>
      named_buff_;  // 缓存duration_s_的所有数据
  std::map<size_t, sp_cRaysunFrame> buffer_;
  std::mutex mtx_named_buff_;
  double duration_s_ = 1.5;
  double max_delay_ms_ = 1000;
  double last_recieve_time_s_ = -1;
  double current_recieve_time_s_ = -1;
  // OCLong main_devid_;
  int id = 0;
};

template <typename T>
std::deque<sp_cRaysunFrame> Cacher::push_back(sp(T) msg) {
  // que_.push_back(msg);
  double dt_ms_single_buff = 0;
  double dt_ms_buffer = 0;
  {
    current_recieve_time_s_ = msg->head().timestamp;

    std::map<size_t, sp_cRaysunFrame> &single_buff =
        named_buff_[msg->head().devid];
    single_buff[msg->head().timestamp] = msg;
    dt_ms_single_buff = single_buff.rbegin()->second->head().timestamp -
                        single_buff.begin()->second->head().timestamp;
    // named_buff_[msg->head().devid][msg->head().timestamp]=msg;
    if (dt_ms_single_buff >= s2ms(duration_s_)) {
      size_t remove_ms = single_buff.rbegin()->second->head().timestamp -
                              s2ms(duration_s_);
      single_buff.erase(single_buff.begin(),
                        single_buff.lower_bound(remove_ms));
    }

    buffer_[msg->head().timestamp] = msg;
    dt_ms_buffer = buffer_.rbegin()->second->head().timestamp -
                   buffer_.begin()->second->head().timestamp;

    if (dt_ms_buffer >= s2ms(duration_s_)) {
      size_t remove_ms =
          buffer_.rbegin()->second->head().timestamp - s2ms(duration_s_);
      buffer_.erase(buffer_.begin(), buffer_.lower_bound(remove_ms));
    }

    return std::deque<sp_cRaysunFrame>();
  }
}

}  // namespace v2x
}  // namespace coop
