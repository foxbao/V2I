#include <iostream>
#include <memory>
#include "imgprocessor.hpp"
#include "common/coordinate_transform/earth.hpp"
#include "common/util/util.h"
// #include "modules/HMM/hmm.h"
#include "modules/HMM/hmm_loc.h"
#include "modules/inner_types.hpp"
#include <boost/array.hpp>
#include "common/math/line_segment.hpp"
#include "opencv2/core/version.hpp"
int main()
{
    using namespace civ::common::math;

    using namespace civ::V2I;
    using namespace civ::V2I::view;
    std::cout << CV_VERSION << std::endl;
    civ::common::coord_transform::Earth::SetOrigin(Eigen::Vector3d(civ::common::util::g_ori_pos_deg[0],
                                                                   civ::common::util::g_ori_pos_deg[1],
                                                                   civ::common::util::g_ori_pos_deg[2]),
                                                   true);

    std::shared_ptr<IMGPROCESSOR> sp_imgprocessor = std::make_shared<IMGPROCESSOR>();
    spTrajectoryProcessor sp_trajectory_processor = std::make_shared<TrajectoryProcessor>();
    // read and plot the trajectory
    sp_trajectory_processor->ReadTrajectory("/home/baojiali/Projects/civ/V2I/data/trajectory.txt");
    sp_imgprocessor->PlotTrajectory("/home/baojiali/Projects/civ/V2I/data/trajectory.txt");

    // read the map and plot it
    using namespace civ::V2I::modules;
    sp_imgprocessor->SetHDMap("/home/baojiali/Projects/civ/V2I/map_data/hdmap");
    sp_imgprocessor->PlotHDMap();
    // plot the projection of trajectory on the closest lane
    // sp_imgprocessor->PlotClosestMapPointToTrajectory("/home/baojiali/Projects/civ/V2I/data/trajectory.txt");
    std::shared_ptr<civ::V2I::modules::HMMLoc> sp_model = std::make_shared<civ::V2I::modules::HMMLoc>();

    // Use hmm to read map, trajectory and calculate the best matching states with HMM
    sp_model->ReadHDMap("/home/baojiali/Projects/civ/V2I/map_data/hdmap", "file:///home/baojiali/Projects/civ/V2I/map_data/lanesec.json");
    // std::vector<uint64_t> states_lane = sp_model->viterbiAlgorithmHDMap(sp_trajectory_processor->get_trajectory_enu());
    std::vector<uint64_t> states_lane = sp_model->viterbiAlgorithmHDMap2(sp_trajectory_processor->get_trajectory_enu());

    std::cout << "best states:";
    for (int i = 0; i < states_lane.size(); i++)
    {
        std::cout << states_lane[i] << " ";
    }
    std::cout << std::endl;

    for (int i = 0; i <states_lane.size(); i++)
    {
        Eigen::Vector3d pt_obs = sp_trajectory_processor->get_trajectory_enu()->points_[i];
        sp_imgprocessor->PlotClosestPointsLane(pt_obs, states_lane[i]);
    }

    return 0;
}