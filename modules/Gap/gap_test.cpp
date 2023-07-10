#include <eigen3/Eigen/Core>
#include <iostream>
#include <memory>
#include <vector>
#include "gap.h"
#include "math.h"
#include "Optimizer.h"


int main()
{
    // using namespace V2I;
    std::shared_ptr<V2I::gap> sp_gap = std::make_shared<V2I::gap>();

    // Sample dataset
    Eigen::MatrixXd X(6, 2);
    X << 1, 2,
         1, 3,
         1, 4,
         1, 6,
         1, 7,
         1, 8;

    Eigen::VectorXd y(6);
    y << 0, 0, 0, 1, 1, 1;

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

// #include <iostream>
// #include <cmath>
// #include <eigen3/Eigen/Core>

// using namespace std;
// using namespace Eigen;

// // Define the logistic function
// double sigmoid(double x) {
//     return 1.0 / (1.0 + exp(-x));
// }

// // Perform maximum likelihood estimation using gradient descent
// VectorXd logisticRegressionMLE(const MatrixXd& X, const VectorXd& y, double learningRate, int numIterations) {
//     int m = X.rows(); // Number of training examples
//     int n = X.cols(); // Number of features

//     // Initialize the weight vector
//     VectorXd theta(n);
//     theta.setZero();

//     for (int iter = 0; iter < numIterations; ++iter) {
//         VectorXd h = X * theta;
//         h = h.unaryExpr(&sigmoid); // Apply sigmoid function element-wise

//         // Compute the gradient
//         VectorXd grad = X.transpose() * (h - y);

//         // Update the weight vector using gradient descent
//         theta -= learningRate * grad;
//     }
//     return theta;
// }

// int main() {
//     // Sample dataset
//     MatrixXd X(6, 2);
//     X << 1, 2,
//          1, 3,
//          1, 4,
//          1, 6,
//          1, 7,
//          1, 8;

//     VectorXd y(6);
//     y << 0, 0, 0, 1, 1, 1;

//     // Hyperparameters
//     double learningRate = 0.1;
//     int numIterations = 1000;

//     // Perform maximum likelihood estimation
//     VectorXd theta = logisticRegressionMLE(X, y, learningRate, numIterations);

//     // Print the estimated parameters
//     cout << "Estimated parameters: " << theta.transpose() << endl;

//     return 0;
// }
