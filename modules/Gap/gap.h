#pragma once
#include <eigen3/Eigen/Core>
namespace V2I
{
    class gap
    {
    public:
        gap();
        ~gap();

        /// @brief Calculate the gap acceptance probability
        /// @param D2 distance between comming vehicle and conflict point
        /// @param V2 speed of comming vehicle
        /// @param D1 distance between ego vehicle and conflict point
        /// @param V1 speed of ego vehicle
        /// @return probability of gap acceptance
        double CalculateGap(double D2, double V2, double D1, double V1);

    private:
        double alpha;
        Eigen::Vector4d beta;
    };
} // namespace V2I