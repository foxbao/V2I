#include <math.h>
#include "mapcore/shift.h"

namespace zas {
namespace mapcore {

const double a = 6378245.0;
const double ee = 0.00669342162296594323;

// World Geodetic System ==> Mars Geodetic System
bool coord_t::outOfChina(void)
{
	if (_lon < 72.004 || _lon > 137.8347) {
		return true;
	}
	if (_lat < 0.8293 || _lat > 55.8271) {
		return true;
	}
	return false;
}

double coord_t::transform_lat(double x, double y)
{
	double ret = -100.0 + 2.0 * x + 3.0 * y + 0.2 * y * y + 0.1 * x * y + 0.2 * sqrt(abs(x));
	ret += (20.0 * sin(6.0 * x * M_PI) + 20.0 * sin(2.0 * x * M_PI)) * 2.0 / 3.0;
	ret += (20.0 * sin(y * M_PI) + 40.0 * sin(y / 3.0 * M_PI)) * 2.0 / 3.0;
	ret += (160.0 * sin(y / 12.0 * M_PI) + 320 * sin(y * M_PI / 30.0)) * 2.0 / 3.0;
	return ret;
}

double coord_t::transform_lon(double x, double y)
{
	double ret = 300.0 + x + 2.0 * y + 0.1 * x * x + 0.1 * x * y + 0.1 * sqrt(abs(x));
	ret += (20.0 * sin(6.0 * x * M_PI) + 20.0 * sin(2.0 * x * M_PI)) * 2.0 / 3.0;
	ret += (20.0 * sin(x * M_PI) + 40.0 * sin(x / 3.0 * M_PI)) * 2.0 / 3.0;
	ret += (150.0 * sin(x / 12.0 * M_PI) + 300.0 * sin(x / 30.0 * M_PI)) * 2.0 / 3.0;
	return ret;
}

// (WGS-84) -> (GCJ-02)
coord_t& coord_t::wgs84_to_gcj02(void)
{
	if (outOfChina()) {
		return *this;
	}
	double wgLat = _lat;
	double wgLon = _lon;
	double dlat = transform_lat(_lon - 105.0, _lat - 35.0);
	double dlon = transform_lon(_lon - 105.0, _lat - 35.0);
	double radlat = _lat / 180.0 * M_PI;
	
	double magic = sin(radlat);
	magic = 1 - ee * magic * magic;
    
	double sqrtmagic = sqrt(magic);
	dlat = (dlat * 180.0) / ((a * (1 - ee)) / (magic * sqrtmagic) * M_PI);
	dlon = (dlon * 180.0) / (a / sqrtmagic * cos(radlat) * M_PI);

	_lat += dlat; _lon += dlon;
	return *this;
}

//  (GCJ-02) -> (WGS-84)
coord_t& coord_t::gcj02_to_wgs84(void)
{
	if (outOfChina()) {
		return *this;
	}

	double d = .0000001;
	coord_t tmp(*this);

	for (;;) {
		coord_t transform(tmp);
		transform.wgs84_to_gcj02();

		tmp._lat += _lat - transform._lat;
		tmp._lon += _lon - transform._lon;
		if (_lat - transform._lat <= d || _lon - transform._lon <= d) {
			break;
		}
	}

	_lat = tmp._lat; _lon = tmp._lon;
	return *this;
}

const double x_M_PI = M_PI * 3000.0 / 180.0;

// (GCJ-02) -> (BD-09)
coord_t& coord_t::gcj02_to_bd09(void)
{
	double x = _lon, y = _lat;
	double z = sqrt(x * x + y * y) + 0.00002 * sin(y * x_M_PI);
	double theta = atan2(y, x) + 0.000003 * cos(x * x_M_PI);

	_lat = z * sin(theta) + 0.006;
	_lon = z * cos(theta) + 0.0065;
	return *this;
}

// (WGS-84) -> (BD09)
coord_t& coord_t::wgs84_to_bd09(void)
{
	return wgs84_to_gcj02().gcj02_to_bd09();
}

// (BD-09) -> (GCJ-02)
coord_t& coord_t::bd09_to_gcj02(void)
{
	double x = _lat - 0.0065, y = _lon - 0.006;
	double z = sqrt(x * x + y * y) - 0.00002 * sin(y * x_M_PI);
	double theta = atan2(y, x) - 0.000003 * cos(x * x_M_PI);
	
	_lat = z * sin(theta);
	_lon = z * cos(theta);
	return *this;
}

inline static int outOfChina(double lat, double lng)
{
	if (lng < 72.004 || lng > 137.8347) {
		return 1;
	}
	if (lat < 0.8293 || lat > 55.8271) {
		return 1;
	}
	return 0;
}

#define EARTH_R 6378137.0

void transform(double x, double y, double *lat, double *lng)
{
	double xy = x * y;
	double absX = sqrt(fabs(x));
	double xPi = x * M_PI;
	double yPi = y * M_PI;
	double d = 20.0*sin(6.0*xPi) + 20.0*sin(2.0*xPi);

	*lat = d;
	*lng = d;

	*lat += 20.0*sin(yPi) + 40.0*sin(yPi/3.0);
	*lng += 20.0*sin(xPi) + 40.0*sin(xPi/3.0);

	*lat += 160.0*sin(yPi/12.0) + 320*sin(yPi/30.0);
	*lng += 150.0*sin(xPi/12.0) + 300.0*sin(xPi/30.0);

	*lat *= 2.0 / 3.0;
	*lng *= 2.0 / 3.0;

	*lat += -100.0 + 2.0*x + 3.0*y + 0.2*y*y + 0.1*xy + 0.2*absX;
	*lng += 300.0 + x + 2.0*y + 0.1*x*x + 0.1*xy + 0.1*absX;
}

static void delta(double lat, double lng, double *dLat, double *dLng)
{
	if ((dLat == NULL) || (dLng == NULL)) {
		return;
	}
	const double ee = 0.00669342162296594323;
	transform(lng-105.0, lat-35.0, dLat, dLng);
	double radLat = lat / 180.0 * M_PI;
	double magic = sin(radLat);
	magic = 1 - ee*magic*magic;
	double sqrtMagic = sqrt(magic);
	*dLat = (*dLat * 180.0) / ((EARTH_R * (1 - ee)) / (magic * sqrtMagic) * M_PI);
	*dLng = (*dLng * 180.0) / (EARTH_R / sqrtMagic * cos(radLat) * M_PI);
}

void wgs2gcj(double wgsLat, double wgsLng, double *gcjLat, double *gcjLng)
{
	if ((gcjLat == NULL) || (gcjLng == NULL)) {
		return;
	}
	if (outOfChina(wgsLat, wgsLng)) {
		*gcjLat = wgsLat;
		*gcjLng = wgsLng;
		return;
	}
	double dLat, dLng;
	delta(wgsLat, wgsLng, &dLat, &dLng);
	*gcjLat = wgsLat + dLat;
	*gcjLng = wgsLng + dLng;
}

void gcj2wgs(double gcjLat, double gcjLng, double *wgsLat, double *wgsLng)
{
	if ((wgsLat == NULL) || (wgsLng == NULL)) {
		return;
	}
	if (outOfChina(gcjLat, gcjLng)) {
		*wgsLat = gcjLat;
		*wgsLng = gcjLng;
		return;
	}
	double dLat, dLng;
	delta(gcjLat, gcjLng, &dLat, &dLng);
	*wgsLat = gcjLat - dLat;
	*wgsLng = gcjLng - dLng;
}

void gcj2wgs_exact(double gcjLat, double gcjLng, double *wgsLat, double *wgsLng)
{
	double initDelta = 0.01;
	double threshold = 0.000001;
	double dLat, dLng, mLat, mLng, pLat, pLng;
	dLat = dLng = initDelta;
	mLat = gcjLat - dLat;
	mLng = gcjLng - dLng;
	pLat = gcjLat + dLat;
	pLng = gcjLng + dLng;
	int i;
	for (i = 0; i < 30; i++) {
		*wgsLat = (mLat + pLat) / 2;
		*wgsLng = (mLng + pLng) / 2;
		double tmpLat, tmpLng;
		wgs2gcj(*wgsLat, *wgsLng, &tmpLat, &tmpLng);
		dLat = tmpLat - gcjLat;
		dLng = tmpLng - gcjLng;
		if ((fabs(dLat) < threshold) && (fabs(dLng) < threshold)) {
			return;
		}
		if (dLat > 0) {
			pLat = *wgsLat;
		} else {
			mLat = *wgsLat;
		}
		if (dLng > 0) {
			pLng = *wgsLng;
		} else {
			mLng = *wgsLng;
		}
	}
}

}} // end of namespace zas::mapcore
/* EOF */
