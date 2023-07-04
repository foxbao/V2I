#pragma once
#include <eigen3/Eigen/Core>
#include <vector>
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

        /// @brief calculate the logistic regression of input
        /// @param beta parameter of logistic regression
        /// @param x input data
        /// @return probability
        double logit(std::vector<double> beta, std::vector<double> x);


        double logit(Eigen::Vector2d beta, double x);
        void set_alpha(double alpha);
        void set_beta(Eigen::Vector4d beta);

    private:
        double alpha_;
        Eigen::Vector4d beta_;
    };
} // namespace V2I