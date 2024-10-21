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

#include "mec/zsfd/inc/mapcore/hdmap.h"

// This is the sample program which optimize the trajectory with HD map, and illustrates the
// result with imgprocessor

int main()
{
    using namespace civ::common::math;
    using namespace civ::V2I;
    using namespace civ::V2I::view;

    // Set the original point of ENU coordinate
    civ::common::coord_transform::Earth::SetOrigin(Eigen::Vector3d(civ::common::util::g_ori_pos_deg[0],
                                                                   civ::common::util::g_ori_pos_deg[1],
                                                                   civ::common::util::g_ori_pos_deg[2]),
                                                   true);

    std::shared_ptr<IMGPROCESSOR> sp_imgprocessor = std::make_shared<IMGPROCESSOR>();
    spTrajectoryProcessor sp_trajectory_processor = std::make_shared<TrajectoryProcessor>();
    // Use the trajectory processor to read the trajectory, and then pass the pointer to imgprocessor to plot the trajectory
    sp_trajectory_processor->ReadTrajectory("/home/baojiali/Projects/civ/V2I/data/trajectory.txt");
    sp_imgprocessor->PlotTrajectory(sp_trajectory_processor);

    using namespace civ::V2I::modules;

    // Read the simplified HD map which contains only the centers lines, and pass the 
    // map reader to image processor to plot
    std::shared_ptr<zas::mapcore::hdmap> sp_hdmap = std::make_shared<zas::mapcore::hdmap>();
    std::string map_path = "/home/baojiali/Projects/civ/V2I/map_data/hdmap";
    sp_hdmap->load_fromfile(map_path.c_str());
    sp_imgprocessor->PlotHDMap(sp_hdmap);

    // plot the projection of trajectory on the closest lane
    // sp_imgprocessor->PlotClosestMapPointToTrajectory("/home/baojiali/Projects/civ/V2I/data/trajectory.txt");
    // Read the full map which contains the relation between lanes 
    std::string json_path="file:///home/baojiali/Projects/civ/V2I/map_data/lanesec.json";
    sp_hdmap->generate_lanesect_transition(json_path.c_str());

    std::shared_ptr<civ::V2I::modules::HMMLoc> sp_model = std::make_shared<civ::V2I::modules::HMMLoc>();
    // Apply HMM on the map, trajectory and calculate the best matching states with HMM
    sp_model->SetHDMap(sp_hdmap);
    // std::vector<uint64_t> states_lane = sp_model->viterbiAlgorithmHDMap(sp_trajectory_processor->get_trajectory_enu());
    std::vector<uint64_t> states_lane = sp_model->viterbiAlgorithmHDMap2(sp_trajectory_processor->get_trajectory_enu());

    std::cout << "best states:";
    for (int i = 0; i < states_lane.size(); i++)
    {
        std::cout << states_lane[i] << " ";
    }
    std::cout << std::endl;

    for (int i = 0; i < states_lane.size(); i++)
    {
        Eigen::Vector3d pt_obs = sp_trajectory_processor->get_trajectory_enu()->points_[i];
        sp_imgprocessor->PlotClosestPointsLane(pt_obs, states_lane[i]);
    }

    return 0;
}