#include <iostream>
#include <memory>
#include "imgprocessor.hpp"
#include "common/coordinate_transform/earth.hpp"
#include "common/util/util.h"
#include <boost/array.hpp>

int main()
{
    using namespace std;
    boost::array<int, 4> arr = {{1, 2, 3, 4}};
    cout << "hi:" << arr[0] << std::endl;
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

    return 0;
}