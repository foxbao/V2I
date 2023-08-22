#include <iostream>
#include "common/inner_types.hpp"
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
        };
        DEFINE_EXTEND_TYPE(TrajectoryProcessor);
    }
}