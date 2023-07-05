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
        double Jacobian(std::vector<double> y_batch, std::vector<std::vector<double>> x_batch);
        double Hessian(std::vector<double> y_batch, std::vector<std::vector<double>> x_batch);
        double Newton_Raphson(std::vector<double> y_batch, std::vector<std::vector<double>> x_batch);

    private:
        Eigen::Vector2d beta;
    };
}