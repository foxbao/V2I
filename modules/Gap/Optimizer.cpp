#include "Optimizer.h"
namespace V2I
{
    Optimizer::Optimizer()
    {
        beta<<2,-2;
    }
    Optimizer::~Optimizer()
    {
    }
    double Optimizer::Jacobian(std::vector<double> y_batch, std::vector<std::vector<double>> x_batch)
    {
        return 0.0;
    }

    double Optimizer::Hessian(std::vector<double> y_batch, std::vector<std::vector<double>> x_batch)
    {
        return 0.0;
    }

    double Optimizer::Newton_Raphson(std::vector<double> y_batch, std::vector<std::vector<double>> x_batch)
    {

        Jacobian(y_batch, x_batch);
        Hessian(y_batch, x_batch);
        return 0.0;
    }
}