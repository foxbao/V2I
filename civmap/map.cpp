#include "map.h"
#include <fstream>
#include <iostream>
// #include "modules/common/coordinate_transform/LocalCartesian_util.h"
// #include "modules/common/coordinate_transform/earth.hpp"
namespace V2I
{
    std::vector<std::string> split(const std::string &str,
                                   const std::string &pattern)
    {
        // const char* convert to char*
        char *strc = new char[strlen(str.c_str()) + 1];
        strcpy(strc, str.c_str());
        std::vector<std::string> resultVec;

        char *tmpStr = strtok(strc, pattern.c_str());
        while (tmpStr != NULL)
        {
            resultVec.push_back(std::string(tmpStr));
            tmpStr = strtok(NULL, pattern.c_str());
        }

        delete[] strc;

        return resultVec;
    }

    CivMap::CivMap() {}

    CivMap::~CivMap() {}

    bool CivMap::ReadData(std::string file_path)
    {
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
            // std::cout << str_data << std::endl;
            if (str_data.find("---") != string::npos)
            {
                lines_.push_back(line);
                line = std::make_shared<ZMapLineSegment>();
                // std::cout << "new one" << std::endl;
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

    std::vector<sp_cZMapLineSegment> CivMap::get_lines_enu()
    {
        //   using namespace civ::common::coord_transform;
        std::vector<sp_cZMapLineSegment> lines_enu;
        for (const auto &line_llh : lines_)
        {
            ZMapLineSegment line;
            for (const auto &pt_llh : line_llh->points_)
            {
                Eigen::Vector3d pt_enu;
                //       // Eigen::Vector3d pt_enu = Earth::LLH2ENU(pt_llh, true);
                //       ConvertLLAToENU(Earth::GetOrigin(), pt_llh, &pt_enu);
                //       line.points_.push_back(pt_enu);
            }
            //     lines_enu.push_back(std::make_shared<ZMapLineSegment>(line));
        }
        return lines_enu;
    }

} // namespace coop
