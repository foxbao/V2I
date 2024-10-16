#include "OpenDriveMap.h"
#include "Geometries/Arc.h"
#include "Geometries/CubicSpline.h"
#include "Geometries/Line.h"
#include "Geometries/ParamPoly3.h"
#include "Geometries/Spiral.h"
#include "LaneSection.h"
#include "Lanes.h"
#include "RefLine.h"
#include "Road.h"
#include "Utils.hpp"

#include <iostream>
#include <math.h>
#include <string>
#include <utility>

namespace odr
{

ConstRoadSet OpenDriveMap::get_roads() const
{
    ConstRoadSet roads;
    for (const auto& id_road : this->roads)
        roads.insert(id_road.second);
    return roads;
}

RoadSet OpenDriveMap::get_roads()
{
    RoadSet roads;
    for (const auto& id_road : this->roads)
        roads.insert(id_road.second);
    return roads;
}

RoutingGraph OpenDriveMap::get_routing_graph() const
{
    RoutingGraph routing_graph;
    using RoadPtr = std::shared_ptr<Road>;
    using LanePtr = std::shared_ptr<Lane>;
    using LanesecPtr = std::shared_ptr<LaneSection>;

    for (const bool find_successor : {true, false})
    {
        for (const auto& id_road : this->roads)
        {
            RoadPtr         road = id_road.second;
            const RoadLink& road_link = find_successor ? road->successor : road->predecessor;
            if (road_link.type != RoadLink::Type::Road || road_link.contact_point == RoadLink::ContactPoint::None)
                continue;

            RoadPtr next_road = try_get_val(this->roads, road_link.id, RoadPtr(nullptr));
            if (!next_road)
                continue;

            LanesecPtr next_road_lanesec = (road_link.contact_point == RoadLink::ContactPoint::Start) ? next_road->s_to_lanesection.begin()->second
                                                                                                      : next_road->s_to_lanesection.rbegin()->second;
            for (auto s_lanesec_iter = road->s_to_lanesection.begin(); s_lanesec_iter != road->s_to_lanesection.end(); s_lanesec_iter++)
            {
                LanesecPtr lanesec = s_lanesec_iter->second;
                LanesecPtr next_lanesec = nullptr;
                if (find_successor && std::next(s_lanesec_iter) == road->s_to_lanesection.end())
                    next_lanesec = next_road_lanesec; // take next road to find successor
                else if (!find_successor && s_lanesec_iter == road->s_to_lanesection.begin())
                    next_lanesec = next_road_lanesec; // take prev. road to find predecessor
                else
                    next_lanesec = find_successor ? std::next(s_lanesec_iter)->second : std::prev(s_lanesec_iter)->second;

                for (const auto& id_lane : lanesec->id_to_lane)
                {
                    LanePtr   lane = id_lane.second;
                    const int next_lane_id = find_successor ? lane->successor : lane->predecessor;
                    if (next_lane_id == 0)
                        continue;

                    LanePtr next_lane = try_get_val(next_lanesec->id_to_lane, next_lane_id, LanePtr(nullptr));
                    if (!next_lane)
                        continue;

                    LanePtr from = find_successor ? lane : next_lane;
                    LanePtr to = find_successor ? next_lane : lane;
                    routing_graph.add_edge(RoutingGraphEdge{from, to});
                }
            }
        }
    }

    for (const auto& id_junc : this->junctions)
    {
        for (const auto& id_conn : id_junc.second->connections)
        {
            const JunctionConnection& conn = id_conn.second;

            RoadPtr incoming_road = try_get_val(this->roads, conn.incoming_road, RoadPtr(nullptr));
            RoadPtr connecting_road = try_get_val(this->roads, conn.connecting_road, RoadPtr(nullptr));
            if (!incoming_road || !connecting_road)
                continue;

            const bool is_succ_junc = incoming_road->successor.type == RoadLink::Type::Junction && incoming_road->successor.id == conn.id;
            const bool is_pred_junc = incoming_road->predecessor.type == RoadLink::Type::Junction && incoming_road->predecessor.id == conn.id;
            if (!is_succ_junc && !is_pred_junc)
                continue;

            LanesecPtr incoming_lanesec =
                is_succ_junc ? incoming_road->s_to_lanesection.rbegin()->second : incoming_road->s_to_lanesection.begin()->second;
            LanesecPtr connecting_lanesec = (conn.contact_point == JunctionConnection::ContactPoint::Start)
                                                ? connecting_road->s_to_lanesection.begin()->second
                                                : connecting_road->s_to_lanesection.rbegin()->second;
            for (const JunctionLaneLink& lane_link : conn.lane_links)
            {
                if (lane_link.from == 0 || lane_link.to == 0)
                    continue;
                LanePtr from = try_get_val(incoming_lanesec->id_to_lane, lane_link.from, LanePtr(nullptr));
                LanePtr to = try_get_val(connecting_lanesec->id_to_lane, lane_link.to, LanePtr(nullptr));
                if (!from || !to)
                    continue;
                routing_graph.add_edge(RoutingGraphEdge{from, to});
            }
        }
    }

    return routing_graph;
}

} // namespace odr
