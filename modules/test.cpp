#include <Eigen/Dense>
#include <iostream>
#include <memory>
#include <vector>
#include "HMM/hmm.h"
// #include "modules/Map/map.h"
// #include "modules/common/type_define.hpp"
using namespace Eigen;
void test_ttc()
{

}
void test_gap()
{

}

void test_hmm()
{
    using namespace V2I;
    std::shared_ptr<V2I::model> sp_model=std::make_shared<V2I::model>();
    std::vector<int> observations = {0, 1, 0, 1, 1}; // Sample observation sequence
    std::vector<int> best_path = sp_model->viterbiAlgorithm(observations);
    std::cout << "Most probable state sequence:" << std::endl;
    for (int state : best_path) {
        std::cout << state << " ";
    }
    std::cout << std::endl;
}

int main() {
    test_hmm();
    test_ttc();
    test_gap();
    return 0;
}
