#include "Spiral.h"
#include "Math.hpp"
#include "Spiral/odrSpiral.h"
#include "Utils.hpp"

#include <array>
#include <cmath>
#include <functional>
#include <vector>

namespace odr
{
Spiral::Spiral(void** buf)
: RoadGeometry(buf)
{
    assert(nullptr != buf);
	void* dsbuf = *buf;
	assert(nullptr != dsbuf);
    double* curvstart = (double*)dsbuf;
	curv_start = *curvstart;
	dsbuf = (void*)(curvstart+1);
    double* curvend = (double*)dsbuf;
	curv_end = *curvend;
	dsbuf = (void*)(curvend+1);
	*buf = dsbuf;
    
    this->c_dot = (curv_end - curv_start) / length;
    this->s_start = curv_start / c_dot;
    this->s_end = curv_end / c_dot;
    s0_spiral = curv_start / c_dot;
    odrSpiral(s0_spiral, c_dot, &x0_spiral, &y0_spiral, &a0_spiral);
}
Spiral::Spiral(double s0, double x0, double y0, double hdg0, double length, double curv_start, double curv_end) :
    RoadGeometry(s0, x0, y0, hdg0, length, GeometryType::Spiral), curv_start(curv_start), curv_end(curv_end)
{
    this->c_dot = (curv_end - curv_start) / length;
    this->s_start = curv_start / c_dot;
    this->s_end = curv_end / c_dot;
    s0_spiral = curv_start / c_dot;
    odrSpiral(s0_spiral, c_dot, &x0_spiral, &y0_spiral, &a0_spiral);
}

Vec2D Spiral::get_xy(double s) const
{
    double xs_spiral, ys_spiral, as_spiral;
    odrSpiral(s - s0 + s0_spiral, c_dot, &xs_spiral, &ys_spiral, &as_spiral);

    double hdg = hdg0 - a0_spiral;
    double xt = (std::cos(hdg) * (xs_spiral - x0_spiral)) - (std::sin(hdg) * (ys_spiral - y0_spiral)) + x0;
    double yt = (std::sin(hdg) * (xs_spiral - x0_spiral)) + (std::cos(hdg) * (ys_spiral - y0_spiral)) + y0;
    return Vec2D{xt, yt};
}

Vec2D Spiral::get_grad(double s) const
{
    double xs_spiral, ys_spiral, as_spiral;
    odrSpiral(s - s0 + s0_spiral, c_dot, &xs_spiral, &ys_spiral, &as_spiral);
    const double hdg = as_spiral + hdg0 - a0_spiral;
    const double dx = std::cos(hdg);
    const double dy = std::sin(hdg);
    return {{dx, dy}};
}

std::set<double> Spiral::approximate_linear(double eps) const
{
    // TODO: properly implement
    std::set<double> s_vals;
    for (double s = s0; s < (s0 + length); s += (10 * eps))
        s_vals.insert(s);
    s_vals.insert(s0 + length);

    return s_vals;
}

uint64_t Spiral::getsize()
{
	uint64_t sz = RoadGeometry::getsize();
	sz += sizeof(double);
	sz += sizeof(double);
	return sz;
}

void* Spiral::serialize(void* buf)
{
	buf = RoadGeometry::serialize(buf);
    double* curvstart = (double*)buf;
	*curvstart = curv_start;
	buf = (void*)(curvstart + 1);
    double* curvend = (double*)buf;
	*curvend = curv_end;
	buf = (void*)(curvend + 1);
	return buf;
}

} // namespace odr