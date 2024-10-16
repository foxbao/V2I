#include <cmath>
#include "math.h"
namespace V2I
{
    double sigmoid(double x)
    {
        return 1.0 / (1.0 + exp(-x));
    }
}
