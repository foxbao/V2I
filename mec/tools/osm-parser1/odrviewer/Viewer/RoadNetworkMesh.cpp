#include "RoadNetworkMesh.h"

#include <algorithm>

namespace odr
{
template<typename T>
std::vector<size_t> get_outline_indices(const std::map<size_t, T>& start_indices, const size_t num_vertices)
{
    std::vector<size_t> out_indices;
    for (auto idx_val_iter = start_indices.begin(); idx_val_iter != start_indices.end(); idx_val_iter++)
    {
        const size_t start_idx = idx_val_iter->first;
        const size_t end_idx = (std::next(idx_val_iter) == start_indices.end()) ? num_vertices : std::next(idx_val_iter)->first;
        for (size_t idx = start_idx; idx < end_idx - 2; idx += 2)
        {
            out_indices.push_back(idx);
            out_indices.push_back(idx + 2);
        }
        for (size_t idx = start_idx + 1; idx < end_idx - 2; idx += 2)
        {
            out_indices.push_back(idx);
            out_indices.push_back(idx + 2);
        }
        out_indices.push_back(start_idx);
        out_indices.push_back(start_idx + 1);
        out_indices.push_back(end_idx - 2);
        out_indices.push_back(end_idx - 1);
    }

    return out_indices;
}

std::string RoadsMesh::get_road_id(size_t vert_idx) const { return get_nearest_val<size_t, std::string>(this->road_start_indices, vert_idx); }

double LanesMesh::get_lanesec_s0(size_t vert_idx) const { return get_nearest_val<size_t, double>(this->lanesec_start_indices, vert_idx); }

int LanesMesh::get_lane_id(size_t vert_idx) const { return get_nearest_val<size_t, int>(this->lane_start_indices, vert_idx); }

std::array<size_t, 2> RoadsMesh::get_idx_interval_road(size_t vert_idx) const
{
    return get_key_interval<size_t, std::string>(this->road_start_indices, vert_idx, this->vertices.size());
}

std::array<size_t, 2> LanesMesh::get_idx_interval_lanesec(size_t vert_idx) const
{
    return get_key_interval<size_t, double>(this->lanesec_start_indices, vert_idx, this->vertices.size());
}

std::array<size_t, 2> LanesMesh::get_idx_interval_lane(size_t vert_idx) const
{
    return get_key_interval<size_t, int>(this->lane_start_indices, vert_idx, this->vertices.size());
}

std::vector<size_t> LanesMesh::get_lane_outline_indices() const { return get_outline_indices<int>(this->lane_start_indices, this->vertices.size()); }

std::string RoadmarksMesh::get_roadmark_type(size_t vert_idx) const
{
    return get_nearest_val<size_t, std::string>(this->roadmark_type_start_indices, vert_idx);
}

std::array<size_t, 2> RoadmarksMesh::get_idx_interval_roadmark(size_t vert_idx) const
{
    return get_key_interval<size_t, std::string>(this->roadmark_type_start_indices, vert_idx, this->vertices.size());
}

std::vector<size_t> RoadmarksMesh::get_roadmark_outline_indices() const
{
    return get_outline_indices<std::string>(this->roadmark_type_start_indices, this->vertices.size());
}

std::string RoadObjectsMesh::get_road_object_id(size_t vert_idx) const
{
    return get_nearest_val<size_t, std::string>(this->road_object_start_indices, vert_idx);
}

std::array<size_t, 2> RoadObjectsMesh::get_idx_interval_road_object(size_t vert_idx) const
{
    return get_key_interval<size_t, std::string>(this->road_object_start_indices, vert_idx, this->vertices.size());
}

std::array<size_t, 2> RoadObjectsMesh::get_idx_interval_road_object_type(size_t vert_idx) const
{
    return get_key_interval<size_t, std::string>(this->road_object_type, vert_idx, this->vertices.size());
}

Mesh3D RoadNetworkMesh::get_mesh() const
{
    Mesh3D out_mesh;
    out_mesh.add_mesh(this->lanes_mesh);
    out_mesh.add_mesh(this->roadmarks_mesh);
    out_mesh.add_mesh(this->road_objects_mesh);
    out_mesh.add_mesh(this->refline);
    return out_mesh;
}

double distance_utm(double x1, double y1, double x2, double y2)
{
    return (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);
}

double RoadNetworkMesh::get_road_height(double x, double y, double distance)
{
    double height = 0.0;
    size_t max_vsz = lanes_mesh.vertices.size();
	std::map<size_t, int>::iterator it;
    size_t prev = 0;
    size_t next = 0;
    
	for(it = lanes_mesh.lane_start_indices.begin();
         it != lanes_mesh.lane_start_indices.end(); ++it) {
        next = it->first;
        if (prev == next) {
            continue;
        }
        if(lanes_mesh.lane_start_type[prev].compare("none")) {
            for (size_t i = prev + 1; i < next; i += 2) {
                auto tmp = distance_utm(lanes_mesh.vertices[i][0],
                    lanes_mesh.vertices[i][1], x, y);
                if (tmp < distance) {
                    distance = tmp;
                    height = lanes_mesh.vertices[i][2];
                    if (distance < 1.0) {
                        return height;
                    }
                }
            }
        } 
        prev = next;
    }
    if (next < max_vsz) {
        if(lanes_mesh.lane_start_type[next].compare("none")) {
            for (size_t i = prev + 1; i < max_vsz; i += 2) {
                auto tmp = distance_utm(lanes_mesh.vertices[i][0],
                    lanes_mesh.vertices[i][1], x, y);
                if (tmp < distance) {
                    distance = tmp;
                    height = lanes_mesh.vertices[i][2];
                    if (distance < 1.0) {
                        return height;
                    }
                }
            }
        }
    }
    return height;
}

} // namespace odr