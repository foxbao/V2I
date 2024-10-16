/** @file mapcore.h
 */

#ifndef __CXX_ZAS_MAPCORE_H__
#define __CXX_ZAS_MAPCORE_H__

#include "std/zasbsc.h"

#ifdef LIBMAPCORE
#define MAPCORE_EXPORT		__attribute__((visibility("default")))
#else
#define MAPCORE_EXPORT
#endif

namespace zas {
namespace mapcore {

struct point2d {
	double x, y;
} PACKED;

inline static double dbl_equal(double a, double b)
{
	double ret = a - b;
	// fabs(ret)
	*((size_t*)&ret) &= 0x7FFFFFFFFFFFFFFF;
	return (ret < 1e-5) ? true : false;
}

struct point3d
{
	point3d() { v[0] = v[1] = v[2] = 0.; }
	point3d(double x, double y) {
		set(x, y);
	}
	point3d(double x, double y, double z) {
		set(x, y, z);
	}
	void set(double x, double y) {
		v[0] = x, v[1] = y, v[2] = 0.;
	}
	void set(double x, double y, double z) {
		v[0] = x, v[1] = y, v[2] = z;
	}
	point3d(const point3d& pt) {
		v[0] = pt.v[0], v[1] = pt.v[1], v[2] = pt.v[2];
	}
	point3d& add(const point3d& o) {
		v[0] += o.v[0], v[1] += o.v[1], v[2] += o.v[2];
		return *this;
	}
	point3d& operator+=(const point3d& o) {
		return add(o);
	}
	point3d& sub(const point3d& o) {
		v[0] -= o.v[0], v[1] -= o.v[1], v[2] -= o.v[2];
		return *this;
	}
	bool operator==(const point3d& o) {
		if (this == &o) return true;
		return (dbl_equal(v[0], o.v[0]) && dbl_equal(v[1], o.v[1])
			&& dbl_equal(v[2], o.v[2])) ? true : false;
	}
	bool operator!=(const point3d& o) {
		if (this != &o) return true;
		return (!dbl_equal(v[0], o.v[0]) || !dbl_equal(v[1], o.v[1])
			|| !dbl_equal(v[2], o.v[2])) ? true : false;
	}
	point3d& operator-=(const point3d& o) {
		return sub(o);
	}
	point3d& operator=(const point3d& o) {
		if (this != &o) {
			v[0] = o.v[0], v[1] = o.v[1],
			v[2] = o.v[2];
		}
		return *this;
	}
	union {
		double v[3];
		struct {
			double lon, lat, height;
		} llh;
		struct {
			double x, y, z;
		} xyz;
	};
} PACKED;

inline static const point3d llh(double lon, double lat, double height = 0.) {
	return {lon, lat, height};
}

// geographic reference encoding type
#define GREF_WGS84			"EPSG:4326"
#define GREF_CGCS2000		"EPSG:4490"

// definition of road objects
enum hdrmap_roadobject_type
{
	hdrm_robjtype_unknown = 0,
	hdrm_robjtype_unclassfied = 100,
	hdrm_robjtype_signal_light,
	hdrm_robjtype_rsd,	// road side device
	hdrm_robjtype_arrow,
	hdrm_robjtype_junconver,
	hdrm_robjtype_sigboard,

	// for road object that is classified
	// but it's type is not cared
	hdrm_robjtype_classified,
	
	// end of types
	hdrm_robjtype_max,
};

enum hdrmap_roadside_devtype
{
	hdrm_rsdtype_unknown = 0,
	hdrm_rsdtype_radar_vision,
	hdrm_rsdtype_camera,
	hdrm_rsdtype_radar,

	// end of types
	hdrm_rsdtype_max,
};

/* use OR operation to combine */
enum hdrmap_arrow_type
{
	hdrm_arrowtype_straight_ahead = 1,
	hdrm_arrowtype_turn_left = 2,
	hdrm_arrowtype_turn_right = 4,
	hdrm_arrowtype_approaching_left = 8,
	hdrm_arrowtype_approaching_right = 16,
	hdrm_arrowtype_u_turn = 32,
};

enum hdrmap_sigboard_sharp_type
{
	hdrm_sigboard_shptype_unknown = 0,
	hdrm_sigboard_shptype_square,
	hdrm_sigboard_shptype_rectangle,
	hdrm_sigboard_shptype_circle,
	hdrm_sigboard_shptype_triangle,
	hdrm_sigboard_shptype_inv_triangle,
};

enum hdrmap_sigboard_kind
{
	hdrm_sigboard_kind_unknown = 0,
	hdrm_sigboard_kind_main,
	hdrm_sigboard_kind_auxillary,
	hdrm_sigboard_kind_composite,
};

enum hdmap_geometry_type
{
	hdmap_geometry_unknown = 0,
	hdmap_geometry_line,
	hdmap_geometry_spiral,
	hdmap_geometry_arc,
	hdmap_geometry_param_poly3,
};

enum hdmap_poly3_range
{
	hdmap_poly3_range_unknown = 0,
	hdmap_poly3_range_arc_length,
	hdmap_poly3_range_normalized,
};

/**
 * calculate the distance between two points
 * @param lon1, lat1: point 1
 * @param lon2, lat2: point 2
 * @return the distance
 */
MAPCORE_EXPORT double distance(
	double lon1, double lat1, double lon2, double lat2);

/**
 * calculate the azimuth between two points
 * @param lon1, lat1: point 1
 * @param lon2, lat2: point 2
 * @return the azimuth, < 0 means an error
 */
MAPCORE_EXPORT double azimuth(
	double lon1, double lat1, double lon2, double lat2);

MAPCORE_EXPORT bool in_scope(
	double dst_lon, double dst_lat,
	double lon1, double lat1,
	double lon2, double lat2);

class MAPCORE_EXPORT proj
{
public:
	proj();
	/*
	 * projection from [src CRS] to [dst CRS]
	 * @param pfrom the georef come from
	 * @param pto the georef to
	 */
	proj(const char* pfrom, const char* pto);
	~proj();

	/*
	 * check if the projection object is well initialized
	 * @return true for success
	 */
	bool valid(void) const;

	/*
	 * reset projection from [src CRS] to [dst CRS]
	 * @param pfrom the georef come from
	 * @param pto the georef to
	 */
	void reset(const char* pfrom, const char* pto);

	/*
	 * transfrom from the source CRS to the target CRS
	 * @param src the source coordination
	 * @param inv inverse transformation from target CRS to source CRS
	 * @return the transformed coordination
	 */
	point3d transform(const point3d& src, bool inv = false) const;
	void transform_point(point3d& src, bool inv = false) const;

	/*
	 * transfrom from the target CRS to the src CRS
	 * @param src the source coordination
	 * @return the transformed coordination
	 */
	point3d inv_transform(const point3d& src) const;
	void inv_transform_point(point3d& src) const;

private:
	void* _data;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(proj);
};

}} // end of name space zas::mapcore
#endif /* __CXX_ZAS_MAPCORE_H__ */
/* EOF */
