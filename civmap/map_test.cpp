#include <iostream>
#include "map.h"

int main()
{
    using namespace V2I;
    std::string file_path("/home/baojiali/Projects/V2I/map_data/map.txt");
    spCivMap sp_map = std::make_shared<CivMap>();
    if (!sp_map->ReadData(file_path))
    {
        return false;
    }                                                                                       
    std::vector<sp_cZMapLineSegment> lines = sp_map->get_lines_enu();
    std::cout<<"line number:"<<lines.size()<<std::endl;
    std::cout << "map_test" << std::endl;
    return 0;
}
