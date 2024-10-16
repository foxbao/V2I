#pragma once
namespace civ
{
    namespace V2I
    {
        class ttc
        {
        public:
            ttc();
            ~ttc();
            /// @brief
            /// @param X1 Position of Vehicle 1
            /// @param X2 Position of Vehicle 2
            /// @param v1 Speed of Vehicle 1
            /// @param v2 Speed of Vehicle 2
            /// @param l2 Length of Vehicle 2
            /// @return
            double CalculateTTC(double X1, double X2, double v1, double v2, double l2 = 5);

        private:
        };
    }
} // namespace V2I