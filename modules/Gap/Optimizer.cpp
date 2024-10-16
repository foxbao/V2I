#include <eigen3/Eigen/Dense>
#include <iostream>
#include "Optimizer.h"
#include "math.h"
namespace V2I
{
    Optimizer::Optimizer()
    {
    }
    Optimizer::~Optimizer()
    {
    }

    // Perform maximum likelihood estimation using gradient descent
    Eigen::VectorXd Optimizer::logisticRegressionMLE(const Eigen::MatrixXd &X, const Eigen::VectorXd &y, double learningRate, int numIterations)
    {
        int m = X.rows(); // Number of training examples
        int n = X.cols(); // Number of features

        // Initialize the weight vector
        Eigen::VectorXd theta(n);
        theta.setZero();

        for (int iter = 0; iter < numIterations; ++iter)
        {
            Eigen::VectorXd h = X * theta;
            h = h.unaryExpr(&sigmoid); // Apply sigmoid function element-wise
            // Compute the gradient
            Eigen::VectorXd grad = X.transpose() * (h - y);

            // Update the weight vector using gradient descent
            theta -= learningRate * grad;
        }
        return theta;
    }

    // Perform maximum likelihood estimation using Newton-Raphson
    Eigen::VectorXd Optimizer::logisticRegressionMLE(const Eigen::MatrixXd &X, const Eigen::VectorXd &y, int numIterations)
    {
        int m = X.rows(); // Number of training examples
        int n = X.cols(); // Number of features

        // Initialize the weight vector
        Eigen::VectorXd theta(n);
        theta.setZero();

        for (int iter = 0; iter < numIterations; ++iter)
        {
            Eigen::VectorXd h = X * theta;
            h = h.unaryExpr(&sigmoid); // Apply sigmoid function element-wise

            // Compute the gradient and Hessian matrix
            Eigen::VectorXd grad = X.transpose() * (h - y);
            Eigen::MatrixXd Hessian = X.transpose() * (h.array() * (1.0 - h.array())).matrix().asDiagonal() * X;

            // // Update the weight vector using Newton-Raphson method
            theta -= Hessian.inverse() * grad;
            std::cout<<theta<<std::endl;
        }
        return theta;
    }
}