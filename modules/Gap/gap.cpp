#include "gap.h"

#include <iostream>
namespace V2I
{
    gap::gap()
    {
        alpha_ = 1;
        beta_ << 1, 2, 3, 4;
    }

    gap::~gap()
    {
    }

    void gap::set_alpha(double alpha)
    {
        alpha_ = alpha;
    }
    void gap::set_beta(Eigen::Vector4d beta)
    {
        beta_ = beta;
    }
    double gap::CalculateGap(double D2, double V2, double D1, double V1)
    {
        // P(x)=exp(alpha+beta*x)/(1+exp(alpha+beta*x))
        // x=[D2,V2,D1,V1]
        Eigen::Vector4d X;
        X << D2, V2, D1, V1;
        double tmp = beta_.dot(X);
        return exp(alpha_ + tmp) / (1 + exp(alpha_ + tmp));
    }

}
