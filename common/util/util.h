#pragma once
#include <vector>
#include <iostream>
namespace civ
{
    namespace common
    {
        namespace util
        {
            extern const double g_ori_pos_deg[3]; //llh

            std::vector<std::string> split(const std::string &str,
                                           const std::string &pattern);

#define PROJECT_ROOT_PATH "/home/baojiali/Projects/civ/V2I"

        }
    }
}
