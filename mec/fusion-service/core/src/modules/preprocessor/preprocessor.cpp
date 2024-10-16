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

#include "modules/preprocessor/preprocessor.h"
#include <algorithm>
#include <cmath>
#include <opencv2/opencv.hpp>
#include "modules/common/util/inner_types.hpp"
#include "modules/common/util/util.h"
#include "modules/dataType/vehicle_obs.hpp"

namespace coop {
namespace v2x {

DataReader::DataReader() {}

int DataReader::ReadRaysunHead(FILE* fp, RaysunHead* head) {
  fread(&head->flag, sizeof(head->flag), 1, fp);
  fread(&head->devtype, sizeof(head->devtype), 1, fp);
  fread(&head->devid, sizeof(head->devid), 1, fp);
  fread(&head->timestamp, sizeof(head->timestamp), 1, fp);
  fread(&head->frame, sizeof(head->frame), 1, fp);
  fread(&head->datalen, sizeof(head->datalen), 1, fp);
  fread(&head->targetnum, sizeof(head->targetnum), 1, fp);
  fread(&head->lon, sizeof(head->lon), 1, fp);
  fread(&head->lat, sizeof(head->lat), 1, fp);
  int result = fread(&head->rdir, sizeof(head->rdir), 1, fp);
  return result;
}
int DataReader::ReadRaysunTarget(FILE* fp, RaysunTarget* target) {
  fread(&target->id, sizeof(target->id), 1, fp);
  fread(&target->obj_class, sizeof(target->obj_class), 1, fp);
  fread(&target->lon, sizeof(target->lon), 1, fp);
  fread(&target->lat, sizeof(target->lat), 1, fp);
  fread(&target->obj_length, sizeof(target->obj_length), 1, fp);
  fread(&target->obj_width, sizeof(target->obj_width), 1, fp);
  fread(&target->obj_height, sizeof(target->obj_height), 1, fp);
  fread(&target->sog, sizeof(target->sog), 1, fp);
  fread(&target->cog, sizeof(target->cog), 1, fp);
  fread(&target->dist, sizeof(target->dist), 1, fp);
  fread(&target->bearing, sizeof(target->bearing), 1, fp);
  fread(&target->objpos, sizeof(target->objpos), 1, fp);
  fread(&target->stable, sizeof(target->stable), 1, fp);
  return fread(&target->origin_timestamp, sizeof(target->origin_timestamp), 1,
               fp);
}

void DataReader::ReadRaysunFrames(std::string file,
                                  std::vector<RaysunFrame>* raysun_frames,
                                  OCLong manual_devid) {
  // raysunFrames_.clear();
  RaysunHead head;
  RaysunTarget target;
  OCShort crc;
  OCShort eflag;
  memset(&head, 0, sizeof(head));
  memset(&target, 0, sizeof(target));
  FILE* fp = fopen(file.c_str(), "rb");

  if (fp == NULL) {
    std::cout << "open file fail" << std::endl;
    return;
  }

  OCLong last_time = 0;
  OCLong time_diff = 0;
  while (1) {
    std::vector<Eigen::Vector3d> targets_enu;
    std::vector<RaysunTarget> targets;
    VehicleObs obj;
    if (ReadRaysunHead(fp, &head) == 0) {
      break;
    }

    time_diff = head.timestamp - last_time;

    last_time = head.timestamp;
    if (manual_devid != -1) {
      head.devid = manual_devid;
    }
    for (int j = 0; j < head.targetnum; j++) {
      ReadRaysunTarget(fp, &target);
      targets.push_back(target);
    }

    fread(&crc, sizeof(crc), 1, fp);
    fread(&eflag, sizeof(eflag), 1, fp);
    RaysunFrame frame(head, targets);
    raysun_frames->push_back(frame);
  }
}


bool cmp_raysunFrame(sp_cRaysunFrame x, sp_cRaysunFrame y) {
  return x->head().timestamp < y->head().timestamp;
}

void DataReader::ReadRaysunFiles(std::vector<std::string> raysun_files,
                                 std::vector<sp_cRaysunFrame>& data_buffer) {
  std::vector<std::vector<RaysunFrame>> RFrames_cams;

  for (auto raysun_file : raysun_files) {
    std::vector<RaysunFrame> RFrames;
    this->ReadRaysunFrames(raysun_file, &RFrames);
    RFrames_cams.push_back(RFrames);
    std::cout << "file_path" << raysun_file << std::endl;
    std::cout << "device:" << RFrames[0].head().devid
              << " begin time:" << RFrames[0].head().timestamp << std::endl;
    std::cout << "device:" << RFrames[0].head().devid
              << " end time  :" << RFrames.back().head().timestamp << std::endl;
  }

  for (auto frames_cam : RFrames_cams) {
    for (auto frame : frames_cam) {
      data_buffer.push_back(std::make_shared<RaysunFrame>(frame));
    }
  }
  sort(data_buffer.begin(), data_buffer.end(), cmp_raysunFrame);
}
}  // namespace v2x
}  // namespace coop
