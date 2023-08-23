#pragma once
#include <Eigen/Dense>
#include <vector>
#include "common/inner_types.hpp"
#include "civmap/map.h"
#include "inner_types.hpp"
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
                HMMLoc();
                ~HMMLoc();
                void ReadMap(std::string map_path);
                /// @brief the viterbiAlgorithm
                /// @param observations
                /// @return
                std::vector<int> viterbiAlgorithm(const std::vector<int> &observations);
                std::vector<sp_cState> viterbiAlgorithm(sp_cZTrajectory observations);

            private:
                /// @brief 
                /// @param w lane width
                /// @param sigma sigma standard deviation of the GPS error
                /// @param d perpendicular distance between the GPS coordinate and the lane centerline
                /// @return
                double EmissionProbability(double w, double sigma, double d);

                double EmissionProbability(civ::V2I::map::sp_cZMapLineSegment state, Eigen::Vector3d pt_enu);

                // /// @brief Calculate the cummulative probability function
                // /// @param low_limit low limit of integral
                // /// @param up_limit up limit of integral
                // /// @param sigma the sigmal
                // /// @param mean the mean value
                // /// @return
                // double CDF_normal(double low_limit, double up_limit, double sigma, double mean);

                Eigen::MatrixXd CalculateTransitionalProbability(std::vector<sp_cZMapLineSegment> curves_near);
                
                // Define HMM parameters
                int num_states_;
                int num_observations_;
                double lane_width_ = 3.5;
                double gps_sigma_ = 4;

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