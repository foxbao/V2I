#include <iostream>
#include <vector>
#include <Eigen/Dense>

using namespace Eigen;

// Define HMM parameters
const int num_states = 2;
const int num_observations = 2;

// Transition probability matrix
Matrix2d transition_matrix;
// Initial state probability vector
Vector2d initial_probs;
// Emission probability matrix
Matrix2d emission_matrix;

// Function to initialize HMM parameters
void initializeHMM() {
    transition_matrix << 0.7, 0.3,
                         0.4, 0.6;

    initial_probs << 0.6, 0.4;

    emission_matrix << 0.9, 0.1,
                       0.2, 0.8;
}

// Function to implement the Viterbi algorithm
std::vector<int> viterbiAlgorithm(const std::vector<int>& observations) {
    int T = observations.size();
    MatrixXd delta(T, num_states);
    MatrixXi psi(T, num_states);
    std::vector<int> best_path(T);

    // Initialization step
    for (int s = 0; s < num_states; ++s) {
        delta(0, s) = initial_probs(s) * emission_matrix(s, observations[0]);
        psi(0, s) = 0;
    }

    // Recursion step
    for (int t = 1; t < T; ++t) {
        for (int s = 0; s < num_states; ++s) {
            double max_prob = 0.0;
            int max_state = 0;

            for (int s_prev = 0; s_prev < num_states; ++s_prev) {
                double prob = delta(t - 1, s_prev) * transition_matrix(s_prev, s) * emission_matrix(s, observations[t]);
                if (prob > max_prob) {
                    max_prob = prob;
                    max_state = s_prev;
                }
            }

            delta(t, s) = max_prob;
            psi(t, s) = max_state;
        }
    }

    // Termination step
    double max_prob = 0.0;
    int best_final_state = 0;
    for (int s = 0; s < num_states; ++s) {
        if (delta(T - 1, s) > max_prob) {
            max_prob = delta(T - 1, s);
            best_final_state = s;
        }
    }

    // Backtrack to find the best path
    best_path[T - 1] = best_final_state;
    for (int t = T - 2; t >= 0; --t) {
        best_path[t] = psi(t + 1, best_path[t + 1]);
    }

    return best_path;
}

int main() {
    initializeHMM();

    std::vector<int> observations = {0, 1, 0, 1, 1}; // Sample observation sequence

    std::vector<int> best_path = viterbiAlgorithm(observations);

    std::cout << "Most probable state sequence:" << std::endl;
    for (int state : best_path) {
        std::cout << state << " ";
    }
    std::cout << std::endl;

    return 0;
}
