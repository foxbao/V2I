#ifndef __CXX_OSM_PARSER1_ODR_UTILS_H__
#define __CXX_OSM_PARSER1_ODR_UTILS_H__

#include <stdio.h>
#include <math.h>
#include <vector>

namespace osm_parser1 {
using namespace std;

#define EPS		1E-8

struct point
{
	point() : x(0.), y(0.) {}
	point(double xx, double yy)
	: x(xx), y(yy) {}
	double x, y;
};

static bool operator<(const point& a, const point& b)
{
	return (a.x == b.x) ? a.y > b.y : a.x > b.x;
}

bool polygon_clip(const vector<point> &poly1,
	const vector<point> &poly2, vector<point>& output);
	
bool is_polygons_intersected(const vector<point> &poly1,
	const vector<point> &poly2);

double area(vector<point> pts);

} // end of namespace osm_parser1
#endif // __CXX_OSM_PARSER1_ODR_UTILS_H__
/* EOF */
