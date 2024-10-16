#include <iostream>
#include "ttc.h"
#include "common/common.h"
namespace civ
{
    namespace V2I
    {
        ttc::ttc()
        {
        }

        ttc::~ttc()
        {
        }

        double ttc::CalculateTTC(double X1, double X2, double v1, double v2, double l2)
        {
            if (abs(v1 - v2) < 0.00000001)
            {
                return MAXVAL;
            }
            return (X1 - X2 - l2) / (v1 - v2);
        }

    }
}
