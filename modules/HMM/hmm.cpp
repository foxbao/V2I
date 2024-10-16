#include "hmm.h"
#include <iostream>
#include <math.h>
#include <boost/math/distributions/normal.hpp>
namespace civ
{
    namespace V2I
    {
        HMM::HMM()
        {

            num_states_ = 2;
            num_observations_ = 2;
            transition_matrix_ << 0.7, 0.3,
                0.4, 0.6;

            initial_probs_ << 0.6, 0.4;

            emission_matrix_ << 0.9, 0.1,
                0.2, 0.8;
        }

        HMM::~HMM()
        {
        }

        std::vector<int> HMM::viterbiAlgorithm(const std::vector<int> &observations)
        {
            int T = observations.size();
            // The maximum probability of a given node
            Eigen::MatrixXd delta(T, num_states_);
            // the path of a given node
            Eigen::MatrixXi psi(T, num_states_);
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

        double HMM::EmissionProbability(double w, double sigma, double d)
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
