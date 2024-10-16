#include "util/util.h"
#include <cstring>
namespace civ
{
    namespace common
    {
        namespace util
        {
            // use the following website to convert mercator offset from map provider to llh
            // 325649.707155,3462338.816188, zone=51=>121.1684194823 , 31.2823264458
            // https://www.lddgo.net/coordinate/projection
            // double const g_ori_pos_deg[3]{31.284156453, 121.170937985, 16.504}; // original point set by Bao
            double const g_ori_pos_deg[3]{31.2823264458, 121.1684194823, 16.504};    // original point set by Zhonghaiting
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
        }
    }
}