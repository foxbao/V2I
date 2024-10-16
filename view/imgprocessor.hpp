#pragma once
#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include "civmap/map.h"
#include "common/math/vec2d.h"
#include "mec/zsfd/inc/mapcore/hdmap.h"
#include "modules/Trajectory/trajectory_processor.h"

namespace civ
{
    namespace V2I
    {
        namespace view
        {
            using Vec2d = civ::common::math::Vec2d;
            using namespace civ::V2I::map;
            using namespace zas::mapcore;
            class IMGPROCESSOR
            {
            public:
                IMGPROCESSOR();
                ~IMGPROCESSOR();
                void PlotMap(std::string map_path);
                void SetHDMap(std::string map_path);
                void PlotHDMap();
                void PlotTrajectory(std::string trajectory_path);
                /// @brief Plot the closest points of the trajectory on the map lane
                /// @param trajectory_path
                /// @param map_path
                void PlotClosestPoints(std::string trajectory_path, std::string map_path);

                void PlotClosestMapPointToTrajectory(std::string trajectory_path);
                /// @brief
                /// @param pt
                /// @param curve
                void PlotClosestPointsCurve(const Eigen::Vector3d &pt, const std::vector<Eigen::Vector3d> &curve);
                void PlotClosestPointsLane(const Eigen::Vector3d &pt, const uint64_t &state);
                /// @brief plot a curve of ENU coordinate
                /// @param curve the curve composed of points(x,y,z)
                /// @param color
                void PlotCurveEnu(const std::vector<Eigen::Vector3d> &curve, cv::Scalar color = cv::Scalar(0, 256, 128));
                void SaveImage(cv::Mat *ptr_img);

            private:
                void SetMap(std::string map_path);
                void SetTrajectory(std::string trajectory_path);
                /**
                 * @brief Plot map lines on image
                 * @param ptr_img image
                 * @param lines_enu map lines
                 * @return
                 */
                void PlotMapEnu(cv::Mat *ptr_img,
                                const std::vector<sp_cZMapLineSegment> &curves_enu);
                void PlotCurvesEnu(cv::Mat *ptr_img,
                                   const std::vector<std::vector<Eigen::Vector3d>> &curves_enu);
                void PlotTrajectoryEnu(cv::Mat *ptr_img,
                                       const sp_cZTrajectory &trajectory_enu);
                void PlotPointEnu(cv::Mat *ptr_img, const Eigen::Vector3d &pt_enu, cv::Scalar color = cv::Scalar(255, 255, 255));
                void PlotCurveId(cv::Mat *ptr_img, const uint64_t &id, const Eigen::Vector3d &pt_enu, cv::Scalar color = cv::Scalar(0, 256, 128));
                /// @brief Plot the circle of range of state selction of all the points in trajectory
                /// @param range range of selection, unit meter
                /// @param color
                void PlotTrajectoryInitialRange(cv::Mat *ptr_img,double range_meter = 5.0, cv::Scalar color = cv::Scalar(256, 128, 128));
                /// @brief
                /// @param ptr_img
                /// @param curve
                /// @param color
                void PlotCurveEnu(cv::Mat *ptr_img, const std::vector<Eigen::Vector3d> curve, cv::Scalar color = cv::Scalar(256, 128, 128));
                Eigen::Vector3d Convert2IMG(const Eigen::Vector3d &pt_enu);
                Vec2d Convert2IMG(const Vec2d &pt_enu);
                std::vector<cv::Point> Convert2IMG(const std::vector<Eigen::Vector3d> &pts);

            private:
                cv::Mat result_img_;
                int scale_;
                int H_;
                int W_;
                int shiftY_;
                int shiftX_;
                std::string img_folder_;
                std::string img_nofuse_folder_;
                std::map<int, cv::Scalar> plotColor_;
                std::map<int, cv::Point2d> title_position_;
                std::vector<cv::Scalar> color_set_;
                spCivMap sp_map_;
                std::shared_ptr<zas::mapcore::hdmap> sp_hdmap_;
                spTrajectoryProcessor sp_trajectory_processor_;
                int font_;
            };
            // DEFINE_EXTEND_TYPE(IMGPROCESSOR);
        }
    } // namespace civloc
} // namespace civ