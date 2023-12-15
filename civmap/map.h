#pragma once
#include <eigen3/Eigen/Core>
#include <vector>
#include "inner_types.hpp"
namespace civ
{
    namespace V2I
    {
        namespace map
        {
            class CivMap
            {
            public:
                CivMap();
                ~CivMap();
                /// @brief Read in the curves from a map.txt file
                /// @param file_path
                /// @return
                bool ReadData(std::string file_path);
                std::vector<sp_cZMapLineSegment> get_curves() { return lines_; };
                /// @brief get the curves in enu coordinate
                /// @return
                std::vector<sp_cZMapLineSegment> get_curves_enu();

                /// @brief the distance from a given llh point to the nearest point in map
                /// @param pt_llh point to search
                /// @param cross_pt_map_llh the nearest point on the map
                /// @return the distance

                double get_distance_pt_map_llh(const Eigen::Vector3d &pt_llh, Eigen::Vector3d &cross_pt_map_llh);
                /// @brief the distance from a given enu point to the nearest point in map
                /// @param pt_enu
                /// @param cross_pt_map_enu
                /// @return
                double get_distance_pt_map_enu(const Eigen::Vector3d &pt_enu, Eigen::Vector3d &cross_pt_map_enu);

                std::vector<sp_cZMapLineSegment> get_lanes_near_enu(const Eigen::Vector3d &pt_enu, double threshold);

                /// @brief calculate the distance from an ENU point to a curve
                /// @param pt_enu
                /// @param curve the curve to estimate distance
                /// @param cross_pt_curve the nearest point on the curve to the pt_enu
                /// @return distance
                double get_distance_pt_curve_enu(const Eigen::Vector3d &pt_enu, sp_cZMapLineSegment curve, Eigen::Vector3d &cross_pt_curve);


                double get_distance_pt_curve_enu(const Eigen::Vector3d &pt_enu, const std::vector<Eigen::Vector3d> curve, Eigen::Vector3d &cross_pt_curve);
                /// @brief 
                /// @param curve0 
                /// @param curve1 
                /// @return 
                double get_distance_curve_cuvre_enu(sp_cZMapLineSegment curve0,sp_cZMapLineSegment curve1);
            private:
                /// @brief
                /// @param pt_enu
                /// @param line_segment_two_points
                /// @return
                double min_distance_point_to_line(const Eigen::Vector3d &pt_enu, const std::vector<Eigen::Vector3d> &line_segment_two_points, Eigen::Vector3d &pt_nearest_enu);
                std::vector<sp_cZMapLineSegment> lines_;     // llh
                std::vector<sp_cZMapLineSegment> lines_enu_; // enu
                int id_indicator;
            };
            DEFINE_EXTEND_TYPE(CivMap);
        }
    } // namespace coop
}