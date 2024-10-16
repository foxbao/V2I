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

#include "modules/imgprocessor/imgprocessor.h"
#include <memory>
#include <string>
#include <vector>
#include "modules/common/earth.h"
#include "modules/common/time_system.h"

namespace coop {
namespace v2x {
IMGPROCESSOR::IMGPROCESSOR() {
  scale_ = 6;
  H_ = 1000;
  W_ = 1000;
  result_img_ = cv::Mat(H_, W_, CV_32FC3);
  img_folder_ = "img_result";
  img_nofuse_folder_ = "img_nofuse_result";

  color_set_.push_back((0, 0, 255));
  color_set_.push_back((0, 255, 0));
  color_set_.push_back((0, 255, 255));
  color_set_.push_back((255, 0, 0));
  color_set_.push_back((255, 0, 255));
  color_set_.push_back((255, 255, 0));
  color_set_.push_back((255, 255, 255));

  plotColor_[101] = cv::Scalar(255, 100, 0);
  plotColor_[102] = cv::Scalar(0, 255, 0);
  plotColor_[103] = cv::Scalar(0, 0, 255);
  plotColor_[104] = cv::Scalar(0, 255, 255);

  title_position_[101] = cv::Point2d(100, 100);
  title_position_[102] = cv::Point2d(100, 150);
  title_position_[103] = cv::Point2d(100, 200);
  title_position_[104] = cv::Point2d(100, 250);

  map_ = std::make_shared<CivMap>();
  // Read lla map information
  map_->ReadData(std::string(PROJECT_ROOT_PATH) + "/data/map.txt");
  font_ = cv::FONT_HERSHEY_SIMPLEX;
}

void IMGPROCESSOR::PlotCombinedResult(cv::Mat *ptr_img,
                                      const cv::Mat &vehicle_fusion_img,
                                      const cv::Mat &camera_merge_img,
                                      OCLong current_time, OCLong last_time) {
  // cv::Mat combine;
  hconcat(vehicle_fusion_img, camera_merge_img, *ptr_img);

  std::string img_name = Unix2YMDHSMS(current_time);
  cv::putText(*ptr_img, img_name, cv::Point2d(800, 50),
              cv::FONT_HERSHEY_SIMPLEX, 2, cv::Scalar(255, 255, 255), 2);

  OCLong dt_ms = current_time - last_time;
  double FPS;
  if (dt_ms != 0) {
    FPS = 1.0 / (dt_ms / 1000.0);
  } else {
    FPS = 0;
  }

  std::string s_dev_fps = "devid: FPS:" + std::to_string(FPS);
  cv::putText(*ptr_img, s_dev_fps, cv::Point2d(800, 100),
              cv::FONT_HERSHEY_SIMPLEX, 2, cv::Scalar(255, 255, 255), 2);

  // return combine;
}

void IMGPROCESSOR::PlotVehicleSnapshot(
    cv::Mat *ptr_img, std::map<int, spVehicleState> origin,
    std::map<int, spVehicleState> transf, OCLong current_time,
    OCLong last_time, OCDouble x, OCDouble y) {
  cv::Mat vehicle_fusion_img = cv::Mat(H_, W_, CV_32FC3, cv::Scalar(0, 0, 0));
  PlotFusionVehicles(&vehicle_fusion_img, origin);
  cv::Mat camera_merge_img = cv::Mat(H_, W_, CV_32FC3, cv::Scalar(0, 0, 0));
  PlotSnapshot(&camera_merge_img, transf);

  PlotCombinedResult(ptr_img, camera_merge_img, vehicle_fusion_img,
                     current_time, last_time);
  if (std::abs(x) > 0 && std::abs(y) > 0)
  {
    std::string tmp = std::to_string(x);
    tmp += " , ";
    tmp += std::to_string(y);
    cv::putText(*ptr_img, tmp, cv::Point2d(50, 150), cv::FONT_HERSHEY_SIMPLEX, 2, cv::Scalar(255, 255, 255), 2);
  }
}

void IMGPROCESSOR::PlotVehicleStatesAndFrames(
    cv::Mat *ptr_img, std::map<int, spVehicleState> vehicles,
    std::vector<spRaysunFrame> syncFrames,
    std::map<int, spVehicleObs> vehicle_obsers, OCLong current_time,
    OCLong last_time) {
  cv::Mat vehicle_fusion_img = cv::Mat(H_, W_, CV_32FC3, cv::Scalar(0, 0, 0));
  PlotFusionVehicles(&vehicle_fusion_img, vehicles);
  cv::Mat camera_merge_img = cv::Mat(H_, W_, CV_32FC3, cv::Scalar(0, 0, 0));

  PlotMulCamsRaysunframe(&camera_merge_img, syncFrames);
  PlotObsTrajectory(&camera_merge_img, vehicle_obsers);
  PlotCombinedResult(ptr_img, vehicle_fusion_img, camera_merge_img,
                     current_time, last_time);
}

void IMGPROCESSOR::PlotAxis(cv::Mat *ptr_img) {
  cv::Point center(W_ / 2, H_ / 2);
  cv::Point eastEnd(W_, H_ / 2);
  cv::Point northEnd(W_ / 2, 0);
  cv::line(*ptr_img, center, eastEnd, cv::Scalar(0, 255, 0), 1, CV_AA);
  cv::line(*ptr_img, center, northEnd, cv::Scalar(0, 255, 0), 1, CV_AA);

  int thickness = 2;
  Vec2d label_100_0(100, 0);
  Vec2d label_100_0_IMG = Convert2IMG(label_100_0);
  cv::Point point_label_100_0_IMG(label_100_0_IMG.x(), label_100_0_IMG.y());
  cv::putText(*ptr_img, "100m", point_label_100_0_IMG, font_, 0.5,
              cv::Scalar(255, 255, 255), thickness);

  Vec2d label_0_100(0, 100);
  Vec2d label_0_100_IMG = Convert2IMG(label_0_100);
  cv::Point point_label_0_100_IMG(label_0_100_IMG.x(), label_0_100_IMG.y());
  cv::putText(*ptr_img, "100m", point_label_0_100_IMG, font_, 0.5,
              cv::Scalar(255, 255, 255), thickness);
}

void IMGPROCESSOR::set_img_folder(std::string img_folder) {
  img_folder_ = img_folder;
}

Vec2d IMGPROCESSOR::Convert2IMG(const Vec2d &pt) {
  Vec2d pt_IMG;
  Eigen::Vector3d pt_3d{pt.x(), pt.y(), 0};
  Eigen::Vector3d pt_3d_IMG = Convert2IMG(pt_3d);
  pt_IMG.set_x(pt_3d_IMG(0));
  pt_IMG.set_y(pt_3d_IMG(1));
  return pt_IMG;
}

Eigen::Vector3d IMGPROCESSOR::Convert2IMG(const Eigen::Vector3d &pos_enu) {
  Eigen::Vector3d pos_enu_IMG;
  pos_enu_IMG[0] = pos_enu[0] * scale_ + (W_ / 2);
  pos_enu_IMG[1] = -pos_enu[1] * scale_ + (H_ / 2) + shiftY_;
  pos_enu_IMG[2] = pos_enu[2] * scale_;
  return pos_enu_IMG;
}

void IMGPROCESSOR::PlotVehicle(cv::Mat *ptr_img, sp_cVehicleBase vehicle,
                               cv::Scalar color) {
  PlotPosition(ptr_img, vehicle, color);
  PlotTriangleContour(ptr_img, vehicle, color);
  PlotVehicleID(ptr_img, vehicle, color);
  // PlotCog(ptr_img, vehicle, color);
  PlotTrajectory(ptr_img, vehicle, color);
  PlotVelocity(ptr_img, vehicle, color);
}

void IMGPROCESSOR::PlotVehicleCount(cv::Mat *ptr_img, sp_cRaysunFrame frame) {
  int thickness = 2;
  cv::Scalar plot_color = GetColorByID(frame->head().devid);
  cv::Point2d title_position = GetTitlePositionByID(frame->head().devid);
  std::string context = std::to_string(frame->targets().size()) + " vehicles";
  cv::putText(*ptr_img, context, title_position, font_, 1.0, plot_color,
              thickness);
}

void IMGPROCESSOR::PlotPosition(cv::Mat *ptr_img, sp_cVehicleBase vehicle,
                                cv::Scalar color) {
  Eigen::Vector3d Center_ENU = vehicle->pos_enu();
  Vec2d Center_ENU_2d{Center_ENU[0], Center_ENU[1]};
  Vec2d Center_ENU_IMG = Convert2IMG(Center_ENU_2d);
  cv::Point center_pos(Center_ENU_IMG.x(), Center_ENU_IMG.y());
  cv::circle(*ptr_img, center_pos, 2, color, 2);
}

void IMGPROCESSOR::PlotTrajectory(cv::Mat *ptr_img, sp_cVehicleBase vehicle,
                                  cv::Scalar color) {
  PlotLine3D(ptr_img, vehicle->trajectory_enu(), color);
}

void IMGPROCESSOR::PlotTriangleContour(cv::Mat *ptr_img,
                                       sp_cVehicleBase vehicle,
                                       cv::Scalar color) {
  std::vector<common::math::Vec2d> corners;
  corners = vehicle->GetTriangleContourENU();
  PlotPoly(ptr_img, corners, color);
}

void IMGPROCESSOR::PlotVehicleID(cv::Mat *ptr_img, sp_cVehicleBase vehicle,
                                 cv::Scalar color) {
  int thickness = 1;

  // plot object ID and Class at object center
  unsigned int id = vehicle->id();
  Eigen::Vector3d Center_ENU = vehicle->pos_enu();
  Vec2d Center_ENU_2d{Center_ENU[0] + 1, Center_ENU[1]};
  Vec2d Center_ENU_IMG = Convert2IMG(Center_ENU_2d);
  cv::Point ID_pos(Center_ENU_IMG.x(), Center_ENU_IMG.y());
  cv::putText(*ptr_img, std::to_string(id), ID_pos, font_, 0.5,
              cv::Scalar(255, 255, 255), thickness);
}

void IMGPROCESSOR::PlotCog(cv::Mat *ptr_img, sp_cVehicleBase vehicle,
                           cv::Scalar color) {
  int thickness = 1;

  // plot object ID and Class at object center
  int cog = vehicle->cog();
  Eigen::Vector3d Center_ENU = vehicle->pos_enu();
  Vec2d Center_ENU_2d{Center_ENU[0], Center_ENU[1]};
  Vec2d Center_ENU_IMG = Convert2IMG(Center_ENU_2d);
  cv::Point ID_pos(Center_ENU_IMG.x(), Center_ENU_IMG.y() - 50);
  cv::putText(*ptr_img, std::to_string(cog), ID_pos, font_, 0.5,
              cv::Scalar(255, 255, 255), thickness);
}

void IMGPROCESSOR::PlotVelocity(cv::Mat *ptr_img, sp_cVehicleBase vehicle,
                                cv::Scalar color) {
  Line Velocity_line = vehicle->GetVelocityArrowENU();
  Vec2d velocity_start_IMG = Convert2IMG(Velocity_line[0]);
  Vec2d velocity_end_IMG = Convert2IMG(Velocity_line[1]);

  int thickness = 1;
  // plot speed text
  // cv::putText(*ptr_img, std::to_string(vehicle->velocity_scale()),
  //             cv::Point(velocity_end_IMG.x(), velocity_end_IMG.y()), font_,
  //             0.5, cv::Scalar(255, 255, 255), thickness);

  cv::line(*ptr_img, cv::Point(velocity_start_IMG.x(), velocity_start_IMG.y()),
           cv::Point(velocity_end_IMG.x(), velocity_end_IMG.y()), color);
}

void IMGPROCESSOR::PlotMap(cv::Mat *ptr_img,
                           const std::vector<sp_cZMapLineSegment> &lines_enu) {
  for (const auto &line_enu : lines_enu) {
    if (line_enu->points_.size() < 2) {
      continue;
    }
    std::vector<cv::Point> pts_IMG;
    for (const auto &pt_enu : line_enu->points_) {
      Vec2d pt_enu_2d = Vec2d(pt_enu[0], pt_enu[1]);
      Vec2d pt_IMG = Convert2IMG(pt_enu_2d);
      pts_IMG.push_back(cv::Point(pt_IMG.x(), pt_IMG.y()));
    }

    for(const auto& pt_img:pts_IMG)
    {
      cv::circle(*ptr_img,pt_img,2,cv::Scalar(128, 128, 128));
    }

    for (int i = 1; i < pts_IMG.size(); i++) {
      cv::line(*ptr_img, pts_IMG[i - 1], pts_IMG[i], cv::Scalar(128, 128, 128),
               1, 8);
    }
  }
}

void IMGPROCESSOR::PlotCenterLine(cv::Mat *ptr_img,
                              const std::vector<Vec2d> &line_enu,
                              cv::Scalar color) {
  std::vector<cv::Point> pts_IMG;
  for (auto pt : line_enu) {
    auto pt_IMG = Convert2IMG(pt);
    pts_IMG.push_back(cv::Point(pt_IMG.x(), pt_IMG.y()));
  }
  for (int i = 1; i < pts_IMG.size(); i++) {
    cv::line(*ptr_img, pts_IMG[i - 1], pts_IMG[i], color, 1, 8);
  }
}

void IMGPROCESSOR::PlotLine3D(cv::Mat *ptr_img,
                              const std::vector<Eigen::Vector3d> &line_enu,
                              cv::Scalar color) {
  std::vector<cv::Point> pts_IMG;
  for (auto pt : line_enu) {
    Eigen::Vector3d pt_IMG = Convert2IMG(pt);
    pts_IMG.push_back(cv::Point(pt_IMG(0), pt_IMG(1)));
  }
  for (int i = 1; i < pts_IMG.size(); i++) {
    cv::line(*ptr_img, pts_IMG[i - 1], pts_IMG[i], color, 1, 8);
  }
}

void IMGPROCESSOR::PlotObsTrajectory(
    cv::Mat *ptr_img, std::map<int, spVehicleObs> vehicle_obsers) {
  for (const auto &obs : vehicle_obsers) {
    PlotLine3D(ptr_img, obs.second->trajectory_enu(),
               GetColorByID(obs.second->devid()));
  }
}

void IMGPROCESSOR::PlotPoly(cv::Mat *ptr_img, const Poly &poly,
                            cv::Scalar color) {
  std::vector<cv::Point> pts_IMG;
  for (auto pt : poly) {
    Vec2d pt_IMG = Convert2IMG(pt);
    pts_IMG.push_back(cv::Point(pt_IMG.x(), pt_IMG.y()));
  }
  cv::polylines(*ptr_img, pts_IMG, true, color, 2, 8);
}

cv::Scalar IMGPROCESSOR::GetColorByID(OCLong devid) {
  cv::Scalar plot_color;
  auto iter = plotColor_.find(devid);
  if (iter != plotColor_.end()) {
    plot_color = iter->second;
  } else {
    plot_color = cv::Scalar(255, 255, 255);
  }
  return plot_color;
}

cv::Point2d IMGPROCESSOR::GetTitlePositionByID(OCLong devid) {
  cv::Point2d title_position;
  auto iter = title_position_.find(devid);
  if (iter != title_position_.end()) {
    title_position = iter->second;
  } else {
    title_position = cv::Point2d(100, 100);
  }
  return title_position;
}

void IMGPROCESSOR::PlotFusionVehicles(
    cv::Mat *ptr_img, const std::map<int, spVehicleState> &vehicles) {
  PlotMap(ptr_img, map_->get_lines_enu());

  int thickness = 2;
  std::string context = std::to_string(vehicles.size()) + " vehicles";
  cv::putText(*ptr_img, context, cv::Point2d(100, 100), font_, 1.0,
              cv::Scalar(255, 255, 255), thickness);

  for (auto &&vehicle : vehicles) {
    PlotVehicle(ptr_img, vehicle.second);
  }
}

// plot the results from different cameras in one frame, and also the camera
// position
void IMGPROCESSOR::PlotMulCamsRaysunframe(cv::Mat *ptr_img,
                                          std::vector<spRaysunFrame> frames) {
  // cv::Mat result_img = cv::Mat(H_, W_, CV_32FC3, cv::Scalar(0, 0, 0));
  PlotMap(ptr_img, map_->get_lines_enu());
  for (auto frame : frames) {
    PlotRaysunframe(ptr_img, frame);
  }
}
void IMGPROCESSOR::PlotSnapshot(cv::Mat *ptr_img,
                                          std::map<int, spVehicleState> vehs) {
  // cv::Mat result_img = cv::Mat(H_, W_, CV_32FC3, cv::Scalar(0, 0, 0));
  PlotMap(ptr_img, map_->get_lines_enu());
  cv::Scalar plot_color = GetColorByID(103);
  for (auto veh : vehs) {
    PlotVehicle(ptr_img, veh.second, plot_color);
  }
  for (auto veh : vehs) {
    PlotLine3D(ptr_img, veh.second->trajectory_enu(),
              plot_color);
  }
}

void IMGPROCESSOR::PlotRaysunframe(cv::Mat *ptr_img, sp_cRaysunFrame frame) {
  PlotVehicleCount(ptr_img, frame);
  cv::Scalar plot_color = GetColorByID(frame->head().devid);
  for (const auto &vehicle : frame->targets()) {
    PlotVehicle(ptr_img, std::make_shared<VehicleBase>(vehicle), plot_color);
  }
}

void IMGPROCESSOR::SaveImage(cv::Mat *ptr_img, OCLong current_time) {
  std::string img_name = Unix2YMDHSMS(current_time);
  std::string file_path =
      std::string(PROJECT_ROOT_PATH) + "/img_both/" + img_name + ".jpg";
  std::cout << file_path << std::endl;
  cv::imwrite(file_path, *ptr_img);
}
}  // namespace v2x
}  // namespace coop
