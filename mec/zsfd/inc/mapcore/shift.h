#ifndef __CXX_ZAS_MAPCORE_SHIFT_H__
#define __CXX_ZAS_MAPCORE_SHIFT_H__

#include "mapcore/mapcore.h"

namespace zas {
namespace mapcore {

class MAPCORE_EXPORT coord_t
{
public:
	coord_t() : _lat(0.), _lon(0.) {}
	coord_t(double lat, double lon)
	: _lat(lat), _lon(lon) {
	}

	coord_t(const coord_t& c) {
		_lat = c._lat; _lon = c._lon;
	}

	coord_t& operator=(const coord_t& c) {
		if (this == &c) {
			return *this;
		}
		_lat = c._lat; _lon = c._lon;
		return *this;
	}

	void set(double lat, double lon) {
		_lat = lat, _lon = lon;
	}

	/**
	 * Convert coordinate from WGS84 -> GCJ02
	 * @return the converted coordination
	 */
	coord_t& wgs84_to_gcj02(void);
	coord_t& gcj02_to_wgs84(void);
	coord_t& wgs84_to_bd09(void);
	coord_t& gcj02_to_bd09(void);
	coord_t& bd09_to_gcj02(void);

	double lon(void) const { return _lon; }
	double lat(void) const { return _lat; }

private:
	bool outOfChina(void);
	double transform_lat(double x, double y);
	double transform_lon(double x, double y);

private:
	double _lat, _lon;
};

// in C form
MAPCORE_EXPORT void wgs2gcj(double wgslat, double wgslon, double *gcjlat, double *gcjlon);
MAPCORE_EXPORT void gcj2wgs(double gcjlat, double gcjlon, double *wgslat, double *wgslon);
MAPCORE_EXPORT void gcj2wgs_exact(double gcjlat, double gcjlon, double *wgslat, double *wgslon);

}} // end of namespace zas::mapcore
#endif // __CXX_ZAS_MAPCORE_SHIFT_H__
/* EOF */
