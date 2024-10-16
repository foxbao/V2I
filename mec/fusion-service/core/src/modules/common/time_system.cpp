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
#include "modules/common/time_system.h"
namespace coop {
namespace v2x {
std::string Unix2YMDHSMS(__int64_t unix_time_ms) {
  // if (unix_time_ms < 1000000000000) {

  // }
  __int64_t unix_time_s = unix_time_ms / 1000;
  time_t rawtime(unix_time_s);  // time_t rawtime; time(&rawtime);
  struct tm *timeinfo = localtime(&rawtime);
  int year = timeinfo->tm_year + 1900;
  int mon = timeinfo->tm_mon + 1;
  int day = timeinfo->tm_mday;
  int hour = timeinfo->tm_hour;
  int min = timeinfo->tm_min;
  int second = timeinfo->tm_sec;
  int millisecond = unix_time_ms - unix_time_s * 1000;

  std::string s_year = std::to_string(year);
  std::string s_mon = std::to_string(mon);
  if (s_mon.size() < 2) {
    s_mon = "0" + s_mon;
  }
  std::string s_day = std::to_string(day);
  if (s_day.size() < 2) {
    s_day = "0" + s_day;
  }
  std::string s_hour = std::to_string(hour);
  if (s_hour.size() < 2) {
    s_hour = "0" + s_hour;
  }
  std::string s_min = std::to_string(min);
  if (s_min.size() < 2) {
    s_min = "0" + s_min;
  }
  std::string s_second = std::to_string(second);
  if (s_second.size() < 2) {
    s_second = "0" + s_second;
  }
  std::string s_millisecond = std::to_string(millisecond);

  while (s_millisecond.size() < 3) {
    s_millisecond = "0" + s_millisecond;
    int aaa = 1;
  }

  std::string s_time =
      s_year + s_mon + s_day + s_hour + s_min + s_second + "." + s_millisecond;
  return s_time;
}
}  // namespace v2x
}  // namespace coop
