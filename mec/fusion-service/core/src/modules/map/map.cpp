#include "modules/map/map.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include "modules/common/earth.h"
#include "modules/common/util/util.h"

namespace coop {
namespace v2x {
CivMap::CivMap() {}

CivMap::~CivMap() {}

void CivMap::ReadData(std::string file_path) {
  using std::stof;
  std::ifstream file;
  file.open(file_path.c_str(), std::ios_base::in);
  if (!file.is_open()) {
    std::cout << "打开地图失败";
  }
  std::string str_data;
  spZMapLineSegment line = std::make_shared<ZMapLineSegment>();
  while (std::getline(file, str_data)) {
    if (str_data.find("---") != std::string::npos) {
      lines_.push_back(line);
      line = std::make_shared<ZMapLineSegment>();
      // std::cout << "new one" << std::endl;
    } else {
      std::vector<std::string> split_result = split(str_data, " ");
      double lat = stod(split_result[0]);
      double lon = stod(split_result[1]);
      double alt = stod(split_result[2]);
      if (alt > 100 || alt < -100) {
        continue;
      }
      line->points_.push_back(Eigen::Vector3d(lat, lon, alt));
    }
  }
  file.close();
}

std::vector<sp_cZMapLineSegment> CivMap::get_lines_enu() {
  std::vector<sp_cZMapLineSegment> lines_enu;
  for (const auto& line_llh : lines_) {
    ZMapLineSegment line;
    for (const auto& pt_llh : line_llh->points_) {
      Eigen::Vector3d pt_enu = Earth::LLH2ENU(pt_llh, true);
      line.points_.push_back(pt_enu);
    }
    lines_enu.push_back(std::make_shared<ZMapLineSegment>(line));
  }
  return lines_enu;
}

}  // namespace v2x
}  // namespace coop

