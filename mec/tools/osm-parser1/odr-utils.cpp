#include <set>
#include "odr-utils.h"

namespace osm_parser1 {
using namespace std;

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

class test {
public:
	test()
	{
		vector<point> vp1, vp2;
		vector<point> output;

		vp1.push_back({ 0, 0 });
		vp1.push_back({ 1, 0 });
		vp1.push_back({ 1, 1 });
		vp1.push_back({ 0, 1 });

		double x = -0.5;
		vp2.push_back({ x, 0 });
		vp2.push_back({ x + 1, 0 });
		vp2.push_back({ x + 0.5, 1 });

		bool ret = polygon_clip(vp1, vp2, output);
		printf("ret = %s, cnt = %lu\n", ret ? "true" : "false", output.size());
		for (auto&v : output) {
			printf("<%.2f, %.2f>-", v.x, v.y);
		}
		printf("\narea = %.2f\n", area(output));


		x = 0;
		vp2.clear();
		vp2.push_back({ x, 0 });
		vp2.push_back({ x + 1, 0 });
		vp2.push_back({ x + 0.5, 1 });

		ret = polygon_clip(vp1, vp2, output);
		printf("ret = %s, cnt = %lu\n", ret ? "true" : "false", output.size());
		for (auto&v : output) {
			printf("<%.2f, %.2f>-", v.x, v.y);
		}
		printf("\narea = %.2f\n", area(output));

		vp2.clear();
		vp2.push_back({ 0, 0 });
		vp2.push_back({ 0.5, 0 });
		vp2.push_back({ 0.5, 0.5 });
		vp2.push_back({ 0, 0.5 });

		ret = polygon_clip(vp1, vp2, output);
		printf("ret = %s, cnt = %lu\n", ret ? "true" : "false", output.size());
		for (auto&v : output) {
			printf("<%.2f, %.2f>-", v.x, v.y);
		}
		printf("\narea = %.2f\n", area(output));	
	}
};

//static test x;
} // end of namespace osm_parser1
/* EOF */
