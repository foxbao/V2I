#pragma once
namespace V2I
{
    class gap
    {
    public:
        gap();
        ~gap();
        double CalculateGap(double D2, double V2, double D1, double V1);

    private:
        double alpha;
        double beta;
    };
} // namespace V2I