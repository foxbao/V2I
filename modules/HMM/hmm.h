#pragma once
#include <Eigen/Dense>
#include <vector>
namespace V2I
{
    class model
    {
    public:
        model();
        ~model();
        /// @brief the viterbiAlgorithm 
        /// @param observations 
        /// @return 
        std::vector<int> viterbiAlgorithm(const std::vector<int>& observations);

    private:
        // Define HMM parameters
        int num_states_;
        int num_observations_;
        // Transition probability matrix
        Eigen::Matrix2d transition_matrix_;
        // Initial state probability vector
        Eigen::Vector2d initial_probs_;
        // Emission probability matrix
        Eigen::Matrix2d emission_matrix_;
    };
} // namespace V2I