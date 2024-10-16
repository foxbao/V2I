#include "RefLine.h"
#include "Geometries/RoadGeometry.h"
#include "Math.hpp"
#include "Utils.hpp"

#include <functional>
#include <utility>

namespace odr
{

RefLine::RefLine(void** buf) 
{
	assert(nullptr != buf);
	void* dsbuf = *buf;
	assert(nullptr != dsbuf);
	double* len = (double*)dsbuf;
	length = *len;
	dsbuf = elevation_profile.deserialize((void*)(len + 1));
	uint64_t* sz = (uint64_t*)dsbuf;
	dsbuf = (void*)(sz + 1);
	for (uint64_t i = 0; i < *sz; i++) {
		double* s0  = (double*)dsbuf;
		uint32_t* type = (uint32_t*)(s0 + 1);
		dsbuf = (void*)(type + 1);
        if(*type == 1){
            std::shared_ptr<RoadGeometry> line = std::make_shared<Line>(&dsbuf);
		    s0_to_geometry.insert(std::map<double, std::shared_ptr<RoadGeometry>>::value_type(*s0, line));
        }
        else if(*type == 2) {
            std::shared_ptr<RoadGeometry> arc = std::make_shared<Arc>(&dsbuf);
		    s0_to_geometry.insert(std::map<double, std::shared_ptr<RoadGeometry>>::value_type(*s0, arc));
        }
        else if(*type == 3) {
            std::shared_ptr<RoadGeometry> sprial = std::make_shared<Spiral>(&dsbuf);
		    s0_to_geometry.insert(std::map<double, std::shared_ptr<RoadGeometry>>::value_type(*s0, sprial));
        }
        else if(*type == 4) {
            std::shared_ptr<RoadGeometry> parampoly3 = std::make_shared<ParamPoly3>(&dsbuf);
		    s0_to_geometry.insert(std::map<double, std::shared_ptr<RoadGeometry>>::value_type(*s0, parampoly3));
        }
	}
    *buf = dsbuf;
}

RefLine::RefLine(double length) : length(length) {}

ConstRoadGeometrySet RefLine::get_geometries() const
{
    ConstRoadGeometrySet geometries;
    for (const auto& s0_geometry : this->s0_to_geometry)
        geometries.insert(s0_geometry.second);

    return geometries;
}

RoadGeometrySet RefLine::get_geometries()
{
    RoadGeometrySet geometries;
    for (const auto& s0_geometry : this->s0_to_geometry)
        geometries.insert(s0_geometry.second);

    return geometries;
}

std::shared_ptr<const RoadGeometry> RefLine::get_geometry(double s) const
{
    if (this->s0_to_geometry.size() > 0)
    {
        auto target_geom_iter = this->s0_to_geometry.upper_bound(s);
        if (target_geom_iter != s0_to_geometry.begin())
            target_geom_iter--;
        return target_geom_iter->second;
    }
    return nullptr;
}

Vec3D RefLine::get_xyz(double s) const
{
    std::shared_ptr<const RoadGeometry> geom = this->get_geometry(s);

    Vec2D pt_xy{0, 0};
    if (geom)
        pt_xy = geom->get_xy(s);

    return Vec3D{pt_xy[0], pt_xy[1], this->elevation_profile.get(s)};
}

Vec3D RefLine::get_grad(double s) const
{
    std::shared_ptr<const RoadGeometry> geom = this->get_geometry(s);

    Vec2D d_xy{0, 0};
    if (geom)
        d_xy = geom->get_grad(s);

    return Vec3D{d_xy[0], d_xy[1], this->elevation_profile.get_grad(s)};
}

double RefLine::match(double x, double y) const
{
    std::function<double(double)> f_dist = [&](const double s)
    {
        const Vec3D pt = this->get_xyz(s);
        return euclDistance(Vec2D{pt[0], pt[1]}, {x, y});
    };
    return golden_section_search<double>(f_dist, 0.0, length, 1e-2);
}

Line3D RefLine::get_line(double s_start, double s_end, double eps) const
{
    std::set<double> s_vals = this->approximate_linear(eps, s_start, s_end);

    Line3D out_line;
    for (const double& s : s_vals)
        out_line.push_back(this->get_xyz(s));
    return out_line;
}

std::set<double> RefLine::approximate_linear(double eps, double s_start, double s_end) const
{
    if ((s_start == s_end) || this->s0_to_geometry.empty())
        return {};

    auto s_end_geom_iter = this->s0_to_geometry.lower_bound(s_end);
    auto s_start_geom_iter = this->s0_to_geometry.upper_bound(s_start);
    if (s_start_geom_iter != s0_to_geometry.begin())
        s_start_geom_iter--;

    std::vector<double> s_vals{s_start};
    for (auto s0_geom_iter = s_start_geom_iter; s0_geom_iter != s_end_geom_iter; s0_geom_iter++)
    {
        const std::set<double> s_vals_geom = s0_geom_iter->second->approximate_linear(eps);
        if (s_vals_geom.size() < 2)
            throw std::runtime_error("expected at least two sample points");
        for (const double& s : s_vals_geom)
        {
            if (s > s_start && s < s_end)
                s_vals.push_back(s);
        }
        if (std::next(s0_geom_iter) != s_end_geom_iter)
            s_vals.pop_back();
    }

    std::set<double> s_vals_elevation = this->elevation_profile.approximate_linear(eps, s_start, s_end);
    for (const double& s : s_vals_elevation)
    {
        if (s > s_start && s < s_end)
            s_vals.push_back(s);
    }

    s_vals.push_back(s_end);

    std::set<double> s_vals_set(s_vals.begin(), s_vals.end());
    return s_vals_set;
}

uint64_t RefLine::getsize()
{
	uint64_t sz = sizeof(double); // length
	sz += elevation_profile.getsize();  // elevation_profile
	sz += sizeof(uint64_t); // s0_to_geometry number;
	std::map<double, std::shared_ptr<RoadGeometry>>::iterator it;
	for(it = s0_to_geometry.begin(); it != s0_to_geometry.end(); ++it) {
		sz += sizeof(double);   // map double
		sz += sizeof(uint32_t); // geometry type
        if(it->second->type == GeometryType::Line) {
            std::shared_ptr<Line> line = std::dynamic_pointer_cast<Line>(it->second);
            sz += line->getsize();
        }
        else if(it->second->type == GeometryType::Arc) {
		    std::shared_ptr<Arc> arc = std::dynamic_pointer_cast<Arc>(it->second);
            sz += arc->getsize();
        }
        else if(it->second->type == GeometryType::Spiral) {
		    std::shared_ptr<Spiral> spiral = std::dynamic_pointer_cast<Spiral>(it->second);
            sz += spiral->getsize();
        }
        else if(it->second->type == GeometryType::ParamPoly3) {
		    std::shared_ptr<ParamPoly3> ppoly3 = std::dynamic_pointer_cast<ParamPoly3>(it->second);
            sz += ppoly3->getsize();
        }
	}
	return sz;
}

void* RefLine::serialize(void* buf) 
{
	double* len = (double*)buf;
	*len = length;
	buf = elevation_profile.serialize((void*)(len + 1));
	uint64_t* number = (uint64_t*)buf;
	*number = s0_to_geometry.size();
	buf = (void*)(number + 1);
	std::map<double, std::shared_ptr<RoadGeometry>>::iterator it;
	for(it = s0_to_geometry.begin(); it != s0_to_geometry.end(); ++it) {
		double* s0 = (double*)buf;
		*s0 = it->first;
		uint32_t* type = (uint32_t*)(s0 + 1);
		if(it->second->type == GeometryType::Line) {
            *type = 1;
            buf = (void*)(type + 1);
            std::shared_ptr<Line> line = std::dynamic_pointer_cast<Line>(it->second);
		    buf = line->serialize(buf);
        }
        else if(it->second->type == GeometryType::Arc) {
            *type = 2;
            buf = (void*)(type + 1);
		    std::shared_ptr<Arc> arc = std::dynamic_pointer_cast<Arc>(it->second);
		    buf = arc->serialize(buf);
        }
        else if(it->second->type == GeometryType::Spiral) {
            *type = 3;
            buf = (void*)(type + 1);
		    std::shared_ptr<Spiral> spiral = std::dynamic_pointer_cast<Spiral>(it->second);
		    buf = spiral->serialize(buf);
        }
        else if(it->second->type == GeometryType::ParamPoly3) {
            *type = 4;
            buf = (void*)(type + 1);
		    std::shared_ptr<ParamPoly3> ppoly3 = std::dynamic_pointer_cast<ParamPoly3>(it->second);
		    buf = ppoly3->serialize(buf);
        }
	}
	return buf;
}

} // namespace odr