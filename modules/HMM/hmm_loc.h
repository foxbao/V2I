#pragma once
#include <Eigen/Dense>
#include <vector>
#include "common/inner_types.hpp"
#include "civmap/map.h"
#include "modules/inner_types.hpp"
namespace civ
{
    namespace V2I
    {
        namespace modules
        {

            using namespace civ::common;
            using namespace civ::V2I::map;
            class HMMLoc
            {
            public:
                Bao aaa;
                HMMLoc();
                ~HMMLoc();
                void ReadMap(std::string map_path);
                /// @brief the viterbiAlgorithm
                /// @param observations
                /// @return
                std::vector<int> viterbiAlgorithm(const std::vector<int> &observations);

                /// @brief use viterbi algorithm to calculate the bess states of observations composed of a position trajectory
                /// @param observations
                /// @return
                std::vector<sp_cState> viterbiAlgorithm(sp_cZTrajectory observations);



                std::vector<sp_cState> viterbiAlgorithm2(sp_cZTrajectory observations);

            private:

                /// @brief 
                /// @param observations 
                /// @return 
                std::vector<sp_cState> GenerateStateFromTrajectory(sp_cZTrajectory observations);
                /// @brief calculate emission probability P(Z|X)
                /// @param state  state
                /// @param pt_enu observation
                /// @return
                double EmissionProbability(sp_cState state, Eigen::Vector3d pt_enu);

                /// @brief Calculate the CDF integrating from -0.5w to 0.5w, with sigma and mean
                /// @param w lane width
                /// @param sigma sigma standard deviation of the GPS error
                /// @param d perpendicular distance between the GPS coordinate and the lane centerline
                /// @return
                double CDF(double w, double sigma, double d);

                /// @brief Calculate transitional probability matrix, containing P(Xj|Xi)
                /// @param states
                /// @return
                Eigen::MatrixXd CalculateTransitionalProbability(std::vector<sp_cState> states);

                // Define HMM parameters
                int num_states_;
                int num_observations_;
                double lane_width_ = 3.5;
                double gps_sigma_ = 4;
                int near_threshold_=5;

                Eigen::MatrixXd transition_matrix_lane_;
                // Transition probability matrix
                Eigen::Matrix2d transition_matrix_;
                // Initial state probability vector
                Eigen::Vector2d initial_probs_;
                // Emission probability matrix
                Eigen::Matrix2d emission_matrix_;

                spCivMap sp_map_;
            };
        }
    } // namespace V2I
}