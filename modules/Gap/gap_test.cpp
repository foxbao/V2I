#include <iostream>
#include <memory>
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
    return 0;


    
}
