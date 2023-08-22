#include "hmm_loc.h"
#include <iostream>
#include <math.h>
#include <boost/math/distributions/normal.hpp>
namespace civ
{
    namespace V2I
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

            sp_map_ = std::make_shared<CivMap>();
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

        std::vector<State> HMMLoc::viterbiAlgorithm(sp_cZTrajectory observations)
        {
            int T = observations->points_.size();
            // Initialization step
            std::vector<sp_cZMapLineSegment> curves_near = sp_map_->get_lanes_near_enu(observations->points_[0], 5);

            Eigen::MatrixXd transition_matrix=CalculateTransitionalProbability(curves_near);

            int num_states=transition_matrix.cols();
            observations->points_[0];
            for (int s = 0; s < num_states; ++s)
            {


            }

            std::vector<State> best_states;
            return best_states;
        }

        double HMMLoc::EmissionProbability(double w, double sigma, double d)
        {
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
            boost::math::normal_distribution<> norm(d, sigma); // 期望，方差
            double cdf_up = boost::math::cdf(norm, w / 2);
            double cdf_down = boost::math::cdf(norm, -w / 2);
            return (cdf_up - cdf_down) / w;
        }

        void HMMLoc::ReadMap(std::string map_path)
        {
            sp_map_->ReadData(map_path);
        }

        void HMMLoc::CalculateTransitionalProbability()
        {
            int aaa = 1;
        }

        Eigen::MatrixXd HMMLoc::CalculateTransitionalProbability(std::vector<sp_cZMapLineSegment> curves_near)
        {
            Eigen::MatrixXd transition_matrix;
            int num_state=curves_near.size();
            transition_matrix=Eigen::MatrixXd(num_state,num_state);

            for(int i=0;i<num_state;i++)
            {
                for(int j=0;j<num_state;j++)
                {
                    
                    if(i==j)
                    {
                        transition_matrix(i,j)=1;
                    }
                    else
                    {
                        // calculate the transitional probability between two lanes
                        // double distance=0.5;

                        transition_matrix(i,j)=0.5;
                    }

                }
                // normalize the transition probability of state to make the sum =1
                double sum=transition_matrix.block<1,4>(i,0).sum();
                transition_matrix.block<1,4>(i,0)/=sum;
            }

            return transition_matrix;
        }

        // double model::MeasurementProbability(Eigen::Vector2d z, ZMapLineSegment r)
        // {
        //     double sigma_z=1;
        //     double greatCircle=1;
        //     double prob=(1/(sqrt(2)*M_PI*sigma_z))*exp(-0.5*greatCircle/sigma_z);
        //     return prob;
        // }

        // double model::MeasurementProbability(Eigen::Vector2d z)
        // {
        //     return 0.0;
        // }
        // double model::TransitionalProbability()
        // {
        //     return 0.0;
        // }
    }
}
