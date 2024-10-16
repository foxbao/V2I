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
#include <map>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include "modules/common/util/inner_types.hpp"
#include "modules/common/util/util.h"
#include "modules/dataType/raysunframe.hpp"
#include "modules/dataType/vehicle_state.h"
#include "modules/map/map.hpp"

namespace coop {
namespace v2x {

using Vec2d = common::math::Vec2d;
using Poly = std::vector<common::math::Vec2d>;
using Line = std::vector<common::math::Vec2d>;

class IMGPROCESSOR {
 public:
  IMGPROCESSOR();

  void PlotCombinedResult(cv::Mat* ptr_img, const cv::Mat& vehicle_fusion_img,
                          const cv::Mat& camera_merge_img, OCLong current_time,
                          OCLong last_time);
  void PlotFusionVehicles(cv::Mat* ptr_img,
                          const std::map<int, spVehicleState>& vehicles);
  /**
   * @brief Plot raysun frame data from multi device with different color
   * @param frames Fused vehicle states to plot
   * @return cv::Mat on which the observed vehicles are plotted
   */

  void PlotMulCamsRaysunframe(cv::Mat* ptr_img,
                              std::vector<spRaysunFrame> frames);
  /**
   * @brief Plot the vehicle states on left and the synced observation result on
   * right
   * @param vehicles Fused vehicle states to plot
   * @param syncFrames Synced observation observation to plot
   * @return
   */
  void PlotRaysunframe(cv::Mat* ptr_img, sp_cRaysunFrame frame);
  /**
   * @brief Plot the vehicle states on left and the synced observation result on
   * right
   * @param vehicles Fused vehicle states to plot
   * @param syncFrames Synced observation observation to plot
   * @return
   */
  void PlotVehicleStatesAndFrames(cv::Mat* ptr_img,
                                  std::map<int, spVehicleState> vehicles,
                                  std::vector<spRaysunFrame> syncFrames,
                                  std::map<int, spVehicleObs> vehicle_obsers,
                                  OCLong current_time, OCLong last_time);

  void PlotVehicleSnapshot(cv::Mat* ptr_img,
                                  std::map<int, spVehicleState> origin,
                                  std::map<int, spVehicleState> transf,
                                  OCLong current_time,
                                  OCLong last_time, OCDouble x = 0.0, OCDouble y = 0.0);

  void PlotCenterLine(cv::Mat* ptr_img,
                  const std::vector<Vec2d>& line_enu,
                  cv::Scalar color = cv::Scalar(255, 255, 255));

  void SaveImage(cv::Mat* ptr_img, OCLong current_time);
  void set_img_folder(std::string img_folder);

 private:
  Eigen::Vector3d Convert2IMG(const Eigen::Vector3d& pos_enu);
  Vec2d Convert2IMG(const Vec2d& pt);
  void PlotLine(cv::Mat* ptr_img, const Line& line,
                cv::Scalar color = cv::Scalar(255, 255, 255));
  void PlotLine3D(cv::Mat* ptr_img,
                  const std::vector<Eigen::Vector3d>& line_enu,
                  cv::Scalar color = cv::Scalar(255, 255, 255));

  /**
   * @brief Plot map lines on image
   * @param ptr_img image
   * @param lines_enu map lines
   * @return
   */
  void PlotMap(cv::Mat* ptr_img,
               const std::vector<sp_cZMapLineSegment>& lines_enu);
  void PlotObsTrajectory(cv::Mat* ptr_img,std::map<int, spVehicleObs> vehicle_obsers);
  void PlotPoly(cv::Mat* ptr_img, const Poly& poly,
                cv::Scalar color = cv::Scalar(255, 255, 255));
  void PlotPosition(cv::Mat* ptr_img, sp_cVehicleBase vehicle,
                    cv::Scalar color = cv::Scalar(255, 255, 255));
  void PlotTrajectory(cv::Mat* ptr_img, sp_cVehicleBase vehicle,
                      cv::Scalar color = cv::Scalar(255, 255, 255));
  void PlotTriangleContour(cv::Mat* ptr_img, sp_cVehicleBase vehicle,
                           cv::Scalar color = cv::Scalar(255, 255, 255));
  void PlotVehicle(cv::Mat* ptr_img, sp_cVehicleBase vehicle,
                   cv::Scalar color = cv::Scalar(255, 255, 255));
  void PlotVehicleCount(cv::Mat* ptr_img, sp_cRaysunFrame frame);
  void PlotVehicleID(cv::Mat* ptr_img, sp_cVehicleBase vehicle,
                     cv::Scalar color = cv::Scalar(255, 255, 255));
  void PlotVelocity(cv::Mat* ptr_img, sp_cVehicleBase vehicle,
                    cv::Scalar color = cv::Scalar(255, 255, 255));
  void PlotCog(cv::Mat* ptr_img, sp_cVehicleBase vehicle,
               cv::Scalar color = cv::Scalar(255, 255, 255));

  void PlotSnapshot(cv::Mat* ptr_img,
                      std::map<int, spVehicleState> veh);
                      
  cv::Scalar GetColorByID(OCLong devid);
  cv::Point2d GetTitlePositionByID(OCLong devid);
  void PlotAxis(cv::Mat* img);

 private:
  cv::Mat result_img_;
  int scale_;
  int H_;
  int W_;
  int shiftY_ = 450;
  int shiftX_ = 0;
  std::string img_folder_;
  std::string img_nofuse_folder_;
  std::map<int, cv::Scalar> plotColor_;
  std::map<int, cv::Point2d> title_position_;
  std::vector<cv::Scalar> color_set_;
  spCivMap map_;
  int font_;
};
}  // namespace v2x
}  // namespace coop
