#include <iostream>
#include <memory>
#include "imgprocessor.hpp"
#include "common/coordinate_transform/earth.hpp"
#include "common/util/util.h"
#include "modules/HMM/hmm.h"
#include "modules/HMM/hmm_loc.h"
#include "modules/inner_types.hpp"
#include <boost/array.hpp>

#include "common/math/line_segment.hpp"
int main()
{
    using namespace civ::common::math;

    Eigen::Vector3d a1(1, 1, 0);
    Eigen::Vector3d a0(3, 1, 0);
    Eigen::Vector3d b0(1, 2, 0);
    Eigen::Vector3d b1(3, 1.5, 0);
    Eigen::Vector3d v(1, 2, 3);

    Eigen::Vector3d w(0, 1, 2);
    closestDistanceBetweenLines(a0, a1, b0, b1);
    // line l1;
    // line l2;
    // l1.x1 = 1;
    // l1.y1 = 1;
    // l1.z1 = 0.0;
    // l1.x2 = 3;
    // l1.y2 = 1;
    // l1.z1 = 0.0;

    // l2.x1 = 1;
    // l2.y1 = 2;
    // l2.z1 = 0.0;
    // l2.x2 = 3;
    // l2.y2 = 1;
    // l2.z2 = 0.0;

    // double distance = getShortestDistance(l1, l2);
    // std::cout<<"distance:"<<distance<<std::endl;

    using namespace civ::V2I;
    using namespace civ::V2I::view;
    civ::common::coord_transform::Earth::SetOrigin(Eigen::Vector3d(civ::common::util::g_ori_pos_deg[0],
                                                                   civ::common::util::g_ori_pos_deg[1],
                                                                   civ::common::util::g_ori_pos_deg[2]),
                                                   true);

    std::shared_ptr<IMGPROCESSOR> sp_imgprocessor = std::make_shared<IMGPROCESSOR>();
    sp_imgprocessor->PlotMap("/home/baojiali/Projects/V2I/map_data/map.txt");
    sp_imgprocessor->PlotTrajectory("/home/baojiali/Projects/V2I/data/trajectory.txt");
    sp_imgprocessor->PlotClosestPoints("/home/baojiali/Projects/V2I/data/trajectory.txt", "/home/baojiali/Projects/V2I/map_data/map.txt");

    spTrajectoryProcessor sp_trajectory_processor = std::make_shared<TrajectoryProcessor>();
    sp_trajectory_processor->ReadTrajectory("/home/baojiali/Projects/V2I/data/trajectory.txt");
    using namespace civ::V2I::modules;

    std::shared_ptr<civ::V2I::modules::HMMLoc> sp_model = std::make_shared<civ::V2I::modules::HMMLoc>();
    sp_model->ReadMap("/home/baojiali/Projects/V2I/map_data/map.txt");
    std::vector<sp_cState> states = sp_model->viterbiAlgorithm(sp_trajectory_processor->get_trajectory_enu());

    for (int i = 0; i < states.size(); i++)
    {
        Eigen::Vector3d pt_obs = sp_trajectory_processor->get_trajectory_enu()->points_[i];
        std::vector<Eigen::Vector3d> curve = states[i]->curve_->points_;
        sp_imgprocessor->PlotClosestPointsCurve(pt_obs, curve);
    }

    return 0;
}