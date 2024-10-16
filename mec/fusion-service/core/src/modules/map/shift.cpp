/******************************************************************************
 * Copyright 2022 The CIV Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/
#include "shift.h"
#include <math.h>

namespace zas {
namespace mapcore {

const double a = 6378245.0;
const double ee = 0.00669342162296594323;

// World Geodetic System ==> Mars Geodetic System
bool coord_t::outOfChina(void) {
  if (_lon < 72.004 || _lon > 137.8347) {
    return true;
  }
  if (_lat < 0.8293 || _lat > 55.8271) {
    return true;
  }
  return false;
}

double coord_t::transform_lat(double x, double y) {
  double ret = -100.0 + 2.0 * x + 3.0 * y + 0.2 * y * y + 0.1 * x * y +
               0.2 * sqrt(abs(x));
  ret += (20.0 * sin(6.0 * x * M_PI) + 20.0 * sin(2.0 * x * M_PI)) * 2.0 / 3.0;
  ret += (20.0 * sin(y * M_PI) + 40.0 * sin(y / 3.0 * M_PI)) * 2.0 / 3.0;
  ret +=
      (160.0 * sin(y / 12.0 * M_PI) + 320 * sin(y * M_PI / 30.0)) * 2.0 / 3.0;
  return ret;
}

double coord_t::transform_lon(double x, double y) {
  double ret =
      300.0 + x + 2.0 * y + 0.1 * x * x + 0.1 * x * y + 0.1 * sqrt(abs(x));
  ret += (20.0 * sin(6.0 * x * M_PI) + 20.0 * sin(2.0 * x * M_PI)) * 2.0 / 3.0;
  ret += (20.0 * sin(x * M_PI) + 40.0 * sin(x / 3.0 * M_PI)) * 2.0 / 3.0;
  ret +=
      (150.0 * sin(x / 12.0 * M_PI) + 300.0 * sin(x / 30.0 * M_PI)) * 2.0 / 3.0;
  return ret;
}

// (WGS-84) -> (GCJ-02)
coord_t& coord_t::wgs84_to_gcj02(void) {
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

  _lat += dlat;
  _lon += dlon;
  return *this;
}

//  (GCJ-02) -> (WGS-84)
coord_t& coord_t::gcj02_to_wgs84(void) {
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

  _lat = tmp._lat;
  _lon = tmp._lon;
  return *this;
}

const double x_M_PI = M_PI * 3000.0 / 180.0;

// (GCJ-02) -> (BD-09)
coord_t& coord_t::gcj02_to_bd09(void) {
  double x = _lon, y = _lat;
  double z = sqrt(x * x + y * y) + 0.00002 * sin(y * x_M_PI);
  double theta = atan2(y, x) + 0.000003 * cos(x * x_M_PI);

  _lat = z * sin(theta) + 0.006;
  _lon = z * cos(theta) + 0.0065;
  return *this;
}

// (WGS-84) -> (BD09)
coord_t& coord_t::wgs84_to_bd09(void) {
  return wgs84_to_gcj02().gcj02_to_bd09();
}

// (BD-09) -> (GCJ-02)
coord_t& coord_t::bd09_to_gcj02(void) {
  double x = _lat - 0.0065, y = _lon - 0.006;
  double z = sqrt(x * x + y * y) - 0.00002 * sin(y * x_M_PI);
  double theta = atan2(y, x) - 0.000003 * cos(x * x_M_PI);

  _lat = z * sin(theta);
  _lon = z * cos(theta);
  return *this;
}

}  // namespace mapcore
}  // namespace zas
/* EOF */
