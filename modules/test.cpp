#include <Eigen/Dense>
#include <iostream>
#include <memory>
#include <vector>
#include "HMM/hmm.h"
#include "Gap/gap.h"
#include "Gap/Optimizer.h"
#include "Gap/math.h"
#include "TTC/ttc.h"

using namespace Eigen;
void test_ttc()
{
    using namespace civ::V2I;
    std::cout << "ttc_test" << std::endl;
    std::shared_ptr<civ::V2I::ttc> sp_ttc=std::make_shared<civ::V2I::ttc>();
    sp_ttc->CalculateTTC(100,100,200,200);
    
}
void test_gap()
{
    using namespace V2I;
    std::cout<<"test gap"<<std::endl;
    std::shared_ptr<V2I::gap> sp_gap = std::make_shared<V2I::gap>();

    // Sample dataset
    Eigen::MatrixXd X(20, 2);
    X << 1, 2,
         1, 3,
         1, 4,
         1, 6,
         1, 7,
         1, 8,
         1, 9,
         1, 10,
         1, 11,
         1, 12,
         1, 13,
         1, 14,
         1, 15,
         1, 16,
         1, 17,
         1, 18,
         1, 19,
         1, 20,
         1, 21,
         1, 22;

    Eigen::VectorXd y(20);
    y << 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1;

    // Hyperparameters
    double learningRate = 0.1;
    int numIterations = 1000;

    std::shared_ptr<V2I::Optimizer> sp_opt=std::make_shared<V2I::Optimizer>();
    Eigen::VectorXd theta = sp_opt->logisticRegressionMLE(X, y, learningRate, numIterations);
    // Print the estimated parameters
    std::cout << "Estimated parameters by gradient descent: " << theta.transpose() << std::endl;

    // Test
    Eigen::VectorXd h=X*theta;
    h = h.unaryExpr(&V2I::sigmoid);
    std::cout<<"X:"<<X<<std::endl;
    std::cout<<"y:"<<y.transpose()<<std::endl;
    std::cout<<"result:"<<h.transpose()<<std::endl;
}

void test_hmm()
{
    using namespace civ::V2I;
    std::cout<<"test hmm"<<std::endl;
    std::shared_ptr<civ::V2I::HMM> sp_model=std::make_shared<civ::V2I::HMM>();
    std::vector<int> observations = {0, 1, 0, 1, 1}; // Sample observation sequence
    double prob=sp_model->EmissionProbability(5,1,1);
    std::vector<int> best_path = sp_model->viterbiAlgorithm(observations);
    std::cout << "Most probable state sequence:" << std::endl;
    for (int state : best_path) {
        std::cout << state << " ";
    }
    std::cout << std::endl;
}

int main() {
    // test_gap();
    test_hmm();
    // test_ttc();
    return 0;
}
