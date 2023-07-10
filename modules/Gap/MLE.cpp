#include "MLE.h"

#include <iostream>
namespace V2I
{
    Estimator::Estimator()
    {
        sp_optimizer=std::make_shared<Optimizer>();
    }
    Estimator::~Estimator()
    {
    }

    void Estimator::ReadData()
    {

    }

    double Estimator::MLE_logit(std::vector<double> y_batch, std::vector<Eigen::VectorXd> x_batch)
    {
        sp_optimizer->Newton_Raphson(y_batch,x_batch);
        return 0.0;
    }
}
