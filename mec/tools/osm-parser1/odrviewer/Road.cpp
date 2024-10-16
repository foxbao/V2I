#include "Road.h"
#include "RefLine.h"

#include <array>
#include <cmath>
#include <iterator>
#include <math.h>
#include <utility>

namespace odr
{
double Crossfall::get_crossfall(double s, bool on_left_side) const
{
    const Poly3 poly = this->get_poly(s);

    if (this->s0_to_poly.size() > 0)
    {
        auto target_poly_iter = this->s0_to_poly.upper_bound(s);
        if (target_poly_iter != this->s0_to_poly.begin())
            target_poly_iter--;

        Side side = Side::Both; // applicable side of the road
        if (this->sides.find(target_poly_iter->first) != this->sides.end())
            side = this->sides.at(target_poly_iter->first);

        if (on_left_side && side == Side::Right)
            return 0;
        else if (!on_left_side && side == Side::Left)
            return 0;

        return target_poly_iter->second.get(s);
    }

    return 0;
}

ConstLaneSectionSet Road::get_lanesections() const
{
    ConstLaneSectionSet lanesections;
    for (const auto& s_lansection : this->s_to_lanesection)
        lanesections.insert(s_lansection.second);

    return lanesections;
}

LaneSectionSet Road::get_lanesections()
{
    LaneSectionSet lanesections;
    for (const auto& s_lansection : this->s_to_lanesection)
        lanesections.insert(s_lansection.second);

    return lanesections;
}

ConstRoadObjectSet Road::get_road_objects() const
{
    ConstRoadObjectSet road_objects;
    for (const auto& id_obj : this->id_to_object)
        road_objects.insert(id_obj.second);

    return road_objects;
}

RoadObjectSet Road::get_road_objects()
{
    RoadObjectSet road_objects;
    for (const auto& id_obj : this->id_to_object)
        road_objects.insert(id_obj.second);

    return road_objects;
}

std::shared_ptr<const LaneSection> Road::get_lanesection(double s) const
{
    if (this->s_to_lanesection.size() > 0)
    {
        auto target_lane_sec_iter = this->s_to_lanesection.upper_bound(s);
        if (target_lane_sec_iter != this->s_to_lanesection.begin())
            target_lane_sec_iter--;
        return target_lane_sec_iter->second;
    }

    return nullptr;
}

std::shared_ptr<LaneSection> Road::get_lanesection(double s)
{
    std::shared_ptr<LaneSection> lanesection = std::const_pointer_cast<LaneSection>(static_cast<const Road&>(*this).get_lanesection(s));
    return lanesection;
}

Vec3D Road::get_xyz(double s, double t, double h, Vec3D* _e_s, Vec3D* _e_t, Vec3D* _e_h) const
{
    const Vec3D  s_vec = this->ref_line->get_grad(s);
    const double theta = this->superelevation.get(s);

    const Vec3D e_s = normalize(s_vec);
    const Vec3D e_t = normalize(Vec3D{std::cos(theta) * -e_s[1] + std::sin(theta) * -e_s[2] * e_s[0],
                                      std::cos(theta) * e_s[0] + std::sin(theta) * -e_s[2] * e_s[1],
                                      std::sin(theta) * (e_s[0] * e_s[0] + e_s[1] * e_s[1])});
    const Vec3D e_h = normalize(crossProduct(s_vec, e_t));
    const Vec3D p0 = this->ref_line->get_xyz(s);
    const Mat3D trans_mat{{{e_t[0], e_h[0], p0[0]}, {e_t[1], e_h[1], p0[1]}, {e_t[2], e_h[2], p0[2]}}};

    const Vec3D xyz = MatVecMultiplication(trans_mat, Vec3D{t, h, 1});

    if (_e_s)
        *_e_s = e_s;
    if (_e_t)
        *_e_t = e_t;
    if (_e_h)
        *_e_h = e_h;

    return xyz;
}
uint64_t Road::getsize(void) 
{  
    uint64_t sz = sizeof(double);
    sz += sizeof(uint64_t);
    sz += id.length();
    sz += sizeof(uint64_t);
    sz += junction.length();
    sz += sizeof(uint64_t);
    sz += name.length();

    sz += predecessor.getsize();
    sz += successor.getsize();

    sz += lane_offset.getsize();
    sz += superelevation.getsize();
    sz += ref_line->getsize();

    sz += sizeof(uint64_t);
    std::map<double, std::shared_ptr<LaneSection>>::iterator sec_it;
	for(sec_it = s_to_lanesection.begin(); sec_it != s_to_lanesection.end(); ++sec_it) {
		sz += sizeof(double);   // map double
		sz += sec_it->second->getsize(); //geometry data
	}

    sz += sizeof(uint64_t);
    std::map<double, std::string>::iterator str_it;
	for(str_it = s_to_type.begin(); str_it != s_to_type.end(); ++str_it) {
		sz += sizeof(double);   
        sz += sizeof(uint64_t);
		sz += str_it->second.length(); 
	}

    sz += sizeof(uint64_t);
    std::map<double, SpeedRecord>::iterator spe_it;
	for(spe_it = s_to_speed.begin(); spe_it != s_to_speed.end(); ++spe_it) {
		sz += sizeof(double); 
		sz += spe_it->second.getsize();
	}

    sz += sizeof(uint64_t);
    std::map<std::string, std::shared_ptr<RoadObject>>::iterator obj_it;
	for(obj_it = id_to_object.begin(); obj_it != id_to_object.end(); ++obj_it) {
		sz += sizeof(uint64_t); 
        sz += obj_it->first.length();
		sz += obj_it->second->getsize();
	}

    return sz;
}

void* Road::serialize(void* buf) 
{
    double* len = (double*)buf;
    *len = length;
    buf = (void*)(len + 1);
    uint64_t* id_len = (uint64_t*)buf;
    *id_len = id.length();
    buf = (void*)(id_len + 1);
    memcpy(buf, id.c_str(), id.length());
    buf = (void*)((char*)buf + id.length());
    uint64_t* junction_len = (uint64_t*)buf;
    *junction_len = junction.length();
    buf = (void*)(junction_len + 1);
    memcpy(buf, junction.c_str(), junction.length());
    buf = (void*)((char*)buf + junction.length());
    uint64_t* name_len = (uint64_t*)buf;
    *name_len = name.length();
    buf = (void*)(name_len + 1);
    memcpy(buf, name.c_str(), name.length());
    buf = (void*)((char*)buf + name.length());

    buf = predecessor.serialize(buf);
    buf = successor.serialize(buf);

    buf = lane_offset.serialize(buf);
    buf = superelevation.serialize(buf);
    buf = ref_line->serialize(buf);

    uint64_t* sec_sz = (uint64_t*)buf;
    *sec_sz = s_to_lanesection.size();
    buf = (void*)(sec_sz + 1);
    std::map<double, std::shared_ptr<LaneSection>>::iterator sec_it;
	for(sec_it = s_to_lanesection.begin(); sec_it != s_to_lanesection.end(); ++sec_it) {
        double* s0 = (double*)buf;
        *s0 = sec_it->first;
        buf = (void*)(s0 + 1);
        buf = sec_it->second->serialize(buf);
    }

    uint64_t* str_sz = (uint64_t*)buf;
    *str_sz = s_to_type.size();
    buf = (void*)(str_sz + 1);
    std::map<double, std::string>::iterator str_it;
	for(str_it = s_to_type.begin(); str_it != s_to_type.end(); ++str_it) {
        double* s0 = (double*)buf;
        *s0 = str_it->first;
        uint64_t* strlen = (uint64_t*)(s0 + 1);
        *strlen = str_it->second.length();
        buf = (void*)(strlen + 1);
        memcpy(buf, str_it->second.c_str(), *strlen);
        buf = (void*)((char*)buf + *strlen);
    }

    uint64_t* spe_sz = (uint64_t*)buf;
    *spe_sz = s_to_speed.size();
    buf = (void*)(spe_sz + 1);
    std::map<double, SpeedRecord>::iterator spe_it;
	for(spe_it = s_to_speed.begin(); spe_it != s_to_speed.end(); ++spe_it) {
        double* s0 = (double*)buf;
        *s0 = spe_it->first;
        buf = (void*)(s0 + 1);
        buf = spe_it->second.serialize(buf);
    }

    uint64_t* obj_sz = (uint64_t*)buf;
    *obj_sz = id_to_object.size();
    buf = (void*)(obj_sz + 1);
    std::map<std::string, std::shared_ptr<RoadObject>>::iterator obj_it;
	for(obj_it = id_to_object.begin(); obj_it != id_to_object.end(); ++obj_it) {
        uint64_t* s0 = (uint64_t*)buf;
        *s0 = obj_it->first.length();
        buf = (void*)(s0 + 1);
        memcpy(buf, obj_it->first.c_str(), *s0);
        buf = (void*)((char*)buf + *s0);
        buf = obj_it->second->serialize(buf);
    }

    return buf;
}

Road::Road(void** buff) 
{
    assert(nullptr != buff);
	void* buf = *buff;
	assert(nullptr != buf);
    double* len = (double*)buf;
    length = *len;

    uint64_t* id_sz = (uint64_t*)(len + 1);
    buf = (void*)(id_sz + 1);
    id = std::string((char*)buf, *id_sz);
    buf = (void*)((char*)buf + *id_sz);
    uint64_t* jun_sz = (uint64_t*)buf;
    buf = (void*)(jun_sz + 1);
    junction = std::string((char*)buf, *jun_sz);
    buf = (void*)((char*)buf + *jun_sz);
    uint64_t* n_sz = (uint64_t*)buf;
    buf = (void*)(n_sz + 1);
    name = std::string((char*)buf, *n_sz);
    buf = (void*)((char*)buf + *n_sz);
    
    buf = predecessor.deserialize(buf);
    buf = successor.deserialize(buf);

    buf = lane_offset.deserialize(buf);
    buf = superelevation.deserialize(buf);
    ref_line = std::make_shared<RefLine>(&buf);

    uint64_t* sec_sz = (uint64_t*)buf;
    buf = (void*)(sec_sz + 1);
    for(uint64_t i = 0; i < *sec_sz; ++i) {
        double* s0 = (double*)buf;
        buf = (void*)(s0 + 1);
        std::shared_ptr<LaneSection> sec = std::make_shared<LaneSection>(&buf);
        s_to_lanesection.insert(std::map<double, std::shared_ptr<LaneSection>>::value_type(*s0, sec));
    }

    uint64_t* t_sz = (uint64_t*)buf;
    buf = (void*)(t_sz + 1);
    for(uint64_t i = 0; i < *t_sz; ++i) {
        double* s0 = (double*)buf;
        uint64_t* st_sz = (uint64_t*)(s0 + 1);
        buf = (void*)(st_sz + 1);
        std::string s_tr((char*)buf, *st_sz);
        buf = (void*)((char*)buf + *st_sz);
        s_to_type.insert(std::map<double, std::string>::value_type(*s0, s_tr));
    }

    uint64_t* spe_sz = (uint64_t*)buf;
    buf = (void*)(spe_sz + 1);
    for(uint64_t i = 0; i < *spe_sz; ++i) {
        double* s0 = (double*)buf;
        buf = (void*)(s0 + 1);
        SpeedRecord speed(&buf);
        s_to_speed.insert(std::map<double, SpeedRecord>::value_type(*s0, speed));
    }

    uint64_t* obj_sz = (uint64_t*)buf;
    buf = (void*)(obj_sz + 1);
    for(uint64_t i = 0; i < *obj_sz; ++i) {
        uint64_t* st_sz = (uint64_t*)buf;
        buf = (void*)(st_sz + 1);
        std::string s_tr((char*)buf, *st_sz);
        buf = (void*)((char*)buf + *st_sz);
        std::shared_ptr<RoadObject> obj = std::make_shared<RoadObject>(&buf);
        id_to_object.insert(std::map<std::string, std::shared_ptr<RoadObject>>::value_type(s_tr, obj));
    }

    *buff = buf;
}

uint64_t RoadLink::getsize() 
{
    uint64_t sz = sizeof(uint32_t);
    sz += id.length();
    sz += sizeof(Type);
    sz += sizeof(ContactPoint);
    return sz;
}

void* RoadLink::serialize(void* buf)
{
    uint32_t* len = (uint32_t*)buf;
    *len = id.length();
    buf = (void*)(len + 1);
    memcpy(buf, id.c_str(), id.length());
    buf = (void*)((char*)buf + id.length());
    int* t = (int*)buf;
    *t = (int)type;
    buf = (void*)(t + 1);
    int* con = (int*)buf;
    *con = (int)contact_point;
    buf = (void*)(con + 1);
    return buf;
}

void* RoadLink::deserialize(void* buf)
{
    assert(nullptr != buf);
    uint32_t* len = (uint32_t*)buf;
    uint32_t length = *len;
    buf = (void*)(len + 1);
    id = std::string((char*)buf, length);
    // memcpy(i, buf, length);
    buf = (void*)((char*)buf + length);
    // 枚举转换
    int* t = (int*)buf;
    type = Type(*t);
    buf = (void*)(t + 1);
    int* con = (int*)buf;
    contact_point = ContactPoint(*con);
    buf = (void*)(con + 1);
    return buf;
}

SpeedRecord::SpeedRecord(void** buf)
{
    assert(nullptr != buf);
	void* dsbuf = *buf;
	assert(nullptr != dsbuf);
	uint32_t* mlen = (uint32_t*)dsbuf;
	uint32_t mlength = *mlen;
    dsbuf = (void*)(mlen + 1);
    max = std::string((char*)dsbuf, mlength);
    dsbuf = (void*)((char*)dsbuf + mlength);
    uint32_t* ulen = (uint32_t*)dsbuf;
	uint32_t ulength = *ulen;
    dsbuf = (void*)(ulen + 1);
    unit = std::string((char*)dsbuf, ulength);
    dsbuf = (void*)((char*)dsbuf + ulength);
    *buf = dsbuf;
}

uint64_t SpeedRecord::getsize() 
{
    uint64_t sz = max.length();
    sz += unit.length();
    return sz;
}

void* SpeedRecord::serialize(void* buf)
{
    uint32_t* mlen = (uint32_t*)buf;
    *mlen = max.length();
    buf = (void*)(mlen + 1);
    memcpy(buf, max.c_str(), max.length());
    buf = (void*)((char*)buf + max.length());
    uint32_t* ulen = (uint32_t*)buf;
    *ulen = unit.length();
    buf = (void*)(ulen + 1);
    memcpy(buf, unit.c_str(), unit.length());
    buf = (void*)((char*)buf + unit.length());
    return buf;
}

} // namespace odr