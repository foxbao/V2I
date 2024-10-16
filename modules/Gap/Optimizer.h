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

        /// @brief Perform maximum likelihood estimation using gradient descent
        /// @param X Input data NxK
        /// @param y Output data Nx1
        /// @param learningRate learning rate of gradient descent
        /// @param numIterations
        /// @return
        Eigen::VectorXd logisticRegressionMLE(const Eigen::MatrixXd &X, const Eigen::VectorXd &y, double learningRate, int numIterations);

        /// @brief With Error! Perform maximum likelihood estimation using Newton-Raphson. Don't use, there are bugs
        /// @param X Input data NxK
        /// @param y Output data Nx1
        /// @param numIterations 
        /// @return 
        Eigen::VectorXd logisticRegressionMLE(const Eigen::MatrixXd &X, const Eigen::VectorXd &y, int numIterations);

    private:
    };
}