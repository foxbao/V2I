// #include <eigen3/Eigen/Core>
// #include <iostream>
// #include <memory>
// #include <vector>
// #include "gap.h"
// #include "MLE.h"

// int main()
// {
//     using namespace V2I;
//     std::cout << "gap_test" << std::endl;
//     std::shared_ptr<V2I::gap> sp_gap=std::make_shared<gap>();
//     double result=sp_gap->CalculateGap(1,2,3,4);
//     std::cout<<"result:"<<result<<std::endl;

//     std::shared_ptr<V2I::Estimator> sp_estimator=std::make_shared<V2I::Estimator>();

//     std::vector<double> x;

//     std::vector<Eigen::VectorXd> x_batch;
//     std::vector<double> y_batch;

//     Eigen::Vector2d beta;
//     beta<<2,-2;

//     int len=100;
//     for(int i=0;i<len;i++)
//     {
//         Eigen::VectorXd x(1);
//         x[0]=i*0.1;
//         x_batch.push_back(x);
//         y_batch.push_back(sp_gap->logit(beta,x));
//     }

//     sp_estimator->MLE_logit(y_batch,x_batch);
//     return 0;

// }

#include <iostream>
#include <cmath>
#include <eigen3/Eigen/Core>

using namespace std;
using namespace Eigen;

// Define the logistic function
double sigmoid(double x) {
    return 1.0 / (1.0 + exp(-x));
}

// Perform maximum likelihood estimation using gradient descent
VectorXd logisticRegressionMLE(const MatrixXd& X, const VectorXd& y, double learningRate, int numIterations) {
    int m = X.rows(); // Number of training examples
    int n = X.cols(); // Number of features

    // Initialize the weight vector
    VectorXd theta(n);
    theta.setZero();

    for (int iter = 0; iter < numIterations; ++iter) {
        VectorXd h = X * theta;
        h = h.unaryExpr(&sigmoid); // Apply sigmoid function element-wise

        // Compute the gradient
        VectorXd grad = X.transpose() * (h - y);

        // Update the weight vector using gradient descent
        theta -= learningRate * grad;
    }
    return theta;
}

int main() {
    // Sample dataset
    MatrixXd X(6, 2);
    X << 1, 2,
         1, 3,
         1, 4,
         1, 6,
         1, 7,
         1, 8;

    VectorXd y(6);
    y << 0, 0, 0, 1, 1, 1;

    // Hyperparameters
    double learningRate = 0.1;
    int numIterations = 1000;

    // Perform maximum likelihood estimation
    VectorXd theta = logisticRegressionMLE(X, y, learningRate, numIterations);

    // Print the estimated parameters
    cout << "Estimated parameters: " << theta.transpose() << endl;

    return 0;
}
