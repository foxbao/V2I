#include "raysunframe.hpp"
#include "proto/junction_package.pb.h"
namespace coop {
namespace v2x {
RaysunFrame::RaysunFrame() {}

RaysunFrame::RaysunFrame(const RaysunHead &head,
                         const std::vector<RaysunTarget> &targets)
    : head_(head) {
  for (auto target : targets) {
    targets_.push_back(VehicleObs(target));
  }
}

RaysunFrame::RaysunFrame(const jos::radar_vision_info &info) {
  head_.devid = info.id();
  head_.devtype = info.type();
  head_.lat = info.lat();
  head_.lon = info.lon();
  head_.rdir = info.rdir();
  head_.timestamp = info.timestamp();
  head_.targetnum = info.targets_size();
  for (int i = 0; i < info.targets_size(); i++) {
    auto target = info.targets(i);
    targets_.push_back(VehicleObs(&target));
  }
}

void RaysunFrame::AddTarget(const VehicleObs &obs) {
  targets_.push_back(obs);
}

void RaysunFrame::AddTargets(const std::vector<VehicleObs> &targets) {
  for (auto &&target : targets) {
    targets_.push_back(target);
  }
}

}  // namespace v2x
}  // namespace coop
