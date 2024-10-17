#include "modules/HMM/hmm_loc.h"
#include <iostream>
#include <set>
#include <math.h>
#include "common/math/tools.h"
// #include "mec/zsfd/inc/mapcore/hdmap.h"
#include "hmm_loc.h"

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

                sp_map_ = std::make_shared<civ::V2I::map::CivMap>();
                sp_hdmap_ = std::make_shared<zas::mapcore::hdmap>();

                initial_range_ = 5.0;
            }

            HMMLoc::~HMMLoc()
            {
            }

            void HMMLoc::ReadHDMap(std::string map_path, std::string json_path)
            {
                sp_hdmap_->load_fromfile(map_path.c_str());
                sp_hdmap_->generate_lanesect_transition(json_path.c_str());
            }
            void HMMLoc::ReadMap(std::string map_path)
            {
                sp_map_->ReadData(map_path);
            }


            void HMMLoc::SetHDMap(std::shared_ptr<zas::mapcore::hdmap> sp_hdmap)
            {
                sp_hdmap_=sp_hdmap;
            }
            std::vector<int> HMMLoc::viterbiAlgorithm(const std::vector<int> &observations)
            {

                // Transition probability matrix
                Eigen::Matrix2d transition_matrix_;
                // Initial state probability vector
                Eigen::Vector2d initial_probs_;
                // Emission probability matrix
                Eigen::Matrix2d emission_matrix_;

                transition_matrix_ << 0.7, 0.3,
                    0.4, 0.6;

                initial_probs_ << 0.6, 0.4;

                emission_matrix_ << 0.9, 0.1,
                    0.2, 0.8;

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
                    // viterbi[s,1]<-P(X0Z0)=P(X0)*P(Z0|X0)
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
                            // the value of a node is the maximum probability
                            // v(t+1)=max(v(t)*P(Xt+1|Xt)*P(Zt|Xt))
                            // x(t+1)=argmax(v(t)*P(Xt+1|Xt)*P(Zt|Xt))
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

                // generate states which contain the curves near the observation points
                std::vector<sp_cState> states = GenerateStateFromTrajectory(observations);

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
                // unified distribution
                Eigen::VectorXd initial_probs(num_states);
                for (int s = 0; s < num_states; ++s)
                {
                    initial_probs(s) = 1;
                }
                // normalize
                initial_probs = initial_probs / initial_probs.sum();

                // use the first observed result to get the most pro
                for (int s = 0; s < num_states; ++s)
                {
                    // use the distance from position to lane center to calculate intitial prob
                    Eigen::Vector3d cross_pt_curve_enu;
                    // double distance = sp_map_->get_distance_pt_curve_enu(observations->points_[0], states[s]->curve_, cross_pt_curve_enu);
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
                            double transitional_prob = transition_matrix(s_prev, s);
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



            std::vector<uint64_t> HMMLoc::viterbiAlgorithmHDMap(sp_cZTrajectory observations)
            {
                int T = observations->points_.size();
                // generate states which contain the curves near the observation points
                // the range is given by initial_range_, ex. 5 meters
                std::vector<uint64_t> lanes = sp_hdmap_->get_lanes_near_pt_enu(observations->points_, initial_range_);
                // std::cout << "lane states:" << std::endl;
                // for (auto const &lane : lanes)
                // {
                //     std::cout << "lane:" << lane << std::endl;
                // }
                // Initialization step
                // Calculate the transitional probability
                int num_states = lanes.size();

                Eigen::MatrixXd transition_matrix = sp_hdmap_->CalculateTransitionalProbability(lanes, lanes);
                std::cout << "num_states:" << num_states << std::endl;
                std::cout.precision(3);
                std::cout << "transitional matrix:" << std::endl;
                std::cout << transition_matrix << std::endl;
                // The maximum probability of a given node
                Eigen::MatrixXd delta(T, num_states);
                // the path of a given node
                Eigen::MatrixXi psi(T, num_states);
                // the best state for the observation
                std::vector<int> best_path(T);

                // calculate the initial probability
                // unified distribution
                Eigen::VectorXd initial_probs(num_states);
                for (int s = 0; s < num_states; ++s)
                {
                    initial_probs(s) = 1;
                }

                // normalize
                initial_probs = initial_probs / initial_probs.sum();
                std::cout << "-------------------------" << std::endl;
                std::cout << "initial_probs:" << initial_probs.transpose() << std::endl;

                // use the first observed result to get the most pro
                for (int s = 0; s < num_states; ++s)
                {
                    // use the distance from position to lane center to calculate intitial prob
                    Eigen::Vector3d cross_pt_curve_enu;
                    // viterbi[s,1]<-P(X0Z0)=P(X0)*P(Z0|X0)
                    delta(0, s) = initial_probs(s) * EmissionProbability(lanes[s], observations->points_[0]);
                    std::cout << "delta(0," << s << "):" << delta(0, s) << " " << std::endl;
                    // backpointer[s,1]<-0
                    psi(0, s) = 0;
                }
                std::vector<uint64_t> best_states;

                // Recursion step
                for (int t = 1; t < T; ++t)
                {
                    for (int s = 0; s < num_states; ++s)
                    {
                        double max_prob = 0.0;
                        int max_state = 0;
                        for (int s_prev = 0; s_prev < num_states; ++s_prev)
                        {
                            double transitional_prob = transition_matrix(s_prev, s);
                            // v(t+1)=max(v(t)*P(Xt+1|Xt)*P(Zt|Xt))
                            // x(t+1)=argmax(v(t)*P(Xt+1|Xt)*P(Zt|Xt))
                            double prob = delta(t - 1, s_prev) * transitional_prob * EmissionProbability(lanes[s], observations->points_[t]);
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
                for (int s = 0; s < num_states; ++s)
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
                    best_states.push_back(lanes[idx_path]);
                }
                return best_states;
            } // the comparator used in set to remove repetition of curves found



            template <typename T>
            void print_vec(std::vector<T> &vec)
            {

                std::cout << "打印vector:" << std::endl;
                typename std::vector<T>::iterator iter = vec.begin();
                while (iter != vec.end())
                {
                    std::cout << *iter++ << "\t";
                }
                std::cout << std::endl;
            }

            std::vector<uint64_t> HMMLoc::viterbiAlgorithmHDMap2(sp_cZTrajectory observations)
            {
                int T = observations->points_.size();
                std::vector<uint64_t> best_states;
                if (T < 1)
                {
                    return best_states;
                }
                // generate states which contain the curves near the observation points
                // the range is given by initial_range_, ex. 5 meters
                std::vector<uint64_t> lanes = sp_hdmap_->get_lanes_near_pt_enu(observations->points_, initial_range_);
                // Initialization step
                // Calculate the transitional probability
                int num_states = lanes.size();
                // the candidate states for each time epoch
                std::vector<std::vector<uint64_t>> time_nodes;

                Eigen::MatrixXd transition_matrix = sp_hdmap_->CalculateTransitionalProbability(lanes, lanes);
                std::cout<<"lanes"<<std::endl;
                print_vec(lanes);
                std::cout <<"transition matrix"<<std::endl;
                std::cout<<transition_matrix<<std::endl;
                
                
                // The maximum probability of a given node
                Eigen::MatrixXd delta = Eigen::MatrixXd::Zero(T, num_states);
                // the path of a given node
                Eigen::MatrixXi psi = Eigen::MatrixXi::Zero(T, num_states);
                // the best state for the observation
                std::vector<int> best_path(T);

                Eigen::MatrixXi mask = Eigen::MatrixXi::Zero(T, num_states);

                // calculate the initial probability
                // only the lane state candidate within the range of first observation will be calculated
                Eigen::VectorXd initial_probs = Eigen::VectorXd::Zero(num_states);
                std::vector<uint64_t> initial_states;
                for (int s = 0; s < num_states; ++s)
                {
                    initial_probs(s) = 1;
                    initial_states.push_back(lanes[s]);
                }

                time_nodes.push_back(initial_states);
                // normalize
                initial_probs = initial_probs / initial_probs.sum();
                std::cout << "-------------------------" << std::endl;
                std::cout << "initial_probs:" << initial_probs.transpose() << std::endl;

                // use the first observed result to calculate the probabilities of first node
                std::cout<<"initial delta:"<<std::endl;
                std::cout.precision(3);
                for (int s = 0; s < num_states; ++s)
                {
                    // use the distance from position to lane center to calculate intitial prob
                    // viterbi[s,1]<-P(X0Z0)=P(X0)*P(Z0|X0)
                    std::cout<<"EmissionProbability("<<lanes[s]<<"):"<<EmissionProbability(lanes[s], observations->points_[0]);
                    delta(0, s) = initial_probs(s) * EmissionProbability(lanes[s], observations->points_[0]);
                    std::cout << " delta "<<  delta(0, s) << std::endl;
                    // backpointer[s,1]<-0
                    psi(0, s) = 0;
                }

                // Recursion step
                // for the steps after initialization
                for (int t = 1; t < T; ++t)
                {
                    // process only the cadidate states near to observation
                    std::vector<uint64_t> lanes_neighbour = sp_hdmap_->get_lanes_near_pt_enu(observations->points_[t], initial_range_);
                    time_nodes.push_back(lanes_neighbour);

                    std::cout << "time_nodes[t - 1]" << std::endl;
                    print_vec(time_nodes[t - 1]);
                    std::cout << "lanes_neighbour" << std::endl;
                    print_vec(lanes_neighbour);
                    // std::cout<<lanes_neighbour<<std::endl;

                    Eigen::MatrixXd transition_matrix_tmp = sp_hdmap_->CalculateTransitionalProbability(time_nodes[t - 1], lanes_neighbour);

                    std::cout << "matrix_tmp" << std::endl;
                    std::cout << transition_matrix_tmp << std::endl;

                    // current node
                    for (int s = 0; s < lanes_neighbour.size(); s++)
                    {
                        uint64_t lane_neighbour = lanes_neighbour[s];
                        double max_prob = 0.0;
                        int max_state = 0;
                        for (int s_prev = 0; s_prev < time_nodes[t - 1].size(); ++s_prev)
                        {
                            double transitional_prob = transition_matrix_tmp(s_prev, s);
                            // // v(t+1)=max(v(t)*P(Xt+1|Xt)*P(Zt|Xt))
                            // // x(t+1)=argmax(v(t)*P(Xt+1|Xt)*P(Zt|Xt))
                            double prob = delta(t - 1, s_prev) * transitional_prob * EmissionProbability(lanes_neighbour[s], observations->points_[t]);
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
                for (int s = 0; s < num_states; ++s)
                {
                    if (delta(T - 1, s) > max_prob)
                    {
                        max_prob = delta(T - 1, s);
                        best_final_state = s;
                    }
                }

                std::cout<<"delta:"<<std::endl;
                std::cout<<delta<<std::endl;
                // Backtrack to find the best path
                best_path[T - 1] = best_final_state;
                for (int t = T - 2; t >= 0; --t)
                {
                    best_path[t] = psi(t + 1, best_path[t + 1]);
                }

                for (const auto &idx_path : best_path)
                {
                    best_states.push_back(lanes[idx_path]);
                }
                return best_states;
            }
            struct set_share_ptr_compptr
            {
                bool operator()(const sp_cZMapLineSegment &left, const sp_cZMapLineSegment &right) // 重载（）运算符
                {
                    if (std::stoi(left->id_) == std::stoi(right->id_))
                    {
                        return false;
                    }
                    else
                    {
                        return std::stoi(left->id_) < std::stoi(right->id_);
                    }
                }
            };

            std::vector<sp_cState> HMMLoc::GenerateStateFromTrajectory(sp_cZTrajectory observations)
            {
                std::vector<sp_cState> sp_states;
                std::vector<State> states;
                std::vector<sp_cZMapLineSegment> curves;
                std::set<sp_cZMapLineSegment, set_share_ptr_compptr> set_curves;
                std::set<State> set_states;
                for (const auto &pt : observations->points_)
                {
                    std::vector<sp_cZMapLineSegment> curves_near = sp_map_->get_lanes_near_enu(pt, near_threshold_);

                    for (const auto &curve : curves_near)
                    {
                        curves.push_back(curve);
                        set_curves.insert(curve);
                    }
                }

                for (const auto &curve : set_curves)
                {
                    std::shared_ptr<State> sp_state = std::make_shared<State>();
                    sp_state->curve_ = curve;
                    states.push_back(*sp_state);
                    sp_states.push_back(sp_state);
                }
                return sp_states;
            }

            double HMMLoc::EmissionProbability(sp_cState state, Eigen::Vector3d pt_enu)
            {
                Eigen::Vector3d cross_pt_curve_enu;
                double distance = sp_map_->get_distance_pt_curve_enu(pt_enu, state->curve_, cross_pt_curve_enu);
                double prob = CDF(lane_width_, gps_sigma_, distance);
                return prob;
            }

            double HMMLoc::EmissionProbability(uint64_t state, Eigen::Vector3d obs_pt_enu)
            {
                Eigen::Vector3d cross_pt_curve_enu;
                double distance = sp_hdmap_->get_distance_pt_curve_enu(obs_pt_enu, state, cross_pt_curve_enu);
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
                            double distance = sp_map_->get_distance_curve_cuvre_enu(states[i]->curve_, states[j]->curve_);
                            transition_matrix(i, j) = exp(-distance / 5);
                            // transition_matrix(i, j) = CDF(lane_width_,gps_sigma_,distance);
                        }
                    }
                    // normalize the transition probability of state to make the sum =1
                    double sum = transition_matrix.block<1, 4>(i, 0).sum();
                    transition_matrix.block<1, 4>(i, 0) /= sum;
                }
                // std::cout << transition_matrix << std::endl;
                return transition_matrix;
            }

            Eigen::MatrixXd HMMLoc::CalculateTransitionalProbability(std::vector<sp_cState> current_states, std::vector<sp_cState> previous_states)
            {
                Eigen::MatrixXd transition_matrix;
                int current_num_states = current_states.size();
                int previous_num_states = previous_states.size();
                return transition_matrix;
            }

        }
    }
}