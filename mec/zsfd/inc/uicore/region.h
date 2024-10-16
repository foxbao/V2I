/** @file uirgn.h
 * Definition of the ui region object
 */

#ifndef __CXX_ZAS_UICORE_REGION_H__
#define __CXX_ZAS_UICORE_REGION_H__

#include "uicore/uicore.h"

namespace zas {
namespace uicore {

template <typename T> struct geometry_
{
	T width;
	T height;
	geometry_() : width(-1), height(-1) {}
	geometry_(T w, T h) : width(w), height(h) {}
	geometry_(const geometry_& g) {
		if (&g == this) return;
		width = g.width;
		height = g.height;
	}
	geometry_& operator=(const geometry_& g) {
		if (&g != this) {
			width = g.width;
			height = g.height;
		}
		return *this;
	}
	bool operator==(const geometry_& g) {
		return (width == g.width && height == g.height)
		? true : false;
	}
	bool valid(void) {
		return (width <= 0 && height <= 0)
		? false : true;
	}
};

template <typename T> struct position_
{
	T x, y;
	position_() : x(0), y(0) {}
	position_(T _x, T _y) : x(_x), y(_y) {}
	position_(const position_& p) {
		if (&p == this) return;
		x = p.x; y = p.y;
	}
	position_& operator=(const position_& p) {
		if (&p != this) {
			x = p.x; y = p.y;
		}
		return *this;
	}
};

template <typename T> struct rect_
{
	T left;
	T top;
	T width;
	T height;

	rect_() : left((T)-1), top((T)-1), width((T)-1), height((T)-1) {}
	rect_(const rect_& r) : left(r.left), top(r.top), width(r.width), height(r.height) {}
	void operator=(const rect_&r) {
		left = r.left;
		top = r.top;
		width = r.width;
		height = r.height;
	}
	rect_(T l, T t, T w, T h)
		: left(l), top(t), width(w), height(h) {}
	rect_(T l, T t)
		: left(l), top(t), width(0), height(0) {}
	void set(T l, T t, T w, T h) {
		left = l;
		top = t;
		width = w;
		height = h;
	}
	void setpos(T l, T t) {
		left = l;
		top = t;
	}
	void setsize(T w, T h) {
		width = w;
		height = h;
	}
	void move(T dx, T dy) {
		left += dx;
		top += dy;
	}
	void clear(void) {
		left = (T)-1;
		top = (T)-1;
		width = (T)-1;
		height = (T)-1;
	}
	bool isnull(void) {
		return (width <= (T)0 || height <= (T)0)
			? true : false;
	}
	bool equal(const rect_<T>& r) {
		return (left == r.left && top = r.top
			&& width == r.width && height == r.height)
			? true : false;
	}
	bool operator==(const rect_<T>& r) {
		return equal(r);
	}
};

typedef rect_<int32_t> irect_t;
typedef rect_<float> rect_t;
typedef geometry_<int32_t> igeometry_t;
typedef geometry_<float> geometry_t;
typedef position_<int32_t> iposition_t;
typedef position_<float> position_t;

/**
  covert a position to pixel based position
  @param p the position (float) object
  @return iposition (int32_t) object
  */
UICORE_EXPORT iposition_t px(position_t& p);

/**
  covert a geometry to pixel based geometry
  @param g the geometry (float) object
  @return igeometry (int32_t) object
  */
UICORE_EXPORT igeometry_t px(geometry_t& g);

}} // end of namespace zas::uicore
#endif /* __CXX_ZAS_UICORE_REGION_H__ */
/* EOF */
