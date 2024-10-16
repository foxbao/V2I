#include "RoadMark.h"

namespace odr
{
    uint64_t RoadMarksLine::getsize()
    {
        uint64_t sz = sizeof(double);
        sz += sizeof(double);
        sz += sizeof(double);
        sz += sizeof(double);
        sz += sizeof(double);
        sz += sizeof(uint64_t);
        sz += name.length(); 
        sz += sizeof(uint64_t);
        sz += rule.length();
        return sz;
    }

    void* RoadMarksLine::serialize(void* buf)
    {
        double* w = (double*)buf;
        *w = width;
        buf = (void*)(w + 1);
        double* l = (double*)buf;
        *l = length;
        buf = (void*)(l + 1);
        double* s = (double*)buf;
        *s = space;
        buf = (void*)(s + 1);
        double* to = (double*)buf;
        *to = t_offset;
        buf = (void*)(to + 1);
        double* so = (double*)buf;
        *so = s_offset;
        buf = (void*)(so + 1);
        uint64_t* name_len = (uint64_t*)buf;
        *name_len = name.length();
        buf = (void*)(name_len + 1);
        memcpy(buf, name.c_str(), name.length());
        buf = (void*)((char*)buf + name.length());
        uint64_t* rule_len = (uint64_t*)buf;
        *rule_len = rule.length();
        buf = (void*)(rule_len + 1);
        memcpy(buf, rule.c_str(), rule.length());
        buf = (void*)((char*)buf + rule.length());
        return buf;
    }

    void* RoadMarksLine::deserialize(void* buf)
    {
        assert(nullptr != buf);
        double* w = (double*)buf;
        width = *w;
        buf = (void*)(w + 1);
        double* l = (double*)buf;
        length = *l;
        buf = (void*)(l + 1);
        double* s = (double*)buf;
        space = *s;
        buf = (void*)(s + 1);
        double* to = (double*)buf;
        t_offset = *to;
        buf = (void*)(to + 1);
        double* so = (double*)buf;
        s_offset = *so;
        buf = (void*)(so + 1);
        uint64_t* name_sz = (uint64_t*)buf;
        buf = (void*)(name_sz + 1);
        name = std::string((char*)buf, *name_sz);
        buf = (void*)((char*)buf + *name_sz);
        uint64_t* rule_sz = (uint64_t*)buf;
        buf = (void*)(rule_sz + 1);
        rule = std::string((char*)buf, *rule_sz);
        buf = (void*)((char*)buf + *rule_sz);
        return buf;
    }

    uint64_t RoadMarkGroup::getsize()
    {
        uint64_t sz = sizeof(double);
        sz += sizeof(double);
        sz += sizeof(double);
        sz += sizeof(uint64_t);
        sz += type.length();
        sz += sizeof(uint64_t);
        sz += weight.length();
        sz += sizeof(uint64_t);
        sz += color.length();
        sz += sizeof(uint64_t);
        sz += material.length();
        sz += sizeof(uint64_t);
        sz += laneChange.length();
        sz += sizeof(uint64_t);
        std::map<double, RoadMarksLine>::iterator line_it;
        for(line_it = s_to_roadmarks_line.begin(); line_it != s_to_roadmarks_line.end(); ++line_it) {
            sz += sizeof(double);   // map double
            sz += line_it->second.getsize(); //geometry data
        }
        return sz;
    }

    void* RoadMarkGroup::serialize(void* buf)
    {
        double* w = (double*)buf;
        *w = width;
        buf = (void*)(w + 1);
        double* h = (double*)buf;
        *h = height;
        buf = (void*)(h + 1);
        double* so = (double*)buf;
        *so = s_offset;
        buf = (void*)(so + 1);
        uint64_t* type_len = (uint64_t*)buf;
        *type_len = type.length();
        buf = (void*)(type_len + 1);
        memcpy(buf, type.c_str(), type.length());
        buf = (void*)((char*)buf + type.length());
        uint64_t* weight_len = (uint64_t*)buf;
        *weight_len = weight.length();
        buf = (void*)(weight_len + 1);
        memcpy(buf, weight.c_str(), weight.length());
        buf = (void*)((char*)buf + weight.length());
        uint64_t* color_len = (uint64_t*)buf;
        *color_len = color.length();
        buf = (void*)(color_len + 1);
        memcpy(buf, color.c_str(), color.length());
        buf = (void*)((char*)buf + color.length());
        uint64_t* material_len = (uint64_t*)buf;
        *material_len = material.length();
        buf = (void*)(material_len + 1);
        memcpy(buf, material.c_str(), material.length());
        buf = (void*)((char*)buf + material.length());
        uint64_t* laneChange_len = (uint64_t*)buf;
        *laneChange_len = laneChange.length();
        buf = (void*)(laneChange_len + 1);
        memcpy(buf, laneChange.c_str(), laneChange.length());
        buf = (void*)((char*)buf + laneChange.length());
        uint64_t* line_sz = (uint64_t*)buf;
        *line_sz = s_to_roadmarks_line.size();
        buf = (void*)(line_sz + 1);
        std::map<double, RoadMarksLine>::iterator line_it;
        for(line_it = s_to_roadmarks_line.begin(); line_it != s_to_roadmarks_line.end(); ++line_it) {
            double* s0 = (double*)buf;
            *s0 = line_it->first;
            buf = (void*)(s0 + 1);
            buf = line_it->second.serialize(buf);
        }
        return buf;
    }

    void* RoadMarkGroup::deserialize(void* buf)
    {
        assert(nullptr != buf);
        double* w = (double*)buf;
        width = *w;
        buf = (void*)(w + 1);
        double* h = (double*)buf;
        height = *h;
        buf = (void*)(h + 1);
        double* so = (double*)buf;
        s_offset = *so;
        buf = (void*)(so + 1);
        uint64_t* type_sz = (uint64_t*)buf;
        buf = (void*)(type_sz + 1);
        type = std::string((char*)buf, *type_sz);
        buf = (void*)((char*)buf + *type_sz);
        uint64_t* weight_sz = (uint64_t*)buf;
        buf = (void*)(weight_sz + 1);
        weight = std::string((char*)buf, *weight_sz);
        buf = (void*)((char*)buf + *weight_sz);
        uint64_t* color_sz = (uint64_t*)buf;
        buf = (void*)(color_sz + 1);
        color = std::string((char*)buf, *color_sz);
        buf = (void*)((char*)buf + *color_sz);
        uint64_t* material_sz = (uint64_t*)buf;
        buf = (void*)(material_sz + 1);
        material = std::string((char*)buf, *material_sz);
        buf = (void*)((char*)buf + *material_sz);
        uint64_t* laneChange_sz = (uint64_t*)buf;
        buf = (void*)(laneChange_sz + 1);
        laneChange = std::string((char*)buf, *laneChange_sz);
        buf = (void*)((char*)buf + *laneChange_sz);
        uint64_t* line_sz = (uint64_t*)buf;
        buf = (void*)(line_sz + 1);
        for(uint64_t i = 0; i < *line_sz; ++i) {
            double* s0 = (double*)buf;
            buf = (void*)(s0 + 1);
            RoadMarksLine roadMarkLine;
            buf = roadMarkLine.deserialize(buf);
            s_to_roadmarks_line.insert(std::map<double, RoadMarksLine>::value_type(*s0, roadMarkLine));
        }
        return buf;
    }

} // namespace odr