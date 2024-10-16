#include <iostream>
#include "common/inner_types.hpp"
#include "mec/zsfd/inc/mapcore/shift.h"
// #include "common/type_define.hpp"

namespace civ
{
    namespace V2I
    {
        class TrajectoryProcessor
        {
        public:
            TrajectoryProcessor();
            ~TrajectoryProcessor();
            bool ReadTrajectory(std::string trajectory_path);
            sp_cZTrajectory get_trajectory_enu();
            sp_cZTrajectory get_trajectory(){return trajectory_;}
        private:
            spZTrajectory trajectory_;
            // the initial range to search lane candidates
        };
        
        DEFINE_EXTEND_TYPE(TrajectoryProcessor);
    }
}

