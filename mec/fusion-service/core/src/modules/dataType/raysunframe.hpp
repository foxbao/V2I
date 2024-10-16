#pragma once
#include <vector>
#include "modules/common/util/util.h"
#include "modules/dataType/vehicle_obs.hpp"
// #include "proto/junction_package.pb.h"

namespace jos{
class radar_vision_info;
};
namespace coop {
namespace v2x {

class RaysunFrame {
 public:
  RaysunFrame();
  RaysunFrame(const RaysunHead &head, const std::vector<RaysunTarget> &targets);
  RaysunFrame(const jos::radar_vision_info &info);

 public:
  std::vector<VehicleObs> targets() const { return targets_; }
  size_t targets_size() const { return targets_.size(); }
  void set_head(const RaysunHead &head) { head_ = head; }
  void set_timestamp(const OCLong &timestamp) { head_.timestamp = timestamp; }
  void AddTarget(const VehicleObs &obs);
  void AddTargets(const std::vector<VehicleObs> &obs);
  RaysunHead head() const { return head_; }
  int TargetsNum() { return targets_.size(); }

 private:
  RaysunHead head_;
  std::vector<VehicleObs> targets_;

};

DEFINE_EXTEND_TYPE(RaysunFrame);

}  // namespace v2x
}  // namespace coop
