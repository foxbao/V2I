#include <set>
#include <math.h>

#include "inc/maputils.h"

namespace zas {
namespace mapcore {
using namespace std;

const double EPS = 1E-8;
const double EARTH_RADIUS = 6378137;
const double pai = 3.1415926535897931;

inline static double rad(double d) {
	return d * pai / 180.0;
}

inline static double deg(double r) {
	return r * 180 / pai;
}

double distance(double lon1,
	double lat1, double lon2, double lat2)
{
	double radlat1 = rad(lat1);  
	double radlat2 = rad(lat2);  
	double radlon1 = rad(lon1);  
	double radlon2 = rad(lon2);

	if (radlat1 < 0)
		radlat1 = pai / 2 + fabs(radlat1); // south  
	if (radlat1 > 0)
		radlat1 = pai / 2 - fabs(radlat1); // north  
	if (radlon1 < 0)
		radlon1 = pai * 2 - fabs(radlon1); // west  
	if (radlat2 < 0)
		radlat2 = pai / 2 + fabs(radlat2); // south
	if (radlat2 > 0)
		radlat2 = pai / 2 - fabs(radlat2); // north  
	if (radlon2 < 0)
		radlon2 = pai * 2 - fabs(radlon2); // west  

	double x1 = EARTH_RADIUS * cos(radlon1) * sin(radlat1);
	double y1 = EARTH_RADIUS * sin(radlon1) * sin(radlat1);
	double z1 = EARTH_RADIUS * cos(radlat1);

	double x2 = EARTH_RADIUS * cos(radlon2) * sin(radlat2);
	double y2 = EARTH_RADIUS * sin(radlon2) * sin(radlat2);
	double z2 = EARTH_RADIUS * cos(radlat2);

	double d = sqrt((x1 - x2) * (x1 - x2) + (y1 - y2)
		* (y1 - y2)+ (z1 - z2) * (z1 - z2));
	double theta = acos((EARTH_RADIUS * EARTH_RADIUS
		+ EARTH_RADIUS * EARTH_RADIUS - d * d)
		/ (2 * EARTH_RADIUS * EARTH_RADIUS));
	return theta * EARTH_RADIUS;
}

bool in_scope(double dst_lon, double dst_lat,
	double lon1, double lat1, double lon2, double lat2)
{
	double t;
	if (lon1 > lon2) {
		t = lon1, lon1 = lon2, lon2 = t;
	}
	if (lat1 > lat2) {
		t = lat1, lat1 = lat2, lat2 = t;
	}
	if (dst_lon < lon1 || dst_lon >= lon2) {
		return false;
	}
	if (dst_lat < lat1 || dst_lat >= lat2) {
		return false;
	}
	return true;
}

static inline bool equal(double v1, double v2)
{
	const double epsilon = 1E-5;
	return (fabs(v1 - v2) < epsilon)
		? true : false;
}

double azimuth(double lon1, double lat1, double lon2, double lat2)
{	
	lat1 = rad(lat1);
	lon1 = rad(lon1);
	lat2 = rad(lat2);
	lon2 = rad(lon2);

	double ret = sin(lat1) * sin(lat2) + cos(lat1) * cos(lat2) * cos(lon2 - lon1);
	ret = sqrt(1 - ret * ret);
	ret = cos(lat2) * sin(lon2 - lon1) / ret;
	ret = deg(asin(ret));

	if (lon1 > 0 && lon1 < 360
		&& lat1 > 0 && lat1 < 360) {
		if (lon2 > 0 && lon2 < 360
			&& lat2 > 0 && lat2 < 360) {
			if (lat2 > lat1) {	
				// if (lon2 > lon1) { do nothing }
				if (lon2 < lon1) {
					ret += 360;
				}
				else if (equal(lon2, lon1)) {
					ret = 0.;
				}
			} else if (lat2 < lat1) {
				if (lon2 > lon1) {
					ret += 90.; // east-south
				}
				else if (lon2 < lon1) {
					ret += 270.; // west-sourch
				}
				else {
					ret = 180;	// south
				}
			} else {
				if (lon2 > lon1) {
					ret = 90; // east
				}
				else if (lon2 < lon1) {
					ret = 270;	// west
				}
				else {
					ret = -3.;	// error
				}
			}
		} else {
			ret = -2; // invalid coord (<lon2, lat2>)
		}
	} else {
		ret = -1;	// invalid coord (<lon1, lat1>)
	}
	return ret;
}

static int dblcmp(double a, double b)
{
	if (fabs(a - b) <= EPS) return 0;
	if (a > b) return 1;
	else return -1;
}

static double dot(double x1, double y1, double x2, double y2) {
	return x1 * x2 + y1 * y2;
}

static int point_on_line(const point& a, const point& b, const point& c)
{
	return dblcmp(dot(b.x - a.x, b.y - a.y,
		c.x - a.x, c.y - a.y), 0);
}

static double cross(double x1, double y1, double x2, double y2) {
	return x1 * y2 - x2 * y1;
}

static double ab_cross_ac(const point& a, const point& b, const point& c) {
	return cross(b.x - a.x, b.y - a.y, c.x - a.x, c.y - a.y);
}

int get_cross_point(point& p,
	const point& a, const point& b, const point& c, const point& d)
{
	int d1, d2, d3, d4;
	double s1, s2, s3, s4;

	d1 = dblcmp(s1 = ab_cross_ac(a, b, c), 0);
	d2 = dblcmp(s2 = ab_cross_ac(a, b, d), 0);
	d3 = dblcmp(s3 = ab_cross_ac(c, d, a), 0);
	d4 = dblcmp(s4 = ab_cross_ac(c, d, b), 0);

	if ((d1 ^ d2) == -2 && (d3 ^ d4) == -2) {
		p.x = (c.x * s2 - d.x * s1) / (s2 - s1);
		p.y = (c.y * s2 - d.y * s1) / (s2 - s1);
		return 1;
	}

	if (d1 == 0 && point_on_line(c, a, b) <= 0) {
		p = c; return 0;
	}

	if (d2 == 0 && point_on_line(d, a, b) <= 0) {
		p = d; return 0;
	}

	if (d3 == 0 && point_on_line(a, c, d) <= 0) {
		p = a; return 0;
	}

	if (d4 == 0 && point_on_line(b, c, d) <= 0) {
		p = b; return 0;
	}
	return -1;
}

static bool is_point_in_polygon(vector<point> poly, point pt)
{
	int i, j;
	bool c = false;
	for (i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
	{
		if ((((poly[i].y <= pt.y) && (pt.y < poly[j].y)) ||
			((poly[j].y <= pt.y) && (pt.y < poly[i].y)))
			&& (pt.x < (poly[j].x - poly[i].x) * (pt.y - poly[i].y)
			/ (poly[j].y - poly[i].y) + poly[i].x)) {
			c = !c;
		}
	}
	return c;
}

static bool point_cmp(const point &a, const point &b, const point &center)
{
	if (a.x >= 0 && b.x < 0)
		return true;
	if (a.x == 0 && b.x == 0)
		return a.y > b.y;

	int det = (a.x - center.x) * (b.y - center.y) - (b.x - center.x) * (a.y - center.y);
	if (det < 0) return true;
	if (det > 0) return false;

	int d1 = (a.x - center.x) * (a.x - center.x) + (a.y - center.y) * (a.y - center.y);
	int d2 = (b.x - center.x) * (b.x - center.y) + (b.y - center.y) * (b.y - center.y);
	return d1 > d2;
}

static void clockwise_sort_points(vector<point> &vpoints)
{
	point center;
	double x = 0,y = 0;
	for (int i = 0;i < vpoints.size();i++)
	{
		x += vpoints[i].x;
		y += vpoints[i].y;
	}
	center.x = (int)x/vpoints.size();
	center.y = (int)y/vpoints.size();

	for(int i = 0;i < vpoints.size() - 1;i++)
	{
		for (int j = 0;j < vpoints.size() - i - 1;j++)
		{
			if (point_cmp(vpoints[j],vpoints[j+1],center))
			{
				point tmp = vpoints[j];
				vpoints[j] = vpoints[j + 1];
				vpoints[j + 1] = tmp;
			}
		}
	}
}

bool polygon_clip(const vector<point> &poly1,
	const vector<point> &poly2, vector<point>& output)
{
	if (poly1.size() < 3 || poly2.size() < 3) {
		return false;
	}

	point tmp;
	set<point> interpoly;
	for (int i = 0; i < poly1.size(); i++)
	{
		int poly1_next_idx = (i + 1) % poly1.size();
		for (int j = 0; j < poly2.size(); j++)
		{
			int poly2_next_idx = (j + 1) % poly2.size();
			if (get_cross_point(tmp, poly1[i], poly1[poly1_next_idx],
				poly2[j], poly2[poly2_next_idx]) >= 0) {
				interpoly.insert(tmp);
			}
		}
	}

	for (int i = 0; i < poly1.size(); i++) {
		if (is_point_in_polygon(poly2, poly1[i])) {
			interpoly.insert(poly1[i]);
		}
	}
	for (int i = 0; i < poly2.size(); i++) {
		if (is_point_in_polygon(poly1, poly2[i])) {
			interpoly.insert(poly2[i]);
		}
	}

	if(interpoly.size() <= 0)
		return false;

	output.clear();
	output.assign(interpoly.begin(), interpoly.end());
	clockwise_sort_points(output);
	return true;
}

double area(vector<point> pts)
{
	double ret = 0;
	int sz = pts.size();
	if(sz < 3) return 0.;
	for(int i = 2; i < sz; i++) {
		double x1 = pts[i].x - pts[0].x;
		double y1 = pts[i].y - pts[0].y;
		double x2 = pts[i - 1].x - pts[0].x;
		double y2 = pts[i - 1].y - pts[0].y;
		ret += x1 * y2 - y1 * x2;
	}
	return fabs(ret) / 2;
}

void reverse(vector<point>& pts)
{
	int n = pts.size();
	int cnt = (n - 1)  / 2 + 1;
	for (int i = 1; i < cnt; ++i) {
		point tmp = pts[i];
		pts[i] = pts[n - i];
		pts[n - i] = tmp;
	}
}

bool is_polygons_intersected(const vector<point> &poly1,
	const vector<point> &poly2)
{
	if (poly1.size() < 3 || poly2.size() < 3) {
		return false;
	}

	point tmp;
	for (int i = 0; i < poly1.size(); i++)
	{
		int poly1_next_idx = (i + 1) % poly1.size();
		for (int j = 0; j < poly2.size(); j++)
		{
			int poly2_next_idx = (j + 1) % poly2.size();
			if (get_cross_point(tmp, poly1[i], poly1[poly1_next_idx],
				poly2[j], poly2[poly2_next_idx]) >= 0) {
				return true;
			}
		}
	}

	for (int i = 0; i < poly1.size(); i++) {
		if (is_point_in_polygon(poly2, poly1[i])) {
			return true;
		}
	}
	for (int i = 0; i < poly2.size(); i++) {
		if (is_point_in_polygon(poly1, poly2[i])) {
			return true;
		}
	}
	return false;
}

}} // end of namespace zas::mapcore
/* EOF */
