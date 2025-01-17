#include <fstream>
#include <Eigen/Core>
#include "imgprocessor.hpp"
#include "common/util/util.h"
namespace civ
{
    namespace V2I
    {
        namespace view
        {
            IMGPROCESSOR::IMGPROCESSOR()
            {
                scale_ = 10;
                H_ = 1000;
                W_ = 2000;
                shiftY_ = 2300;
                shiftX_ = -2200;
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

                sp_map_ = std::make_shared<civ::V2I::map::CivMap>();
                font_ = cv::FONT_HERSHEY_SIMPLEX;
            }
            IMGPROCESSOR::~IMGPROCESSOR() {}

            // void IMGPROCESSOR::SetMap(std::string map_path)
            // {
            //     sp_map_->ReadData(map_path);
            // }

            void IMGPROCESSOR::PlotMap(std::string map_path)
            {
                spCivMap sp_map = std::make_shared<civ::V2I::map::CivMap>();
                sp_map->ReadData(map_path);
                PlotMapEnu(&result_img_, sp_map->get_curves_enu());
                SaveImage(&result_img_);
            }

            void IMGPROCESSOR::PlotCurves(std::shared_ptr<std::vector<Curve>> sp_curves, bool i_showid, cv::Scalar color)
            {
                for (const auto &curve : *sp_curves)
                {
                    PlotCurveEnu(&result_img_, curve.p3d, color);
                    if (curve.p3d.size() > 0 && i_showid)
                    {
                        plotCurveId(&result_img_, curve.id, curve.p3d[0], color);
                    }
                }
            }

            void IMGPROCESSOR::PlotHDMap(std::shared_ptr<zas::mapcore::hdmap> sp_hdmap)
            {
                std::vector<Curve> central_curves_enu = sp_hdmap->get_central_curves_enu();
                std::vector<Curve> inner_boundaries_enu = sp_hdmap->get_inner_boundaries_enu();
                std::vector<Curve> outer_curves_enu = sp_hdmap->get_outer_boundaries_enu();

                std::shared_ptr<std::vector<Curve>> inner_boundaries_ptr = std::make_shared<std::vector<Curve>>(inner_boundaries_enu);
                std::shared_ptr<std::vector<Curve>> outer_curves_ptr = std::make_shared<std::vector<Curve>>(outer_curves_enu);
                std::shared_ptr<std::vector<Curve>> central_curves_ptr = std::make_shared<std::vector<Curve>>(central_curves_enu);

                PlotCurves(central_curves_ptr, true, cv::Scalar(128, 128, 128));
                PlotCurves(inner_boundaries_ptr, false, cv::Scalar(255, 0, 0));
                PlotCurves(outer_curves_ptr, false, cv::Scalar(255, 0, 0));
                SaveImage(&result_img_);
            }

            void IMGPROCESSOR::PlotTrajectory(spTrajectoryProcessor sp_traj_processor)
            {
                PlotTrajectoryEnu(&result_img_, sp_traj_processor->get_trajectory_enu());
                PlotTrajectoryInitialRange(&result_img_, sp_traj_processor, 5.0);
                SaveImage(&result_img_);
            }
            void IMGPROCESSOR::PlotTrajectory(std::string trajectory_path)
            {
                spTrajectoryProcessor sp_trajectory_processor = std::make_shared<TrajectoryProcessor>();
                sp_trajectory_processor->ReadTrajectory(trajectory_path);
                this->PlotTrajectory(sp_trajectory_processor);
            }

            void IMGPROCESSOR::PlotMapEnu(cv::Mat *ptr_img,
                                          const std::vector<sp_cZMapLineSegment> &curves_enu)
            {
                for (const auto &curve_enu : curves_enu)
                {
                    PlotCurveEnu(ptr_img, curve_enu->points_, cv::Scalar(128, 128, 128));
                }
            }

            void IMGPROCESSOR::PlotCurvesEnu(cv::Mat *ptr_img, const std::vector<std::vector<Eigen::Vector3d>> &curves_enu)
            {
                for (const auto &curve_enu : curves_enu)
                {
                    PlotCurveEnu(ptr_img, curve_enu, cv::Scalar(128, 128, 128));
                }
            }

            void IMGPROCESSOR::PlotTrajectoryEnu(cv::Mat *ptr_img, const sp_cZTrajectory &trajectory_enu)
            {
                PlotCurveEnu(ptr_img, trajectory_enu->points_, cv::Scalar(0, 0, 255));
                for (const auto &pt_enu : trajectory_enu->points_)
                {
                    PlotPointEnu(ptr_img, pt_enu, cv::Scalar(0, 0, 255));
                }
            }

            void IMGPROCESSOR::PlotPointEnu(cv::Mat *ptr_img, const Eigen::Vector3d &pt_enu, cv::Scalar color)
            {
                Eigen::Vector3d pt_IMG = Convert2IMG(pt_enu);
                cv::circle(*ptr_img, cv::Point(pt_IMG[0], pt_IMG[1]), 5, color, 1); // 画圆
            }

            void IMGPROCESSOR::plotCurveId(cv::Mat *ptr_img, const uint64_t &id, const Eigen::Vector3d &pt_enu, cv::Scalar color)
            {
                Eigen::Vector3d pt_IMG = Convert2IMG(pt_enu);
                cv::putText(*ptr_img, std::to_string(id), cv::Point(pt_IMG[0], pt_IMG[1]), cv::FONT_HERSHEY_PLAIN, 1.5, 255, 2);
            }

            void IMGPROCESSOR::PlotTrajectoryInitialRange(cv::Mat *ptr_img, spTrajectoryProcessor sp_trajectory_processor, double range_meter, cv::Scalar color)
            {
                std::vector<Eigen::Vector3d> points_enu = sp_trajectory_processor->get_trajectory_enu()->points_;
                for (const auto &pt : points_enu)
                {
                    Eigen::Vector3d pt_IMG = Convert2IMG(pt);
                    cv::circle(*ptr_img, cv::Point(pt_IMG[0], pt_IMG[1]), range_meter * scale_, color, 1); // 画圆
                }
            }

            Vec2d IMGPROCESSOR::Convert2IMG(const Vec2d &pt)
            {
                Vec2d pt_IMG;
                Eigen::Vector3d pt_3d{pt.x(), pt.y(), 0};
                Eigen::Vector3d pt_3d_IMG = Convert2IMG(pt_3d);
                pt_IMG.set_x(pt_3d_IMG(0));
                pt_IMG.set_y(pt_3d_IMG(1));
                return pt_IMG;
            }

            std::vector<cv::Point> IMGPROCESSOR::Convert2IMG(const std::vector<Eigen::Vector3d> &pts_enu)
            {
                std::vector<cv::Point> pts_IMG;
                for (const auto &pt_enu : pts_enu)
                {
                    Vec2d pt_enu_2d = Vec2d(pt_enu[0], pt_enu[1]);
                    Vec2d pt_IMG = Convert2IMG(pt_enu_2d);
                    pts_IMG.push_back(cv::Point(pt_IMG.x(), pt_IMG.y()));
                }
                return pts_IMG;
            }

            void IMGPROCESSOR::PlotCurveEnu(cv::Mat *ptr_img, const std::vector<Eigen::Vector3d> curve, cv::Scalar color)
            {
                if (curve.size() < 2)
                {
                    return;
                }
                std::vector<cv::Point> pts_IMG = Convert2IMG(curve);
                for (int i = 1; i < pts_IMG.size(); i++)
                {
                    cv::line(*ptr_img, pts_IMG[i - 1], pts_IMG[i], color,
                             1, 8);
                }
            }

            Eigen::Vector3d IMGPROCESSOR::Convert2IMG(const Eigen::Vector3d &pos_enu)
            {
                Eigen::Vector3d pos_enu_IMG;
                pos_enu_IMG[0] = pos_enu[0] * scale_ + (W_ / 2) + shiftX_;
                pos_enu_IMG[1] = -pos_enu[1] * scale_ + (H_ / 2) + shiftY_;
                pos_enu_IMG[2] = pos_enu[2] * scale_;
                return pos_enu_IMG;
            }

            void IMGPROCESSOR::PlotClosestPoints(std::string trajectory_path, std::string map_path)
            {
                spTrajectoryProcessor sp_trajectory_processor=std::make_shared<TrajectoryProcessor>();
                sp_trajectory_processor->ReadTrajectory(trajectory_path);


                spCivMap sp_map=std::make_shared<CivMap>();
                sp_map->ReadData(map_path);

                std::vector<Eigen::Vector3d> points_enu = sp_trajectory_processor->get_trajectory_enu()->points_;
                for (const auto &pt_enu : points_enu)
                {
                    Eigen::Vector3d cross_pt_map_enu;
                    double distance = sp_map->get_distance_pt_map_enu(pt_enu, cross_pt_map_enu);
                    PlotPointEnu(&result_img_, pt_enu, cv::Scalar(0, 0, 255));
                    PlotPointEnu(&result_img_, cross_pt_map_enu);
                }
                SaveImage(&result_img_);
            }

            // void IMGPROCESSOR::PlotClosestPointsCurve(const Eigen::Vector3d &pt, const std::vector<Eigen::Vector3d> &curve)
            // {
            //     Eigen::Vector3d cross_pt_enu;
            //     double closest_distance = sp_map_->get_distance_pt_curve_enu(pt, curve, cross_pt_enu);
            //     std::cout << cross_pt_enu.transpose() << std::endl;
            //     PlotPointEnu(&result_img_, cross_pt_enu, cv::Scalar(255, 0, 255));
            //     SaveImage(&result_img_);
            // }

            void IMGPROCESSOR::PlotClosestPointsLane(const Eigen::Vector3d &pt, const uint64_t &lane_id,std::shared_ptr<zas::mapcore::hdmap> sp_hdmap)
            {
                Eigen::Vector3d cross_pt_enu;
                double closest_distance = sp_hdmap->get_distance_pt_curve_enu(pt, lane_id, cross_pt_enu);
                std::cout << "purple distance:" << closest_distance << std::endl;
                std::cout << cross_pt_enu.transpose() << std::endl;
                PlotPointEnu(&result_img_, cross_pt_enu, cv::Scalar(255, 0, 255));
                SaveImage(&result_img_);
            }

            void IMGPROCESSOR::PlotCurveEnu(const std::vector<Eigen::Vector3d> &curve, cv::Scalar color)
            {
                PlotCurveEnu(&result_img_, curve, color);
                SaveImage(&result_img_);
            }

            void IMGPROCESSOR::SaveImage(cv::Mat *ptr_img, std::string img_name)
            {
                PROJECT_ROOT_PATH;
                // std::string img_name = "map";
                std::string file_path =
                    std::string(PROJECT_ROOT_PATH) + "/img_result/" + img_name;
                std::cout << "saving int to:" << file_path << std::endl;
                cv::imwrite(file_path, *ptr_img);
            }
        }
    } // namespace view
}