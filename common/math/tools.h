#pragma once
namespace civ
{
    namespace common
    {
        namespace math
        {
            /// @brief Calculate the cummulative probability function
            /// @param low_limit low limit of integral
            /// @param up_limit up limit of integral
            /// @param sigma the sigmal
            /// @param mean the mean value 
            /// @return 
            double CDF_normal(double low_limit, double up_limit, double sigma, double mean);
        
        
        
        }
    }
}