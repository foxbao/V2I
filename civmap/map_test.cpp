#include <iostream>
#include "map.h"
#include "common/coordinate_transform/earth.hpp"
#include "common/util/util.h"
#include <boost/array.hpp>
int main()
{
    using namespace std;
    boost::array<int, 4> arr = {{1,2,3,4}};
    cout << "hi" << arr[0];
    using namespace civ::V2I::map;
    civ::common::coord_transform::Earth::SetOrigin(Eigen::Vector3d(civ::common::util::g_ori_pos_deg[0],
                                    civ::common::util::g_ori_pos_deg[1],
                                    civ::common::util::g_ori_pos_deg[2]),
                   true);
    std::string file_path("/home/baojiali/Projects/V2I/map_data/map.txt");
    spCivMap sp_map = std::make_shared<CivMap>();
    if (!sp_map->ReadData(file_path))
    {
        return false;
    }                                                                                       
    std::vector<sp_cZMapLineSegment> lines_enu = sp_map->get_curves_enu();
    std::cout<<"line number:"<<lines_enu.size()<<std::endl;
    std::cout << "map_test" << std::endl;
    return 0;
}
