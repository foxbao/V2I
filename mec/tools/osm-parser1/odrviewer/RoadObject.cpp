#include "RoadObject.h"
#include "RefLine.h"
#include "Road.h"

#include "earcut/earcut.hpp"

#include <cmath>
#include <math.h>

namespace odr
{
Mesh3D RoadObject::get_cylinder(double eps, double radius, double height)
{
    Mesh3D cylinder_mesh;
    cylinder_mesh.vertices.push_back({0, 0, 0});
    cylinder_mesh.vertices.push_back({0, 0, height});

    eps = 0.5 * eps; // reduce eps a bit, cylinders more subsceptible to low resolution
    const double eps_angle = (radius <= eps) ? M_PI / 6 : std::acos((radius * radius - 4 * radius * eps + 2 * eps * eps) / (radius * radius));

    std::vector<double> angles;
    for (double alpha = 0; alpha < 2 * M_PI; alpha += eps_angle)
        angles.push_back(alpha);
    angles.push_back(2 * M_PI);

    for (const double& alpha : angles)
    {
        const Vec3D circle_pt_bottom = {radius * std::cos(alpha), radius * std::sin(alpha), 0};
        const Vec3D circle_pt_top = {radius * std::cos(alpha), radius * std::sin(alpha), height};
        cylinder_mesh.vertices.push_back(circle_pt_bottom);
        cylinder_mesh.vertices.push_back(circle_pt_top);

        if (cylinder_mesh.vertices.size() > 5)
        {
            const size_t          cur_idx = cylinder_mesh.vertices.size() - 1;
            std::array<size_t, 6> top_bottom_idx_patch = {0, cur_idx - 1, cur_idx - 3, 1, cur_idx - 2, cur_idx};
            cylinder_mesh.indices.insert(cylinder_mesh.indices.end(), top_bottom_idx_patch.begin(), top_bottom_idx_patch.end());
            std::array<size_t, 6> wall_idx_patch = {cur_idx, cur_idx - 2, cur_idx - 3, cur_idx, cur_idx - 3, cur_idx - 1};
            cylinder_mesh.indices.insert(cylinder_mesh.indices.end(), wall_idx_patch.begin(), wall_idx_patch.end());
        }
    }

    return cylinder_mesh;
}

Mesh3D RoadObject::get_box(double w, double l, double h)
{
    Mesh3D box_mesh;
    box_mesh.vertices = {Vec3D{l / 2, w / 2, 0},
                         Vec3D{-l / 2, w / 2, 0},
                         Vec3D{-l / 2, -w / 2, 0},
                         Vec3D{l / 2, -w / 2, 0},
                         Vec3D{l / 2, w / 2, h},
                         Vec3D{-l / 2, w / 2, h},
                         Vec3D{-l / 2, -w / 2, h},
                         Vec3D{l / 2, -w / 2, h}};
    box_mesh.indices = {0, 3, 1, 3, 2, 1, 4, 5, 7, 7, 5, 6, 7, 6, 3, 3, 6, 2, 5, 4, 1, 1, 4, 0, 0, 4, 7, 7, 3, 0, 1, 6, 5, 1, 2, 6};

    return box_mesh;
}

Mesh3D RoadObject::get_mesh(double eps) const
{
    auto road_ptr = this->road.lock();
    if (!road_ptr)
        throw std::runtime_error("could not access parent road for road object");

    std::vector<RoadObjectRepeat> repeats_copy = this->repeats; // make copy to keep method const
    if (repeats_copy.empty() && this->outline.empty())          // handle single object as 1 object repeat
    {
        RoadObjectRepeat rp;
        rp.s0 = rp.t_start = rp.t_end = rp.width_start = rp.width_end = rp.height_start = rp.height_end = rp.z_offset_start = rp.z_offset_end = NAN;
        rp.length = 0;
        rp.distance = 1;
        repeats_copy.push_back(rp);
    }

    const Mat3D rot_mat = EulerAnglesToMatrix<double>(roll, pitch, hdg);

    /* helper functions - object repeat's attributes override object's attributes */
    auto get_t_s = [&](const RoadObjectRepeat& r, const double& p) -> double
    { return (isnan(r.t_start) || isnan(r.t_end)) ? this->t0 : r.t_start + p * (r.t_end - r.t_start); };

    auto get_z_s = [&](const RoadObjectRepeat& r, const double& p) -> double
    { return (isnan(r.z_offset_start) || isnan(r.z_offset_end)) ? this->z0 : r.z_offset_start + p * (r.z_offset_end - r.z_offset_start); };

    auto get_height_s = [&](const RoadObjectRepeat& r, const double& p) -> double
    { return (isnan(r.height_start) || isnan(r.height_end)) ? this->height : r.height_start + p * (r.height_end - r.height_start); };

    auto get_width_s = [&](const RoadObjectRepeat& r, const double& p) -> double
    { return (isnan(r.width_start) || isnan(r.width_end)) ? this->width : r.width_start + p * (r.width_end - r.width_start); };

    Mesh3D road_obj_mesh;

    for (const RoadObjectRepeat& repeat : repeats_copy)
    {
        const double s_start = isnan(repeat.s0) ? this->s0 : repeat.s0;
        const double s_end = std::min(s_start + repeat.length, road_ptr->length);

        if (repeat.distance != 0)
        {
            for (double s = s_start; s <= s_end; s += repeat.distance)
            {
                const double p = (s_end == s_start) ? 1.0 : (s - s_start) / (s_end - s_start);
                const double t_s = get_t_s(repeat, p);
                const double z_s = get_z_s(repeat, p);
                const double height_s = get_height_s(repeat, p);
                const double w_s = get_width_s(repeat, p);

                Mesh3D single_road_obj_mesh;
                if (this->radius > 0)
                {
                    single_road_obj_mesh = this->get_cylinder(eps, this->radius, height_s);
                }
                else if (w_s > 0 && this->length > 0)
                {
                    single_road_obj_mesh = this->get_box(w_s, this->length, height_s);
                }

                Vec3D       e_s, e_t, e_h;
                const Vec3D p0 = road_ptr->get_xyz(s, t_s, z_s, &e_s, &e_t, &e_h);
                const Mat3D base_mat{{{e_s[0], e_t[0], e_h[0]}, {e_s[1], e_t[1], e_h[1]}, {e_s[2], e_t[2], e_h[2]}}};
                for (Vec3D& pt_uvz : single_road_obj_mesh.vertices)
                {
                    pt_uvz = MatVecMultiplication(rot_mat, pt_uvz);
                    pt_uvz = MatVecMultiplication(base_mat, pt_uvz);
                    pt_uvz = add(pt_uvz, p0);

                    single_road_obj_mesh.st_coordinates.push_back({s, t_s});
                }

                road_obj_mesh.add_mesh(single_road_obj_mesh);
            }
        }
        else
        {
            Mesh3D continuous_road_obj_mesh;

            const std::array<size_t, 24> idx_patch_template = {1, 5, 4, 1, 4, 0, 2, 7, 6, 2, 3, 7, 1, 6, 5, 1, 2, 6, 0, 4, 7, 0, 7, 3};
            for (const double& s : road_ptr->ref_line->approximate_linear(eps, s_start, s_end))
            {
                const double p = (s_end == s_start) ? 1.0 : (s - s_start) / (s_end - s_start);
                const double t_s = get_t_s(repeat, p);
                const double z_s = get_z_s(repeat, p);
                const double height_s = get_height_s(repeat, p);
                const double w_s = get_width_s(repeat, p);

                continuous_road_obj_mesh.vertices.push_back(road_ptr->get_xyz(s, t_s - 0.5 * w_s, z_s));
                continuous_road_obj_mesh.vertices.push_back(road_ptr->get_xyz(s, t_s + 0.5 * w_s, z_s));
                continuous_road_obj_mesh.vertices.push_back(road_ptr->get_xyz(s, t_s + 0.5 * w_s, z_s + height_s));
                continuous_road_obj_mesh.vertices.push_back(road_ptr->get_xyz(s, t_s - 0.5 * w_s, z_s + height_s));

                const std::array<Vec2D, 4> s_t_coords = {{{s, t_s - 0.5 * w_s}, {s, t_s + 0.5 * w_s}, {s, t_s + 0.5 * w_s}, {s, t_s - 0.5 * w_s}}};
                continuous_road_obj_mesh.st_coordinates.insert(continuous_road_obj_mesh.st_coordinates.end(), s_t_coords.begin(), s_t_coords.end());

                if (continuous_road_obj_mesh.vertices.size() == 4)
                {
                    const std::array<size_t, 6> front_idx_patch = {0, 2, 1, 0, 3, 2};
                    continuous_road_obj_mesh.indices.insert(continuous_road_obj_mesh.indices.end(), front_idx_patch.begin(), front_idx_patch.end());
                }

                if (continuous_road_obj_mesh.vertices.size() > 7)
                {
                    const size_t           cur_offs = continuous_road_obj_mesh.vertices.size() - 8;
                    std::array<size_t, 24> wall_idx_patch;
                    for (size_t idx = 0; idx < idx_patch_template.size(); idx++)
                        wall_idx_patch.at(idx) = idx_patch_template.at(idx) + cur_offs;
                    continuous_road_obj_mesh.indices.insert(continuous_road_obj_mesh.indices.end(), wall_idx_patch.begin(), wall_idx_patch.end());
                }
            }

            const size_t                last_idx = continuous_road_obj_mesh.vertices.size() - 1;
            const std::array<size_t, 6> back_idx_patch = {last_idx - 3, last_idx - 2, last_idx - 1, last_idx - 3, last_idx - 1, last_idx};
            continuous_road_obj_mesh.indices.insert(continuous_road_obj_mesh.indices.end(), back_idx_patch.begin(), back_idx_patch.end());

            road_obj_mesh.add_mesh(continuous_road_obj_mesh);
        }
    }

    if (this->outline.size() > 1)
    {
        Vec3D       e_s, e_t, e_h;
        const Vec3D p0 = road_ptr->get_xyz(this->s0, this->t0, this->z0, &e_s, &e_t, &e_h);

        const Mat3D base_mat{{{e_s[0], e_t[0], e_h[0]}, {e_s[1], e_t[1], e_h[1]}, {e_s[2], e_t[2], e_h[2]}}};

        Mesh3D outline_road_obj_mesh;

        /* add top outline first - ensure the top vertices are at the front */
        const bool is_flat_object = std::all_of(this->outline.begin(), this->outline.end(), [](const RoadObjectCorner& c) { return c.height == 0; });
        if (!is_flat_object)
        {
            for (const RoadObjectCorner& corner : this->outline)
            {
                Vec3D pt_top;
                if (corner.type == RoadObjectCorner::Type::Local_AbsZ || corner.type == RoadObjectCorner::Type::Local_RelZ)
                {
                    pt_top = {corner.pt[0], corner.pt[1], corner.pt[2]};
                    if (corner.type == RoadObjectCorner::Type::Local_AbsZ)
                        pt_top[2] -= p0[2]; // make road relative
                    pt_top = add(pt_top, Vec3D{0, 0, corner.height});
                    pt_top = add(MatVecMultiplication(base_mat, MatVecMultiplication(rot_mat, pt_top)), p0);
                }
                else
                {
                    pt_top = add(MatVecMultiplication(rot_mat, {corner.pt[0], corner.pt[1], corner.pt[2] + corner.height}), outline_base);
                }

                outline_road_obj_mesh.vertices.push_back(pt_top);
                outline_road_obj_mesh.st_coordinates.push_back({this->s0, this->t0});
            }
        }

        /* add bottom outline */
        for (const RoadObjectCorner& corner : this->outline)
        {
            Vec3D pt_base;
            if (corner.type == RoadObjectCorner::Type::Local_AbsZ || corner.type == RoadObjectCorner::Type::Local_RelZ)
            {
                pt_base = {corner.pt[0], corner.pt[1], corner.pt[2]};
                if (corner.type == RoadObjectCorner::Type::Local_AbsZ)
                    pt_base[2] -= p0[2]; // make road relative
                pt_base = add(MatVecMultiplication(base_mat, MatVecMultiplication(rot_mat, pt_base)), p0);
            }
            else
            {
                pt_base = add(MatVecMultiplication(rot_mat, {corner.pt[0], corner.pt[1], corner.pt[2]}), outline_base);
            }

            outline_road_obj_mesh.vertices.push_back(pt_base);
            outline_road_obj_mesh.st_coordinates.push_back({this->s0, this->t0});
        }

        /* run 2D triangulation on top vertices */
        const std::vector<size_t> idx_patch_top = mapbox::earcut<size_t>(outline_road_obj_mesh.vertices.data(), this->outline.size());
        outline_road_obj_mesh.indices.insert(outline_road_obj_mesh.indices.end(), idx_patch_top.begin(), idx_patch_top.end());

        /* add walls */
        if (!is_flat_object)
        {
            const std::size_t N = this->outline.size();
            for (size_t idx = 0; idx < N - 1; idx++)
            {
                std::array<size_t, 6> wall_idx_patch = {idx, idx + N, idx + 1, idx + 1, idx + N, idx + N + 1};
                outline_road_obj_mesh.indices.insert(outline_road_obj_mesh.indices.end(), wall_idx_patch.begin(), wall_idx_patch.end());
            }

            std::array<size_t, 6> last_idx_patch = {N - 1, 2 * N - 1, 0, 0, 2 * N - 1, N};
            outline_road_obj_mesh.indices.insert(outline_road_obj_mesh.indices.end(), last_idx_patch.begin(), last_idx_patch.end());
        }

        road_obj_mesh.add_mesh(outline_road_obj_mesh);
    }

    return road_obj_mesh;
}

RoadObject::RoadObject(void** buf)
{
    assert(nullptr != buf);
	void* dsbuf = *buf;
	assert(nullptr != dsbuf);
    uint64_t* id_len = (uint64_t*)dsbuf;
    dsbuf = (void*)(id_len + 1);
    id = std::string((char*)dsbuf, *id_len);
    dsbuf = (void*)((char*)dsbuf + *id_len);
    uint64_t* type_len = (uint64_t*)dsbuf;
    dsbuf = (void*)(type_len + 1);
    type = std::string((char*)dsbuf, *type_len);
    dsbuf = (void*)((char*)dsbuf + *type_len);
    uint64_t* name_len = (uint64_t*)dsbuf;
    dsbuf = (void*)(name_len + 1);
    name = std::string((char*)dsbuf, *name_len);
    dsbuf = (void*)((char*)dsbuf + *name_len);
    uint64_t* orientation_len = (uint64_t*)dsbuf;
    dsbuf = (void*)(orientation_len + 1);
    orientation = std::string((char*)dsbuf, *orientation_len);
    dsbuf = (void*)((char*)dsbuf + *orientation_len);

    double* s = (double*)dsbuf;
    s0 = *s;
    dsbuf = (void*)(s + 1);
    double* t = (double*)dsbuf;
    t0 = *t;
    dsbuf = (void*)(t + 1);
    double* z = (double*)dsbuf;
    z0 = *z;
    dsbuf = (void*)(z + 1);

    double* len = (double*)dsbuf;
    length = *len;
    dsbuf = (void*)(len + 1);
    double* valid = (double*)dsbuf;
    valid_length = *valid;
    dsbuf = (void*)(valid + 1);
    double* w = (double*)dsbuf;
    width = *w;
    dsbuf = (void*)(w + 1);
    double* r = (double*)dsbuf;
    radius = *r;
    dsbuf = (void*)(r + 1);
    double* h = (double*)dsbuf;
    height = *h;
    dsbuf = (void*)(h + 1);
    double* hg = (double*)dsbuf;
    hdg = *hg;
    dsbuf = (void*)(hg + 1);
    double* pi = (double*)dsbuf;
    pitch = *pi;
    dsbuf = (void*)(pi + 1);
    double* ro = (double*)dsbuf;
    roll = *ro;
    dsbuf = (void*)(ro + 1);

    uint64_t* os = (uint64_t*)dsbuf;
    dsbuf = (void*)(os + 1);
    for (uint64_t i =0; i < *os; i ++) {
        outline.push_back(RoadObjectCorner(&dsbuf));
    }
    if (*os > 0) {
        double* bx = (double*)dsbuf;
        outline_base[0] = *bx;
        dsbuf = (void*)(bx + 1);
        double* by = (double*)dsbuf;
        outline_base[1] = *by;
        dsbuf = (void*)(by + 1);
        double* bz = (double*)dsbuf;
        outline_base[2] = *bz;
        dsbuf = (void*)(bz + 1);
    }

    *buf = dsbuf;
}

uint64_t RoadObject::getsize()
{
    uint64_t sz = sizeof(uint64_t);
    sz += id.length();
    sz += sizeof(uint64_t);
    sz += type.length();
    sz += sizeof(uint64_t);
    sz += name.length();
    sz += sizeof(uint64_t);
    sz += orientation.length();

    sz += sizeof(double);
    sz += sizeof(double);
    sz += sizeof(double);

    sz += sizeof(double);
    sz += sizeof(double);
    sz += sizeof(double);
    sz += sizeof(double);
    sz += sizeof(double);
    sz += sizeof(double);
    sz += sizeof(double);
    sz += sizeof(double);

    sz += sizeof(uint64_t);
    for (uint64_t i =0; i < outline.size(); i ++) {
        sz += outline[i].getsize();
    }

    if (outline.size() > 0) {
        //outline_base
        sz += (sizeof(double) * 3);
    }

    return sz;
}

void* RoadObject::serialize(void* buf)
{
    uint64_t* id_len = (uint64_t*)buf;
    *id_len = id.length();
    buf = (void*)(id_len + 1);
    memcpy(buf, id.c_str(), id.length());
    buf = (void*)((char*)buf + id.length());
    uint64_t* type_len = (uint64_t*)buf;
    *type_len = type.length();
    buf = (void*)(type_len + 1);
    memcpy(buf, type.c_str(), type.length());
    buf = (void*)((char*)buf + type.length());
    uint64_t* name_len = (uint64_t*)buf;
    *name_len = name.length();
    buf = (void*)(name_len + 1);
    memcpy(buf, name.c_str(), name.length());
    buf = (void*)((char*)buf + name.length());
    uint64_t* orientation_len = (uint64_t*)buf;
    *orientation_len = orientation.length();
    buf = (void*)(orientation_len + 1);
    memcpy(buf, orientation.c_str(), orientation.length());
    buf = (void*)((char*)buf + orientation.length());

    double* s = (double*)buf;
    *s = s0;
    buf = (void*)(s + 1);
    double* t = (double*)buf;
    *t = t0;
    buf = (void*)(t + 1);
    double* z = (double*)buf;
    *z = z0;
    buf = (void*)(z + 1);

    double* len = (double*)buf;
    *len = length;
    buf = (void*)(len + 1);
    double* valid = (double*)buf;
    *valid = valid_length;
    buf = (void*)(valid + 1);
    double* w = (double*)buf;
    *w = width;
    buf = (void*)(w + 1);
    double* r = (double*)buf;
    *r = radius;
    buf = (void*)(r + 1);
    double* h = (double*)buf;
    *h = height;
    buf = (void*)(h + 1);
    double* hg = (double*)buf;
    *hg = hdg;
    buf = (void*)(hg + 1);
    double* pi = (double*)buf;
    *pi = pitch;
    buf = (void*)(pi + 1);
    double* ro = (double*)buf;
    *ro = roll;
    buf = (void*)(ro + 1);

    uint64_t* os = (uint64_t*)buf;
    *os = outline.size();
    buf = (void*)(os + 1);
    for (uint64_t i =0; i < outline.size(); i ++) {
        buf = outline[i].serialize(buf);
    }

    if (*os > 0) {
        double* bx = (double*)buf;
        *bx = outline_base[0];
        buf = (void*)(bx + 1);
        double* by = (double*)buf;
        *by = outline_base[1];
        buf = (void*)(by + 1);
        double* bz = (double*)buf;
        *bz = outline_base[2];
        buf = (void*)(bz + 1);
    }
    return buf;
}

RoadObjectCorner::RoadObjectCorner(void** buf)
{
    assert(nullptr != buf);
	void* dsbuf = *buf;
	assert(nullptr != dsbuf);
	double* v1 = (double*)dsbuf;
    double v_1 = *v1;
    dsbuf = (void*)(v1 + 1);
    double* v2 = (double*)dsbuf;
    double v_2 = *v2;
    dsbuf = (void*)(v2 + 1);
    double* v3 = (double*)dsbuf;
    double v_3 = *v3;
    dsbuf = (void*)(v3 + 1);
    pt = {v_1, v_2, v_3};
    double* hei = (double*)dsbuf;
    height = *hei;
    dsbuf = (void*)(hei + 1);
    uint32_t* t = (uint32_t*)dsbuf;
    if(*t == 0) {
        type = Type::Local_RelZ;
    }
    else if(*t == 1) {
        type = Type::Local_AbsZ;
    }
    else if(*t == 2) {
        type = Type::Road;
    }
    dsbuf = (t + 1);
    *buf = dsbuf;
}

uint64_t RoadObjectCorner::getsize()
{
    uint64_t sz = sizeof(double);
    sz += sizeof(double);
    sz += sizeof(double);
    sz += sizeof(double);
    sz += sizeof(uint32_t);
    return sz;
}

void* RoadObjectCorner::serialize(void* buf)
{
    double* v1 = (double*)buf;
    *v1 = pt[0];
    buf = (void*)(v1 + 1);
    double* v2 = (double*)buf;
    *v2 = pt[1];
    buf = (void*)(v2 + 1);
    double* v3 = (double*)buf;
    *v3 = pt[2];
    buf = (void*)(v3 + 1);
    double* hei = (double*)buf;
    *hei = height;
    buf = (void*)(hei + 1);
    uint32_t* t = (uint32_t*)buf;
    if(type == Type::Local_RelZ) {
        *t = 0;
    }
    else if(type == Type::Local_AbsZ) {
        *t = 1;
    }
    else if(type == Type::Road) {
        *t = 2;
    }
    buf = (void*)(t + 1);
    return buf;
}


} // namespace odr