#ifndef __CXX_OSM_PARSER1_ODR_LOADER_H__
#define __CXX_OSM_PARSER1_ODR_LOADER_H__

#include <stdint.h>
#include <stack>

#include "std/list.h"
#include "utils/avltree.h"
#include "odr-map.h"
#include "hdmap-format.h"

#include "xml.h"
#include "odr-extdata.h"

#include "Road.h"
#include "RefLine.h"
#include "Geometries/CubicSpline.h"
#include "Geometries/Line.h"
#include "Geometries/Arc.h"
#include "Geometries/Spiral.h"
#include "Geometries/ParamPoly3.h"
#include "Junction.h"
#include "Mesh.h"

// todo: re-arrange the following header
#include "./../hwgrph/inc/gbuf.h"

namespace osm_parser1 {

using namespace odr;
using namespace std;
using namespace zas::utils;
using namespace zas::mapcore::render_hdmap;
// to be removed
using namespace zas::hwgrph;
using namespace XERCES_CPP_NAMESPACE;

class odr_loader;
class odr_elevationProfile;
class odr_elevation;
class odr_lateralProfile;
class odr_superelevation;
class odr_lane;
class odr_lanes;
class odr_roadmark;
class odr_laneoffset;
class odr_lane_section;
class odr_line;
class odr_width;
class odr_height;
class odr_speed;
class odr_objects;
class odr_object;
class odr_signals;
class odr_signal;
class odr_outline;
class odr_cornerlocal;
class odr_junction;
class odr_junction_connection;
class odr_junction_lanelink;

inline static void ltoa(int64_t v, string& str) {
	char buf[64];
	sprintf(buf, "%ld", v);
	str = buf;
}

enum odr_element_type
{
	odr_elemtype_unknown = 0,
	odr_elemtype_anonymous,
	odr_elemtype_road,
	odr_elemtype_road_link,
	odr_elemtype_geometry,
	odr_elemtype_elevpf,
	odr_elemtype_elevation,
	odr_elemtype_laterlpf,
	odr_elemtype_superelev,
	odr_elemtype_lanes,
	odr_elemtype_lane_section,
	odr_elemtype_lane,
	odr_elemtype_roadmark,
	odr_elemtype_lane_link,
	odr_elemtype_line,
	odr_elemtype_laneoffset,
	odr_elemtype_width,
	odr_elemtype_height,
	odr_elemtype_speed,
	odr_elemtype_objects,
	odr_elemtype_object,
	odr_elemtype_signals,
	odr_elemtype_signal,
	odr_elemtype_posroad,
	odr_elemtype_outline,
	odr_elemtype_cornerlocal,
	odr_elemtype_junction,
	odr_elemtype_junction_connection,
	odr_elemtype_junction_lanelink,
};

class odr_element
{
	friend class odr_map;
	friend class odr_loader;
public:
	odr_element(odr_element* p, const char* name, int type = odr_elemtype_unknown);
	virtual ~odr_element();

	virtual void release(void);
	virtual void on_before_pushed(odr_loader*);
	virtual void on_after_poped(odr_loader*);

public:
	bool same_type(const char* t) {
		return (t && !strcmp(_element_name, t))
		? true : false;
	}

	int get_type(void) {
		return _attrs.type;
	}

	odr_element* get_parents(void) {
		return _parents;
	}

	void set_parents(odr_element* ele) {
		_parents = ele;
	}

	const char* name(void) {
		return _element_name;
	}

	void set_serialized_size(uint32_t sz) {
		_attrs.serialized_datasize = sz;
	}

	int get_serialized_size(void) {
		return _attrs.serialized_datasize;
	}

protected:
	odr_element* _parents;
	const char* _element_name;
	struct {
		uint32_t type : 8;
		uint32_t serialized_datasize : 24;
	} _attrs;
};

class odr_anonymous : public odr_element
{
public:
	odr_anonymous(odr_element* p, const char* en,
		void* data = nullptr);

	void* getdata(void) {
		return _data;
	}

	void* setdata(void* data) {
		void* tmp = _data;
		_data = data;
		return tmp;
	}

private:
	void* _data;
};

typedef odr_element* (*element_parser)(odr_loader*,
	const char*, odr_element*, const Attributes&);

struct element_handler
{
	const char* element_name;
	element_parser parse;
	static int compare(const void*, const void*);
};

enum rl_element_type
{
	rl_elemtype_unknown = 0,
	rl_elemtype_road,
	rl_elemtype_junction,
	// todo:
};

enum rl_connect_point
{
	rl_connpoint_unknown = 0,
	rl_connpoint_start,
	rl_connpoint_end,
	// todo:
};

struct roadlink
{
	roadlink() : attrs(0), elem_id(0) {}
	union {
		uint32_t attrs;
		struct {
			uint32_t elem_type : 4;
			uint32_t conn_point : 4;
		} a;
	};
	uint32_t elem_id;
};

enum rt_type
{
	rt_type_unknown = 0,
	rt_type_town,
};

struct roadtype
{
	double s;
	int type;
};

struct refl_geometry
{
	double s, x, y;
	double hdg, length;
	int type;
	std::shared_ptr<RoadGeometry> road_geometry = nullptr;
};

class odr_road : public odr_element
{
	friend class odr_map;
	friend class odr_loader;
	friend struct odr_roadptr;
	friend class odr_extdata;
	friend class hdmap_lane;
	friend class curve_segment;
	friend class hdmap_roadinfo;
	friend class hdmap_container;
	friend class refline_fragment;
public:
	odr_road(odr_element* p);
	~odr_road();
	void release(void);

	void on_after_poped(odr_loader*);

	void odrv_update_roadobject(odr_object*, shared_ptr<RoadObject>,
		RoadObjectCorner::Type = RoadObjectCorner::Type::Local_RelZ);
	void odrv_create_lane(odr_loader*, odr_lane*, odr_lane_section*);

public:
	int add_geometry(refl_geometry* rlgeo) {
		assert(nullptr != rlgeo);
		auto* g = _geometries.new_object();
		if (nullptr == g) return -1;
		*g = rlgeo;
		return 0;
	}

	int set_elevationprofile(odr_elevationProfile* elevprf) {
		assert(nullptr != elevprf);
		_elevation_profile = elevprf;
		return 0;
	}

	int set_lateralProfile(odr_lateralProfile* lateralProfile) {
		assert(nullptr != lateralProfile);
		_lateral_profile = lateralProfile;
		return 0;
	}

	int set_lanes(odr_lanes* lanes) {
		assert(nullptr != lanes);
		_lanes = lanes;
		return 0;
	}

	int set_objects(odr_objects* objects) {
		assert(nullptr != objects);
		_objects = objects;
		return 0;
	}

	odr_objects* get_objects(void) {
		return _objects;
	} 

private:
	void release_all(void);

	static int avl_id_compare(avl_node_t*, avl_node_t*);
	static int qst_geometry_compare(const void*, const void*);

private:
	listnode_t				_ownerlist;
	avl_node_t				_avlnode;

	listnode_t				_hdmr_ownerlist;
	hdmap_container*		_container;

	xmlstring				_name;
	double					_length;
	uint32_t				_id;
	int						_junction_id;

	roadlink				_prev;
	roadlink				_next;

	odr_elevationProfile* _elevation_profile;
	odr_lateralProfile* _lateral_profile;

	gbuf_<roadtype>			_road_types;
	gbuf_<refl_geometry*>	_geometries;

	odr_lanes*				_lanes;
	odr_objects*            _objects;
	std::shared_ptr<Road>	_odrv_road;

	hdrmap_file_roadseg_v1* _rendermap_road;
};

enum pvg_type
{
	pvg_type_unknown,
	pvg_type_line,
	pvg_type_spiral,
	pvg_type_arc,
	pvg_type_poly3, // Cubic Polynom is deprecated
	pvg_type_param_poly3,
};

struct geo_spiral
{
	double curv_start;
	double curv_end;
};

struct geo_arc {
	double curvature;
};

struct geo_poly3 {
	double a, b, c, d;
};

enum ppoly3_range
{
	ppoly3_range_unknown = 0,
	ppoly3_range_arc_length,
	ppoly3_range_normalized,
};

struct geo_param_poly3
{
	double aU, bU, cU, dU;
	double aV, bV, cV, dV;
	ppoly3_range range;
};

struct refl_geometry_spiral : public refl_geometry {
	geo_spiral spiral;
};

struct refl_geometry_arc : public refl_geometry {
	geo_arc arc;
};

struct refl_geometry_poly3 : public refl_geometry {
	geo_poly3 poly3;
};

struct refl_geometry_param_poly3 : public refl_geometry {
	geo_param_poly3 ppoly3;
};

class odr_geometry : public odr_element
{
	friend class odr_map;
	friend class odr_loader;
public:
	odr_geometry(odr_element* p);
	void on_after_poped(odr_loader*);

private:
	refl_geometry* create_geometry(void);

private:
	double _s, _x, _y;
	double _hdg;
	double _length;

	int _geotype;
	union {
		geo_spiral		_spiral;
		geo_arc			_arc;
		geo_poly3		_poly3;
		geo_param_poly3	_ppoly3;
	};
};

class odr_elevationProfile : public odr_element
{
	friend class odr_road;
	friend class odr_map;
public:
	odr_elevationProfile(odr_element* p);
	~odr_elevationProfile();

	void release(void);

	int add_elevation(odr_elevation* oelev) {
		assert(nullptr != oelev);
		auto* holder = _elevations.new_object();
		if (nullptr == holder) return -1;
		*holder = oelev;
		return 0;
	}

private:
	void release_all();
	gbuf_<odr_elevation*> _elevations;
};

class odr_elevation : public odr_element
{
	friend class odr_elevationProfile;
	friend class odr_road;
	friend class odr_map;
public:
	odr_elevation(odr_element*p, double s, double a, double b, double c,double d);
	~odr_elevation();

	void release(void);

private:
	double _s, _a, _b, _c, _d;

};

class odr_lateralProfile : public odr_element
{
	friend class odr_road;
	friend class odr_map;
public:
	odr_lateralProfile(odr_element* p);
	~odr_lateralProfile();

	void release(void);

	int add_super_elevation(odr_superelevation* ose) {
		assert(nullptr != ose);
		auto* holder = _super_elevations.new_object();
		if (nullptr == holder) return -1;
		*holder = ose;
		return 0;
	}

private:
	void release_all(void);

private:
	gbuf_<odr_superelevation*> _super_elevations;
};

class odr_superelevation : public odr_element
{
	friend class odr_lateralProfile;
	friend class odr_road;
	friend class odr_map;
public:
	odr_superelevation(odr_element* p, double s, double a, double b, double c,double d);
	~odr_superelevation();

	void release(void);

private:
	double _s, _a, _b, _c, _d;

};

class odr_lane_section : public odr_element
{
	friend class odr_lanes;
	friend class odr_road;
	friend class odr_map;
	friend class odr_loader;
	friend class hdmap_roadinfo;
public:
	odr_lane_section(odr_element* p, double s, bool ss);
	~odr_lane_section();
	void release(void);
	void on_after_poped(odr_loader*);

public:

	odr_lane* center_lane(odr_lane* lane) {
		auto* ret = _center_lane;
		if (nullptr == lane) {
			return ret;
		}
		if (_center_lane) return ret;
		_center_lane = lane;
		return ret;
	}

	int add_lane(odr_lane* lane, bool bright) {
		if (nullptr == lane) {
			return -1;
		}
		auto* l = (bright) ? _right_lanes.new_object()
			: _left_lanes.new_object();
		if (nullptr == l) {
			return -2;
		}
		*l = lane; return 0;
	}

	void update_bounding_box(double x, double y)
	{
		if (x < _x0) _x0 = x;
		if (x > _x1) _x1 = x;
		if (y < _y0) _y0 = y;
		if (y > _y1) _y1 = y;
	}

private:
	void release_all(void);
	void fix_left_edge(odr_loader*);
	void fix_right_edge(odr_loader*);

private:
	double _s;
	bool _single_side;
	int _index;

	// center lane
	odr_lane* _center_lane;
	gbuf_<odr_lane*> _left_lanes;
	gbuf_<odr_lane*> _right_lanes;

	// odrviewer object reference
	weak_ptr<LaneSection> _odrv_lanesect;

	double _x0, _y0, _x1, _y1;
	hdrmap_file_lanesec_v1* _rendermap_lanesec;
};

class odr_lanes : public odr_element
{
	friend class odr_road;
	friend class odr_map;
	friend class hdmap_roadinfo;
public:
	odr_lanes(odr_element* p);
	~odr_lanes();
	void release(void);

	int add_lane_section(odr_lane_section* ols) {
		assert(nullptr != ols);
		ols->_index = _lane_sects.getsize();
		auto* holder = _lane_sects.new_object();
		if (nullptr == holder) return -1;
		*holder = ols;
		return 0;
	}

	int add_laneoffset(odr_laneoffset* lo) {
		if (nullptr == lo) {
			return -1;
		}
		auto* holder = _lane_offsets.new_object();
		if (nullptr == holder) {
			return -2;
		}
		*holder = lo;
		return 0;
	}

private:
	void release_all(void);

private:
	gbuf_<odr_lane_section*> _lane_sects;
	gbuf_<odr_laneoffset*> _lane_offsets;
};

enum lane_type
{
	lane_type_unknown = 0,
	lane_type_shoulder,
	lane_type_border,
	lane_type_driving,
	lane_type_stop,
	lane_type_none,
	lane_type_restricted,
	lane_type_parking,
	lane_type_median,
	lane_type_biking,
	lane_type_sidewalk,
	lane_type_curb,
	lane_type_exit,
	lane_type_entry,
	lane_type_onramp,
	lane_type_offramp,
	lane_type_connecting_ramp,
};

class odr_lane : public odr_element
{
	friend class odr_lanes;
	friend class odr_road;
	friend class odr_map;
	friend class odr_loader;
	friend class hdmap_roadinfo;
	friend class hdmap_lane;
	friend class curve_segment;
	friend class hdmap_laneseg;
	friend class odr_lane_section;
public:
	odr_lane(odr_element* p, int id, int type, bool level);
	~odr_lane();
	void release(void);

public:
	int add_roadmark(odr_roadmark* rm) {
		if (nullptr == rm) {
			return -1;
		}
		auto* holder = _roadmarks.new_object();
		if (nullptr == holder) {
			return -2;
		}
		*holder = rm;
		return 0;
	}

	int add_width(odr_width* w) {
		if (nullptr == w) {
			return -1;
		}
		auto* holder = _widths.new_object();
		if (nullptr == holder) {
			return -2;
		}
		*holder = w;
		return 0;
	}

	int add_height(odr_height* h) {
		if (nullptr == h) {
			return -1;
		}
		auto* holder = _heights.new_object();
		if (nullptr == holder) {
			return -2;
		}
		*holder = h;
		return 0;
	}

	int set_speed(odr_speed* speed) {
		if (nullptr == speed) {
			return -1;
		}
		_speed = speed;
		return 0;
	}

private:
	void release_all(void);
	static int qst_id_asc_compare(const void*, const void*);
	static int qst_id_dsc_compare(const void*, const void*);

private:
	int _id;
	int _prev, _next;
	struct {
		uint32_t type : 8;
		uint32_t level : 1;
		uint32_t has_prev : 1;
		uint32_t has_next : 1;
		uint32_t rendermap_curb : 1;
	} _attrs;
	
	gbuf_<odr_width*> _widths;
	gbuf_<odr_height*> _heights;
	gbuf_<odr_roadmark*> _roadmarks;
	odr_speed* _speed;

	// odrviewer objects
	weak_ptr<Lane> _odrv_lane;
	Mesh3D _odrv_mesh;
};

class odr_laneoffset : public odr_element
{
	friend class odr_lanes;
	friend class odr_road;
	friend class odr_map;
public:
	odr_laneoffset(odr_element* p, double s, double a,
		double b, double c, double d);
	~odr_laneoffset();
	void release(void);

private:
	double _s, _a, _b, _c, _d;
};

enum rm_lanechg_type
{
	rm_lanechg_type_unknown = 0,
	rm_lanechg_type_none,
	rm_lanechg_type_both,
	rm_lanechg_type_increase,
	rm_lanechg_type_decrease,
};

class odr_roadmark : public odr_element
{
	friend class odr_road;
public:
	odr_roadmark(odr_element* p, double soff,
		double width, double height, int lanechg,
		string marktype, string weight, string color);
	~odr_roadmark();
	void release(void);

public:

	bool verify_update_width(double w) {
		bool ret = (w == _width) ? true : false;
		w = _width;
		return ret;
	}

	int add_line(odr_line* line) {
		if (nullptr == line) {
			return -1;
		}
		auto* l = _lines.new_object();
		if (nullptr == l) {
			return -2;
		}
		*l = line; return 0;
	}

private:
	void release_all(void);

private:
	double _s_offset;
	double _width, _height;
	int _lane_change;
	string _mark_type;
	string _weight;
	string _color;
	gbuf_<odr_line*> _lines;
};

class odr_line : public odr_element
{
	friend class odr_road;
public:
	odr_line(odr_element* p, const Attributes& attrs);
	void release(void);

private:
	double _length;
	double _space;
	double _s_offset, _t_offset;
	double _width;
	struct {
		uint32_t rule : 4;
	} _attrs;
};

class odr_width : public odr_element
{
	friend class odr_lane;
	friend class odr_lanes;
	friend class odr_road;
	friend class odr_loader;
	friend class odr_map;
public:
	odr_width(odr_element* p, double soff, double a,
		double b, double c, double d);
	void release(void);
	bool is_zero(void);

private:
	double _s_offset;
	double _a, _b, _c, _d;
};

class odr_height : public odr_element
{
	friend class odr_lane;
	friend class odr_lanes;
	friend class odr_road;
	friend class odr_loader;
	friend class odr_map;
public:
	odr_height(odr_element* p, double soff, double inner,
		double outer);
	void release(void);

private:
	double _s_offset;
	double _inner, _outer;
};

class odr_speed : public odr_element {
public:
	odr_speed(odr_element* p, double soff, double max, 
		const XMLCh* unit);
	~odr_speed();
	void release(void);
private:
	double _s_offset;
	double _max;
	xmlstring _unit;
};

class odr_objects : public odr_element
{
	friend class odr_road;
	friend class odr_map;
	friend class odr_loader;
	friend class odr_extdata;
public:
	odr_objects(odr_element* p);
	~odr_objects();

	void release(void);

	int add_object(odr_object* obj) {
		assert(nullptr != obj);
		auto* holder = _objects.new_object();
		if (nullptr == holder) return -1;
		*holder = obj;
		return 0;
	}

private:
	void release_all(void);

private:
	gbuf_<odr_object*> _objects;
};

enum object_type {
	object_type_unknown = 0,
	object_type_none,
	object_type_patch,
	object_type_pole,
	object_type_obstacle,
	object_type_pedestrian,
	object_type_userdef,
	object_type_parkingSpace,
};

class odr_object : public odr_element
{
	friend class odr_objects;
	friend class odr_extdata;
	friend class odr_road;
	friend class odr_map;
	friend class odr_loader;
public:
	odr_object(odr_element* p, uint64_t id, const char* name);
	odr_object(odr_element* p, uint64_t id, uint32_t type, 
		double s, double t, double zoff, double validlength, 
		const XMLCh* orientation, const XMLCh* subtype, const XMLCh* name, 
		const XMLCh* dynamic, double hdg, double pitch, double roll, 
		double height, double length, double width, double radius);
	~odr_object();

public:
	int set_outline(odr_outline* outline) {
		assert(nullptr != outline);
		_outline = outline;
		return 0;
	}

	void release(void);

	void update_bounding_box(double x, double y)
	{
		if (x < _x0) _x0 = x;
		if (x > _x1) _x1 = x;
		if (y < _y0) _y0 = y;
		if (y > _y1) _y1 = y;
	}

private:
	uint64_t _id;
	uint32_t _type;
	union {
		double _s;
		double _lon; // for a new created object, use this to save (ll)h
	};
	union {
		double _t;
		double _lat; // for a new created object, use this to save (ll)h
	};
	double _z_offset;
	double _valid_length;
	string _orientation;
	xmlstring _subtype;
	string _name;
	xmlstring _dynamic;
	double _hdg;
	double _pitch;
	double _roll;
	double _height;
	double _length;
	double _width;
	double _radius;
	point3d _outline_base;

	double _x0, _y0, _x1, _y1;

	odr_outline* _outline;

	// odrviewer object reference
	weak_ptr<RoadObject> _odrv_object;
	Mesh3D _odrv_mesh;

	union {
		ext_siglight_attr _sigl_attrs;
		exd_siglight_info* _siginfo;
		exd_roadside_device_info* _rsdinfo;
		exd_arrow_info* _arrowinfo;
		exd_junconver_info* _juncvrinfo;
		exd_sigboard_info* _sigboardinfo;
		odr_object* _pole_related_sigboard;
		hdrmap_file_road_object_v1* _rendermap_roadobj;
	};
};

class odr_posroad : public odr_element
{
	friend class odr_object;
	friend class odr_objects;
	friend class odr_road;
	friend class odr_map;
	friend class odr_loader;
	friend class odr_extdata;
public:
	odr_posroad(odr_element* p);
	~odr_posroad();
};

class odr_outline : public odr_element
{
	friend class odr_object;
	friend class odr_objects;
	friend class odr_road;
	friend class odr_map;
	friend class odr_loader;
	friend class odr_extdata;
public:
	odr_outline(odr_element* p);
	~odr_outline();

	void release(void);

	int add_cornerlocal(odr_cornerlocal* local) {
		assert(nullptr != local);
		auto* holder = _cornerlocals.new_object();
		if (nullptr == holder) return -1;
		*holder = local;
		return 0;
	}

private:
	void release_all(void);

private:
	gbuf_<odr_cornerlocal*> _cornerlocals;
};

class odr_cornerlocal : public odr_element
{
	friend class odr_outline;
	friend class odr_object;
	friend class odr_objects;
	friend class odr_road;
	friend class odr_map;
	friend class odr_loader;
public:
	odr_cornerlocal(odr_element* p, double u, double v, double z, 
		double height);
	~odr_cornerlocal();

	void release(void);

private:
	double _u;
	double _v;
	double _z;
	double _height;
};

class odr_junction : public odr_element
{
	friend class odr_map;
	friend struct odr_junction_ptr;
public:
	odr_junction(odr_element* p, uint32_t id);
	~odr_junction();

	void release(void);
	void on_after_poped(odr_loader*);

	int add_connection(odr_junction_connection* connection) {
		assert(nullptr != connection);
		auto* holder = _connections.new_object();
		if (nullptr == holder) return -1;
		*holder = connection;
		return 0;
	}

	int add_approach(string appro) {
		auto* holder = _approach.new_object();
		if (nullptr == holder) return -1;
		*holder = new string(appro);
		return 0;
	}

	bool contain_approach(string uuid) {
		for (int i = 0; i < _approach.getsize(); ++i) {
			string* appr_uuid = _approach.buffer()[i];
			if(*appr_uuid == uuid) {
				return true;
			}
		}
		return false;
	}

	void update_bounding_box(double x, double y)
	{
		if (x < _x0) _x0 = x;
		if (x > _x1) _x1 = x;
		if (y < _y0) _y0 = y;
		if (y > _y1) _y1 = y;
	}

private:
	void release_all(void);

	static int avl_id_compare(avl_node_t*, avl_node_t*);

private:
	listnode_t _ownerlist;
	avl_node_t _avlnode;

	uint32_t _id;
	hdmap_container* _container;

	gbuf_<odr_junction_connection*> _connections;
	gbuf_<string*> _approach;
	double _x0, _y0, _x1, _y1;
	std::shared_ptr<Junction>  _odrv_junction;
	uint32_t _rendermap_juncoff;
};

class odr_junction_connection : public odr_element
{
	friend class odr_junction;
	friend class odr_map;
public:
	odr_junction_connection(odr_element* p, uint32_t id, const XMLCh* type, 
		int32_t incomingroad, int32_t connectingroad, const XMLCh* contactpoint);
	~odr_junction_connection();

	int add_lanelink(odr_junction_lanelink* lanelink) {
		assert(nullptr != lanelink);
		auto* holder = _lanelinks.new_object();
		if (nullptr == holder) return -1;
		*holder = lanelink;
		return 0;
	}
	void release(void);

private:
	void release_all(void);
	
private:
	uint32_t _id;
	xmlstring _type;
	int32_t _incoming_road;
	int32_t _connecting_road;
	union {
		uint32_t _attrs;
		struct {
			uint32_t type : 4;
			uint32_t conn_point : 4;
		} _a;
	};

	gbuf_<odr_junction_lanelink*> _lanelinks;
};

class odr_junction_lanelink : public odr_element
{
	friend class odr_junction_connection;
	friend class odr_junction;
	friend class odr_map;
public:
	odr_junction_lanelink(odr_element* p, int32_t from, int32_t to);
	void release(void);

private:
	int32_t _from;
	int32_t _to;
};

class odr_loader : public DefaultHandler
{
	friend class odr_map;
public:
	odr_loader(cmdline_parser& psr);
	~odr_loader();

	int parse(void);

public:
	odr_map& getmap(void) {
		return _map;
	}

	void fix_edge_lane(odr_lane* lane);
	
protected:

	void startElement(
		const XMLCh* const uri,
		const XMLCh* const localname,
		const XMLCh* const qname,
		const Attributes& attrs);
	
    void endElement(
		const XMLCh* const uri,
		const XMLCh* const localname,
		const XMLCh* const qname);

	void characters(
		const XMLCh* const chars, const XMLSize_t length);

	void fatalError(const SAXParseException&);

private:
	void handle_header(const Attributes& attrs);

	// create a new element
	void push_element(xmlstring& ele, const Attributes& attrs);
	int parse_roadlink(roadlink& rl, const Attributes& attrs);
	int parse_add_roadtype(odr_road* r, const Attributes& attrs);
	int parse_add_refline_geometry(odr_road* r, const Attributes& attrs);
	void parse_abcd(const Attributes& attrs,
		double& a, double& b, double& c, double& d);
	int parse_bool(const Attributes& attrs, const XMLCh* t, bool& b);

private:

	static odr_element* elem_road_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_link_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_predecessor_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_successor_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_type_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_geometry_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_line_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_spiral_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_arc_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_poly3_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_ppoly3_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_elevpf_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_elevation_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_laterlpf_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_superelev_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_lanes_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_lanesection_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_lane_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_roadmark_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_laneoffset_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_width_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_height_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_speed_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_objects_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_object_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_signals_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_signal_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_posroad_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_outline_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_cornerlocal_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_junction_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_junction_connection_hdr(odr_loader*, const char*, odr_element*, const Attributes&);
	static odr_element* elem_junction_lanelink_hdr(odr_loader*, const char*, odr_element*, const Attributes&);

	static element_handler _elemhdr_tbl[];

private:
	odr_map _map;
	cmdline_parser& _psr;

	// xerces objects
	SAX2XMLReader*		_reader;

	union {
		uint64_t _attr;
		struct {
			uint64_t odr_parsed : 1;
			uint64_t header_parsed : 1;
			uint64_t georef_parsing : 1;
			uint64_t odr_finished : 1;
		} _attrs;
	};
	stack<odr_element*> _stack;
	int _lane_section_count;
	map<long, set<odr_object*>> _odr_sinlightmap;
};

} // end of namespace osm_parser1
#endif // __CXX_OSM_PARSER1_ODR_LOADER_H__
/* EOF */