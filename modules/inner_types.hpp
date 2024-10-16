#pragma once
#include <vector>
#include <eigen3/Eigen/Core>
#include "common/type_define.hpp"
#include "civmap/inner_types.hpp"
namespace civ
{
    namespace V2I
    {
        namespace modules
        {
            using namespace civ::V2I::map;
            class Bao
            {
                int aaa=2;
            };
            struct State
            {
                std::string id_;
                sp_cZMapLineSegment curve_;
            };

            DEFINE_EXTEND_TYPE(State);
        }
    }
}