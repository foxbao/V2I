#pragma once
#include<vector>
namespace V2I
{
    class Estimator
    {
    public:
        Estimator();
        ~Estimator();
        void ReadData();
        double MLE_logit(std::vector<double>y,std::vector<double> x);

        double Jacobian();
        double Hessian();
        double Newton_Raphson();        
    private:
    };
} // namespace V2I