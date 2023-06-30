#include "gap.h"

#include <iostream>
namespace V2I
{
    gap::gap()
    {
        alpha = 1;
        beta << 1, 2, 3, 4;
    }

    gap::~gap()
    {
    }
    double gap::CalculateGap(double D2, double V2, double D1, double V1)
    {
        Eigen::Vector4d X;
        X << D2, V2, D1, V1;
        double tmp=beta.dot(X);

        return exp(alpha+tmp)/(1+exp(alpha+tmp));
    }

}
