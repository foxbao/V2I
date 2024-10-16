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
#include "modules/common/util/util.h"
#include "proto/config.pb.h"
namespace coop {
namespace v2x {

OCLong hms2unix(const std::string &data) {
  std::string::size_type size;
  std::vector<std::string> timestamp = split(data, ":");
  int hour = std::stoi(timestamp[0]);
  int minute = std::stoi(timestamp[1]);
  double second = std::stod(timestamp[2], &size);
  OCLong timestamp_mms =
      hour * 3600 * 1000 + minute * 60 * 1000 + second * 1000;
  return timestamp_mms;
}

std::vector<std::string> split(const std::string &str,
                               const std::string &pattern) {
  // const char* convert to char*
  char *strc = new char[strlen(str.c_str()) + 1];
  strcpy(strc, str.c_str());
  std::vector<std::string> resultVec;
  char *tmpStr = strtok(strc, pattern.c_str());
  while (tmpStr != NULL) {
    resultVec.push_back(std::string(tmpStr));
    tmpStr = strtok(NULL, pattern.c_str());
  }

  delete[] strc;

  return resultVec;
}

std::string trimstr(std::string s) {
  size_t n = s.find_last_not_of(" \r\n\t\"\tab");
  if (n != std::string::npos) {
    s.erase(n + 1, s.size() - n);
  }
  n = s.find_first_not_of(" \r\n\t\"\tab");
  if (n != std::string::npos) {
    s.erase(0, n);
  }
  return s;
}

double const g_ori_pos_deg[3]{31.284156453, 121.170937985, 0};
}  // namespace v2x

}  // namespace coop
