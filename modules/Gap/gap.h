#pragma once
#include <eigen3/Eigen/Core>
// #include <eigen3/Eigen/Dense>
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

        /// @brief 
        /// @param x 
        /// @param theta 
        /// @return 
        double logit( const Eigen::VectorXd &x,const Eigen::VectorXd& theta);
        /// @brief 
        /// @param X 
        /// @param theta 
        /// @return 
        Eigen::VectorXd logit(const Eigen::MatrixXd &X, const Eigen::VectorXd &theta);

        void set_theta(const Eigen::VectorXd& theta);
    private:
        Eigen::VectorXd theta_;
    };
} // namespace V2I