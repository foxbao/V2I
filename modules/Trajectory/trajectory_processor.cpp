#include <fstream>
#include <vector>
#include "trajectory_processor.h"
#include "common/util/util.h"
#include "common/coordinate_transform/earth.hpp"
#include "common/coordinate_transform/LocalCartesian_util.h"

namespace civ
{
    namespace V2I
    {
        TrajectoryProcessor::TrajectoryProcessor()
        {
            trajectory_ = std::make_shared<ZTrajectory>();
        }

        TrajectoryProcessor::~TrajectoryProcessor()
        {
        }

        bool TrajectoryProcessor::ReadTrajectory(std::string file_path)
        {
            trajectory_->points_.clear();
            using namespace std;
            ifstream file;
            file.open(file_path.c_str(), ios_base::in);
            if (!file.is_open())
            {
                std::cout << "fail to open trajectory";
                return false;
            }

            string str_data;
            // spZMapLineSegment line = std::make_shared<ZMapLineSegment>();
            // spZMapLineSegment line = std::make_shared<ZMapLineSegment>();
            while (getline(file, str_data))
            {

                std::vector<std::string> split_result = civ::common::util::split(str_data, " ");
                double lat = stod(split_result[0]);
                double lon = stod(split_result[1]);
                double alt = stod(split_result[2]);
                if (alt > 100 || alt < -100)
                {
                    continue;
                }
                trajectory_->points_.push_back(Eigen::Vector3d(lat, lon, alt));
            }
            file.close();
            return true;
        }
        sp_cZTrajectory TrajectoryProcessor::get_trajectory_enu()
        {
            spZTrajectory line_enu = std::make_shared<ZTrajectory>();
            for (const auto &pt_llh : trajectory_->points_)
            {
                Eigen::Vector3d pt_enu;
                using namespace civ::common::coord_transform;
                ConvertLLAToENU(Earth::GetOrigin(), pt_llh, &pt_enu);
                line_enu->points_.push_back(pt_enu);
            }
            return line_enu;
        }
    }
}