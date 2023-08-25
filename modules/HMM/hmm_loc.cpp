#include "hmm_loc.h"
#include <iostream>
#include <math.h>
#include "common/math/tools.h"

namespace civ
{
    namespace V2I
    {
        namespace modules
        {
            HMMLoc::HMMLoc()
            {
                num_states_ = 2;
                num_observations_ = 2;
                transition_matrix_ << 0.7, 0.3,
                    0.4, 0.6;

                initial_probs_ << 0.6, 0.4;

                emission_matrix_ << 0.9, 0.1,
                    0.2, 0.8;

                sp_map_ = std::make_shared<civ::V2I::map::CivMap>();
            }

            HMMLoc::~HMMLoc()
            {
            }

            std::vector<int> HMMLoc::viterbiAlgorithm(const std::vector<int> &observations)
            {

                int T = observations.size();
                // The maximum probability of a given node
                Eigen::MatrixXd delta(T, num_states_);
                // the path of a given node
                Eigen::MatrixXi psi(T, num_states_);
                // the best state for the observation
                std::vector<int> best_path(T);

                // Initialization step
                for (int s = 0; s < num_states_; ++s)
                {
                    // viterbi[s,1]<-
                    delta(0, s) = initial_probs_(s) * emission_matrix_(s, observations[0]);
                    // backpointer[s,1]<-0
                    psi(0, s) = 0;
                }

                // Recursion step
                for (int t = 1; t < T; ++t)
                {
                    for (int s = 0; s < num_states_; ++s)
                    {
                        double max_prob = 0.0;
                        int max_state = 0;
                        for (int s_prev = 0; s_prev < num_states_; ++s_prev)
                        {
                            double prob = delta(t - 1, s_prev) * transition_matrix_(s_prev, s) * emission_matrix_(s, observations[t]);
                            if (prob > max_prob)
                            {
                                max_prob = prob;
                                max_state = s_prev;
                            }
                        }
                        delta(t, s) = max_prob;
                        psi(t, s) = max_state;
                    }
                }

                // termination step, select the maximum probability
                double max_prob = 0.0;
                int best_final_state = 0;
                for (int s = 0; s < num_states_; ++s)
                {
                    if (delta(T - 1, s) > max_prob)
                    {
                        max_prob = delta(T - 1, s);
                        best_final_state = s;
                    }
                }

                // Backtrack to find the best path
                best_path[T - 1] = best_final_state;
                for (int t = T - 2; t >= 0; --t)
                {
                    best_path[t] = psi(t + 1, best_path[t + 1]);
                }

                return best_path;
            }

            std::vector<sp_cState> HMMLoc::viterbiAlgorithm(sp_cZTrajectory observations)
            {
                int T = observations->points_.size();
                // generate states which contain the curves near the first observation point
                std::vector<sp_cZMapLineSegment> curves_near = sp_map_->get_lanes_near_enu(observations->points_[0], 5);
                
                std::vector<sp_cState> states;
                for (const auto &curve : curves_near)
                {
                    std::shared_ptr<State> sp_state = std::make_shared<State>();
                    sp_state->curve_ = curve;
                    states.push_back(sp_state);
                }

                // Initialization step
                // Calculate the transitional probability
                Eigen::MatrixXd transition_matrix = CalculateTransitionalProbability(states);
                int num_states = transition_matrix.cols();

                // The maximum probability of a given node
                Eigen::MatrixXd delta(T, num_states);
                // the path of a given node
                Eigen::MatrixXi psi(T, num_states);
                // the best state for the observation
                std::vector<int> best_path(T);

                // calculate the initial probability
                Eigen::VectorXd initial_probs(num_states);
                for (int s = 0; s < num_states; ++s)
                {
                    initial_probs(s) = 1;
                }
                initial_probs = initial_probs / initial_probs.sum();
                std::cout << initial_probs << std::endl;

                for (int s = 0; s < num_states; ++s)
                {
                    // use the distance from position to lane center to calculate intitial prob
                    Eigen::Vector3d cross_pt_curve_enu;
                    double distance = sp_map_->get_distance_pt_curve_enu(observations->points_[0], curves_near[s], cross_pt_curve_enu);
                    // viterbi[s,1]<-P(X0Z0)=P(X0)*P(Z0|X0)
                    delta(0, s) = initial_probs(s) * EmissionProbability(states[s], observations->points_[0]);
                    // backpointer[s,1]<-0
                    psi(0, s) = 0;
                }

                std::vector<sp_cState> best_states;

                // Recursion step
                for (int t = 1; t < T; ++t)
                {
                    for (int s = 0; s < num_states; ++s)
                    {
                        double max_prob = 0.0;
                        int max_state = 0;
                        for (int s_prev = 0; s_prev < num_states; ++s_prev)
                        {
                            // todo the calculation of transitional probability
                            double transitional_prob = 1;
                            // v(t+1)=max(v(t)*P(Xt+1|Xt)*P(Zt|Xt))
                            // x(t+1)=argmax(v(t)*P(Xt+1|Xt)*P(Zt|Xt))
                            double prob = delta(t - 1, s_prev) * transitional_prob * EmissionProbability(states[s], observations->points_[t]);
                            // double prob = delta(t - 1, s_prev) * transition_matrix_(s_prev, s) * emission_matrix_(s, observations[t]);
                            if (prob > max_prob)
                            {
                                max_prob = prob;
                                max_state = s_prev;
                            }
                        }
                        delta(t, s) = max_prob;
                        psi(t, s) = max_state;
                    }
                }

                // termination step, select the maximum probability
                double max_prob = 0.0;
                int best_final_state = 0;
                for (int s = 0; s < num_states_; ++s)
                {
                    if (delta(T - 1, s) > max_prob)
                    {
                        max_prob = delta(T - 1, s);
                        best_final_state = s;
                    }
                }

                // Backtrack to find the best path
                best_path[T - 1] = best_final_state;
                for (int t = T - 2; t >= 0; --t)
                {
                    best_path[t] = psi(t + 1, best_path[t + 1]);
                }

                for (const auto &idx_path : best_path)
                {
                    best_states.push_back(states[idx_path]);
                }
                return best_states;
            }


            std::vector<State> HMMLoc::GenerateStateFromTrajectory(sp_cZTrajectory observations)
            {
                std::vector<State> states;

                std::vector<sp_cZMapLineSegment> curves_near = sp_map_->get_lanes_near_enu(observations->points_[0], 5);
                for (const auto &curve : curves_near)
                {
                    std::shared_ptr<State> sp_state = std::make_shared<State>();
                    sp_state->curve_ = curve;
                    states.push_back(*sp_state);
                }
                return states;
            }

            double HMMLoc::EmissionProbability(sp_cState state, Eigen::Vector3d pt_enu)
            {
                Eigen::Vector3d cross_pt_curve_enu;
                double distance = sp_map_->get_distance_pt_curve_enu(pt_enu, state->curve_, cross_pt_curve_enu);
                double prob = CDF(lane_width_, gps_sigma_, distance);
                return prob;
            }

            double HMMLoc::CDF(double w, double sigma, double d)
            {
                // using namespace civ::common::math;
                if (w <= 0)
                {
                    return 0.0;
                }
                if (d < 0)
                {
                    return 0.0;
                }
                if (sigma < 0)
                {
                    return 0.0;
                }

                // page 2, equation (4), from paper Lane-Level Map Matching based on HMM

                double cdf = civ::common::math::CDF_normal(-w / 2, w / 2, sigma, d);
                return cdf / w;
            }

            void HMMLoc::ReadMap(std::string map_path)
            {
                sp_map_->ReadData(map_path);
            }

            Eigen::MatrixXd HMMLoc::CalculateTransitionalProbability(std::vector<sp_cState> states)
            {
                Eigen::MatrixXd transition_matrix;
                int num_states = states.size();
                transition_matrix = Eigen::MatrixXd(num_states, num_states);

                for (int i = 0; i < num_states; i++)
                {
                    for (int j = 0; j < num_states; j++)
                    {

                        if (i == j)
                        {
                            transition_matrix(i, j) = 1;
                        }
                        else
                        {
                            // calculate the transitional probability between two lanes
                            double distance=sp_map_->get_distance_curve_cuvre_enu(states[i]->curve_,states[j]->curve_);
                            
                            transition_matrix(i, j) = exp(-distance/5);
                            // transition_matrix(i, j) = CDF(lane_width_,gps_sigma_,distance);
                        }
                    }
                    // normalize the transition probability of state to make the sum =1
                    double sum = transition_matrix.block<1, 4>(i, 0).sum();
                    transition_matrix.block<1, 4>(i, 0) /= sum;
                }
                std::cout<<transition_matrix<<std::endl;
                return transition_matrix;
            }

        }
    }
}
