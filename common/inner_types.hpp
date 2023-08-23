#pragma once
#include <vector>
#include <eigen3/Eigen/Core>
#include "common/type_define.hpp"
namespace civ
{
    namespace V2I
    {
        using namespace civ::common::util;
        struct ZTrajectory
        {
            std::string id_;                      // string id
            std::vector<Eigen::Vector3d> points_; // 每一个点
        };
        DEFINE_EXTEND_TYPE(ZTrajectory);


    }
}