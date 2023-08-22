#pragma once
#include <Eigen/Dense>
#include <vector>
#include "common/inner_types.hpp"
#include "civmap/map.h"


namespace civ
{
    namespace V2I
    {
        using namespace civ::common;
        class HMMLoc
        {
        public:
            HMMLoc();
            ~HMMLoc();
            /// @brief the viterbiAlgorithm
            /// @param observations
            /// @return
            std::vector<int> viterbiAlgorithm(const std::vector<int> &observations);
            std::vector<State> viterbiAlgorithm(sp_cZTrajectory observations);

            /// @brief
            /// @param w lane width
            /// @param sigma sigma standard deviation of the GPS error
            /// @param d perpendicular distance between the GPS coordinate and the lane centerline
            /// @return
            double EmissionProbability(double w, double sigma, double d);

            void ReadMap(std::string map_path);

        private:
            // double MeasurementProbability(Eigen::Vector2d z,ZMapLineSegment r);
            void CalculateTransitionalProbability();
            Eigen::MatrixXd CalculateTransitionalProbability(std::vector<sp_cZMapLineSegment> curves_near);
            // Define HMM parameters
            int num_states_;
            int num_observations_;

            Eigen::MatrixXd transition_matrix_lane_;
            // Transition probability matrix
            Eigen::Matrix2d transition_matrix_;
            // Initial state probability vector
            Eigen::Vector2d initial_probs_;
            // Emission probability matrix
            Eigen::Matrix2d emission_matrix_;

            spCivMap sp_map_;
        };
    } // namespace V2I
}