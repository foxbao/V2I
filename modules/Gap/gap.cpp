

#include <iostream>
#include "gap.h"
#include "math.h"
namespace V2I
{
    gap::gap()
    {
        theta_.resize(5);
        theta_ << 1, 2, 3, 4, 5;
    }

    gap::~gap()
    {
    }

    double gap::CalculateGap(double D2, double V2, double D1, double V1)
    {
        Eigen::VectorXd x(5);
        x << 1, D2, V2, D1, V1;
        return logit(x,theta_);
    }

    double gap::logit(const Eigen::VectorXd &x, const Eigen::VectorXd &theta)
    {
        double h = theta.dot(x);
        return sigmoid(h);
    }

    Eigen::VectorXd gap::logit(const Eigen::MatrixXd &X, const Eigen::VectorXd &theta)
    {
        Eigen::VectorXd h = X * theta;
        h = h.unaryExpr(&sigmoid);
        return h;
    }
    void gap::set_theta(const Eigen::VectorXd& theta)
    {
        theta_=theta;
    }
}
