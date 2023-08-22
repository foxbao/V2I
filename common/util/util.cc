#include "util/util.h"
#include <cstring>
namespace civ
{
    namespace common
    {
        namespace util
        {
            double const g_ori_pos_deg[3]{31.284156453, 121.170937985, 16.504};
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