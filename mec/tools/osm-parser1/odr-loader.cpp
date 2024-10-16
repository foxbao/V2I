#include <float.h>
#include <stdio.h>
#include "odr-loader.h"

namespace osm_parser1 {

static const char* en_opendrive = "OpenDRIVE";
static const char* en_header = "header";
static const char* en_georef = "geoReference";

static const char* en_road = "road";
static const char* en_link = "link";
static const char* en_predecessor = "predecessor";
static const char* en_successor = "successor";
static const char* en_type = "type";
static const char* en_planview = "planView";
static const char* en_geometry = "geometry";
static const char* en_line = "line";
static const char* en_spiral = "spiral";
static const char* en_arc = "arc";
static const char* en_poly3 = "poly3";
static const char* en_ppoly3 = "paramPoly3";
static const char* en_elevpf = "elevationProfile";
static const char* en_elevation = "elevation";
static const char* en_lateralpf = "lateralProfile";
static const char* en_superelev = "superelevation";
static const char* en_lanes = "lanes";
static const char* en_lanesection = "laneSection";
static const char* en_center = "center";
static const char* en_lane = "lane";
static const char* en_roadmark = "roadMark";
static const char* en_right = "right";
static const char* en_left = "left";
static const char* en_laneoffset = "laneOffset";
static const char* en_width = "width";
static const char* en_height = "height";
static const char* en_speed = "speed";
static const char* en_objects = "objects";
static const char* en_object = "object";
static const char* en_outline = "outline";
static const char* en_cornerlocal = "cornerLocal";
static const char* en_bridge = "bridge";
static const char* en_signals = "signals";
static const char* en_signal = "signal";
static const char* en_posroad = "positionRoad";
static const char* en_validity = "validity";
static const char* en_signalref = "signalReference";
static const char* en_junction = "junction";
static const char* en_junction_connection = "connection";
static const char* en_junction_lanelink = "laneLink";
static const char* en_junction_priority = "priority";
static const char* en_controller = "controller";
static const char* en_control = "control";


element_handler odr_loader::_elemhdr_tbl[] =
{
	{ en_road,						odr_loader::elem_road_hdr },
	{ en_link,						odr_loader::elem_link_hdr },
	{ en_predecessor,				odr_loader::elem_predecessor_hdr },
	{ en_successor,					odr_loader::elem_successor_hdr },
	{ en_type,						odr_loader::elem_type_hdr },
	{ en_planview,					nullptr },
	{ en_geometry,					odr_loader::elem_geometry_hdr },
	{ en_line,						odr_loader::elem_line_hdr },
	{ en_spiral,					odr_loader::elem_spiral_hdr },
	{ en_arc,						odr_loader::elem_arc_hdr },
	{ en_poly3,						odr_loader::elem_poly3_hdr },
	{ en_ppoly3,					odr_loader::elem_ppoly3_hdr },
	{ en_elevpf,					odr_loader::elem_elevpf_hdr },	// todo:
	{ en_elevation,					odr_loader::elem_elevation_hdr },	// todo:
	{ en_lateralpf,					odr_loader::elem_laterlpf_hdr },	// todo:
	{ en_superelev,					odr_loader::elem_superelev_hdr },	// todo:
	{ en_lanes,						odr_loader::elem_lanes_hdr },
	{ en_lanesection,				odr_loader::elem_lanesection_hdr },
	{ en_center,					nullptr },
	{ en_lane,						odr_loader::elem_lane_hdr },
	{ en_roadmark,					odr_loader::elem_roadmark_hdr },
	{ en_right,						nullptr },
	{ en_left,						nullptr },
	{ en_laneoffset,				odr_loader::elem_laneoffset_hdr },
	{ en_width,						odr_loader::elem_width_hdr },
	{ en_height,					odr_loader::elem_height_hdr },
	{ en_speed,						odr_loader::elem_speed_hdr },
	{ en_objects,					odr_loader::elem_objects_hdr },
	{ en_object,					odr_loader::elem_object_hdr },
	{ en_outline,					odr_loader::elem_outline_hdr },
	{ en_cornerlocal,				odr_loader::elem_cornerlocal_hdr },
	{ en_bridge,					nullptr },	// todo:
	{ en_signals,					odr_loader::elem_signals_hdr },	
	{ en_signal,					odr_loader::elem_signal_hdr },
	{ en_posroad,					odr_loader::elem_posroad_hdr },
	{ en_validity,					nullptr },	// todo:
	{ en_signalref,					nullptr },	// todo:
	{ en_junction,					odr_loader::elem_junction_hdr },
	{ en_junction_connection,		odr_loader::elem_junction_connection_hdr },
	{ en_junction_lanelink,			odr_loader::elem_junction_lanelink_hdr },
	{ en_controller,				nullptr },	// todo:
	{ en_control,					nullptr },	// todo:
};

int element_handler::compare(const void* a, const void* b)
{
	auto* aa = (element_handler*)a;
	auto* bb = (element_handler*)b;
	int ret = strcmp(aa->element_name, bb->element_name);
	if (!ret) return 0;
	else if (ret > 0) return 1;
	else return -1;
}

static void fatal_error(const char* func, int errcode)
{
	fprintf(stderr, "error in parsing odr-xml: %s (errcode: %d)\n",
		func, errcode);
	fprintf(stderr, "abort.\n");
	__asm__("int $0x3");
	exit(2500 + errcode);
}

#define parse_error(ec)	\
	do { fatal_error(__FUNCTION__, ec); } while (0)

static string get_object_type_str(object_type objTp)
{
	if(objTp == object_type_none) {
		return "none";
	} 
	else if (objTp == object_type_patch) {
		return "patch";
	}
	else if (objTp == object_type_pole) {
		return "pole";
	}
	else if (objTp == object_type_obstacle) {
		return "obstacle";
	}
	else if (objTp == object_type_pedestrian) {
		return "pedestrian";
	} else {
		return "";
	}
}

static string get_lane_type_str(lane_type laTp)
{
	if (laTp == lane_type_shoulder) {
		return "shoulder";
	}
	else if (laTp == lane_type_border) {
		return "border";
	}
	else if (laTp == lane_type_driving) {
		return "driving";
	}
	else if (laTp == lane_type_stop) {
		return "stop";
	}
	else if (laTp == lane_type_none) {
		return "none";
	}
	else if (laTp == lane_type_restricted) {
		return "restricted";
	}
	else if (laTp == lane_type_parking) {
		return "parking";
	}
	else if (laTp == lane_type_median) {
		return "median";
	}
	else if (laTp == lane_type_biking) {
		return "biking";
	}
	else if (laTp == lane_type_sidewalk) {
		return "sidewalk";
	}
	else if (laTp == lane_type_curb) {
		return "curb";
	}
	else if (laTp == lane_type_exit) {
		return "exit";
	}
	else if (laTp == lane_type_entry) {
		return "entry";
	}
	else if (laTp == lane_type_onramp) {
		return "onramp";
	}
	else if (laTp == lane_type_offramp) {
		return "offRamp";
	}
	else if (laTp == lane_type_connecting_ramp) {
		return "connectingRamp";
	} 
	else {
		return "";
	}
}

static int parse_lane_type(const char* t)
{
	int ret = lane_type_unknown;
	if (!strcmp(t, "shoulder")) {
		ret = lane_type_shoulder;
	}
	else if (!strcmp(t, "border")) {
		ret = lane_type_border;
	}
	else if (!strcmp(t, "driving")) {
		ret = lane_type_driving;
	}
	else if (!strcmp(t, "stop")) {
		ret = lane_type_stop;
	}
	else if (!strcmp(t, "none")) {
		ret = lane_type_none;
	}
	else if (!strcmp(t, "restricted")) {
		ret = lane_type_restricted;
	}
	else if (!strcmp(t, "parking")) {
		ret = lane_type_parking;
	}
	else if (!strcmp(t, "median")) {
		ret = lane_type_median;
	}
	else if (!strcmp(t, "biking")) {
		ret = lane_type_biking;
	}
	else if (!strcmp(t, "sidewalk")) {
		ret = lane_type_sidewalk;
	}
	else if (!strcmp(t, "curb")) {
		ret = lane_type_curb;
	}
	else if (!strcmp(t, "exit")) {
		ret = lane_type_exit;
	}
	else if (!strcmp(t, "entry")) {
		ret = lane_type_entry;
	}
	else if (!strcmp(t, "onramp")) {
		ret = lane_type_onramp;
	}
	else if (!strcmp(t, "offRamp")) {
		ret = lane_type_offramp;
	}
	else if (!strcmp(t, "connectingRamp")) {
		ret = lane_type_connecting_ramp;
	}
	return ret;
}

static int parse_object_type(const char* t)
{
	int ret = object_type_unknown;
	if(!strcmp(t, "none")) {
		ret = object_type_none;
	}
	else if (!strcmp(t, "patch")) {
		ret = object_type_patch;
	} 
	else if (!strcmp(t, "pole")) {
		ret = object_type_pole;
	}
	else if (!strcmp(t, "obstacle")) {
		ret = object_type_obstacle;
	}
	else if (!strcmp(t, "pedestrian")) {
		ret = object_type_pedestrian;
	}
	else if (!strcmp(t, "parkingSpace")) {
		ret = object_type_parkingSpace;
	}
	return ret;
}

void odr_loader::startElement(
	const XMLCh* const uri, const XMLCh* const localname,
	const XMLCh* const qname, const Attributes& attrs)
{
	xmlstring element(qname);

	if (!_attrs.odr_parsed && !strcmp(element, en_opendrive)) {
		_attrs.odr_parsed = 1;
	}
	else if (!_attrs.header_parsed && !strcmp(element, en_header)) {
		handle_header(attrs);
		_attrs.header_parsed = 1;
	}
	else if (!_attrs.georef_parsing && !strcmp(element, en_georef)) {
		_attrs.georef_parsing = 1;
	}
	else push_element(element, attrs);
}

void odr_loader::characters(
	const XMLCh* const chars, const XMLSize_t length)
{
	if (!_attrs.georef_parsing) {
		return;
	}
	assert(*chars != u'\n');
	xmlstring tmp(chars);
	_map.set_georef(tmp.c_str());
}

void odr_loader::endElement(
	const XMLCh* const uri, const XMLCh* const localname,
	const XMLCh* const qname)
{
	xmlstring element(qname);

	// check if we match with the stack
	auto* odrobj = _stack.empty() ? nullptr : _stack.top();
	if (odrobj && odrobj->same_type(element)) {
		_stack.pop();
		odrobj->on_after_poped(this);
		// release the object if necessary
		odrobj->release();
		return;
	}

	if (_attrs.odr_parsed && !strcmp(element, en_opendrive)) {
		_attrs.odr_finished = 1;
	}
	else if (!strcmp(element, en_header)) {
		return; // no problem
	}
	else if (!strcmp(element, en_georef)) {
		assert(_attrs.georef_parsing);
		_attrs.georef_parsing = 0;
		return; // no problem
	}
	else parse_error(-1);
}

void odr_loader::fatalError(const SAXParseException& exception)
{
	char* message = XMLString::transcode(exception.getMessage());
	fprintf(stderr, "fatal error: %s at line: %lu\n", message,
		exception.getLineNumber());
	XMLString::release(&message);
	exit(1002);
}

// odr-loader
odr_loader::odr_loader(cmdline_parser& psr)
: _map(psr), _reader(nullptr), _attr(0)
, _lane_section_count(0), _psr(psr)
{
	// order the elemhdr_tbl
	qsort(_elemhdr_tbl,
		sizeof(_elemhdr_tbl) / sizeof(element_handler),
		sizeof(element_handler),
		element_handler::compare);

	try {
		// initialize the xerces
		XMLPlatformUtils::Initialize();
	} catch (const XMLException& toCatch) {
		char* message = XMLString::transcode(toCatch.getMessage());
		fprintf(stderr, "fatal error: fail in init xerces.\n");
		fprintf(stderr, "error desc: %s\n", message);
		XMLString::release(&message);
	}
}

odr_loader::~odr_loader()
{
	if (_reader) {
		delete _reader, _reader = nullptr;
	}
}

int odr_loader::parse(void)
{
	for (int i = 0;; ++i)
	{
		// parse the xml file
		const char* odrfile = _map.get_parser().get_srcfile(i);
		if (nullptr == odrfile) break;
		fprintf(stdout, "parsing odr-file: \"%s\" ... ", odrfile);
		fflush(nullptr);

		_reader = XMLReaderFactory::createXMLReader();
		assert(nullptr != _reader);
		
		_reader->setFeature(XMLUni::fgSAX2CoreValidation, true);
		_reader->setFeature(XMLUni::fgSAX2CoreNameSpaces, true);   // optional

		_reader->setContentHandler(this);
		_reader->setErrorHandler(this);

		try { 
			_reader->parse(odrfile);
			map<long, set<odr_object*>>::reverse_iterator iter;
			for(iter = _odr_sinlightmap.rbegin(); iter != _odr_sinlightmap.rend(); iter++){
				odr_road* r = _map.getroad_byid(iter->first);
				if (nullptr == r) {
					continue;
				}
				odr_objects* os = r->get_objects();
				if (nullptr == os) {
					os = new odr_objects(nullptr);
					r->set_objects(os);
				}
				set<odr_object*>::iterator it;
				for (it = iter->second.begin(); it != iter->second.end(); it++){
					os->add_object(*it);
					auto roadObj = make_shared<RoadObject>();
					r->odrv_update_roadobject(*it, roadObj);
				}
			}
			
		} catch (const XMLException& c) {
			char* message = XMLString::transcode(c.getMessage());
			fprintf(stdout, "\rerror in parsing xml: %s\n", message);
			XMLString::release(&message);
			return -1;
		}
		catch (const SAXParseException& c) {
			char* message = XMLString::transcode(c.getMessage());
			fprintf(stdout, "\rerror in parsing xml: %s\n", message);
			XMLString::release(&message);
			return -2;
		} catch (...) {
			fprintf(stdout, "unknown error in parsing xml.\n");
			return -3;
		}
		delete _reader; _reader = nullptr;
		_attr = 0;
		fprintf(stdout, "[success]\n");
	}
	return 0;
}

void odr_loader::handle_header(const Attributes& attrs)
{
	xmlstring ver(attrs.getValue(xc("revMajor")));
	if (nullptr == ver) parse_error(-1);
	if (strtol(ver, nullptr, 10) > 1) {
		parse_error(-2);
	}

	ver = attrs.getValue(xc("revMinor"));
	if (nullptr == ver) parse_error(-3);
	if (strtol(ver, nullptr, 10) > 6) {
		parse_error(-3);
	}
	// get vender
	ver = attrs.getValue(xc("vendor"));
	_map.set_vendor(ver ? ver : "");
}

void odr_loader::push_element(xmlstring& ele, const Attributes& attrs)
{
	if (_attrs.odr_finished) {
		return;
	}

	if (!_attrs.odr_parsed || !_attrs.header_parsed) {
		parse_error(-1);
	}

	odr_element* odrobj = nullptr;
	auto* parents = (_stack.empty()) ? nullptr : _stack.top();

	// search for a supported element
	int mid;
	int start = 0;
	element_handler* elemhdr = nullptr;
	int end = sizeof(_elemhdr_tbl) / sizeof(element_handler) - 1;
	
	while (start <= end) {
		mid = (start + end) / 2;
		auto& val = _elemhdr_tbl[mid];

		int ret = strcmp(ele, val.element_name);
		if (!ret) {
			elemhdr = &val; break;
		}
		else if (ret > 0) start = mid + 1;
		else end = mid - 1;
	}

	if (nullptr == elemhdr) {
		printf("<%s> not found\n", ele.c_str());
		parse_error(-2);
	}
	if (elemhdr->parse) {
		odrobj = elemhdr->parse(this, elemhdr->element_name, parents, attrs);
	}
	if (nullptr == odrobj) {
		odrobj = new odr_anonymous(parents, elemhdr->element_name);
	}
	odrobj->on_before_pushed(this);
	_stack.push(odrobj);
}

odr_element::odr_element(odr_element* p, const char* name, int type)
: _parents(p), _element_name(name) {
	_attrs.type = type;
	_attrs.serialized_datasize = 0;
}

odr_element::~odr_element() {
}

void odr_element::release(void)
{
	delete this;
}

void odr_element::on_before_pushed(odr_loader*)
{
}

void odr_element::on_after_poped(odr_loader*)
{
}

odr_anonymous::odr_anonymous(odr_element* p,
	const char* en, void* data)
: odr_element(p, en, odr_elemtype_anonymous)
, _data(_data) {
}

odr_road::odr_road(odr_element* p)
: odr_element(p, en_road, odr_elemtype_road)
, _length(0.), _id(0), _junction_id(0)
, _road_types(4), _geometries(8)
, _lanes(nullptr), _objects(nullptr)
, _container(nullptr)
, _rendermap_road(nullptr)
{
	listnode_init(_ownerlist);
	listnode_init(_hdmr_ownerlist);
	_odrv_road = std::make_shared<Road>();
}

odr_road::~odr_road()
{
	release_all();
}

void odr_road::on_after_poped(odr_loader* ldr)
{
	// sort all possible objects
	qsort(_geometries.buffer(), _geometries.getsize(),
		sizeof(refl_geometry*), qst_geometry_compare);

	auto& mapobj = ldr->getmap();
	if (mapobj.road_exists_and_merged(this)) {
		return;
	}

	_odrv_road->length = _length;
	ltoa(_id, _odrv_road->id);
	ltoa(_junction_id, _odrv_road->junction);
	if(nullptr != _name) {
		_odrv_road->name = _name.c_str();
	}
	CHECK_AND_REPAIR(_odrv_road->length >= 0, "road::length < 0", _odrv_road->length = 0);
	
	ltoa(_prev.elem_id, _odrv_road->predecessor.id);
	_odrv_road->predecessor.type = RoadLink::Type::Road;
	_odrv_road->predecessor.contact_point = (RoadLink::ContactPoint)_prev.a.conn_point;
	ltoa(_next.elem_id, _odrv_road->successor.id);
	_odrv_road->successor.type = RoadLink::Type::Road;
	_odrv_road->successor.contact_point = (RoadLink::ContactPoint)_next.a.conn_point;

	for (int i = 0; i < _lateral_profile->_super_elevations.getsize(); ++i) {
		auto lasterpro = _lateral_profile->_super_elevations.buffer()[i];
		Poly3 p = Poly3(lasterpro->_s, lasterpro->_a, lasterpro->_b, lasterpro->_c, lasterpro->_d);
		_odrv_road->superelevation.s0_to_poly.insert(std::map<double, Poly3>::value_type(lasterpro->_s, p));
	}

	for (int i = 0; i < _road_types.getsize(); ++i) {
		auto type = _road_types.buffer()[i];
		double s = type.s;
		
		std::string typeStr; 
		if(type.type == 1) {
			typeStr = "town";
		} else {
			typeStr = "";
		}
		_odrv_road->s_to_type.insert(map<double, std::string>::value_type(s,typeStr));
	}
	_odrv_road->ref_line = std::make_shared<RefLine>(_length);
	for (int i = 0; i < _geometries.getsize(); ++i) {
		auto* geo = _geometries.buffer()[i];
		std::shared_ptr<RoadGeometry> road_geometry = nullptr;
		if(geo->type == pvg_type::pvg_type_line) {
			road_geometry = std::make_shared<Line>(geo->s, geo->x, geo->y, geo->hdg, geo->length);
		} 
		else if (geo->type == pvg_type::pvg_type_arc) {
			auto* arcgeo = zas_downcast(refl_geometry , refl_geometry_arc, geo);
			road_geometry = std::make_shared<Arc>(arcgeo->s, arcgeo->x, arcgeo->y, arcgeo->hdg, 
				arcgeo->length, arcgeo->arc.curvature);
		} 
		else if (geo->type == pvg_type::pvg_type_spiral) {
			auto* spiralgeo = zas_downcast(refl_geometry , refl_geometry_spiral, geo);
			road_geometry = std::make_shared<Spiral>(spiralgeo->s, spiralgeo->x, spiralgeo->y, 
				spiralgeo->hdg, spiralgeo->length, spiralgeo->spiral.curv_start, spiralgeo->spiral.curv_end);
		}
		else if (geo->type == pvg_type::pvg_type_param_poly3) {
			auto* parampoly3geo = zas_downcast(refl_geometry , refl_geometry_param_poly3, geo);
			bool poly3range = true;
			if(parampoly3geo->ppoly3.range == ppoly3_range::ppoly3_range_arc_length) {
				poly3range = false;
			}
			road_geometry = std::make_shared<ParamPoly3>(parampoly3geo->s, parampoly3geo->x, parampoly3geo->y, 
				parampoly3geo->hdg, parampoly3geo->length, parampoly3geo->ppoly3.aU, parampoly3geo->ppoly3.bU, 
				parampoly3geo->ppoly3.cU, parampoly3geo->ppoly3.dU, parampoly3geo->ppoly3.aV, 
				parampoly3geo->ppoly3.bV, parampoly3geo->ppoly3.cV, parampoly3geo->ppoly3.dV, poly3range);
		}
		else {
			parse_error(-1);
		}
		// _odrv_road->ref_line->s0_to_geometry.insert(map<double, std::shared_ptr<RoadGeometry>>::value_type(geo->s, road_geometry));
		_odrv_road->ref_line->s0_to_geometry[geo->s] = road_geometry;
	}

	if (nullptr != _elevation_profile) {
		for (int i = 0; i < _elevation_profile->_elevations.getsize(); ++i) {
			auto elev = _elevation_profile->_elevations.buffer()[i];
			Poly3 p = Poly3(elev->_s, elev->_a, elev->_b, elev->_c, elev->_d);
			_odrv_road->ref_line->elevation_profile.s0_to_poly.insert(std::map<double, Poly3>::value_type(elev->_s, p));
		}
	}

	if (nullptr != _lanes) {
		for (int i = 0; i < _lanes->_lane_offsets.getsize(); ++i) {
			auto* offset = _lanes->_lane_offsets.buffer()[i];
			Poly3 p = Poly3(offset->_s, offset->_a, offset->_b, offset->_c, offset->_d);
			_odrv_road->lane_offset.s0_to_poly.insert(map<double, Poly3>::value_type(offset->_s,p));
		}
		for (int i = 0; i < _lanes->_lane_sects.getsize(); ++i) {
			auto* sect = _lanes->_lane_sects.buffer()[i];
			double s = sect->_s;
			std::shared_ptr<LaneSection> ls = std::make_shared<LaneSection>(s);
			ls->road = _odrv_road;
			sect->_odrv_lanesect = ls;

			odr_lane* cl = sect->_center_lane;
			bool laneLvl = false;
			if(cl->_attrs.level > 0) {
				laneLvl = true;
			}
			std::string laneTp = get_lane_type_str(lane_type(cl->_attrs.type));
			std::shared_ptr<Lane> l = std::make_shared<Lane>(cl->_id, laneLvl, laneTp);
			l->successor = cl->_next;
			l->predecessor = cl->_prev;
			l->road = _odrv_road;
			l->lane_section = ls;
			cl->_odrv_lane = l;

			for(int k = 0; k < cl->_widths.getsize();k++) {
				auto* width = cl->_widths.buffer()[k];
				Poly3 p = Poly3(s + width->_s_offset, width->_a, width->_b, width->_c, width->_d);
				l->lane_width.s0_to_poly.insert(map<double, Poly3>::value_type(s + width->_s_offset, p));
			}

			for(int k=0; k < cl->_heights.getsize(); k++) {
				auto* height = cl->_heights.buffer()[k];
				l->s_to_height_offset.insert(map<double, HeightOffset>::value_type(s+height->_s_offset, 
					HeightOffset{height->_inner, height->_outer}));
			}

			for(int k=0; k < cl->_roadmarks.getsize(); k++) {
				auto* roadmark = cl->_roadmarks.buffer()[k];
				RoadMarkGroup roadmark_group;
				roadmark_group.width = roadmark->_width;
				roadmark_group.height = roadmark->_height;
				roadmark_group.s_offset = roadmark->_s_offset;
				roadmark_group.type = roadmark->_mark_type;
				roadmark_group.weight = roadmark->_weight;
				roadmark_group.color = roadmark->_color;
				if (roadmark->_lane_change == rm_lanechg_type_none) {
					roadmark_group.laneChange = "none";
				} else if (roadmark->_lane_change == rm_lanechg_type_both) {
					roadmark_group.laneChange = "both";
				}
				roadmark_group.laneChange = roadmark->_lane_change;
				for (int j=0; j < roadmark->_lines.getsize(); j++) {
					auto* line = roadmark->_lines.buffer()[j];
					RoadMarksLine roadmarks_line;
					roadmarks_line.length = line->_length;
					roadmarks_line.space = line->_space;
					roadmarks_line.s_offset = line->_s_offset;
					roadmarks_line.t_offset  = line->_t_offset;
					roadmarks_line.width = line->_width;
					roadmark_group.s_to_roadmarks_line[s + roadmark_group.s_offset + roadmarks_line.s_offset] = roadmarks_line;
				}
				l->s_to_roadmark_group[s + roadmark_group.s_offset] = roadmark_group;
			}

			ls->id_to_lane.insert(map<int, std::shared_ptr<Lane>>::value_type(l->id,l));

			for (int j = 0; j < sect->_left_lanes.getsize(); ++j) {
				odrv_create_lane(ldr, sect->_left_lanes.buffer()[j], sect);
			}
			for (int j = 0; j < sect->_right_lanes.getsize(); ++j) {
				odrv_create_lane(ldr, sect->_right_lanes.buffer()[j], sect);
			}

			/* derive lane borders from lane widths */
			auto id_lane_iter0 = ls->id_to_lane.find(0);
			if (id_lane_iter0 == ls->id_to_lane.end())
				throw std::runtime_error("lane section does not have lane #0");

			/* iterate from id #0 towards +inf */
			auto id_lane_iter1 = std::next(id_lane_iter0);
			for (auto iter = id_lane_iter1; iter != ls->id_to_lane.end(); iter++)
			{
				if (iter == id_lane_iter0)
				{
					iter->second->outer_border = iter->second->lane_width;
				}
				else
				{
					iter->second->inner_border = std::prev(iter)->second->outer_border;
					iter->second->outer_border = std::prev(iter)->second->outer_border.add(iter->second->lane_width);
				}
			}

			/* iterate from id #0 towards -inf */
			std::map<int, std::shared_ptr<Lane>>::const_reverse_iterator r_id_lane_iter_1(id_lane_iter0);
			for (auto r_iter = r_id_lane_iter_1; r_iter != ls->id_to_lane.rend(); r_iter++)
			{
				if (r_iter == r_id_lane_iter_1)
				{
					r_iter->second->outer_border = r_iter->second->lane_width.negate();
				}
				else
				{
					r_iter->second->inner_border = std::prev(r_iter)->second->outer_border;
					r_iter->second->outer_border = std::prev(r_iter)->second->outer_border.add(r_iter->second->lane_width.negate());
				}
			}

			for (auto& id_lane : ls->id_to_lane)
			{
				id_lane.second->inner_border = id_lane.second->inner_border.add(_odrv_road->lane_offset);
				id_lane.second->outer_border = id_lane.second->outer_border.add(_odrv_road->lane_offset);
			}
			
			_odrv_road->s_to_lanesection.insert(map<double, std::shared_ptr<LaneSection>>::value_type(s, ls));
		}

	}
	if(nullptr != _objects) {
		for(int i = 0; i < _objects->_objects.getsize(); ++i) {
			auto* obj = _objects->_objects.buffer()[i];
			auto roadObj = make_shared<RoadObject>();
			odrv_update_roadobject(obj, roadObj);
		}
	}
	mapobj.add_road(this);
}

void odr_road::odrv_create_lane(odr_loader* ldr,
	odr_lane* lane, odr_lane_section* ls)
{
	bool lane_level = (lane->_attrs.level > 0) ? true : false;
	string lanetype = get_lane_type_str(lane_type(lane->_attrs.type));
	shared_ptr<Lane> odrvlane = std::make_shared<Lane>(lane->_id, lane_level, lanetype);
	
	odrvlane->successor = lane->_next;
	odrvlane->predecessor = lane->_prev;
	odrvlane->road = _odrv_road;
	odrvlane->lane_section = ls->_odrv_lanesect;
	lane->_odrv_lane = odrvlane;

	double s = ls->_s;
	for(int k = 0; k < lane->_widths.getsize();k++) {
		auto* width = lane->_widths.buffer()[k];
		Poly3 p1 = Poly3(s + width->_s_offset, width->_a, 
			width->_b, width->_c, width->_d);
		odrvlane->lane_width.s0_to_poly.insert(map<double, 
			Poly3>::value_type(s + width->_s_offset, p1));
	}

	for(int k=0;k < lane->_heights.getsize();k++) {
		auto* height = lane->_heights.buffer()[k];
		odrvlane->s_to_height_offset.insert(map<double, 
			HeightOffset>::value_type(s + height->_s_offset,
			HeightOffset{height->_inner, height->_outer}));
	}

	for(int k=0; k < lane->_roadmarks.getsize(); k++) {
		auto* roadmark = lane->_roadmarks.buffer()[k];
		RoadMarkGroup roadmark_group;
		roadmark_group.width = roadmark->_width;
		roadmark_group.height = roadmark->_height;
		roadmark_group.s_offset = roadmark->_s_offset;
		roadmark_group.type = roadmark->get_type();
		if (roadmark->_lane_change == rm_lanechg_type_none) {
			roadmark_group.laneChange = "none";
		} else if (roadmark->_lane_change == rm_lanechg_type_both) {
			roadmark_group.laneChange = "both";
		}
		roadmark_group.laneChange = roadmark->_lane_change;
		for (int j=0; j < roadmark->_lines.getsize(); j++) {
			auto* line = roadmark->_lines.buffer()[j];
			RoadMarksLine roadmarks_line;
			roadmarks_line.length = line->_length;
			roadmarks_line.space = line->_space;
			roadmarks_line.s_offset = line->_s_offset;
			roadmarks_line.t_offset  = line->_t_offset;
			roadmarks_line.width = line->_width;
			roadmark_group.s_to_roadmarks_line[s + roadmark_group.s_offset + roadmarks_line.s_offset] = roadmarks_line;
		}
		odrvlane->s_to_roadmark_group[s + roadmark_group.s_offset] = roadmark_group;
	}

	auto odrvls = ls->_odrv_lanesect.lock();
	assert(nullptr != odrvls);
	odrvls->id_to_lane.insert(map<int,
		std::shared_ptr<Lane>>::value_type(odrvlane->id, odrvlane));
}

void odr_road::odrv_update_roadobject(odr_object* obj,
	shared_ptr<RoadObject> roadObj, RoadObjectCorner::Type type)
{
	if (nullptr == roadObj) {
		roadObj = obj->_odrv_object.lock();
		assert(nullptr != roadObj);
	}
	ltoa(obj->_id, roadObj->id);
	
	roadObj->type = get_object_type_str(object_type(obj->_type));
	roadObj->name = obj->_name;
	if(obj->_orientation.c_str()) {
		roadObj->orientation = obj->_orientation.c_str();
	}
	
	roadObj->s0 = obj->_s;
	roadObj->t0 = obj->_t;
	roadObj->z0 = obj->_z_offset;
	roadObj->valid_length = obj->_valid_length;
	roadObj->hdg = obj->_hdg;
	roadObj->pitch = obj->_pitch;
	roadObj->roll = obj->_roll;
	roadObj->height = obj->_height;
	roadObj->length = obj->_length;
	roadObj->width = obj->_width;
	roadObj->radius = obj->_radius;

	roadObj->outline_base[0] = obj->_outline_base.xyz.x;
	roadObj->outline_base[1] = obj->_outline_base.xyz.y;
	roadObj->outline_base[2] = obj->_outline_base.xyz.z;

	roadObj->road = _odrv_road;
	roadObj->outline.clear();

	odr_outline* objline = obj->_outline;
	if(nullptr != objline) {
		for(int j = 0; j < objline->_cornerlocals.getsize(); ++j) {
			auto* local = objline->_cornerlocals.buffer()[j];
			
			RoadObjectCorner corner;
			corner.type = type;
			corner.height = local->_height;
			corner.pt[0] = local->_u;
			corner.pt[1] = local->_v;
			corner.pt[2] = local->_z;
			roadObj->outline.push_back(corner);
		}
	}
	_odrv_road->id_to_object.insert(map<std::string,
		std::shared_ptr<RoadObject>>::value_type(roadObj->id, roadObj));
	obj->_odrv_object = roadObj;
}

void odr_road::release(void)
{
	if (!listnode_isempty(_ownerlist)) {
		return;
	}
	odr_element::release();
}

void odr_road::release_all(void)
{
	// release all lanes
	if (_lanes) delete _lanes;
	for (int i = 0; i < _geometries.getsize(); ++i) {
		auto* geo = _geometries.buffer()[i];
		delete geo;
	}
	if (!listnode_isempty(_ownerlist)) {
		listnode_del(_ownerlist);
	}
}

int odr_road::avl_id_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(odr_road, _avlnode, a);
	auto* bb = AVLNODE_ENTRY(odr_road, _avlnode, b);
	if (aa->_id > bb->_id) return 1;
	else if (aa->_id < bb->_id) return -1;
	else return 0;
}

int odr_road::qst_geometry_compare(const void* a, const void* b)
{
	auto* aa = *((refl_geometry**)a);
	auto* bb = *((refl_geometry**)b);
	if (aa->s > bb->s) return 1;
	else if (aa->s < bb->s) return -1;
	else return 0;
}

odr_element* odr_loader::elem_road_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	auto* road = new odr_road(p);
	if (nullptr == road) {
		return nullptr;
	}

	// name
	road->_name = attrs.getValue(xc("name"));

	// length
	xmlstring tmp(attrs.getValue(xc("length")));
	if (tmp.c_str()) {
		road->_length = strtod(tmp, nullptr);
	}

	// id
	tmp = attrs.getValue(xc("id"));
	if (!tmp.c_str()) parse_error(-1);
	road->_id = strtol(tmp, nullptr, 10);

	// junction_id
	tmp = attrs.getValue(xc("junction"));
	if (tmp.c_str()) {
		road->_junction_id = strtol(tmp, nullptr, 10);
	}
	return road;
}

odr_element* odr_loader::elem_link_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	odr_element* ret = nullptr;
	if (p->same_type(en_road)) {
		assert(nullptr != p);
		ret = new odr_element(p, en, odr_elemtype_road_link);
	}
	else if (p->same_type(en_lane)) {
		assert(nullptr != p);
		ret = new odr_element(p, en, odr_elemtype_lane_link);
	}
	// todo:
	return ret;
}

odr_element* odr_loader::elem_predecessor_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	int ret = 0;
	assert(nullptr != p);
	if (p->get_type() == odr_elemtype_road_link) {
		auto* r = zas_downcast(odr_element, odr_road, p->_parents);
		assert(r != nullptr);
		ret = ldr->parse_roadlink(r->_prev, attrs);
	}
	else if (p->get_type() == odr_elemtype_lane_link) {
		auto* lane = zas_downcast(odr_element, odr_lane, p->_parents);
		assert(nullptr != lane);
		xmlstring tmp(attrs.getValue(xc("id")));
		if (!tmp.c_str()) parse_error(-1);
		lane->_prev = strtol(tmp, nullptr, 10);
		lane->_attrs.has_prev = 1;
	}
	if (ret) parse_error(ret);
	return nullptr;
}

odr_element* odr_loader::elem_successor_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	int ret = 0;
	assert(nullptr != p);
	if (p->get_type() == odr_elemtype_road_link) {
		auto* r = zas_downcast(odr_element, odr_road, p->_parents);
		assert(r != nullptr);
		ret = ldr->parse_roadlink(r->_next, attrs);
	}
	else if (p->get_type() == odr_elemtype_lane_link) {
		auto* lane = zas_downcast(odr_element, odr_lane, p->_parents);
		assert(nullptr != lane);
		xmlstring tmp(attrs.getValue(xc("id")));
		if (!tmp.c_str()) parse_error(-1);
		lane->_next = strtol(tmp, nullptr, 10);
		lane->_attrs.has_next = 1;
	}
	if (ret) parse_error(ret);
	return nullptr;
}

int odr_loader::parse_roadlink(roadlink& rl, const Attributes& attrs)
{
	xmlstring tmp(attrs.getValue(xc("elementType")));
	if (!tmp.c_str()) {
		return -1;
	}
	if (!strcmp(tmp, "road")) {
		rl.a.elem_type= rl_elemtype_road;
	}
	else if (!strcmp(tmp, "junction")) {
		rl.a.elem_type = rl_elemtype_junction;
	}
	// todo:
	else return -2;

	rl.a.conn_point = rl_connpoint_unknown;
	tmp = attrs.getValue(xc("contactPoint"));
	if (tmp.c_str()) {
		if (!strcmp(tmp, "start")) {
			rl.a.conn_point = rl_connpoint_start;
		}
		else if (!strcmp(tmp, "end")) {
			rl.a.conn_point = rl_connpoint_end;
		}
		// todo:
		else return -3;
	}

	tmp = attrs.getValue(xc("elementId"));
	if (!tmp.c_str()) return -4;
	rl.elem_id = strtol(tmp, nullptr, 10);
	if (!rl.elem_id) {
		fprintf(stderr, "road: elementId shall not be 0.\n");
		return -4;
	}
	return 0;
}

odr_element* odr_loader::elem_type_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	int ret = 0;
	assert(nullptr != p);
	if (p->get_type() == odr_elemtype_road) {
		// <road>/<type>
		auto* r = zas_downcast(odr_element, odr_road, p);
		ret = ldr->parse_add_roadtype(r, attrs);
	}
	else if (p->get_type() == odr_elemtype_roadmark) {
		// <roadmark>/<type>
		auto* rm = zas_downcast(odr_element, odr_roadmark, p);
		// todo: handle roadmark/type: name
		xmlstring tmp(attrs.getValue(xc("width")));
		if (!tmp.c_str()) parse_error(-1);
		bool eq = rm->verify_update_width(strtod(tmp, nullptr));
		assert(eq == true);
	}
	if (ret) parse_error(ret);
	return nullptr;
}

int odr_loader::parse_add_roadtype(odr_road* r, const Attributes& attrs)
{
	xmlstring tmp(attrs.getValue(xc("s")));
	if (!tmp.c_str()) {
		return -1;
	}
	
	auto* t = r->_road_types.new_object();
	if (nullptr == t) {
		return -2;
	}
	t->s = strtod(tmp, nullptr);
	t->type = rt_type_unknown;

	tmp = attrs.getValue(xc("type"));
	if (tmp.c_str()) {
		if (!strcmp(tmp, "town")) {
			t->type = rt_type_town;
		}
		// todo: else
	}
	return 0;
}

odr_geometry::odr_geometry(odr_element* p)
: odr_element(p, en_geometry, odr_elemtype_geometry)
, _s(0.), _x(0.), _y(0.), _hdg(0.), _length(0.)
{
}

void odr_geometry::on_after_poped(odr_loader* ldr)
{
	if (!_parents->same_type(en_planview)) {
		parse_error(-1);
	}
	auto* p = _parents->get_parents();
	if (p->get_type()!= odr_elemtype_road) {
		parse_error(-2);
	}
	auto* r = zas_downcast(odr_element, odr_road, p);
	
	auto* geo = create_geometry();
	if (nullptr == geo) {
		parse_error(-3);
	}
	if (r->add_geometry(geo)) {
		parse_error(-4);
	}
	ldr->getmap().update_xy_offset(geo);
}

refl_geometry* odr_geometry::create_geometry(void)
{
	refl_geometry* ret = nullptr;
	
	switch (_geotype) {
	case pvg_type_line:
		ret = new refl_geometry();
		if (nullptr == ret) {
			return ret;
		} break;
	
	case pvg_type_spiral: {
		auto* sp = new refl_geometry_spiral();
		if (nullptr == sp) return sp;
		sp->spiral = _spiral;
		ret = sp;
	} break;

	case pvg_type_arc: {
		auto* ar = new refl_geometry_arc();
		if (nullptr == ar) return ar;
		ar->arc = _arc;
		ret = ar;
	} break;

	case pvg_type_poly3: {
		auto* po = new refl_geometry_poly3();
		if (nullptr == po) return po;
		po->poly3 = _poly3;
		ret = po;
	} break;

	case pvg_type_param_poly3: {
		auto* ppo = new refl_geometry_param_poly3();
		if (nullptr == ppo) return ppo;
		ppo->ppoly3 = _ppoly3;
		ret = ppo;
	} break;

	default: parse_error(-1);
	}
	
	assert(nullptr != ret);
	ret->s = _s;
	ret->x = _x;
	ret->y = _y;
	ret->hdg = _hdg;
	ret->length = _length;
	ret->type = _geotype;

	return ret;
}

odr_element* odr_loader::elem_geometry_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (!p->same_type(en_planview)) {
		parse_error(-1);
	}

	xmlstring tmp(attrs.getValue(xc("s")));
	if (!tmp.c_str()) parse_error(-2);
	double s = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("x"));
	if (!tmp.c_str()) parse_error(-3);
	double x = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("y"));
	if (!tmp.c_str()) parse_error(-4);
	double y = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("hdg"));
	if (!tmp.c_str()) parse_error(-5);
	double hdg = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("length"));
	if (!tmp.c_str()) parse_error(-6);
	double length = strtod(tmp, nullptr);

	auto* geo = new odr_geometry(p);
	if (nullptr == geo) {
		return nullptr;
	}
	geo->_s = s, geo->_x = x, geo->_y = y;
	geo->_hdg = hdg, geo->_length = length;
	return geo;
}

odr_element* odr_loader::elem_line_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	odr_element* ret = nullptr;
	if (p->get_type() == odr_elemtype_geometry) {
		// handle <planView>/<geometry>/<line>
		auto* geo = zas_downcast(odr_element, odr_geometry, p);
		geo->_geotype = pvg_type_line;
	}
	else if (p->same_type(en_type)) {
		assert(p->_parents);
		if (p->_parents->get_type() == odr_elemtype_roadmark) {
			auto* rm = zas_downcast(odr_element, odr_roadmark, p->_parents);
			auto* line = new odr_line(p, attrs);
			if (rm->add_line(line)) {
				parse_error(-1);
			}
			ret = line;
		}
	}
	return ret;
}

odr_element* odr_loader::elem_spiral_hdr(
	odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	xmlstring tmp;
	odr_element* ret = nullptr;
	if (p->get_type() == odr_elemtype_geometry) {
		// handle <planView>/<geometry>/<spiral>
		auto* geo = zas_downcast(odr_element, odr_geometry, p);
		geo->_geotype = pvg_type_spiral;

		tmp = attrs.getValue(xc("curvStart"));
		if (!tmp.c_str()) parse_error(-1);
		geo->_spiral.curv_start = strtod(tmp, nullptr);

		tmp = attrs.getValue(xc("curvEnd"));
		if (!tmp.c_str()) parse_error(-2);
		geo->_spiral.curv_end = strtod(tmp, nullptr);
	}
	return ret;
}

odr_element* odr_loader::elem_arc_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	xmlstring tmp;
	odr_element* ret = nullptr;
	if (p->get_type() == odr_elemtype_geometry) {
		// handle <planView>/<geometry>/<arc>
		auto* geo = zas_downcast(odr_element, odr_geometry, p);
		geo->_geotype = pvg_type_arc;

		tmp = attrs.getValue(xc("curvature"));
		if (!tmp.c_str()) parse_error(-1);
		geo->_arc.curvature = strtod(tmp, nullptr);
	}
	return ret;
}

odr_element* odr_loader::elem_poly3_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	odr_element* ret = nullptr;
	if (p->get_type() == odr_elemtype_geometry) {
		// handle <planView>/<geometry>/<poly3>
		auto* geo = zas_downcast(odr_element, odr_geometry, p);
		geo->_geotype = pvg_type_poly3;
		ldr->parse_abcd(attrs, geo->_poly3.a, geo->_poly3.b,
			geo->_poly3.c, geo->_poly3.d);
	}
	return ret;
}

odr_element* odr_loader::elem_ppoly3_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	xmlstring tmp;
	odr_element* ret = nullptr;
	if (p->get_type() == odr_elemtype_geometry) {
		// handle <planView>/<geometry>/<poly3>
		auto* geo = zas_downcast(odr_element, odr_geometry, p);
		geo->_geotype = pvg_type_param_poly3;

		tmp = attrs.getValue(xc("aU"));
		if (!tmp.c_str()) parse_error(-1);
		geo->_ppoly3.aU = strtod(tmp, nullptr);

		tmp = attrs.getValue(xc("bU"));
		if (!tmp.c_str()) parse_error(-2);
		geo->_ppoly3.bU = strtod(tmp, nullptr);

		tmp = attrs.getValue(xc("cU"));
		if (!tmp.c_str()) parse_error(-3);
		geo->_ppoly3.cU = strtod(tmp, nullptr);

		tmp = attrs.getValue(xc("dU"));
		if (!tmp.c_str()) parse_error(-4);
		geo->_ppoly3.dU = strtod(tmp, nullptr);

		tmp = attrs.getValue(xc("aV"));
		if (!tmp.c_str()) parse_error(-5);
		geo->_ppoly3.aV = strtod(tmp, nullptr);

		tmp = attrs.getValue(xc("bV"));
		if (!tmp.c_str()) parse_error(-6);
		geo->_ppoly3.bV = strtod(tmp, nullptr);

		tmp = attrs.getValue(xc("cV"));
		if (!tmp.c_str()) parse_error(-7);
		geo->_ppoly3.cV = strtod(tmp, nullptr);

		tmp = attrs.getValue(xc("dV"));
		if (!tmp.c_str()) parse_error(-8);
		geo->_ppoly3.dV = strtod(tmp, nullptr);

		tmp = attrs.getValue(xc("pRange"));
		if (!tmp.c_str()) parse_error(-9);
		if (!strcmp(tmp, "arcLength")) {
			geo->_ppoly3.range = ppoly3_range_arc_length;
		}
		else if (!strcmp(tmp, "normalized")) {
			geo->_ppoly3.range = ppoly3_range_normalized;
		}
		else parse_error(10);
	}
	return ret;
}

void odr_loader::parse_abcd(const Attributes& attrs,
	double& a, double& b, double& c, double& d)
{
	xmlstring tmp(attrs.getValue(xc("a")));
	if (!tmp.c_str()) parse_error(-1);
	a = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("b"));
	if (!tmp.c_str()) parse_error(-2);
	b = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("c"));
	if (!tmp.c_str()) parse_error(-3);
	c = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("d"));
	if (!tmp.c_str()) parse_error(-4);
	d = strtod(tmp, nullptr);
}

odr_elevationProfile::odr_elevationProfile(odr_element* p)
: odr_element(p, en_elevpf, odr_elemtype_elevpf)
, _elevations(4)
{
}

odr_elevationProfile::~odr_elevationProfile()
{
	release_all();
}

void odr_elevationProfile::release()
{
}

void odr_elevationProfile::release_all()
{
	for(int i = 0; i < _elevations.getsize(); ++i) {
		auto* elev = _elevations.buffer()[i];
		delete elev;
	}
}

odr_element* odr_loader::elem_elevpf_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (p->get_type() != odr_elemtype_road) {
		parse_error(-1);
	}
	auto* l = zas_downcast(odr_element, odr_road, p);
	
	auto* ret = new odr_elevationProfile(p);
	if (l->set_elevationprofile(ret)) parse_error(-3);
	return ret;
}

odr_elevation::odr_elevation(odr_element* p, 
	double s, double a, double b, double c,double d)
: odr_element(p, en_elevation, odr_elemtype_elevation)
, _s(s), _a(a), _b(b), _c(c), _d(d) {

}

odr_elevation::~odr_elevation(){
}

void odr_elevation::release() {}

odr_element* odr_loader::elem_elevation_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (p->get_type() != odr_elemtype_elevpf) {
		parse_error(-1);
	}
	auto* elevprf = zas_downcast(odr_element, odr_elevationProfile, p);

	xmlstring tmp(attrs.getValue(xc("s")));
	if (!tmp.c_str()) parse_error(-2);
	double s = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("a"));
	if (!tmp.c_str()) parse_error(-2);
	double a = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("b"));
	if (!tmp.c_str()) parse_error(-2);
	double b = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("c"));
	if (!tmp.c_str()) parse_error(-2);
	double c = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("d"));
	if (!tmp.c_str()) parse_error(-2);
	double d = strtod(tmp, nullptr);

	auto* ret = new odr_elevation(p, s, a, b, c, d);
	if (nullptr == ret) parse_error(-5);

	if (elevprf->add_elevation(ret)) {
		parse_error(-4);
	}

	return ret;
}

odr_lateralProfile::odr_lateralProfile(odr_element* p)
: odr_element(p, en_lateralpf, odr_elemtype_laterlpf)
, _super_elevations(4) 
{

}

odr_lateralProfile::~odr_lateralProfile()
{
	release_all();
}

void odr_lateralProfile::release()
{
}

void odr_lateralProfile::release_all()
{
	for (int i = 0; i < _super_elevations.getsize(); ++i) {
		auto* ls = _super_elevations.buffer()[i];
		delete ls;
	}
}

odr_element* odr_loader::elem_laterlpf_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (p->get_type() != odr_elemtype_road) {
		parse_error(-1);
	}
	auto* l = zas_downcast(odr_element, odr_road, p);
	
	auto* ret = new odr_lateralProfile(p);
	if (l->set_lateralProfile(ret)) parse_error(-3);
	return ret;
}

odr_superelevation::odr_superelevation(odr_element* p, 
	double s, double a, double b, double c,double d)
: odr_element(p, en_superelev, odr_elemtype_superelev)
, _s(s), _a(a), _b(b), _c(c), _d(d) {

}

odr_superelevation::~odr_superelevation(){
}

void odr_superelevation::release() {}

odr_element* odr_loader::elem_superelev_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (p->get_type() != odr_elemtype_laterlpf) {
		parse_error(-1);
	}
	auto* lasterpro = zas_downcast(odr_element, odr_lateralProfile, p);

	xmlstring tmp(attrs.getValue(xc("s")));
	if (!tmp.c_str()) parse_error(-2);
	double s = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("a"));
	if (!tmp.c_str()) parse_error(-2);
	double a = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("b"));
	if (!tmp.c_str()) parse_error(-2);
	double b = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("c"));
	if (!tmp.c_str()) parse_error(-2);
	double c = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("d"));
	if (!tmp.c_str()) parse_error(-2);
	double d = strtod(tmp, nullptr);

	auto* ret = new odr_superelevation(p, s, a, b, c, d);
	if (nullptr == ret) parse_error(-5);

	if (lasterpro->add_super_elevation(ret)) {
		parse_error(-4);
	}

	return ret;
}

odr_lanes::odr_lanes(odr_element* p)
: odr_element(p, en_lanes, odr_elemtype_lanes)
, _lane_sects(4), _lane_offsets(4) {
}

odr_lanes::~odr_lanes()
{
	release_all();
}

void odr_lanes::release(void) {}

void odr_lanes::release_all(void)
{
	for (int i = 0; i < _lane_sects.getsize(); ++i) {
		auto* ls = _lane_sects.buffer()[i];
		delete ls;
	}

	for (int i = 0; i < _lane_offsets.getsize(); ++i) {
		auto* lo = _lane_offsets.buffer()[i];
		delete lo;
	}
}

odr_element* odr_loader::elem_lanes_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (p->get_type() != odr_elemtype_road) {
		parse_error(-1);
	}
	auto* l = zas_downcast(odr_element, odr_road, p);
	
	auto* ret = new odr_lanes(p);
	if (l->set_lanes(ret)) parse_error(-3);
	return ret;
}

odr_lane_section::odr_lane_section(odr_element* p, double s, bool ss)
: odr_element(p, en_lanesection, odr_elemtype_lane_section)
, _s(s), _single_side(ss), _center_lane(nullptr)
, _left_lanes(4), _right_lanes(4), _index(-1)
, _x0(DBL_MAX), _y0(DBL_MAX), _x1(-DBL_MAX), _y1(-DBL_MAX)
, _rendermap_lanesec(nullptr) {
}

odr_lane_section::~odr_lane_section()
{
	release_all();
}

void odr_lane_section::release(void) {}

void odr_lane_section::fix_right_edge(odr_loader* ldr)
{
	int min = 0;
	odr_lane* min_lane = nullptr;
	for (int i = 0; i < _right_lanes.getsize(); ++i) {
		auto* lane = _right_lanes.buffer()[i];
		if (lane->_id < min) {
			min = lane->_id, min_lane = lane;
		}
	}
	if (!min_lane || min_lane->_attrs.type != lane_type_none) {
		// create a new "none" lane as the edge
		min_lane = new odr_lane(this, --min, lane_type_none, false);
		assert(nullptr != min_lane);
		auto holder = _right_lanes.new_object();
		*holder = min_lane;
	}
	ldr->fix_edge_lane(min_lane);
}

void odr_lane_section::fix_left_edge(odr_loader* ldr)
{
	int max = 0;
	odr_lane* max_lane = nullptr;
	for (int i = 0; i < _left_lanes.getsize(); ++i) {
		auto* lane = _left_lanes.buffer()[i];
		if (lane->_id > max) {
			max = lane->_id, max_lane = lane;
		}
	}
	if (!max_lane || max_lane->_attrs.type != lane_type_none) {
		// create a new "none" lane as the edge
		max_lane = new odr_lane(this, ++max, lane_type_none, false);
		assert(nullptr != max_lane);
		auto holder = _left_lanes.new_object();
		*holder = max_lane;
	}
	ldr->fix_edge_lane(max_lane);
}

void odr_loader::fix_edge_lane(odr_lane* lane)
{
	assert(lane->_attrs.type == lane_type_none);

	// mark this lane as a curb specifically for rendermap
	// this means the lane will not be handled in
	// hdmap generation
	lane->_attrs.rendermap_curb = 1;

	auto mapinfo = _psr.get_hdmap_info();
	if (lane->_widths.getsize() <= 1)
	{
		auto width = new odr_width(lane, 0.,
			mapinfo->roadedge.width, 0., 0., 0.);
		assert(nullptr != width);
		lane->_widths.clear();
		lane->add_width(width);
	}

	auto height = new odr_height(lane, 0.,
		0., mapinfo->roadedge.height);
	assert(nullptr != height);
	lane->_heights.clear();
	lane->add_height(height);
}

void odr_lane_section::on_after_poped(odr_loader* ldr)
{
	// fix left and right edge of the road
	fix_left_edge(ldr);
	fix_right_edge(ldr);

	qsort(_left_lanes.buffer(), _left_lanes.getsize(),
		sizeof(odr_lane*), odr_lane::qst_id_asc_compare);
	qsort(_right_lanes.buffer(), _right_lanes.getsize(),
		sizeof(odr_lane*), odr_lane::qst_id_dsc_compare);
}

void odr_lane_section::release_all(void)
{
	for (int i = 0; i < _left_lanes.getsize(); ++i) {
		auto* lane = _left_lanes.buffer()[i];
		delete lane;
	}
	for (int i = 0; i < _right_lanes.getsize(); ++i) {
		auto* lane = _right_lanes.buffer()[i];
		delete lane;
	}
	if (_center_lane) delete _center_lane;
}

odr_element* odr_loader::elem_lanesection_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (p->get_type() != odr_elemtype_lanes) {
		parse_error(-1);
	}
	auto* lanes = zas_downcast(odr_element, odr_lanes, p);

	xmlstring tmp(attrs.getValue(xc("s")));
	if (!tmp.c_str()) parse_error(-2);
	double s = strtod(tmp, nullptr);

	bool ss = false;
	if (ldr->parse_bool(attrs, xc("singleSide"), ss) < -1) {
		parse_error(-3);
	}

	auto* ret = new odr_lane_section(p, s, ss);
	if (!ret || lanes->add_lane_section(ret)) {
		parse_error(-4);
	}

	++ldr->_lane_section_count;
	return ret;
}

int odr_loader::parse_bool(const Attributes& attrs, const XMLCh* t, bool& b)
{
	xmlstring tmp(attrs.getValue(t));
	if (!tmp.c_str()) {
		return -1;
	}

	if (!strcmp(tmp, "true") || !strcmp(tmp, "1")) {
		b = true; return 0;
	}
	else if (!strcmp(tmp, "false") || !strcmp(tmp, "0")) {
		b = false; return 0;
	}
	return -2;
}

odr_lane::odr_lane(odr_element* p, int id, int type, bool level)
: odr_element((strcmp(p->name(), en_lanesection))
? p->get_parents() : p, en_lane, odr_elemtype_lane)
, _roadmarks(4), _widths(4), _heights(4)
, _id(id), _prev(0), _next(0), _speed(nullptr)
{
	assert(!strcmp(p->name(), en_center)
		|| !strcmp(p->name(), en_left)
		|| !strcmp(p->name(), en_right)
		|| !strcmp(p->name(), en_lanesection));
	memset(&_attrs, 0, sizeof(_attrs));
	_attrs.type = type;
	_attrs.level = (level) ? 1 : 0;
}

odr_lane::~odr_lane()
{
	release_all();
}

void odr_lane::release(void) {}

void odr_lane::release_all(void)
{
	for (int i = 0; i < _widths.getsize(); ++i) {
		auto* w = _widths.buffer()[i];
		delete w;
	}
	for (int i = 0; i < _roadmarks.getsize(); ++i) {
		auto* rm = _roadmarks.buffer()[i];
		delete rm;
	}
	for (int i = 0; i < _heights.getsize(); ++i) {
		auto* h = _heights.buffer()[i];
		delete h;
	}
	if( _speed) {
		delete _speed;
		_speed = nullptr;
	}
}

int odr_lane::qst_id_asc_compare(const void* a, const void* b)
{
	auto* aa = *((odr_lane**)a);
	auto* bb = *((odr_lane**)b);
	if (aa->_id > bb->_id) return 1;
	else if (aa->_id < bb->_id) return -1;
	else return 0;
}

int odr_lane::qst_id_dsc_compare(const void* a, const void* b)
{
	auto* aa = *((odr_lane**)a);
	auto* bb = *((odr_lane**)b);
	if (aa->_id > bb->_id) return -1;
	else if (aa->_id < bb->_id) return 1;
	else return 0;
}

odr_element* odr_loader::elem_lane_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	xmlstring tmp(attrs.getValue(xc("id")));
	if (!tmp.c_str()) parse_error(-1);
	int id = strtol(tmp, nullptr, 10);

	tmp = attrs.getValue(xc("type"));
	if (!tmp.c_str()) parse_error(-2);
	int lane_type = parse_lane_type(tmp);
	if (lane_type_unknown == lane_type) {
		parse_error(-3);
	}

	bool level;
	if (ldr->parse_bool(attrs, xc("level"), level)) {
		parse_error(-4);
	}

	auto* ret = new odr_lane(p, id, lane_type, level);
	if (nullptr == ret) parse_error(-5);

	auto* e = p->get_parents();
	if (e->get_type() != odr_elemtype_lane_section) {
		parse_error(-6);
	}
	auto* ls = zas_downcast(odr_element, odr_lane_section, e);

	if (p->same_type(en_center)) {
		if (ls->center_lane(ret)) parse_error(-7);
	}
	else if (p->same_type(en_right)) {
		if (ls->add_lane(ret, true)) parse_error(-8);
	}
	else if (p->same_type(en_left)) {
		if (ls->add_lane(ret, false)) parse_error(-9);
	}
	else parse_error(-10);
	return ret;
}

odr_roadmark::odr_roadmark(odr_element* p, double soff,
	double width, double height, int lanechg, string marktype,
	string weight, string color)
: odr_element(p, en_roadmark, odr_elemtype_roadmark)
, _s_offset(soff), _width(width), _height(height)
, _lane_change(lanechg), _mark_type(marktype)
, _weight(weight), _color(color), _lines(4) {
}

odr_roadmark::~odr_roadmark()
{
	release_all();
}

void odr_roadmark::release(void) {}

void odr_roadmark::release_all(void)
{
	for (int i = 0; i < _lines.getsize(); ++i) {
		auto* l = _lines.buffer()[i];
		delete l;
	}
}

odr_element* odr_loader::elem_roadmark_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (p->get_type() != odr_elemtype_lane) {
		parse_error(-1);
	}
	auto* lane = zas_downcast(odr_element, odr_lane, p);

	xmlstring tmp(attrs.getValue(xc("sOffset")));
	if (!tmp.c_str()) parse_error(-2);
	double s_off = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("type"));
	string marktype = tmp.c_str();
	tmp = attrs.getValue(xc("weight"));
	string weight = tmp.c_str();
	tmp = attrs.getValue(xc("color"));
	string color = tmp.c_str();

	tmp = attrs.getValue(xc("width"));
	if (!tmp.c_str()) parse_error(-3);
	double width = strtod(tmp, nullptr);

	int lane_change = rm_lanechg_type_unknown;
	tmp = attrs.getValue(xc("laneChange"));
	if (tmp.c_str()) {
		if (!strcmp(tmp, "none")) {
			lane_change = rm_lanechg_type_none;
		} else if (!strcmp(tmp, "both")) {
			lane_change = rm_lanechg_type_both;
		} else if (!strcmp(tmp, "increase")) {
			lane_change = rm_lanechg_type_increase;
		} else if (!strcmp(tmp, "decrease")) {
			lane_change = rm_lanechg_type_decrease;
		}// todo:
		else parse_error(-4);
	}
	
	double height = 0.;
	tmp = attrs.getValue(xc("height"));
	if (tmp.c_str()) {
		height = strtod(tmp, nullptr);
	}

	auto* ret = new odr_roadmark(p, s_off, width, height, lane_change, 
		marktype, weight, color);
	if (lane->add_roadmark(ret)) parse_error(-7);
	return ret;
}

odr_line::odr_line(odr_element* p, const Attributes& attrs)
: odr_element(p, en_line, odr_elemtype_line)
, _width(0.)
{
	xmlstring tmp(attrs.getValue(xc("length")));
	if (!tmp.c_str()) parse_error(-1);
	_length = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("space"));
	if (!tmp.c_str()) parse_error(-2);
	_space = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("tOffset"));
	if (!tmp.c_str()) parse_error(-3);
	_t_offset = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("sOffset"));
	if (!tmp.c_str()) parse_error(-4);
	_s_offset = strtod(tmp, nullptr);

	tmp = attrs.getValue(xc("width"));
	if (tmp.c_str()) _width = strtod(tmp, nullptr);

	memset(&_attrs, 0, sizeof(_attrs));
	// todo: parse the rule
}

void odr_line::release(void) {}

odr_laneoffset::odr_laneoffset(odr_element* p, double s, double a,
	double b, double c, double d)
: odr_element(p, en_laneoffset, odr_elemtype_laneoffset)
, _s(s), _a(a), _b(b), _c(c), _d(d)
{
}

odr_laneoffset::~odr_laneoffset()
{
}

void odr_laneoffset::release(void) {}

odr_element* odr_loader::elem_laneoffset_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (p->get_type() != odr_elemtype_lanes) {
		parse_error(-1);
	}
	auto* l = zas_downcast(odr_element, odr_lanes, p);

	xmlstring tmp(attrs.getValue(xc("s")));
	if (!tmp.c_str()) parse_error(-2);
	double s = strtod(tmp, nullptr);

	double a, b, c, d;
	ldr->parse_abcd(attrs, a, b, c, d);

	auto* ret = new odr_laneoffset(p, s, a, b, c, d);
	if (l->add_laneoffset(ret)) {
		parse_error(-3);
	}
	return ret;
}

odr_width::odr_width(odr_element* p, double soff, double a,
	double b, double c, double d)
: odr_element(p, en_width, odr_elemtype_width)
, _s_offset(soff), _a(a), _b(b), _c(c), _d(d)
{
}

void odr_width::release(void) {}

bool odr_width::is_zero(void)
{
	if (fabs(_a) < 1e-5 && fabs(_b) < 1e-5
		&& fabs(_c) < 1e-5 && fabs(_d) < 1e-5) {
		return true;
	}
	return false;
}

odr_element* odr_loader::elem_width_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (p->get_type() != odr_elemtype_lane) {
		parse_error(-1);
	}
	auto* l = zas_downcast(odr_element, odr_lane, p);

	xmlstring tmp(attrs.getValue(xc("sOffset")));
	if (!tmp.c_str()) parse_error(-2);
	double soff = strtod(tmp, nullptr);

	double a, b, c, d;
	ldr->parse_abcd(attrs, a, b, c, d);

	auto* ret = new odr_width(p, soff, a, b, c, d);
	if (l->add_width(ret)) parse_error(-3);
	return ret;
}

odr_height::odr_height(odr_element* p, double soff,
	double inner, double outer)
: odr_element(p, en_height, odr_elemtype_height)
, _s_offset(soff), _inner(inner), _outer(outer)
{
}

void odr_height::release(void) {}

odr_element* odr_loader::elem_height_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (p->get_type() != odr_elemtype_lane) {
		parse_error(-1);
	}
	auto* l = zas_downcast(odr_element, odr_lane, p);

	xmlstring tmp(attrs.getValue(xc("sOffset")));
	if (!tmp.c_str()) parse_error(-2);
	double soff = strtod(tmp, nullptr);

	double inner = 0.;
	tmp = attrs.getValue(xc("heightInner"));
	if (tmp.c_str()) {
		inner = strtod(tmp, nullptr);
	}

	double outer = 0.;
	tmp = attrs.getValue(xc("heightOuter"));
	if (tmp.c_str()) {
		outer = strtod(tmp, nullptr);
	}

	auto* ret = new odr_height(p, soff, inner, outer);
	if (l->add_height(ret)) parse_error(-3);
	return ret;
}

odr_speed::odr_speed(odr_element* p, double soff, double max, 
		const XMLCh* unit)
: odr_element(p, en_speed, odr_elemtype_speed)
, _s_offset(soff), _max(max), _unit(unit)
{
}

odr_speed::~odr_speed()
{
	release();
}

void odr_speed::release(void) {}

odr_element* odr_loader::elem_speed_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (p->get_type() != odr_elemtype_lane) {
		parse_error(-1);
	}
	auto* l = zas_downcast(odr_element, odr_lane, p);

	xmlstring tmp(attrs.getValue(xc("sOffset")));
	if (!tmp.c_str()) parse_error(-2);
	double soff = strtod(tmp, nullptr);

	double max = 0.;
	tmp = attrs.getValue(xc("max"));
	if (tmp.c_str()) {
		max = strtod(tmp, nullptr);
	}

	const XMLCh* unit = attrs.getValue(xc("unit"));

	auto* ret = new odr_speed(p, soff, max, unit);
	if (l->set_speed(ret)) parse_error(-3);
	return ret;
}

odr_objects::odr_objects(odr_element* p) 
: odr_element(p, en_objects, odr_elemtype_objects)
, _objects(4)
{
}

odr_objects::~odr_objects()
{
	release_all();
}

void odr_objects::release(void) {}

void odr_objects::release_all(void) 
{
	// release all object
	for (int i = 0; i < _objects.getsize(); ++i) {
		auto* object = _objects.buffer()[i];
		delete object;
	}
}

odr_element* odr_loader::elem_objects_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (p->get_type() != odr_elemtype_road) {
		parse_error(-1);
	}
	auto* l = zas_downcast(odr_element, odr_road, p);
	
	auto* ret = new odr_objects(p);
	if (l->set_objects(ret)) parse_error(-3);
	return ret;
}

odr_element* odr_loader::elem_signals_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (p->get_type() != odr_elemtype_road) {
		parse_error(-1);
	}
	auto* l = zas_downcast(odr_element, odr_road, p);
	
	// auto* ret = new odr_objects(p);
	auto* ret = l->get_objects();
	ret->_element_name = en_signals;
	// if (l->set_objects(ret)) parse_error(-3);
	return ret;
}

odr_object::odr_object(odr_element* p, uint64_t id, const char* name)
: odr_element(p, en_object, odr_elemtype_object)
, _id(id), _type(object_type_none), _s(0.), _t(0.), _z_offset(0.)
, _valid_length(0.), _name(name), _hdg(0.), _pitch(0.), _roll(0.)
, _height(0.), _length(0.), _width(0.), _radius(0.), _outline(nullptr)
, _x0(DBL_MAX), _y0(DBL_MAX), _x1(-DBL_MAX), _y1(-DBL_MAX)
, _rendermap_roadobj(nullptr) {
}

odr_object::odr_object(odr_element* p, uint64_t id, uint32_t type, 
	double s, double t, double zoff, double validlength, 
	const XMLCh* orientation, const XMLCh* subtype, const XMLCh* name, 
	const XMLCh* dynamic, double hdg, double pitch, double roll, 
	double height, double length, double width, double radius)
: odr_element(p, en_object, odr_elemtype_object)
, _id(id), _type(type), _s(s), _t(t), _z_offset(zoff), _valid_length(validlength)
, _orientation(xmlstring(orientation).c_str()), _subtype(subtype)
, _name(xmlstring(name).c_str()), _dynamic(dynamic)
, _hdg(hdg), _pitch(pitch), _roll(roll), _height(height), _length(length)
, _width(width), _radius(radius), _outline(nullptr)
, _x0(DBL_MAX), _y0(DBL_MAX), _x1(-DBL_MAX), _y1(-DBL_MAX)
, _rendermap_roadobj(nullptr)
{
}

odr_object::~odr_object()
{
}

void odr_object::release(void) {}

odr_element* odr_loader::elem_object_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (p->get_type() != odr_elemtype_objects) {
		parse_error(-1);
	}
	auto* l = zas_downcast(odr_element, odr_objects, p);

	xmlstring tmp(attrs.getValue(xc("s")));
	if (!tmp.c_str()) parse_error(-2);
	double s = strtod(tmp, nullptr);

	double t = 0.;
	tmp = attrs.getValue(xc("t"));
	if (tmp.c_str()) {
		t = strtod(tmp, nullptr);
	}

	double zOffset = 0.;
	tmp = attrs.getValue(xc("zOffset"));
	if (tmp.c_str()) {
		zOffset = strtod(tmp, nullptr);
	}

	tmp = attrs.getValue(xc("type"));
	if (!tmp.c_str()) parse_error(-2);
	int object_type = parse_object_type(tmp);
	if (object_type_unknown == object_type) {
		parse_error(-3);
	}

	double validlength = 0.;
	tmp = attrs.getValue(xc("validLength"));
	if (tmp.c_str()) {
		validlength = strtod(tmp, nullptr);
	}

	const XMLCh* orientation = attrs.getValue(xc("orientation"));
	const XMLCh* subtype = attrs.getValue(xc("subtype"));
	const XMLCh* name = attrs.getValue(xc("name"));
	const XMLCh* dynamic = attrs.getValue(xc("dynamic"));

	double hdg = 0.;
	tmp = attrs.getValue(xc("hdg"));
	if (tmp.c_str()) {
		hdg = strtod(tmp, nullptr);
	}

	double pitch = 0.;
	tmp = attrs.getValue(xc("pitch"));
	if (tmp.c_str()) {
		pitch = strtod(tmp, nullptr);
	}

	tmp = attrs.getValue(xc("id"));
	if (!tmp.c_str()) parse_error(-1);
	uint64_t id = stoull(tmp.c_str(), nullptr, 10);

	double roll = 0.;
	tmp = attrs.getValue(xc("roll"));
	if (tmp.c_str()) {
		roll = strtod(tmp, nullptr);
	}

	double height = 0.;
	tmp = attrs.getValue(xc("height"));
	if (tmp.c_str()) {
		height = strtod(tmp, nullptr);
	}

	double length = 0.;
	tmp = attrs.getValue(xc("length"));
	if (tmp.c_str()) {
		length = strtod(tmp, nullptr);
	}

	double width = 0.;
	tmp = attrs.getValue(xc("width"));
	if (tmp.c_str()) {
		width = strtod(tmp, nullptr);
	}

	double radius = 0.;
	tmp = attrs.getValue(xc("radius"));
	if (tmp.c_str()) {
		radius = strtod(tmp, nullptr);
	}

	auto* ret = new odr_object(p, id, object_type, s, t, zOffset, 
		validlength, orientation, subtype, name, dynamic, hdg, 
		pitch, roll, height, length, width, radius);
	if (l->add_object(ret)) parse_error(-3);
	ldr->_map.set_road_object(ret);
	return ret;
}

odr_element* odr_loader::elem_signal_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (p->get_type() != odr_elemtype_objects) {
		parse_error(-1);
	}
	auto* l = zas_downcast(odr_element, odr_objects, p);

	xmlstring tmp(attrs.getValue(xc("s")));
	if (!tmp.c_str()) parse_error(-2);
	double s = strtod(tmp, nullptr);

	double t = 0.;
	tmp = attrs.getValue(xc("t"));
	if (tmp.c_str()) {
		t = strtod(tmp, nullptr);
	}

	double zOffset = 0.;
	tmp = attrs.getValue(xc("zOffset"));
	if (tmp.c_str()) {
		zOffset = strtod(tmp, nullptr);
	}

	int object_type = parse_object_type("none");
	if (object_type_unknown == object_type) {
		parse_error(-3);
	}

	double validlength = 0.;

	const XMLCh* orientation = attrs.getValue(xc("orientation"));
	const XMLCh* subtype = attrs.getValue(xc("subtype"));
	const XMLCh* name = xc("signalLight");
	const XMLCh* dynamic = attrs.getValue(xc("dynamic"));

	double hdg = 0.;

	double pitch = 0.;

	tmp = attrs.getValue(xc("id"));
	if (!tmp.c_str()) parse_error(-1);
	uint64_t id = stoull(tmp.c_str(), nullptr, 10);

	double roll = 0.;
	tmp = attrs.getValue(xc("roll"));
	if (tmp.c_str()) {
		roll = strtod(tmp, nullptr);
	}

	double height = 0.;
	tmp = attrs.getValue(xc("height"));
	if (tmp.c_str()) {
		height = strtod(tmp, nullptr);
	}

	double length = 0.;

	double width = 0.;
	tmp = attrs.getValue(xc("width"));
	if (tmp.c_str()) {
		width = strtod(tmp, nullptr);
	}

	double radius = 0.;

	auto* ret = new odr_object(p, id, object_type, s, t, zOffset, 
		validlength, orientation, subtype, name, dynamic, hdg, 
		pitch, roll, height, length, width, radius);
	ret->_element_name = en_signal;
	// if (l->add_object(ret)) parse_error(-3);
	ldr->_map.set_road_object(ret);
	return ret;
}

odr_element* odr_loader::elem_posroad_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs) {
	if (p->get_type() != odr_elemtype_object) {
		parse_error(-1);
	}
	auto* sinobj = zas_downcast(odr_element, odr_object, p);

	xmlstring tmp(attrs.getValue(xc("roadId")));
	if (!tmp.c_str()) parse_error(-2);
	long roadId = strtol(tmp, nullptr, 0);

	double s = 0.;
	tmp = attrs.getValue(xc("s"));
	if (tmp.c_str()) {
		s = strtod(tmp, nullptr);
		sinobj->_s = s;
	}

	double t = 0.;
	tmp = attrs.getValue(xc("t"));
	if (tmp.c_str()) {
		t = strtod(tmp, nullptr);
		sinobj->_t = t;
	}

	double zOffset = 0.;
	tmp = attrs.getValue(xc("zOffset"));
	if (tmp.c_str()) {
		zOffset = strtod(tmp, nullptr);
		sinobj->_z_offset = zOffset;
	}
	if (ldr->_odr_sinlightmap.find(roadId) == ldr->_odr_sinlightmap.end()) {
		set<odr_object*> objs;
		objs.insert(sinobj);
		ldr->_odr_sinlightmap.insert({roadId, objs});
	}
	else {
		ldr->_odr_sinlightmap[roadId].insert(sinobj);
	}
	
	auto* ret = new odr_posroad(p);
	return ret; 
}

odr_posroad::odr_posroad(odr_element* p)
: odr_element(p, en_posroad, odr_elemtype_posroad)
{
}

odr_posroad::~odr_posroad()
{
}

odr_outline::odr_outline(odr_element* p) 
: odr_element(p, en_outline, odr_elemtype_outline)
, _cornerlocals(8)
{	
}

odr_outline::~odr_outline()
{
	release_all();
}

void odr_outline::release(void) {}

void odr_outline::release_all(void) 
{
	// release all object
	for (int i = 0; i < _cornerlocals.getsize(); ++i) {
		auto* local = _cornerlocals.buffer()[i];
		delete local;
	}
}

odr_element* odr_loader::elem_outline_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (p->get_type() != odr_elemtype_object) {
		parse_error(-1);
	}
	auto* obj = zas_downcast(odr_element, odr_object, p);
	auto* ret = new odr_outline(p);
	if (obj->set_outline(ret)) parse_error(-3);
	return ret;
}

odr_cornerlocal::odr_cornerlocal(odr_element*p, double u, double v, double z, 
	double height)
: odr_element(p, en_cornerlocal, odr_elemtype_cornerlocal)
, _u(u), _v(v), _z(z), _height(height)
{
}

odr_cornerlocal::~odr_cornerlocal() {}

void odr_cornerlocal::release(void) {}

odr_element* odr_loader::elem_cornerlocal_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (p->get_type() != odr_elemtype_outline) {
		parse_error(-1);
	}
	auto* l = zas_downcast(odr_element, odr_outline, p);

	double u = 0.;
	xmlstring tmp(attrs.getValue(xc("u")));
	if (tmp.c_str()){
		u = strtod(tmp, nullptr);
	} 

	double v = 0.;
	tmp = attrs.getValue(xc("v"));
	if (tmp.c_str()) {
		v = strtod(tmp, nullptr);
	}

	double z = 0.;
	tmp = attrs.getValue(xc("z"));
	if (tmp.c_str()) {
		z = strtod(tmp, nullptr);
	}

	double height = 0.;
	tmp = attrs.getValue(xc("height"));
	if (tmp.c_str()) {
		height = strtod(tmp, nullptr);
	}

	auto* ret = new odr_cornerlocal(p, u, v, z, height);
	if (l->add_cornerlocal(ret)) parse_error(-3);
	return ret;
}

odr_junction::odr_junction(odr_element* p, uint32_t id)
: odr_element(p, en_junction, odr_elemtype_junction)
, _id(id), _connections(4), _rendermap_juncoff(0)
, _container(nullptr), _approach(4)
, _x0(DBL_MAX), _y0(DBL_MAX), _x1(-DBL_MAX), _y1(-DBL_MAX)
{
	listnode_init(_ownerlist);
	_odrv_junction = std::make_shared<Junction>();
}

odr_junction::~odr_junction()
{
	release_all();
}

void odr_junction::release(void)
{
	if (!listnode_isempty(_ownerlist)) {
		return;
	}
	odr_element::release();
}

void odr_junction::on_after_poped(odr_loader* ldr)
{
	ltoa(_id, _odrv_junction->id);
	
	for(int i = 0;i < _connections.getsize(); i++) {
		auto* conn = _connections.buffer()[i];
		JunctionConnection jc;
		ltoa(conn->_id,jc.id);
		ltoa(conn->_id,jc.incoming_road);
		ltoa(conn->_id,jc.connecting_road);
		if(conn->_a.conn_point == 1) {
			jc.contact_point = JunctionConnection::ContactPoint::Start;
		} else if (conn->_a.conn_point == 2) {
			jc.contact_point = JunctionConnection::ContactPoint::End;
		} else {
			jc.contact_point = JunctionConnection::ContactPoint::None;
		}

		for(int j = 0; j < conn->_lanelinks.getsize(); j++) {
			auto* lanelink = conn->_lanelinks.buffer()[j];
			JunctionLaneLink jll;
			jll.from = lanelink->_from;
			jll.to = lanelink->_to;
			jc.lane_links.push_back(jll);
		}
		
		_odrv_junction->connections.insert(map<std::string, JunctionConnection>::value_type(jc.id, jc));
	}

	auto& map = ldr->getmap();
	if (!map.junction_exists_and_merged(this)) {
		map.add_junction(this);
	}
}

void odr_junction::release_all(void)
{
	// release all object
	for (int i = 0; i < _connections.getsize(); ++i) {
		auto* conn = _connections.buffer()[i];
		if (conn) delete conn;
	}
	for (int i = 0; i < _approach.getsize(); ++i) {
		auto* appro = _approach.buffer()[i];
		delete appro;
	}
}

int odr_junction::avl_id_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(odr_junction, _avlnode, a);
	auto* bb = AVLNODE_ENTRY(odr_junction, _avlnode, b);
	if (aa->_id > bb->_id) return 1;
	else if (aa->_id < bb->_id) return -1;
	else return 0;
}

odr_element* odr_loader::elem_junction_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	uint32_t id = 0;
	xmlstring tmp(attrs.getValue(xc("id")));
	if (!tmp.c_str()) parse_error(-1);
	id = strtol(tmp, nullptr, 10);
	
	auto* ret = new odr_junction(p, id);
	return ret;
}

odr_junction_connection::odr_junction_connection(odr_element* p, uint32_t id, 
	const XMLCh* type, int32_t incomingroad, int32_t connectingroad, 
	const XMLCh* contactpoint)
: odr_element(p, en_junction_connection, odr_elemtype_junction_connection)
, _id(id), _type(type), _incoming_road(incomingroad), _connecting_road(connectingroad)
, _attrs(0), _lanelinks(4)
{
	xmlstring tmp(contactpoint);
	if (!tmp.c_str()) {
		parse_error(-1);
	}
	if (!strcmp(tmp, "start")) {
		_a.conn_point = rl_connpoint_start;
	}
	else if (!strcmp(tmp, "end")) {
		_a.conn_point = rl_connpoint_end;
	}
	// todo:
	else parse_error(-2);
}

odr_junction_connection::~odr_junction_connection() 
{
	release_all();
}

void odr_junction_connection::release() {}

void odr_junction_connection::release_all(void)
{
	for (int i = 0; i < _lanelinks.getsize(); ++i) {
		auto* link = _lanelinks.buffer()[i];
		if (link) delete link;
	}
}

odr_element* odr_loader::elem_junction_connection_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (p->get_type() != odr_elemtype_junction) {
		parse_error(-1);
	}
	auto* l = zas_downcast(odr_element, odr_junction, p);

	uint32_t id = 0;
	xmlstring tmp(attrs.getValue(xc("id")));
	if (!tmp.c_str()) parse_error(-1);
	id = strtol(tmp, nullptr, 10);

	const XMLCh* type = attrs.getValue(xc("type"));

	int32_t incomingroad;
	tmp = attrs.getValue(xc("incomingRoad"));
	if (tmp.c_str()) {
		incomingroad = strtol(tmp, nullptr, 10);
	}

	int32_t connectingroad;
	tmp = attrs.getValue(xc("connectingRoad"));
	if (tmp.c_str()) {
		connectingroad = strtol(tmp, nullptr, 10);
	}

	const XMLCh* contactPoint = attrs.getValue(xc("contactPoint"));

	auto* ret = new odr_junction_connection(p, id, type, 
		incomingroad, connectingroad, contactPoint);
	if (l->add_connection(ret)) parse_error(-3);
	return ret;
}

odr_junction_lanelink::odr_junction_lanelink(odr_element* p, int32_t from, int32_t to)
: odr_element(p, en_junction_lanelink, odr_elemtype_junction_lanelink)
, _from(from), _to(to)
{
}

void odr_junction_lanelink::release(void) {}

odr_element* odr_loader::elem_junction_lanelink_hdr(odr_loader* ldr,
	const char* en, odr_element* p, const Attributes& attrs)
{
	if (p->get_type() != odr_elemtype_junction_connection) {
		parse_error(-1);
	}
	auto* l = zas_downcast(odr_element, odr_junction_connection, p);

	int32_t from;
	xmlstring tmp(attrs.getValue(xc("from")));
	if (tmp.c_str()) {
		from = strtol(tmp, nullptr, 10);
	}

	int32_t to;
	tmp = attrs.getValue(xc("to"));
	if (tmp.c_str()) {
		to = strtol(tmp, nullptr, 10);
	}

	auto* ret = new odr_junction_lanelink(p, from, to);
	if (l->add_lanelink(ret)) parse_error(-3);
	return ret;
}

} // end of namespace osm_parser1
/* EOF */
