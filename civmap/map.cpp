
#include <fstream>
#include <iostream>
#include <cmath>
#include "common/coordinate_transform/LocalCartesian_util.h"
#include "common/coordinate_transform/earth.hpp"
#include "common/util/util.h"
#include "common/common.h"
#include "map.h"
namespace civ
{
    namespace V2I
    {
        namespace map
        {
            CivMap::CivMap()
            {
                id_indicator = 0;
            }

            CivMap::~CivMap() {}

            bool CivMap::ReadData(std::string file_path)
            {
                id_indicator = 0;
                lines_.clear();
                using namespace std;
                ifstream file;
                file.open(file_path.c_str(), ios_base::in);
                if (!file.is_open())
                {
                    std::cout << "打开地图失败";
                    return false;
                }
                string str_data;
                spZMapLineSegment line = std::make_shared<ZMapLineSegment>();
                while (getline(file, str_data))
                {
                    if (str_data.find("---") != string::npos)
                    {
                        line->id_ = std::to_string(id_indicator);
                        id_indicator++;
                        lines_.push_back(line);
                        line = std::make_shared<ZMapLineSegment>();
                    }
                    else
                    {
                        std::vector<std::string> split_result = split(str_data, " ");
                        double lat = stod(split_result[0]);
                        double lon = stod(split_result[1]);
                        double alt = stod(split_result[2]);
                        if (alt > 100 || alt < -100)
                        {
                            continue;
                        }
                        line->points_.push_back(Eigen::Vector3d(lat, lon, alt));
                    }
                }
                file.close();
                return true;
            }

            std::vector<sp_cZMapLineSegment> CivMap::get_curves_enu()
            {
                //   using namespace civ::common::coord_transform;
                std::vector<sp_cZMapLineSegment> lines_enu;
                for (const auto &line_llh : lines_)
                {
                    ZMapLineSegment line;
                    line.id_=line_llh->id_;
                    for (const auto &pt_llh : line_llh->points_)
                    {
                        Eigen::Vector3d pt_enu;
                        //       // Eigen::Vector3d pt_enu = Earth::LLH2ENU(pt_llh, true);
                        using namespace civ::common::coord_transform;
                        ConvertLLAToENU(Earth::GetOrigin(), pt_llh, &pt_enu);
                        line.points_.push_back(pt_enu);
                    }
                    lines_enu.push_back(std::make_shared<ZMapLineSegment>(line));
                }
                return lines_enu;
            }

            double CivMap::get_distance_pt_map_llh(const Eigen::Vector3d &pt_llh, Eigen::Vector3d &cross_pt_map_llh)
            {
                Eigen::Vector3d pt_enu;
                Eigen::Vector3d cross_pt_map_enu;
                using namespace civ::common::coord_transform;
                ConvertLLAToENU(Earth::GetOrigin(), pt_llh, &pt_enu);
                double distance = get_distance_pt_map_enu(pt_enu, cross_pt_map_enu);
                ConvertENUToLLA(Earth::GetOrigin(), cross_pt_map_enu, &cross_pt_map_llh);
                return distance;
            }

            double CivMap::get_distance_pt_map_enu(const Eigen::Vector3d &pt_enu, Eigen::Vector3d &cross_pt_map_enu)
            {
                std::vector<sp_cZMapLineSegment> lines_enu = get_curves_enu();
                using namespace civ::common;
                double closest_distance = MAXVAL;
                for (const auto &curve_enu : lines_enu)
                {
                    Eigen::Vector3d cross_pt_curve_enu;
                    double distance = get_distance_pt_curve_enu(pt_enu, curve_enu, cross_pt_curve_enu);
                    if (distance < closest_distance)
                    {
                        closest_distance = distance;
                        cross_pt_map_enu = cross_pt_curve_enu;
                    }
                }
                return closest_distance;
            }

            std::vector<sp_cZMapLineSegment> CivMap::get_lanes_near_enu(const Eigen::Vector3d &pt_enu, double threshold)
            {
                std::vector<sp_cZMapLineSegment> lanes_near;
                std::vector<sp_cZMapLineSegment> lines_enu = get_curves_enu();
                for (const auto &curve_enu : lines_enu)
                {
                    Eigen::Vector3d cross_pt_curve_enu;
                    double distance = get_distance_pt_curve_enu(pt_enu, curve_enu, cross_pt_curve_enu);
                    if (distance < threshold)
                    {
                        lanes_near.push_back(curve_enu);
                    }
                }
                return lanes_near;
            }

            double CivMap::get_distance_pt_curve_enu(const Eigen::Vector3d &pt_enu, sp_cZMapLineSegment curve, Eigen::Vector3d &cross_pt_curve)
            {
                using namespace civ::common;

                double closest_distance=get_distance_pt_curve_enu(pt_enu,curve->points_,cross_pt_curve);
                // if (curve->points_.size() < 2)
                // {
                //     return MAXVAL;
                // }

                // Eigen::Vector3d crossing_pt_line;
                // double closest_distance = MAXVAL;
                // for (int i = 1; i < curve->points_.size(); i++)
                // {
                //     std::vector<Eigen::Vector3d> line_segment_two_points;
                //     line_segment_two_points.push_back(curve->points_[i - 1]);
                //     line_segment_two_points.push_back(curve->points_[i]);
                //     double distance_to_line = min_distance_point_to_line(pt_enu, line_segment_two_points, crossing_pt_line);
                //     if (distance_to_line < closest_distance)
                //     {
                //         closest_distance = distance_to_line;
                //         cross_pt_curve = crossing_pt_line;
                //     }
                // }
                return closest_distance;
            }

            double CivMap::get_distance_pt_curve_enu(const Eigen::Vector3d &pt_enu, const std::vector<Eigen::Vector3d> curve, Eigen::Vector3d &cross_pt_curve)
            {
                using namespace civ::common;
                if (curve.size() < 2)
                {
                    return MAXVAL;
                }

                Eigen::Vector3d crossing_pt_line;
                double closest_distance = MAXVAL;
                for (int i = 1; i < curve.size(); i++)
                {
                    std::vector<Eigen::Vector3d> line_segment_two_points;
                    line_segment_two_points.push_back(curve[i - 1]);
                    line_segment_two_points.push_back(curve[i]);
                    double distance_to_line = min_distance_point_to_line(pt_enu, line_segment_two_points, crossing_pt_line);
                    if (distance_to_line < closest_distance)
                    {
                        closest_distance = distance_to_line;
                        cross_pt_curve = crossing_pt_line;
                    }
                }
                return closest_distance;
            }

            double CivMap::get_distance_curve_cuvre_enu(sp_cZMapLineSegment curve0, sp_cZMapLineSegment curve1)
            {
                double min_distance=MAXVAL;
                for(const auto& pt_enu:curve0->points_)
                {
                    Eigen::Vector3d cross_pt_curve_enu;
                    double distance = get_distance_pt_curve_enu(pt_enu, curve1, cross_pt_curve_enu);
                    if(distance<min_distance)
                    {
                        min_distance=distance;
                    }
                }
                return min_distance;
            }

            double CivMap::min_distance_point_to_line(const Eigen::Vector3d &pt_enu, const std::vector<Eigen::Vector3d> &line_segment_two_points, Eigen::Vector3d &pt_nearest_enu)
            {
                // https://www.cnblogs.com/flyinggod/p/9359534.html
                if (line_segment_two_points.size() != 2)
                {
                    using namespace civ::common;
                    return MAXVAL;
                }
                Eigen::Vector3d point_start = line_segment_two_points[0];
                Eigen::Vector3d point_end = line_segment_two_points[1];

                double x = pt_enu[0];
                double y = pt_enu[1];
                double x1 = point_start[0];
                double y1 = point_start[1];
                double x2 = point_end[0];
                double y2 = point_end[1];
                double cross = (x2 - x1) * (x - x1) + (y2 - y1) * (y - y1);
                if (cross <= 0)
                {
                    pt_nearest_enu[0]=x1;
                    pt_nearest_enu[1]=y1;
                    return sqrt((x - x1) * (x - x1) + (y - y1) * (y - y1));
                }

                double d2 = (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1);
                if (cross >= d2)
                {
                    pt_nearest_enu[0]=x2;
                    pt_nearest_enu[1]=y2;
                    return sqrt((x - x2) * (x - x2) + (y - y2) * (y - y2));
                }
                double r = cross / d2;
                double px = x1 + (x2 - x1) * r;
                double py = y1 + (y2 - y1) * r;
                pt_nearest_enu[0] = px;
                pt_nearest_enu[1] = py;
                if (sqrt((x - px) * (x - px) + (py - y) * (py - y)) == 0)
                {
                    int cccc = 2;
                }
                return sqrt((x - px) * (x - px) + (py - y) * (py - y));
            }
        }
    }
}