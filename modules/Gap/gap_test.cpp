#include <eigen3/Eigen/Core>
#include <iostream>
#include <memory>
#include <vector>
#include "gap.h"
#include "MLE.h"




int main()
{
    using namespace V2I;
    std::cout << "gap_test" << std::endl;
    std::shared_ptr<V2I::gap> sp_gap=std::make_shared<gap>();
    double result=sp_gap->CalculateGap(1,2,3,4);
    std::cout<<"result:"<<result<<std::endl;

    std::shared_ptr<V2I::Estimator> sp_estimator=std::make_shared<V2I::Estimator>();


    std::vector<double> x;
    std::vector<double> y;
    Eigen::Vector2d beta;
    beta<<2,-2;
    // std::vector<double> beta;
    // beta.push_back(1);
    // beta.push_back(-2);

    int len=100;
    for(int i=0;i<len;i++)
    {
        // x.clear();
        x.push_back(i*0.1);
        double result=sp_gap->logit(beta,x[i]);
        y.push_back(result);
        // y.push_back(sp_gap->logit(beta[1],x[i]));

        std::cout<<"x:"<<x[i]<<",y:"<<y[i]<<std::endl;
    }

    sp_estimator->MLE_logit(y,x);




    // for (int i=0;i<10;i++)
    // {
    //     x[i]=2*i;
    //     y[i]=logit(beta[1],x[i]);
    //     std::cout<<y[i]<<std::endl;
    // }



    return 0;





}
