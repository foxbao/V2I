#include "Optimizer.h"
namespace V2I
{
    Optimizer::Optimizer()
    {
        beta_ << 1, -2;
    }
    Optimizer::~Optimizer()
    {
    }

    double Optimizer::logit(Eigen::VectorXd beta, Eigen::VectorXd x)
    {
        Eigen::VectorXd x_homo(beta.size());
        x_homo(0) = 1;
        x_homo.segment(1, x.size()) = x;
        // for(int i=0;i<x.size();i++)
        // {
        //     x_homo(i+1)=x[i];
        // }
        double result = 1 / (1 + exp(beta.dot(x_homo)));
        return result;
    }

    Eigen::VectorXd Optimizer::Jacobian(Eigen::VectorXd beta, std::vector<double> y_batch, std::vector<Eigen::VectorXd> x_batch)
    {
        Eigen::VectorXd derivate(2);
        for (int i = 0; i < y_batch.size(); i++)
        {
            double residual = (y_batch[i] - logit(beta, x_batch[i]));
            auto aaaa = x_batch[i] * residual;
            derivate += x_batch[i] * residual;
        }
        return derivate;
    }

    Eigen::MatrixXd Optimizer::Hessian(Eigen::VectorXd beta, std::vector<double> y_batch, std::vector<Eigen::VectorXd> x_batch)
    {
        int size = x_batch[0].size();
        Eigen::MatrixXd hessian(size, size);
        for (int i = 0; i < x_batch.size(); i++)
        {
            hessian += -(x_batch[i].transpose() * x_batch[i] * logit(beta, x_batch[i]) * (1 - logit(beta, x_batch[i])));
            logit(beta, x_batch[i]) * (1 - logit(beta, x_batch[i]));
        }

        return hessian;

        // return 0.0;
    }

    double Optimizer::Newton_Raphson(std::vector<double> y_batch, std::vector<Eigen::VectorXd> x_batch)
    {

        Eigen::VectorXd y_hat(x_batch.size());
        Jacobian(beta_, y_batch, x_batch);
        auto hessian=Hessian(beta_, y_batch, x_batch);
        return 0.0;
    }
}