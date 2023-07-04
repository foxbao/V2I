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

    double gap::logit(Eigen::Vector2d beta, double x)
    {
        Eigen::Vector2d input_data;
        input_data<<1,x;
        double result=1/(1+exp(beta.dot(input_data)));
        return result;
    }

    double gap::logit(std::vector<double> beta, std::vector<double> x)
    {

        return 0;
        // double result = 1 / (1 + exp(beta * x));
        // return result;
    }

}
