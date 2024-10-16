
#ifndef __CXX_ZAS_MAPCORE_MAPUTILS_H__
#define __CXX_ZAS_MAPCORE_MAPUTILS_H__

#include <vector>
#include "mapcore/mapcore.h"

namespace zas {
namespace mapcore {
using namespace std;

struct point
{
	point() : x(0.), y(0.) {}
	point(double xx, double yy)
	: x(xx), y(yy) {}
	point(const point2d& p)
	: x(p.x), y(p.y) {}
	bool operator<(const point& other) const {
		return (x == other.x) ? y > other.y : x > other.x;
	}
	double x, y;
};

// clip between two polygons
bool polygon_clip(const vector<point>&,
	const vector<point>&, vector<point>& output);

// check if two polygon intersected
bool is_polygons_intersected(const vector<point> &poly1,
	const vector<point> &poly2);

// calculate the area
double area(vector<point> pts);

// reverse the order of all points
// clockwise <-> counter-clockwise
void reverse(vector<point>& pts);

}} // end of namespace zas::mapcore
#endif // __CXX_ZAS_MAPCORE_MAPUTILS_H__
/* EOF */