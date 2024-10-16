#pragma once
#include "Junction.h"
#include "Mesh.h"
#include "Road.h"
#include "RoutingGraph.h"

#include <map>
#include <memory>
#include <string>

namespace odr
{

struct OpenDriveMapConfig
{
    bool with_lateralProfile = true;
    bool with_laneHeight = true;
    bool with_road_objects = true;
    bool center_map = false;
    bool abs_z_for_for_local_road_obj_outline = true;
};

class OpenDriveMap
{
public:
    OpenDriveMap() = default;

    ConstRoadSet get_roads() const;
    RoadSet      get_roads();

    RoutingGraph get_routing_graph() const;

    std::string xodr_file = "";
    std::string proj4 = "";

    double x_offs = 0;
    double y_offs = 0;

    std::map<std::string, std::shared_ptr<Road>>     roads;
    std::map<std::string, std::shared_ptr<Junction>> junctions;
};

} // namespace odr