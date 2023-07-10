#pragma once
#include <eigen3/Eigen/Core>
#include <vector>
namespace V2I
{
    class Optimizer
    {
    public:
        Optimizer();
        ~Optimizer();
        /// @brief 
        /// @param beta parameter
        /// @param y_batch data output
        /// @param x_batch data input
        /// @return 
        Eigen::VectorXd Jacobian(Eigen::VectorXd beta, std::vector<double> y_batch, std::vector<Eigen::VectorXd> x_batch);
        /// @brief 
        /// @param beta parameter
        /// @param y_batch 
        /// @param x_batch 
        /// @return 
        Eigen::MatrixXd Hessian(Eigen::VectorXd beta,std::vector<double> y_batch, std::vector<Eigen::VectorXd> x_batch);
        double Newton_Raphson(std::vector<double> y_batch, std::vector<Eigen::VectorXd> x_batch);
        double logit(Eigen::VectorXd beta, Eigen::VectorXd x);
    private:
        Eigen::Vector2d beta_;
    };
}