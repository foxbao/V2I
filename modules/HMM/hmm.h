#pragma once
#include <Eigen/Dense>
#include <vector>
#include "civmap/map.h"

namespace civ
{
    namespace V2I
    {
        class HMM
        {
        public:
            HMM();
            ~HMM();
            /// @brief the viterbiAlgorithm
            /// @param observations
            /// @return
            std::vector<int> viterbiAlgorithm(const std::vector<int> &observations);

            /// @brief
            /// @param w lane width
            /// @param sigma sigma standard deviation of the GPS error
            /// @param d perpendicular distance between the GPS coordinate and the lane centerline
            /// @return
            double EmissionProbability(double w, double sigma, double d);

        private:
            // double MeasurementProbability(Eigen::Vector2d z,ZMapLineSegment r);
            double TransitionalProbability();
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
}