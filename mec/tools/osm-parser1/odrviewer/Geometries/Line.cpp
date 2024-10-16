#include "Line.h"
#include "Math.hpp"
#include "RoadGeometry.h"
#include "Utils.hpp"

#include <array>
#include <cmath>
#include <functional>
#include <vector>

namespace odr
{
Line::Line(void** buf)
:RoadGeometry(buf)
{
    
}
Line::Line(double s0, double x0, double y0, double hdg0, double length) : RoadGeometry(s0, x0, y0, hdg0, length, GeometryType::Line) {}

Vec2D Line::get_xy(double s) const
{
    const double x = (std::cos(hdg0) * (s - s0)) + x0;
    const double y = (std::sin(hdg0) * (s - s0)) + y0;
    return Vec2D{x, y};
}

Vec2D Line::get_grad(double s) const { return {{std::cos(hdg0), std::sin(hdg0)}}; }

std::set<double> Line::approximate_linear(double eps) const 
{ 
    std::set<double> s_vals;
    for (double s = s0; s < (s0 + length); s += eps)
        s_vals.insert(s);
    s_vals.insert(s0 + length);
    return s_vals;
}

uint64_t Line::getsize()
{
    return RoadGeometry::getsize();
}

void* Line::serialize(void* buf)
{
    buf = RoadGeometry::serialize(buf);
	return buf;
}

} // namespace odr