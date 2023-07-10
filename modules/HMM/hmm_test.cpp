#include <iostream>
#include <eigen3/Eigen/Core>

using namespace Eigen;

int main() {
    // Transition matrix
    MatrixXd A(2, 2);
    A << 0.7, 0.3,
         0.4, 0.6;
    
    // Observation matrix
    MatrixXd B(2, 3);
    B << 0.1, 0.4, 0.5,
         0.7, 0.2, 0.1;
    
    // Initial state probabilities
    VectorXd pi(2);
    pi << 0.6, 0.4;
    
    // Sequence of observations
    VectorXi observations(4);
    observations << 0, 1, 2, 0;
    
    // Viterbi algorithm
    int T = observations.size();
    MatrixXd delta(T, 2);
    MatrixXi psi(T, 2);
    
    delta.row(0) = (pi.transpose().array() * B.col(observations(0)).array()).log();
    
    for (int t = 1; t < T; ++t) {
        for (int j = 0; j < 2; ++j) {
            VectorXd temp = (delta.row(t - 1).array() + A.col(j).array().log()).transpose();
            delta(t, j) = temp.maxCoeff();
            psi(t, j) = (temp.array() == delta(t, j)).count();
            delta(t, j) += std::log(B(j, observations(t)));
        }
    }
    
    // Backtracking to find the most likely state sequence
    VectorXi stateSequence(T);
    stateSequence(T - 1) = delta.row(T - 1).maxCoeff();
    
    for (int t = T - 2; t >= 0; --t) {
        stateSequence(t) = psi(t + 1, stateSequence(t + 1));
    }
    
    // Print the most likely state sequence
    std::cout << "Most likely state sequence: ";
    for (int t = 0; t < T; ++t) {
        std::cout << stateSequence(t) << " ";
    }
    std::cout << std::endl;
    
    return 0;
}
