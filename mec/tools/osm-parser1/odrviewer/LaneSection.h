#pragma once

#include "Lanes.h"
#include "Math.hpp"
#include "Utils.hpp"

#include <map>
#include <memory>
#include <set>
#include <string>

namespace odr
{
class Road;

struct LaneSection : public std::enable_shared_from_this<LaneSection>
{
    LaneSection(void** buf);
    LaneSection(double s0);
    virtual ~LaneSection() = default;

    double get_end() const;

    ConstLaneSet get_lanes() const;
    LaneSet      get_lanes();

    std::shared_ptr<const Lane> get_lane(double s, double t) const;
    std::shared_ptr<Lane>       get_lane(double s, double t);

    uint64_t getsize(void);
    void* serialize(void* buf);

    double              s0 = 0;
    std::weak_ptr<Road> road;
    double              length = 0;

    std::map<int, std::shared_ptr<Lane>> id_to_lane;
};

using ConstLaneSectionSet = std::set<std::shared_ptr<const LaneSection>, SharedPtrCmp<const LaneSection, double, &LaneSection::s0>>;
using LaneSectionSet = std::set<std::shared_ptr<LaneSection>, SharedPtrCmp<LaneSection, double, &LaneSection::s0>>;

} // namespace odr
