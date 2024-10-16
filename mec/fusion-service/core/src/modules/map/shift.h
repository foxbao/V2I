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
#ifndef __CXX_ZAS_MAPCORE_SHIFT_H__
#define __CXX_ZAS_MAPCORE_SHIFT_H__

// #include "mapcore/mapcore.h"

namespace zas {
namespace mapcore {

class coord_t {
 public:
  coord_t() : _lat(0.), _lon(0.) {}
  coord_t(double lat, double lon) : _lat(lat), _lon(lon) {}

  coord_t(const coord_t& c) {
    _lat = c._lat;
    _lon = c._lon;
  }

  coord_t& operator=(const coord_t& c) {
    if (this == &c) {
      return *this;
    }
    _lat = c._lat;
    _lon = c._lon;
    return *this;
  }

  void set(double lat, double lon) { _lat = lat, _lon = lon; }

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

}  // namespace mapcore
}  // namespace zas
#endif  // __CXX_ZAS_MAPCORE_SHIFT_H__
/* EOF */
