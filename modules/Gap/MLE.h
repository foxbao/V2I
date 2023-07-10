#pragma once
#include <eigen3/Eigen/Core>
#include <memory>
#include <vector>
#include "Optimizer.h"
namespace V2I
{
    class Estimator
    {
    public:
        Estimator();
        ~Estimator();
        void ReadData();
        double MLE_logit(std::vector<double> y_batch, std::vector<Eigen::VectorXd> x_batch);
    private:
        std::shared_ptr<Optimizer> sp_optimizer;
        
    };
} // namespace V2I