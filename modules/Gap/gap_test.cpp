// #include <eigen3/Eigen/Core>
// #include <iostream>
// #include <memory>
// #include <vector>
// #include "gap.h"
// #include "math.h"
// #include "Optimizer.h"

// int main()
// {
//     // using namespace V2I;
//     std::shared_ptr<V2I::gap> sp_gap = std::make_shared<V2I::gap>();

//     // Sample dataset
//     Eigen::MatrixXd X(20, 2);
//     X << 1, 2,
//          1, 3,
//          1, 4,
//          1, 6,
//          1, 7,
//          1, 8,
//          1, 9,
//          1, 10,
//          1, 11,
//          1, 12,
//          1, 13,
//          1, 14,
//          1, 15,
//          1, 16,
//          1, 17,
//          1, 18,
//          1, 19,
//          1, 20,
//          1, 21,
//          1, 22;

//     Eigen::VectorXd y(20);
//     y << 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1;

//     // Hyperparameters
//     double learningRate = 0.1;
//     int numIterations = 1000;

//     std::shared_ptr<V2I::Optimizer> sp_opt=std::make_shared<V2I::Optimizer>();
//     Eigen::VectorXd theta = sp_opt->logisticRegressionMLE(X, y, learningRate, numIterations);
//     // Print the estimated parameters
//     std::cout << "Estimated parameters by gradient descent: " << theta.transpose() << std::endl;

//     // Test
//     Eigen::VectorXd h=X*theta;
//     h = h.unaryExpr(&V2I::sigmoid);
//     std::cout<<"X:"<<X<<std::endl;
//     std::cout<<"y:"<<y.transpose()<<std::endl;
//     std::cout<<"result:"<<h.transpose()<<std::endl;
// }
