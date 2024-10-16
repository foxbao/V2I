#include "RoadGeometry.h"

#include <algorithm>
#include <cmath>

namespace odr
{

RoadGeometry::RoadGeometry(void** buf)
{
	assert(nullptr != buf);
	void* dsbuf = *buf;
	assert(nullptr != dsbuf);
	double* s = (double*)dsbuf;
	s0 = *s;
	dsbuf = (void*)(s+1);
	double* x = (double*)dsbuf;
	x0 = *x;
	dsbuf = (void*)(x+1);
	double* y = (double*)dsbuf;
	y0 = *y;
	dsbuf = (void*)(y+1);
	double* hdg = (double*)dsbuf;
	hdg0 = *hdg;
	dsbuf = (void*)(hdg+1);
	double* len = (double*)dsbuf;
	length = *len;
	dsbuf = (void*)(len+1);
	uint32_t* t = (uint32_t*)dsbuf;
	if(*t == 1) {
		type = GeometryType::Line;
	}
	else if (*t == 2) {
		type = GeometryType::Arc;
	}
	else if (*t == 3) {
		type = GeometryType::Spiral;
	}
	else if (*t == 4) {
		type = GeometryType::ParamPoly3;
	}
	dsbuf = (void*)(t + 1);
	*buf = dsbuf;
}
RoadGeometry::RoadGeometry(double s0, double x0, double y0, double hdg0, double length, GeometryType type) :
    s0(s0), x0(x0), y0(y0), hdg0(hdg0), length(length), type(type)
{
}

uint64_t RoadGeometry::getsize()
{
    uint64_t sz = sizeof(double);
    sz += sizeof(double);
    sz += sizeof(double);
    sz += sizeof(double);
    sz += sizeof(double);
    sz += sizeof(uint32_t);
    return sz;
}

void* RoadGeometry::serialize(void* buf)
{
	double* s = (double*)buf;
	*s = s0;
	buf = (void*)(s + 1);
	double* x = (double*)buf;
	*x = x0;
	buf = (void*)(x + 1);
	double* y = (double*)buf;
	*y = y0;
	buf = (void*)(y + 1);
	double* hdg = (double*)buf;
	*hdg = hdg0;
	buf = (void*)(hdg + 1);
	double* len = (double*)buf;
	*len = length;
	buf = (void*)(len + 1);
	uint32_t* t = (uint32_t*)buf;
	if(type == GeometryType::Line) {
		*t = 1;
	} 
	else if(type == GeometryType::Arc) {
		*t = 2;
	}
	else if(type == GeometryType::Spiral) {
		*t = 3;
	}
	else if(type == GeometryType::ParamPoly3) {
		*t = 4;
	}
	buf = (void*)(t + 1);
	return buf;
}

} // namespace odr