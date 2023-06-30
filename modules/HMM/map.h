#pragma once
#include <vector>
namespace V2I
{
    struct Point{
        double x;
        double y;
    };

    struct Line{
        std::vector<Point> points;
    };

    struct Map
    {
        std::vector<Line> lines;
    };
}