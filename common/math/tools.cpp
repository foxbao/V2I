#include <boost/math/distributions/normal.hpp>
#include "tools.h"

namespace civ
{
    namespace common
    {
        namespace math
        {
            double CDF_normal(double low_limit, double up_limit, double sigma, double mean)
            {
                if (up_limit <= low_limit)
                {
                    return 0;
                }
                boost::math::normal_distribution<> norm(mean, sigma); // 期望，方差
                double cdf_up = boost::math::cdf(norm, up_limit);
                double cdf_low = boost::math::cdf(norm, low_limit);
                return (cdf_up - cdf_low);
            }
        }
    }
}