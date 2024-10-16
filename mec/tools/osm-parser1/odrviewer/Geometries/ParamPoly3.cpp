#include "ParamPoly3.h"
#include "Math.hpp"
#include "Utils.hpp"

#include <array>
#include <cmath>
#include <functional>
#include <vector>

namespace odr
{
ParamPoly3::ParamPoly3(void** buf)
: RoadGeometry(buf)
{
    assert(nullptr != buf);
	void* dsbuf = *buf;
	assert(nullptr != dsbuf);
    double* a_u = (double*)dsbuf;
	aU = *a_u;
	dsbuf = (void*)(a_u+1);
    double* b_u = (double*)dsbuf;
	bU = *b_u;
	dsbuf = (void*)(b_u+1);
    double* c_u = (double*)dsbuf;
	cU = *c_u;
	dsbuf = (void*)(c_u+1);
    double* d_u = (double*)dsbuf;
	dU = *d_u;
	dsbuf = (void*)(d_u+1);
    double* a_v = (double*)dsbuf;
	aV = *a_v;
	dsbuf = (void*)(a_v+1);
    double* b_v = (double*)dsbuf;
	bV = *b_v;
	dsbuf = (void*)(b_v+1);
    double* c_v = (double*)dsbuf;
	cV = *c_v;
	dsbuf = (void*)(c_v+1);
    double* d_v = (double*)dsbuf;
	dV = *d_v;
	dsbuf = (void*)(d_v+1);
    bool* range = (bool*)dsbuf;
	pRange_normalized = *range;
	dsbuf = (void*)(range+1);
	*buf = dsbuf;

    const std::array<Vec2D, 4> coefficients = {{{this->aU, this->aV}, {this->bU, this->bV}, {this->cU, this->cV}, {this->dU, this->dV}}};
    this->cubic_bezier = CubicBezier2D(CubicBezier2D::get_control_points(coefficients));

    this->cubic_bezier.arclen_t[length] = 1.0;
    this->cubic_bezier.valid_length = length;
}
ParamPoly3::ParamPoly3(double s0,
                       double x0,
                       double y0,
                       double hdg0,
                       double length,
                       double aU,
                       double bU,
                       double cU,
                       double dU,
                       double aV,
                       double bV,
                       double cV,
                       double dV,
                       bool   pRange_normalized) :
    RoadGeometry(s0, x0, y0, hdg0, length, GeometryType::ParamPoly3),
    aU(aU), bU(bU), cU(cU), dU(dU), aV(aV), bV(bV), cV(cV), dV(dV), pRange_normalized(pRange_normalized)
{
    if (!pRange_normalized)
    {
        this->bU = bU * length;
        this->bV = bV * length;
        this->cU = cU * length * length;
        this->cV = cV * length * length;
        this->dU = dU * length * length * length;
        this->dV = dV * length * length * length;
    }

    const std::array<Vec2D, 4> coefficients = {{{this->aU, this->aV}, {this->bU, this->bV}, {this->cU, this->cV}, {this->dU, this->dV}}};
    this->cubic_bezier = CubicBezier2D(CubicBezier2D::get_control_points(coefficients));

    this->cubic_bezier.arclen_t[length] = 1.0;
    this->cubic_bezier.valid_length = length;
}

Vec2D ParamPoly3::get_xy(double s) const
{
    const double p = this->cubic_bezier.get_t(s - s0);
    const Vec2D  pt = this->cubic_bezier.get(p);

    const double xt = (std::cos(hdg0) * pt[0]) - (std::sin(hdg0) * pt[1]) + x0;
    const double yt = (std::sin(hdg0) * pt[0]) + (std::cos(hdg0) * pt[1]) + y0;

    return Vec2D{xt, yt};
}

Vec2D ParamPoly3::get_grad(double s) const
{
    const double p = this->cubic_bezier.get_t(s - s0);
    const Vec2D  dxy = this->cubic_bezier.get_grad(p);

    const double h1 = std::cos(hdg0);
    const double h2 = std::sin(hdg0);
    const double dx = h1 * dxy[0] - h2 * dxy[1];
    const double dy = h2 * dxy[0] + h1 * dxy[1];

    return {{dx, dy}};
}

std::set<double> ParamPoly3::approximate_linear(double eps) const
{
    std::set<double> p_vals = this->cubic_bezier.approximate_linear(eps);

    std::set<double> s_vals;
    for (const double& p : p_vals)
        s_vals.insert(p * length + s0);

    return s_vals;
}

uint64_t ParamPoly3::getsize()
{
	uint64_t sz = RoadGeometry::getsize();
    sz += sizeof(double);
    sz += sizeof(double);
    sz += sizeof(double);
    sz += sizeof(double);
    sz += sizeof(double);
    sz += sizeof(double);
    sz += sizeof(double);
    sz += sizeof(double);
    sz += sizeof(bool);
    return sz;
}

void* ParamPoly3::serialize(void* buf)
{
	buf = RoadGeometry::serialize(buf);
    double* a_U = (double*)buf;
	*a_U = aU;
	buf = (void*)(a_U + 1);
    double* b_U = (double*)buf;
	*b_U = bU;
	buf = (void*)(b_U + 1);
    double* c_U = (double*)buf;
	*c_U = cU;
	buf = (void*)(c_U + 1);
    double* d_U = (double*)buf;
	*d_U = dU;
	buf = (void*)(d_U + 1);
    double* a_v = (double*)buf;
	*a_v = aV;
	buf = (void*)(a_v + 1);
    double* b_V = (double*)buf;
	*b_V = bV;
	buf = (void*)(b_V + 1);
    double* c_V = (double*)buf;
	*c_V = cV;
	buf = (void*)(c_V + 1);
    double* d_V = (double*)buf;
	*d_V = dV;
	buf = (void*)(d_V + 1);
    bool* range = (bool*)buf;
	*range = pRange_normalized;
	buf = (void*)(range + 1);
	return buf;
}

} // namespace odr