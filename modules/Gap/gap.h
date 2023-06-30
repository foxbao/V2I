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

        void set_alpha(double alpha);
        void set_beta(Eigen::Vector4d beta);

    private:
        double alpha_;
        Eigen::Vector4d beta_;
    };
} // namespace V2I