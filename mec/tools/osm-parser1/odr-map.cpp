#include <map>
#include <stdio.h>
#include <float.h>
#include "utils/json.h"
#include "utils/uuid.h"
#include "utils/dir.h"
#include "utils/timer.h"
#include "odr-map.h"
#include "odr-loader.h"
#include "odr-extdata.h"
#include "kdtree.h"

namespace osm_parser1 {

#define odr_map_check(expr) \
do {	\
	if (expr) break;	\
	fprintf(stderr, "odr-map: error at %s <ln:%u>: \"%s\", abort!\n", __FILE__, __LINE__, #expr);	\
	__asm__("int $0x03");	\
	exit(113);	\
} while (0)

// odr-map
odr_map::odr_map(cmdline_parser& psr)
:_psr(psr), _idcount(1)
, _odr_road_idtree(nullptr)
, _odr_junction_idtree(nullptr)
, _x_offs(0.), _y_offs(0.)
, _xoffset(0.), _yoffset(0.)
, _geometry_count(1)
, _x0(DBL_MAX), _y0(DBL_MAX), _x1(-DBL_MAX), _y1(-DBL_MAX)
, _hdmap(nullptr), _hdm_road_count(0), _hdm_junc_count(0)
, _rows(0), _cols(0), _rblks(nullptr)
, _rendermap(nullptr)
, _rendermap_fp(nullptr)
, _last_objectid(0)
, _hfm(nullptr)
{
	listnode_init(_odr_roads);
	listnode_init(_hdm_roads);
	listnode_init(_hdm_juncs);
	listnode_init(_odr_junctions);
}

odr_map::~odr_map()
{
	release_all();
	if (_rendermap_fp) {
		fclose(_rendermap_fp);
		_rendermap_fp = nullptr;
	}
	if (_rblks) {
		delete [] _rblks;
		_rblks = nullptr;
	}
}
static proj *_s_proj = nullptr;
static double _s_xoffset = 0.0;
static double _s_yoffset = 0.0;

void odr_map::set_georef(const char* s)
{
	assert(s && *s);
	if (_georef.size() && _georef != s) {
		fprintf(stderr, "georef not aligned and overwrite to '%s'\n", s);
	}
	_georef = s;
	_wgs84prj.reset(GREF_WGS84, s);
	if (nullptr == _s_proj) {
		_s_proj = new proj(GREF_WGS84, s);
	}
}

void odr_map::set_mapoffset(double dx, double dy)
{
	_xoffset = dx;
	_yoffset = dy;
	_s_xoffset = _xoffset;
	_s_yoffset = _yoffset;
}

void odr_map::release_all(void)
{
	// todo: release
	while (!listnode_isempty(_odr_roads)) {
		auto* r = list_entry( \
			odr_road, _ownerlist, _odr_roads.next);
		listnode_del(r->_ownerlist);
		delete r;
	}
}

odr_loader* odr_map::getloader(void)
{
	return zas_ancestor(odr_loader, _map);
}

int odr_map::add_road(odr_road* road)
{
	if (!listnode_isempty(road->_ownerlist)) {
		return -1;
	}

	listnode_add(_odr_roads, road->_ownerlist);
	int ret = avl_insert(&_odr_road_idtree, &road->_avlnode,
		odr_road::avl_id_compare);
	assert(ret == 0);

	return 0;
}

int odr_map::road_similar_check(odr_road* old, odr_road* _new)
{
	if (!old->_name.equal_to(_new->_name) ||
		old->_id != _new->_id || old->_length != _new->_length ||
		old->_junction_id != _new->_junction_id) {
		return -1;
	}

	if (old->_prev.attrs && _new->_prev.attrs) {
		if (old->_prev.attrs != _new->_prev.attrs
			|| old->_prev.elem_id != _new->_prev.elem_id) {
			return -2;
		}
	}
	if (old->_next.attrs && _new->_next.attrs) {
		if (old->_next.attrs != _new->_next.attrs
			|| old->_next.elem_id != _new->_next.elem_id) {
			return -3;
		}
	}

	// check lanes
	auto old_lanes = old->_lanes, new_lanes = _new->_lanes;
	assert(nullptr != old_lanes && nullptr != new_lanes);
	if (old_lanes->_lane_sects.getsize() != new_lanes->_lane_sects.getsize()) {
		return -3;
	}
	for (int i = 0; i < old_lanes->_lane_sects.getsize(); ++i) {
		auto ol = old_lanes->_lane_sects.buffer()[i];
		auto nl = new_lanes->_lane_sects.buffer()[i];
		if (ol->_left_lanes.getsize() != nl->_left_lanes.getsize()) {
			return -4;
		}
		if (ol->_right_lanes.getsize() != nl->_right_lanes.getsize()) {
			return -5;
		}
	}

	// check objects
	if (old->_objects->_objects.getsize() != _new->_objects->_objects.getsize()) {
		return -6;
	}
	return 0;
}

int odr_map::merge_similar_road(odr_road* old, odr_road* _new)
{
	// merge "predecessor" and "successor"
	if (!old->_prev.attrs && _new->_prev.attrs) {
		old->_prev.attrs = _new->_prev.attrs;
		old->_prev.elem_id = _new->_prev.elem_id;
	}
	if (!old->_next.attrs && _new->_prev.attrs) {
		old->_next.attrs = _new->_next.attrs;
		old->_next.elem_id = _new->_next.elem_id;
	}

	// merge "predecessor" and "successor" for all lanes
	auto olanes = old->_lanes, nlanes = _new->_lanes;
	for (int i = 0; i < olanes->_lane_sects.getsize(); ++i) {
		auto ol = olanes->_lane_sects.buffer()[i];
		auto nl = nlanes->_lane_sects.buffer()[i];
		// left lanes
		for (int j = 0; j < ol->_left_lanes.getsize(); ++j) {
			auto olane = ol->_left_lanes.buffer()[j];
			if (olane->_attrs.rendermap_curb) {
				continue;
			}
			bool matched = false;
			for (int k = 0; k < nl->_left_lanes.getsize(); ++k) {
				auto nlane = nl->_left_lanes.buffer()[k];
				if (nlane->_id == olane->_id) {
					if (!olane->_attrs.has_prev && nlane->_attrs.has_prev) {
						olane->_attrs.has_prev = 1;
						olane->_prev = nlane->_prev;
					}
					if (!olane->_attrs.has_next && nlane->_attrs.has_next) {
						olane->_attrs.has_next = 1;
						olane->_next = nlane->_next;
					}
					matched = true; break;
				}
			}
			odr_map_check(matched != false);
		}
		// right lanes
		for (int j = 0; j < ol->_right_lanes.getsize(); ++j) {
			auto olane = ol->_right_lanes.buffer()[j];
			if (olane->_attrs.rendermap_curb) {
				continue;
			}
			bool matched = false;
			for (int k = 0; k < nl->_right_lanes.getsize(); ++k) {
				auto nlane = nl->_right_lanes.buffer()[k];
				if (nlane->_id == olane->_id) {
					if (!olane->_attrs.has_prev && nlane->_attrs.has_prev) {
						olane->_attrs.has_prev = 1;
						olane->_prev = nlane->_prev;
					}
					if (!olane->_attrs.has_next && nlane->_attrs.has_next) {
						olane->_attrs.has_next = 1;
						olane->_next = nlane->_next;
					}
					matched = true; break;
				}
			}
			odr_map_check(matched != false);
		}
	}
	return 0;
}

bool odr_map::road_exists_and_merged(odr_road* road)
{
	auto node = avl_find(_odr_road_idtree, &road->_avlnode,
		odr_road::avl_id_compare);
	if (nullptr == node) {
		return false;
	}

	// the road is exists, do some quick check
	auto old = AVLNODE_ENTRY(odr_road, _avlnode, node);
	if (road_similar_check(old, road)) {
		fprintf(stderr, "road:%u: warning: detect confliction,"
		" the later one is discarded.\n", old->_id);
		return true;
	}
	merge_similar_road(old, road);
	return true;
}

odr_junction_connection* odr_map::junction_find_connection(
	odr_junction* junc,uint32_t incroad,
	uint32_t connroad, int connpoint)
{
	for (int i = 0; i < junc->_connections.getsize(); ++i) {
		auto conn = junc->_connections.buffer()[i];
		if (conn->_incoming_road != incroad
			|| conn->_connecting_road != connroad
			|| conn->_a.conn_point != connpoint) {
			continue;
		}
		return conn;
	}
	return nullptr;
}

void odr_map::junction_merge_lanelinks(odr_junction_connection* old,
	odr_junction_connection* _new)
{
	for (int i = 0; i < _new->_lanelinks.getsize(); ++i) {
		auto link = _new->_lanelinks.buffer()[i];
		// check if the "from" of new link exists in the old connection
		bool found = false;
		for (int j = 0; j < old->_lanelinks.getsize(); ++j) {
			auto olink = old->_lanelinks.buffer()[j];
			if (olink->_from == link->_from) {
				// check if they have the same "to"
				odr_map_check(olink->_to == link->_to);
				found = true; break;	
			}
		}
		if (found) continue;
		// the "from" of new connection not exists in old connection
		// we move the one to the old connection
		auto holder = old->_lanelinks.new_object();
		*holder = link;
		_new->_lanelinks.buffer()[i] = nullptr;
		link->set_parents(old);
	}
}

bool odr_map::junction_exists_and_merged(odr_junction* junc)
{
	auto node = avl_find(_odr_junction_idtree, &junc->_avlnode,
		odr_junction::avl_id_compare);
	if (!node) return false;

	auto old = AVLNODE_ENTRY(odr_junction, _avlnode, node);
	for (int i = 0; i < junc->_connections.getsize(); ++i) {
		auto conn = junc->_connections.buffer()[i];
		auto old_conn = junction_find_connection(old, conn->_incoming_road,
			conn->_connecting_road, conn->_a.conn_point);
		if (nullptr == old_conn) {
			// move the connection from the new conn to old conn
			auto holder = old->_connections.new_object();
			*holder = conn;
			junc->_connections.buffer()[i] = nullptr;
			conn->set_parents(old);
		}
		else {
			// check and merge all lane links
			junction_merge_lanelinks(old_conn, conn);
		}
	}
	return true;
}

int odr_map::add_junction(odr_junction* junction)
{
	if (!listnode_isempty(junction->_ownerlist)) {
		return -1;
	}

	listnode_add(_odr_junctions, junction->_ownerlist);
	int ret = avl_insert(&_odr_junction_idtree,
		&junction->_avlnode,
		odr_junction::avl_id_compare);
	assert(ret == 0);

	return 0;
}

void odr_map::update_xy_offset(refl_geometry* geo)
{
	_x_offs = _x_offs + ((geo->x - _x_offs) / _geometry_count);
	_y_offs = _y_offs + ((geo->y - _y_offs) / _geometry_count);
	++_geometry_count;
}

void odr_map::set_road_object(odr_object* obj)
{
	_odr_objectmap.insert({obj->_id, obj});
	if (obj->_id > _last_objectid) {
		_last_objectid = obj->_id;
	}
}

int odr_map::generate(void)
{
	// load the extra data first, if necessary
	odr_extdata extd(*this);
	if (extd.execute()) {
		return -1;
	}
	
	// generate render map first
	int ret = generate_rendermap();
	if (ret) return ret;

	// release memory of rendermap
	rendermap_release_all();

	// generate the hdmap
	return generate_hdmap();
}

int odr_map::generate_hdmap(void)
{
	fprintf(stdout, "hdmap: generating hdmap ...\n\n");
	hdmap_prepare();

	int ret = generate_roads();
	if (ret) return ret;

	generate_transition();
	foreach_lane_connect_junctions();
	write_lanesect_transition();
	generate_lanesect_transition();

	ret = generate_junctions();
	if (ret) return ret;

	ret = write_hdmap();
	if (ret) return ret;
	return 0;
}
odr_lane* odr_map::find_odr_lane_by_id(odr_lane_section* odr_ls, int id)
{
	int lane_num = odr_ls->_left_lanes.getsize();
	for (int i = 0; i < lane_num; ++i) {
		auto* odrlane = odr_ls->_left_lanes.buffer()[i];
		if (odrlane->get_type() == lane_type_none) {
			continue;
		}
		if (odrlane->_id == id) {
			return odrlane;
		}
	}
	lane_num = odr_ls->_right_lanes.getsize();
	for (int i = 0; i < lane_num; ++i) {
		auto* odrlane = odr_ls->_right_lanes.buffer()[i];
		if (odrlane->get_type() == lane_type_none) {
			continue;
		}
		if (odrlane->_id == id) {
			return odrlane;
		}
	}
	return nullptr;
}

int odr_map::add_road_lanesect_transition(odr_lane_section* lss, int lane_id, std::set<int> &lstset)
{
	if (nullptr == lss) {
		fprintf(stderr, "hdmap: generating transition add road ts lanes is null ..\n\n");
		return -1;
	}
	auto* odrlane = find_odr_lane_by_id(lss, lane_id);
	if (_lane_index.count(odrlane) > 0) {
		lstset.insert(_lane_index[odrlane]);
	} else {
		// fprintf(stderr, "hdmap: generating transition add road ts lane not find ...\n\n");
	}
	return 0;
}

int odr_map::add_lanesect_transition_by_lanelink(odr_road* r, odr_lane* cur_lane, std::set<int> &lstset, bool bnext)
{
	roadlink* lanelnk = nullptr;
	if (bnext) {
		lanelnk = &(r->_next);
	} else {
		lanelnk = &(r->_prev);
	}

	if ((bnext && lanelnk->a.conn_point != rl_connpoint_start)
		|| (!bnext && lanelnk->a.conn_point != rl_connpoint_end)) {
		fprintf(stderr, "hdmap: generating transition  connect lane error lane ...\n\n");
		return 0;
	}
	
	odr_lane_section* lanesec = nullptr;
	if (lanelnk->a.elem_type == rl_elemtype_road) {
		auto* nextrd = getroad_byid(lanelnk->elem_id);
		int laneid;
		if (bnext) {
			laneid = cur_lane->_next;
			lanesec = nextrd->_lanes->_lane_sects.buffer()[0];
		} 
		else {
			laneid = cur_lane->_prev;
			lanesec = nextrd->_lanes->_lane_sects.buffer()[nextrd->_lanes->_lane_sects.getsize() - 1];
		}
		add_road_lanesect_transition(lanesec, cur_lane->_next, lstset);
	}
	else if (lanelnk->a.elem_type == rl_elemtype_junction) {
		auto* nextjunc = getjunction_byid(lanelnk->elem_id);
		if (!bnext) {
			return -2;
		}
		for (int j = 0; j < nextjunc->_connections.getsize(); j++) {
			auto junc_connect = nextjunc->_connections.buffer()[j];
			if (junc_connect->_incoming_road == r->_id) {
				for (int lnkn = 0; lnkn < junc_connect->_lanelinks.getsize(); lnkn++) {
					auto lanelnk = junc_connect->_lanelinks.buffer()[lnkn];
					if (lanelnk->_from == cur_lane->_id) {
						auto* nextrd = getroad_byid(junc_connect->_connecting_road);
						lanesec = nextrd->_lanes->_lane_sects.buffer()[0];
						add_road_lanesect_transition(lanesec, lanelnk->_to, lstset);
					}
				}
			}
		}
	}

	return 0;
}

int odr_map::generate_transition(void)
{
	if (_lsc_transitions.empty()) {
		fprintf(stderr, "hdmap: generating transition ...\n\n");
		return 0;
	}
	for (const auto &lsitem : _lsc_transitions) {
		auto lstrs = lsitem.second;
		odr_lane_section* ls;
		auto rinfo = lstrs->rdinfo;
		auto r = rinfo->get_lane_road(lstrs->lane, ls);
		auto olane = lstrs->lane;
		auto& lss = r->_lanes->_lane_sects;
		for (int i = 0; i < lss.getsize(); i++) {
			auto* lanesec = lss.buffer()[i];
			odr_lane_section* prev = nullptr;
			odr_lane_section* next = nullptr;
			if (lanesec == ls) {
				if (i == (lss.getsize() - 1)) {
					next = nullptr;
					if (lss.getsize() > 1) {
						prev = lss.buffer()[i - 1];
					}
				}
				else if (i == 0) {
					next = lss.buffer()[1];
					prev = nullptr;
				}
				else {
					prev = lss.buffer()[i - 1];
					next = lss.buffer()[i + 1];
				}
				if (olane->_attrs.has_next) {
					if (nullptr == next) {
						add_lanesect_transition_by_lanelink(r, olane, lstrs->lsc_next_index, true);
					} else {
						add_road_lanesect_transition(next, olane->_next, lstrs->lsc_next_index);
					}
				}
				if (olane->_attrs.has_prev) {
					if (nullptr == prev) {
						add_lanesect_transition_by_lanelink(r, olane, lstrs->lsc_prev_index, false);
					} else {
						add_road_lanesect_transition(prev, olane->_prev, lstrs->lsc_prev_index);
					}
				}
				break;
			}
		}

		// left & right
		int index = -1;
		bool is_left = false;
		for (int i = 0; i < ls->_left_lanes.getsize(); i++) {
			auto slane = ls->_left_lanes.buffer()[i];
			if (slane == olane) {
				index = i;
				is_left = true;
				break;
			}
		}
		if (-1 == index) {
			for (int i = 0; i < ls->_right_lanes.getsize(); i++) {
				auto slane = ls->_right_lanes.buffer()[i];
				if (slane == olane) {
					index = i;
					is_left = false;
					break;
				}
			}
		}
		if (-1 == index) {
			fprintf(stderr, "hdmap: generating transition odr lane not find...\n\n");
			continue;
		}

		if(is_left) {
			for (int i = index + 1; i < ls->_left_lanes.getsize(); i++) {
				auto slane = ls->_left_lanes.buffer()[i];
				lstrs->lsc_left_index[i - index - 1] = _lane_index[slane];
			}
			for (int i = index -1; i >= 0; i--) {
				auto slane = ls->_left_lanes.buffer()[i];
				lstrs->lsc_right_index[index - 1 - i] = _lane_index[slane];
			}
			if (ls->_single_side) {
				for (int i = 0; i < ls->_right_lanes.getsize(); i++) {
					auto slane = ls->_right_lanes.buffer()[i];
					lstrs->lsc_right_index[index + i] = _lane_index[slane];
				}
			}
		} else {
			for (int i = index + 1; i < ls->_right_lanes.getsize(); i++) {
				auto slane = ls->_right_lanes.buffer()[i];
				lstrs->lsc_right_index[i - index - 1] = _lane_index[slane];
			}
			for (int i = index -1; i >= 0; i--) {
				auto slane = ls->_right_lanes.buffer()[i];
				lstrs->lsc_left_index[index - 1 - i] = _lane_index[slane];
			}
			if (ls->_single_side) {
				for (int i = 0; i < ls->_left_lanes.getsize(); i++) {
					auto slane = ls->_left_lanes.buffer()[i];
					lstrs->lsc_left_index[index + i] = _lane_index[slane];
				}
			}
		}
	}
	return 0;
}

int odr_map::write_lanesect_transition() {
	if (_lsc_transitions.empty()) {
		fprintf(stderr, "lanesec empty ...\n\n");
		return -1;
	}
	const char* filepath = "/home/coder/workspace/map/lanesec.json";
	jsonobject& rootArr = json::new_array();
	for (const auto &lsitem : _lsc_transitions) {
		jsonobject& lanetransitem = rootArr.new_item();
		lanetransitem.new_number("lanesec_index",lsitem.second->lanesec_index);
		lanetransitem.new_number("road_id",lsitem.second->road_id);
		lanetransitem.new_number("length",lsitem.second->length);
		std::set<int> lsc_prev_index = lsitem.second->lsc_prev_index;
		if (!lsc_prev_index.empty()) {
			jsonobject& prevjobj = lanetransitem.new_array("lsc_prev_index");
			for (auto prev = lsc_prev_index.begin(); prev != lsc_prev_index.end(); prev++) {
				prevjobj.new_item().new_number("prev_index", *prev);
			}
		}
		std::set<int> lsc_next_index = lsitem.second->lsc_next_index;
		if (!lsc_next_index.empty()) {
			jsonobject& nextjobj = lanetransitem.new_array("lsc_next_index");
			for (auto next = lsc_next_index.begin(); next != lsc_next_index.end(); next++) {
				nextjobj.new_item().new_number("next_index", *next);
			}
		}
		std::map<int, int> lsc_left_index = lsitem.second->lsc_left_index;
		if (!lsc_left_index.empty()) {
			jsonobject& leftjobj = lanetransitem.new_array("lsc_left_index");
			for (const auto &leftitem : lsc_left_index) {
				jsonobject& leftitemjobj = json::new_object(nullptr);
				leftitemjobj.new_number("key", leftitem.first);
				leftitemjobj.new_number("value", leftitem.second);
				leftjobj.add(leftitemjobj);
			}
		}
		std::map<int, int> lsc_right_index = lsitem.second->lsc_right_index;
		if (!lsc_right_index.empty()) {
			jsonobject& rightjobj = lanetransitem.new_array("lsc_right_index");
			for (const auto &rightitem : lsc_right_index) {
				jsonobject& rightitemjobj = json::new_object(nullptr);
				rightitemjobj.new_number("key", rightitem.first);
				rightitemjobj.new_number("value", rightitem.second);
				rightjobj.add(rightitemjobj);
			}
		}
		jsonobject& interseclanjobj = lanetransitem.new_object("intersect_lane");
		if (lsitem.second->intersect_lane.intersection_lanes.size() > 0) {
			jsonobject& pointarr = interseclanjobj.new_array("points");
			for (size_t i = 0; i < lsitem.second->intersect_lane.intersection_lanes.size(); i++) {
				std::shared_ptr<lane_point> point = lsitem.second->intersect_lane.intersection_pts[i];
				jsonobject& lpjobj = json::new_object(nullptr);
				lpjobj.new_number("x", point->xyz.x);
				lpjobj.new_number("y", point->xyz.x);
				lpjobj.new_number("length", lsitem.second->intersect_lane.intersection_length[point]);
				lpjobj.new_number("lane_index", lsitem.second->intersect_lane.intersection_lanes[point]);
				pointarr.add(lpjobj);
			}
		}
	}
	rootArr.savefile(filepath);
	return 0;
}

int odr_map::generate_lanesect_transition() {
	const char* filepath = "/home/coder/workspace/map/lanesec.json";
	jsonobject& lanetransjobj = json::loadfromfile(filepath);
	if (lanetransjobj.is_array()) {
		for (size_t i = 0; i < lanetransjobj.count(); i++) {
			std::shared_ptr<odr_map_lanesect_transition> lscts = std::make_shared<odr_map_lanesect_transition>();

			jsonobject& itemjobj = lanetransjobj.get(i);
			int lanesec_index = itemjobj.get("lanesec_index").to_number();
			lscts->lanesec_index = lanesec_index;
			int road_id = itemjobj.get("road_id").to_number();
			lscts->road_id = road_id;
			double length = itemjobj.get("length").to_double();
			lscts->length = length;

			jsonobject& prevjobj = itemjobj.get("lsc_prev_index");
			if (!prevjobj.is_null()) {
				for (size_t j = 0; j < prevjobj.count(); j ++) {
					int previndex = prevjobj.get(j).get("prev_index").to_number();
					lscts->lsc_prev_index.insert(previndex);
				}
			}

			jsonobject& nextjobj = itemjobj.get("lsc_next_index");
			if (!nextjobj.is_null()) {
				for (size_t j = 0; j < nextjobj.count(); j ++) {
					int nextindex = nextjobj.get(j).get("next_index").to_number();
					lscts->lsc_next_index.insert(nextindex);
				}
			}

			jsonobject& leftjobj = itemjobj.get("lsc_left_index");
			if (!leftjobj.is_null() && leftjobj.is_array()) {
				for (size_t j = 0; j < leftjobj.count(); j++) {
					int key = leftjobj.get(j).get("key").to_number();
					int value = leftjobj.get(j).get("value").to_number();
					lscts->lsc_left_index[key] = value;
				}
			}

			jsonobject& rightjobj = itemjobj.get("lsc_right_index");
			if (!rightjobj.is_null() && rightjobj.is_array()) {
				for (size_t j = 0; j < rightjobj.count(); j++) {
					int key = rightjobj.get(j).get("key").to_number();
					int value = rightjobj.get(j).get("value").to_number();
					lscts->lsc_right_index[key] = value;
				}
			}

			jsonobject& intersectlanejobj = itemjobj.get("intersect_lane");
			if (!intersectlanejobj.is_null()) {
				jsonobject& pointarr = intersectlanejobj.get("points");
				if (!pointarr.is_null() && pointarr.is_array()) {
					for (size_t j = 0; j < pointarr.count(); j++) {
						double x = pointarr.get(j).get("x").to_double();
						double y = pointarr.get(j).get("y").to_double();
						double length = pointarr.get(j).get("length").to_double();
						int laneindex = pointarr.get(j).get("lane_index").to_number();

						std::shared_ptr<lane_point> point = std::make_shared<lane_point>(x, y, 0);
						lscts->intersect_lane.intersection_pts.push_back(point);
						lscts->intersect_lane.intersection_length[point] = length;
						lscts->intersect_lane.intersection_lanes[point] = laneindex;
					}
				}
			}
			_lsc_transitions[lanesec_index] = lscts;
		}
	}
	return 0;
}

int odr_map::generate_bounding_box(bounding_box &box, std::vector<lane_point>* center_pts)
{
	double min_x, min_y, max_x, max_y;
	if (nullptr == center_pts || center_pts->size() == 0) {
		return -1;
	}
	for (size_t i = 0; i < center_pts->size(); i++) {
		lane_point lp = center_pts->at(i);
		if (i == 0) {
			min_x = lp.xyz.x;
			max_x = lp.xyz.x;
			min_y = lp.xyz.y;
			max_y = lp.xyz.y;
		} else {
			if (min_x > lp.xyz.x) min_x = lp.xyz.x;
			if (max_x < lp.xyz.x) max_x = lp.xyz.x;
			if (min_y > lp.xyz.y) min_y = lp.xyz.y;
			if (max_y < lp.xyz.y) max_y = lp.xyz.y;
		}
	}
	box.left_bottom_point.x = min_x;
	box.left_bottom_point.y = min_y;
	box.left_top_point.x = min_x;
	box.left_top_point.y = max_y;
	box.right_bottom_point.x = max_x;
	box.right_bottom_point.y = min_y;
	box.right_top_point.x = max_x;
	box.right_top_point.y = max_y;
	return 0;
}

int odr_map::calc_min_intersection_bounding_box(bounding_box box1, bounding_box box2, 
	int32_t index1, int32_t index2)
{
	// printf("box1 x1 = %f, y1 = %f, x2 = %f, y2 = %f.\n", box1.left_bottom_point.x, box1.left_bottom_point.y, box1.right_top_point.x, box1.right_top_point.y);
	// printf("box2 x1 = %f, y1 = %f, x2 = %f, y2 = %f.\n", box2.left_bottom_point.x, box2.left_bottom_point.y, box2.right_top_point.x, box2.right_top_point.y);
	double box_width = abs(box1.left_bottom_point.y - box1.left_top_point.y);
	double box_length = abs(box1.left_bottom_point.x - box1.right_bottom_point.x);
	if (!check_intersection_bounding_box(box1, box2)) {
		return -1;
	} 
	else if (box_width < sqrt(2.) && box_length < sqrt(2.)) {
		double center_x1 = (box1.right_top_point.x + box1.left_top_point.x) / 2;
		double center_y1 = (box1.left_top_point.y + box1.left_bottom_point.y) / 2;
		double center_x2 = (box2.right_top_point.x + box2.left_top_point.x) / 2;
		double center_y2 = (box2.left_top_point.y + box2.left_bottom_point.y) / 2;

		std::shared_ptr<lane_point> point = std::make_shared<lane_point>((center_x1 + center_x2) / 2 ,(center_y1 + center_y2) / 2, 0);

		_lsc_transitions[index1]->intersect_lane.intersection_pts.push_back(point);
		_lsc_transitions[index1]->intersect_lane.intersection_lanes[point] = index2;
		double length = 0.;
		double x = 0, y = 0;
		for(size_t i = 0; i < _lsc_transitions[index1]->center_pts->size(); i++) {
			lane_point poi = _lsc_transitions[index1]->center_pts->at(i);
			if (i > 0) length += POINT_DISTANCE(poi.xyz.x - x, poi.xyz.y - y);
			x = poi.xyz.x;
			y = poi.xyz.y;
			if (point_in_bounding_box(box1, poi)) {
				length += POINT_DISTANCE(point->xyz.x - x, point->xyz.y - y);
				break;
			}
		}
		_lsc_transitions[index1]->intersect_lane.intersection_length[point] = length;
	} 
	else {
		bounding_box box1_1;
		bounding_box box1_2;
		split_bounding_box(box1, _lsc_transitions[index1]->center_pts, box1_1, box1_2);
		// printf("box3 x1 = %f, y1 = %f, x2 = %f, y2 = %f.\n", box1_1.left_bottom_point.x, box1_1.left_bottom_point.y, box1_1.right_top_point.x, box1_1.right_top_point.y);
		// printf("box4 x1 = %f, y1 = %f, x2 = %f, y2 = %f.\n", box1_2.left_bottom_point.x, box1_2.left_bottom_point.y, box1_2.right_top_point.x, box1_2.right_top_point.y);
		bounding_box box2_1;
		bounding_box box2_2;
		split_bounding_box(box2, _lsc_transitions[index2]->center_pts, box2_1, box2_2);
		// printf("box5 x1 = %f, y1 = %f, x2 = %f, y2 = %f.\n", box2_1.left_bottom_point.x, box2_1.left_bottom_point.y, box2_1.right_top_point.x, box2_1.right_top_point.y);
		// printf("box6 x1 = %f, y1 = %f, x2 = %f, y2 = %f.\n", box2_2.left_bottom_point.x, box2_2.left_bottom_point.y, box2_2.right_top_point.x, box2_2.right_top_point.y);

		calc_min_intersection_bounding_box(box1_1, box2_1, index1, index2);
		calc_min_intersection_bounding_box(box1_1, box2_2, index1, index2);
		calc_min_intersection_bounding_box(box1_2, box2_1, index1, index2);
		calc_min_intersection_bounding_box(box1_2, box2_2, index1, index2);
	}
	return 0;
}

int odr_map::check_intersection_bounding_box(bounding_box box1, bounding_box box2)
{
	double width1 = abs(box1.left_bottom_point.y - box1.left_top_point.y);
	double length1 = abs(box1.left_bottom_point.x - box1.right_bottom_point.x);
	double width2 = abs(box2.left_bottom_point.y - box2.left_top_point.y);
	double length2 = abs(box2.left_bottom_point.x - box2.right_bottom_point.x);

	double center_x1 = (box1.right_top_point.x + box1.left_top_point.x) / 2;
	double center_y1 = (box1.left_top_point.y + box1.left_bottom_point.y) / 2;
	double center_x2 = (box2.right_top_point.x + box2.left_top_point.x) / 2;
	double center_y2 = (box2.left_top_point.y + box2.left_bottom_point.y) / 2;

	if (abs(center_x1 - center_x2) < ((length1 + length2) / 2) && 
		abs(center_y1 - center_y2) < ((width1 + width2) / 2))
		return 1;

	return 0;
}

int odr_map::split_bounding_box(bounding_box box, std::vector<lane_point>* center_pts, bounding_box &box1, bounding_box &box2)
{
	if (nullptr == center_pts || center_pts->size() == 0) {
		return -1;
	}

	box1.left_bottom_point = box.left_bottom_point;
	box1.left_top_point = box.left_top_point;
	box1.right_bottom_point = box.right_bottom_point;
	box1.right_top_point = box.right_top_point;
	box2.left_bottom_point = box.left_bottom_point;
	box2.left_top_point = box.left_top_point;
	box2.right_bottom_point = box.right_bottom_point;
	box2.right_top_point = box.right_top_point;

	double width = abs(box.left_bottom_point.y - box.left_top_point.y);
	double length = abs(box.left_bottom_point.x - box.right_bottom_point.x);
	if (width > length) {
		double y = (box.left_bottom_point.y + box.left_top_point.y) / 2;
		box1.left_top_point.y = y;
		box1.right_top_point.y = y;
		box2.left_bottom_point.y = y;
		box2.right_bottom_point.y = y;
	} else {
		double x = (box.left_top_point.x + box.right_top_point.x) / 2;
		box1.right_top_point.x = x;
		box1.right_bottom_point.x = x;
		box2.left_top_point.x = x;
		box2.right_bottom_point.x = x;
	}
	cut_bounding_box(box1, center_pts);
	cut_bounding_box(box2, center_pts);
	return 0;
}

int odr_map::point_in_bounding_box(bounding_box box, lane_point p)
{
	if (p.xyz.x >= box.left_top_point.x &&
		p.xyz.x <= box.right_top_point.x &&
		p.xyz.y >= box.left_bottom_point.y && 
		p.xyz.y <= box.left_top_point.y)
		return 1;
	return 0;
}

int odr_map::cut_bounding_box(bounding_box &box, std::vector<lane_point>* center_pts)
{
	if (nullptr == center_pts || center_pts->size() == 0) {
		return -1;
	}
	bool flag = true;
	double min_x, min_y, max_x, max_y;
	for (size_t i = 0; i < center_pts->size(); i++) {
		lane_point lp = center_pts->at(i);
		if (point_in_bounding_box(box, lp)) {
			if (flag) {
				min_x = lp.xyz.x;
				max_x = lp.xyz.x;
				min_y = lp.xyz.y;
				max_y = lp.xyz.y;
				flag = false;
			} else {
				if (min_x > lp.xyz.x) min_x = lp.xyz.x;
				if (max_x < lp.xyz.x) max_x = lp.xyz.x;
				if (min_y > lp.xyz.y) min_y = lp.xyz.y;
				if (max_y < lp.xyz.y) max_y = lp.xyz.y;
			}
		}
	}
	box.left_bottom_point.x = min_x;
	box.left_bottom_point.y = min_y;
	box.left_top_point.x = min_x;
	box.left_top_point.y = max_y;
	box.right_bottom_point.x = max_x;
	box.right_bottom_point.y = min_y;
	box.right_top_point.x = max_x;
	box.right_top_point.y = max_y;
	return 0;
}

int odr_map::foreach_lane_connect_junctions()
{
	auto* i = _odr_junctions.next;
	for (; i != &_odr_junctions; i = i->next)
	{
		auto* junction = list_entry(odr_junction, _ownerlist, i);
		for(int j = 0;j < junction->_connections.getsize(); j++) {
			auto* conn = junction->_connections.buffer()[j];
			odr_road* connectroad = getroad_byid(conn->_connecting_road);
			for(int k = 0; k < junction->_connections.getsize(); k++) {
				auto* subconn = junction->_connections.buffer()[k];
				odr_road* subroad = getroad_byid(subconn->_connecting_road);
				if (connectroad->_id == subroad->_id) {
					continue;
				}
				calc_intersection_lane(connectroad, subroad);
			}
		}
	}
	return 0;
}

int odr_map::calc_intersection_lane(odr_road* road1, odr_road* road2)
{	
	std::vector<odr_lane*> lanes1;
	std::vector<odr_lane*> lanes2;
	get_lanes_from_road(road1, lanes1);
	get_lanes_from_road(road2, lanes2);
	for (int i = 0; i < lanes1.size(); i++) {
		int32_t index1 = _lane_index[lanes1[i]];
		bounding_box box1;
		generate_bounding_box(box1, _lsc_transitions[index1]->center_pts);
		for (int j = 0; j < lanes2.size(); j++) {
			int32_t index2 = _lane_index[lanes2[j]];
			bounding_box box2;
			generate_bounding_box(box2, _lsc_transitions[index2]->center_pts);
			calc_min_intersection_bounding_box(box1, box2, index1, index2);
		}
	}
	return 0;
}

int odr_map::get_lanes_from_road(odr_road* road, std::vector<odr_lane*> &lanes)
{
	if (nullptr == road->_lanes) {
		return -1;
	}
	if (road->_lanes->_lane_sects.getsize() == 0) {
		return -2;
	}
	for (int i = 0; i < road->_lanes->_lane_sects.getsize(); i++){
		odr_lane_section* lanesec1 = road->_lanes->_lane_sects.buffer()[i];
		for (int j = 0; j < lanesec1->_left_lanes.getsize();j++) {
			odr_lane* lane = lanesec1->_left_lanes.buffer()[j];
			if (!lane_need_handle(lane)) {
				continue;
			}
			lanes.push_back(lane);
		}
		for (int j = 0; j < lanesec1->_right_lanes.getsize();j++) {
			odr_lane* lane = lanesec1->_right_lanes.buffer()[j];
			if (!lane_need_handle(lane)) {
				continue;
			}
			lanes.push_back(lane);
		}
	}
	return 0;
}

hdmap_container* odr_map::get_container(odr_element* el)
{
	assert(nullptr != el);
	if (el->get_type() == odr_elemtype_junction) {
		auto j = zas_downcast(odr_element, odr_junction, el);
		return j->_container;
	}
	else if (el->get_type() == odr_elemtype_road) {
		auto r = zas_downcast(odr_element, odr_road, el);
		return r->_container;
	}
	return nullptr;
}

int odr_map::verify_road_container(void)
{
	listnode_t* i = _odr_roads.next;
	for (; i != &_odr_roads; i = i->next) {
		auto* r = list_entry(odr_road, _ownerlist, i);
		if (!r->_container) return -1;
	}
	return 0;
}

int odr_map::generate_roads(void)
{
	auto* i = _odr_roads.next;
	for (; i != &_odr_roads; i = i->next) {
		auto* r = list_entry(odr_road, _ownerlist, i);
		if (create_odr_road_container(r)) {
			return -1;
		}
	}
	assert(!verify_road_container());

	// generate all road information
	int count = 0;
	auto hi = _hdm_roads.next;

	for (long prev_tick = 0; hi != &_hdm_roads; hi = hi->next, ++count)
	{
		auto* hc = list_entry(hdmap_container, ownerlist, hi);
		if (generate_roadinfo(hc)) return -1;

		// update output info
		long curr_tick = zas::utils::gettick_millisecond();
		if (curr_tick - prev_tick > 20) {
			prev_tick = curr_tick;
			fprintf(stdout, "\rgenerating hdmap roads (%d/%d) ...",
				count, _hdm_road_count);
			fflush(stdout);
		}
	}
	fprintf(stdout, "\rgenerating hdmap roads (%d/%d) ... [success]\n",
		count, _hdm_road_count);
	return 0;
}

int odr_map::generate_junctions(void)
{
	int count;
	long prev_tick = 0;
	auto* i = _odr_junctions.next;
	for (count = 0; i != &_odr_junctions; i = i->next, ++count)
	{
		auto* j = list_entry(odr_junction, _ownerlist, i);
		if (create_odr_junction_container(j)) {
			return -1;
		}
		long curr_tick = zas::utils::gettick_millisecond();
		if (curr_tick - prev_tick > 20) {
			prev_tick = curr_tick;
			fprintf(stdout, "\rgenerating hdmap junctions (%d/%d) ...",
				count, _hdm_junc_count);
			fflush(nullptr);
		}
	}
	fprintf(stdout, "\rgenerating hdmap junctions (%d/%d) ... [success]\n",
		count, _hdm_junc_count);
	return 0;
}

bool odr_map::can_share_container(odr_element* a, odr_element* b)
{
	if (a == b) {
		return true;
	}

	if (a->get_type() != b->get_type()) {
		return false;
	}
	assert(a->get_type() == odr_elemtype_road);

	auto* aa = zas_downcast(odr_element, odr_road, a);
	auto* bb = zas_downcast(odr_element, odr_road, b);

	// if they have the same name
	if (!aa->_name.equal_to(bb->_name)) {
		return false;
	}
	if (aa->_junction_id * bb->_junction_id < 0) {
		return false;
	}
	return true;
}

odr_element* odr_map::get_roadlink_object(roadlink* l)
{
	odr_element* ret = nullptr;
	if (l->a.elem_type == rl_elemtype_road) {
		ret = getroad_byid(l->elem_id);
	}
	else if (l->a.elem_type == rl_elemtype_junction) {
		ret = getjunction_byid(l->elem_id);
	}
	return ret;
}

odr_road* odr_map::getroad_byid(uint32_t id)
{
	if (!id) return nullptr;

	auto* ret = avl_find(_odr_road_idtree,
		MAKE_FIND_OBJECT(id, odr_road, _id, _avlnode),
		odr_road::avl_id_compare);
	if (nullptr == ret) {
		return nullptr;
	}

	return AVLNODE_ENTRY(odr_road, _avlnode, ret);
}

odr_junction* odr_map::getjunction_byapproache(string uuid)
{
	auto* i = _odr_junctions.next;
	for (uint32_t count = 0; i != &_odr_junctions; i = i->next, ++count)
	{
		auto* j = list_entry(odr_junction, _ownerlist, i);
		if(j->contain_approach(uuid)) {
			return j;
		}
	}
	return nullptr;
}

odr_junction* odr_map::getjunction_byid(uint32_t id)
{
	if (!id) return nullptr;

	auto* ret = avl_find(_odr_junction_idtree,
		MAKE_FIND_OBJECT(id, odr_junction, _id, _avlnode),
		odr_junction::avl_id_compare);
	if (nullptr == ret) {
		return nullptr;
	}

	return AVLNODE_ENTRY(odr_junction, _avlnode, ret);
}

uint64_t odr_map::allocate_id(void)
{
	if (_idcount > ((1LL << 41) - 1)) {
		fprintf(stderr, "too many IDs (exceed 2^40)\n");
		exit(111);
	}
	return (uint32_t)_idcount++;
}

void odr_map::update_bounding_box(double x, double y)
{
		if (x < _x0) _x0 = x;
		if (x > _x1) _x1 = x;
		if (y < _y0) _y0 = y;
		if (y > _y1) _y1 = y;
}

uint32_t hdmap_lane::count = 0;

hdmap_lane::hdmap_lane()
: last(nullptr), index(count++)
{
	listnode_init(ownerlist);
	listnode_init(lanesegs);
}

int hdmap_boundary::count = 0;
hdmap_boundary::hdmap_boundary()
: id(-1)
{
	++count;
	listnode_init(ownerlist);
	listnode_init(seg_list);
}

hdmap_container::~hdmap_container()
{
	while (!listnode_isempty(roadseg_list)) {
		auto r = list_entry(odr_road,
			_hdmr_ownerlist, roadseg_list.next);
		listnode_del(r->_hdmr_ownerlist);
	}
	if (roadinfo) {
		delete roadinfo; roadinfo = nullptr;
	}
}

odr_road* hdmap_container::get_first(void)
{
	assert(!listnode_isempty(roadseg_list));
	auto n = roadseg_list.next;
	return list_entry(odr_road, _hdmr_ownerlist, n);
}

odr_road* hdmap_container::get_last(void)
{
	assert(!listnode_isempty(roadseg_list));
	auto n = roadseg_list.prev;
	return list_entry(odr_road, _hdmr_ownerlist, n);
}

int odr_map::create_odr_road_container(odr_road* r)
{
	// check if the odr-road has a container
	if (r->_container) return 0;

	hdmap_container* hc;
	if (r->_junction_id > 0) {
		// this road is a junction connection
		hc = new hdmap_junction_connection(r->_id, r->_junction_id);
	}
	else {
		// create a new container for this road
		hc = new hdmap_road(r->_id);
	}
	if (nullptr == hc) return -1;

	hc->id = allocate_id();
	assert(hc->id);
	if (r->_name.c_str()) hc->name = r->_name;

	// add to list
	listnode_add(_hdm_roads, hc->ownerlist);
	++_hdm_road_count;

	if (walk_roads(r, hc)) {
		return -1;
	}
	return 0;
}

int odr_map::walk_roads(odr_road* r, hdmap_container* hc)
{
	r->_container = hc;
	listnode_add(hc->roadseg_list, r->_hdmr_ownerlist);

	auto fst_prev = r, lst_prev = r;
	odr_element* fst = get_roadlink_object(&r->_prev);
	odr_element* lst = get_roadlink_object(&r->_next);

	while (nullptr != fst || nullptr != lst)
	{
		if (fst && can_share_container(fst, r))
		{
			// add this road segment as the first one in the list
			auto fst_r = zas_downcast(odr_element, odr_road, fst);
			fst_r->_container = hc;
			listnode_insertfirst(hc->roadseg_list, fst_r->_hdmr_ownerlist);

			// get the next road segment
			auto fst_next = get_roadlink_object(&fst_r->_prev);
			if (fst_next == fst_prev) {
				fst_next = get_roadlink_object(&fst_r->_next);
			}
			fst_prev = fst_r, fst = fst_next;
		} else fst = nullptr;

		if (lst && can_share_container(r, lst))
		{
			// add this road segment as the last one in the list
			auto lst_r = zas_downcast(odr_element, odr_road, lst);
			lst_r->_container = hc;
			listnode_add(hc->roadseg_list, lst_r->_hdmr_ownerlist);

			// get the next road segment
			auto lst_next = get_roadlink_object(&lst_r->_next);
			if (lst_next == lst_prev) {
				lst_next = get_roadlink_object(&lst_r->_prev);
			}
			lst_prev = lst_r, lst = lst_next;
		} else lst = nullptr;
	}
	return 0;
}

int odr_map::create_odr_junction_container(odr_junction* j)
{
	if (!j->_connections.getsize()) {
		return 0;
	}

	// create hdmap junction
	auto* hdjunc = new hdmap_junction();
	if (nullptr == hdjunc) {
		return -1;
	}
	j->_container = hdjunc;
	hdjunc->odr_junc = j;

	for (int i = 0; i < j->_connections.getsize(); ++i) {
		auto* conn = j->_connections.buffer()[i];
		int iret = hdmap_junction_add_connection(hdjunc, conn);
		if (iret && iret != -1) {
			return -2;
		}
	}
	listnode_add(_hdm_juncs, hdjunc->ownerlist);
	++_hdm_junc_count;
	return 0;
}

hdmap_lane* hdmap_roadinfo::get_lane(int id, bool last)
{
	auto i = lanes.next;
	for (; i != &lanes; i = i->next)
	{
		auto lane = list_entry(hdmap_lane, ownerlist, i);
		auto j = (last) ? lane->inner.seg_list.prev
			: lane->inner.seg_list.next;
		auto csegref = list_entry(curve_segment_ref, ownerlist, j);
		if (csegref->lane->_id == id) {
			return lane;
		}
	}
	return nullptr;
}

int odr_map::hdmap_junction_link_incoming(odr_road* incoming, odr_road* conn,
	odr_junction_connection* jconn)
{
	auto conn_ri = conn->_container->roadinfo;
	auto incoming_ri = incoming->_container->roadinfo;
	assert(conn_ri && incoming_ri);

	for (int i = 0; i < jconn->_lanelinks.getsize(); ++i)
	{
		auto link = jconn->_lanelinks.buffer()[i];
		auto incoming_lane = incoming_ri->get_lane(link->_from, true);
		auto conn_lane = conn_ri->get_lane(link->_to, false);
		// odr_map_check(incoming_lane && conn_lane);
		if (!(incoming_lane && conn_lane)) {
			continue;
		}

		incoming_lane->nexts.insert(conn_lane);
		conn_lane->prevs.insert(incoming_lane);
	}
	return 0;
}

int odr_map::hdmap_junction_link_outgoing(odr_road* conn, odr_road* outgoing)
{
	auto conn_ri = conn->_container->roadinfo;
	auto outgoing_ri = outgoing->_container->roadinfo;
	assert(conn_ri && outgoing_ri);

	bool is_prev = false;
	if (conn->_prev.elem_id == outgoing->_id) {
		is_prev = true;
	}
	else assert(conn->_next.elem_id == outgoing->_id);

	auto i = conn_ri->lanes.next;
	for (; i != &conn_ri->lanes; i = i->next)
	{
		auto conn_lane = list_entry(hdmap_lane, ownerlist, i);

		// get the last curve_segment and its lane
		auto j = conn_lane->inner.seg_list.prev;
		auto csegref = list_entry(curve_segment_ref, ownerlist, j);		

		// get the outgoing hdmap_lane
		auto outgoing_lane = outgoing_ri->get_lane((is_prev)
			? csegref->lane->_prev : csegref->lane->_next, false);
		if (nullptr == outgoing_lane) {
			// it is possible that one of the lane in connecting road comes
			// to an end (merged to another lane) so it has no outgoing lane
			continue;
		}

		odr_lane_section* ls;
		auto r = conn_ri->get_lane_road(csegref->lane, ls);
		if (((is_prev) ? r->_prev.elem_id : r->_next.elem_id) != outgoing->_id) {
			return -1;
		}

		conn_lane->nexts.insert(outgoing_lane);
		outgoing_lane->prevs.insert(conn_lane);
	}
	return 0;
}

int odr_map::hdmap_junction_add_connection(
	hdmap_junction* hdjunc, odr_junction_connection* odr_conn)
{
	// connection road
	auto conn_road = getroad_byid(odr_conn->_connecting_road);
	if (nullptr == conn_road) {
		return -1;
	}
	auto conn = zas_downcast(hdmap_container,
		hdmap_junction_connection, conn_road->_container);
	assert(conn && conn->a.type == hdmap_container_connecting);

	// get the incoming road
	auto incoming_road = getroad_byid(odr_conn->_incoming_road);
	if (nullptr == incoming_road) {
		return -2;
	}
	if (hdmap_junction_link_incoming(incoming_road, conn_road, odr_conn)) {
		return -3;
	}

	// get the outgoing road
	auto outgoing_road = get_outgoing_road(incoming_road, conn);
	if (nullptr  == outgoing_road) {
		return -4;
	}
	if (hdmap_junction_link_outgoing(conn_road, outgoing_road)) {
		return -5;
	}

	auto pair = check_create_road_pair(
		hdjunc, incoming_road, outgoing_road, conn);
	assert(nullptr != pair);
	return 0;
}

odr_road* odr_map::get_outgoing_road(odr_road* incoming,
	hdmap_junction_connection* conn)
{
	auto first = conn->get_first();
	auto last = conn->get_last();
	auto conn_road = (first == incoming) ? last : first;
	auto r1 = getroad_byid(conn_road->_prev.elem_id);
	auto r2 = getroad_byid(conn_road->_next.elem_id);
	odr_map_check(nullptr != r1 && nullptr != r2);

	auto ret = (r1 == incoming) ? r2 : r1;
	assert(ret->_container->a.type == hdmap_container_road);
	return ret;
}

hdmap_junction_roadpair* odr_map::check_create_road_pair(
	hdmap_junction* junc, odr_road* incoming, odr_road* outgoing,
	hdmap_junction_connection* conn)
{
	auto i = junc->road_pairs.next;
	for (; i != &junc->road_pairs; i = i->next) {
		auto pair = list_entry(hdmap_junction_roadpair, ownerlist, i);
		if (pair->incoming_road == incoming
			&& pair->outgoing_road == outgoing) {
			pair->connections.insert(conn);
			return pair;
		}
	}
	// not found, create a new pair
	auto pair = new hdmap_junction_roadpair();
	odr_map_check(nullptr != pair);

	pair->incoming_road = incoming;
	pair->outgoing_road = outgoing;
	pair->connections.insert(conn);
	listnode_add(junc->road_pairs, pair->ownerlist);
	return pair;
}

int odr_map::generate_roadinfo(hdmap_container* c)
{
	if (c->a.type != hdmap_container_road
		&& c->a.type != hdmap_container_connecting) {
		return -1;
	}

	auto rinfo = new hdmap_roadinfo(&_hdm_blkmgr);
	odr_map_check(nullptr != rinfo);
	rinfo->container = c;

	double s = 0.;
	auto i = c->roadseg_list.next;
	for (; i != &c->roadseg_list; i = i->next)
	{
		auto curr = list_entry(odr_road, _hdmr_ownerlist, i);

		// create the reference line segment for this odr-road
		auto refline = new refline_fragment(curr, s, &_hdm_blkmgr,
			_psr.get_hdmap_info()->hdmap.epsilon);
		odr_map_check(nullptr != refline);
		rinfo->reference_line.push_back(refline);
		s += refline->length();

		// generate lane section related information
		int ret = generate_roadseg_lanesecs(rinfo, curr);
		if (ret) return ret;

		// generate points for reference line in this road
		rinfo->finalize_reference_line(
			_psr.get_hdmap_info()->hdmap.epsilon);
	}

	assert(nullptr == c->roadinfo);
	c->roadinfo = rinfo;
	return 0;
}

int odr_map::generate_roadseg_lanesecs(hdmap_roadinfo* rinfo, odr_road* r)
{
	bool inv = false;
	// check the directon of the roadseg. if we are the first roadseg
	// in hdmap_road, apply reference line direction, otherwise, we
	// find the direction by checking the previous lane section	
	if (r != rinfo->container->get_first()) {
		auto pr = list_entry(odr_road, _hdmr_ownerlist, r->_hdmr_ownerlist.prev);
		if (pr->_next.elem_id == r->_id) {
			if (pr->_next.a.conn_point == rl_connpoint_end) {
				inv = true;
			}
		} else if (pr->_prev.elem_id == r->_id) {
			if (pr->_prev.a.conn_point == rl_connpoint_end) {
				inv = true;
			}
		} else assert(0);
	}

	auto& ls = r->_lanes->_lane_sects;
	if (inv) for (int i = ls.getsize() - 1; i >= 0; --i) {
		if (generate_roadseg_lanesec(rinfo, ls.buffer()[i], inv)) {
			return -1;
		}
	}
	else for (int i = 0; i < ls.getsize(); ++i) {
		if (generate_roadseg_lanesec(rinfo, ls.buffer()[i], inv)) {
			return -1;
		}
	}

	return 0;
}

bool odr_map::lane_need_handle(odr_lane* l)
{
	if (l->_attrs.type != lane_type_none) {
		return true;
	}
	// see if the width of "none" lane is 0
	if (!l->_widths.getsize()) {
		return false;
	}
	for (int i = 0; i < l->_widths.getsize(); ++i) {
		auto w = l->_widths.buffer()[i];
		if (!w->is_zero()) {
			// check if there is prev or next
			if (!l->_attrs.has_next && !l->_attrs.has_prev) {
				return false;
			}
			if (l->_attrs.rendermap_curb) {
				return false;
			}
			return true;
		}
	}
	// no width so we do not need to handle
	return false;
}

uint64_t hdmap_laneseg::count = 0;

hdmap_laneseg::hdmap_laneseg(hdmap_blockmgr *blkmgr,
	curve_segment_ref* i, curve_segment_ref* o)
: inner(i), outer(o), insider(nullptr), outsider(nullptr)
, index(0) {
	listnode_init(ownerlist);
	i->laneseg = this, o->laneseg = this;

	// update the laneseg data size in the block
	if (o->cseg->lane->get_serialized_size()) {
		return;
	}
	assert(o->cseg->blockid >= 0);
	auto& blkinfo = blkmgr->blkinfo_map[o->cseg->blockid];
	auto odrvlane = o->cseg->lane->_odrv_lane.lock();
	assert(nullptr != odrvlane);

	size_t sz = odrvlane->getsize();
	o->cseg->lane->set_serialized_size(sz);
	sz += sizeof(hdmap_datafile_laneseg_data_v1);
	blkinfo.laneseg_total_datasz += sz;
}

curve_segment* odr_map::generate_roadseg_lanesec_lane(
	hdmap_roadinfo* rinfo, odr_lane_section* ls, odr_lane* odrlane,
	bool inv, curve_segment* cseg, hdmap_laneseg*& lseg)
{
	// global config
	auto hdmcfg = _psr.get_hdmap_info();
	double eps = hdmcfg->hdmap.epsilon;

	assert(odrlane->_id);
	auto* lane = rinfo->get_create_lane(odrlane, inv);
	odr_map_check(nullptr != lane);

	// handle the inner boundary
	auto inner = rinfo->append_boundary(lane->inner,
		odrlane, eps, inv, cseg);
	if (nullptr == inner) {
		return nullptr;
	}

	// handle the outer boundary
	auto outer = rinfo->append_boundary(
		lane->outer, odrlane, eps, inv, nullptr, cseg);
	if (nullptr == outer) {
		return nullptr;
	}

	// create the laneseg
	auto laneseg = new hdmap_laneseg(rinfo->blockmgr, inner, outer);
	odr_map_check(nullptr != laneseg);

	// make the link list
	laneseg->insider = lseg;
	if (lseg) { lseg->outsider = laneseg; }
	lseg = laneseg;

	// get the count of lane sections in this lane segment
	auto lanes = ls->get_parents();
	odr_map_check(lanes->get_type() == odr_elemtype_lanes);
	int lscnt = zas_downcast(odr_element,
		odr_lanes, lanes)->_lane_sects.getsize();

	// calcuate the index of laneseg and add to list
	if (((inv) ? -odrlane->_id : odrlane->_id) > 0) {
		if (listnode_isempty(lane->lanesegs)) {
			hdmap_laneseg::count += lscnt;
			laneseg->index = hdmap_laneseg::count - 1;
		}
		else {
			auto last = list_entry(hdmap_laneseg, ownerlist, lane->lanesegs.prev);
			laneseg->index = last->index - ls->_index;
		}
		listnode_insertfirst(lane->lanesegs, laneseg->ownerlist);
	} else {
		if (listnode_isempty(lane->lanesegs)) {
			laneseg->index = hdmap_laneseg::count;
			hdmap_laneseg::count += lscnt;
		}
		else {
			auto first = list_entry(hdmap_laneseg, ownerlist, lane->lanesegs.next);
			laneseg->index = first->index + ls->_index;
		}
		listnode_add(lane->lanesegs, laneseg->ownerlist);		
	}
	std::shared_ptr<odr_map_lanesect_transition> lscts = std::make_shared<odr_map_lanesect_transition>();
	lscts->lanesec_index = laneseg->index;
	lscts->lane = odrlane;
	lscts->rdinfo = rinfo;
	lscts->center_pts = new vector<lane_point>();
	lscts->center_pts->assign(cseg->center_pts->begin(), cseg->center_pts->end());
	lscts->length = 0.;

	double x = 0., y = 0.;
	for(size_t i = 0; i < lscts->center_pts->size(); i++) {
		lane_point poi = lscts->center_pts->at(i);
		if (i > 0) lscts->length += POINT_DISTANCE(poi.xyz.x - x, poi.xyz.y - y);
		x = poi.xyz.x;
		y = poi.xyz.y;
	}
	_lsc_transitions[laneseg->index] = lscts;
	_lane_index[odrlane] = laneseg->index;
	return outer->cseg;
}

int odr_map::generate_roadseg_lanesec(hdmap_roadinfo* rinfo,
	odr_lane_section* ls, bool inv)
{
	curve_segment* prev_outer = nullptr;

	// global config
	auto hdmcfg = _psr.get_hdmap_info();
	double eps = hdmcfg->hdmap.epsilon;

	// create the center lane curve segment
	auto ccseg = new curve_segment(ls->_center_lane);
	odr_map_check(nullptr != ccseg);
	ccseg->generate_points(rinfo, &_hdm_blkmgr, eps);

	// handle right lanes
	hdmap_laneseg* lseg = nullptr;
	int lanes = ls->_right_lanes.getsize();
	for (int i = 0; i < lanes; ++i)
	{
		auto* odrlane = ls->_right_lanes.buffer()[i];
		if (!lane_need_handle(odrlane)) {
			continue;
		}
		prev_outer = generate_roadseg_lanesec_lane(rinfo, ls, odrlane,
			inv, (!i) ? ccseg : prev_outer, lseg);
		if (nullptr == prev_outer) {
			return -1;
		}
	}

	// handle left lanes
	lseg = nullptr;
	prev_outer = nullptr;
	lanes = ls->_left_lanes.getsize();
	for (int i = 0; i < lanes; ++i)
	{
		auto* odrlane = ls->_left_lanes.buffer()[i];
		if (!lane_need_handle(odrlane)) {
			continue;
		}
		prev_outer = generate_roadseg_lanesec_lane(rinfo, ls, odrlane,
			inv, (!i) ? ccseg : prev_outer, lseg);
		if (nullptr == prev_outer) {
			return -1;
		}
	}
	return 0;
}

odr_road* hdmap_roadinfo::get_lane_road(odr_lane* l, odr_lane_section*& ls)
{
	auto _ls = l->get_parents();
	odr_map_check(_ls->get_type() == odr_elemtype_lane_section);
	auto lanes = _ls->get_parents();
	odr_map_check(lanes->get_type() == odr_elemtype_lanes);
	auto road = lanes->get_parents();
	odr_map_check(road->get_type() == odr_elemtype_road);
	ls = zas_downcast(odr_element, odr_lane_section, _ls);
	return zas_downcast(odr_element, odr_road, road);
}

hdmap_lane* hdmap_roadinfo::get_create_lane(odr_lane* l, bool inv)
{
	auto* i = lanes.next;
	odr_lane_section* ls;
	auto r = get_lane_road(l, ls);
	for (; i != &lanes; i = i->next)
	{
		auto* lane = list_entry(hdmap_lane, ownerlist, i);
		assert(nullptr != lane->last);
		auto last = lane->last;
		odr_lane_section* last_ls;
		auto last_r = get_lane_road(last, last_ls);

		// handle multiple lane sections in one roadseg
		if (r == last_r) {
			// make sure the ls is next to last_ls
			int idxdelta = ls->_index - last_ls->_index;
			if (idxdelta * idxdelta != 1) {
				continue;
			}
			assert((inv) ? last->_attrs.has_prev : last->_attrs.has_next);
			if (l->_id != ((inv) ? last->_prev : last->_next)) {
				continue;
			}
		}
		else {
			// lane sections in different roadseg
			if (last_r->_next.elem_id == r->_id) {
				if (l->_id != last->_next) {
					continue;
				}
			}
			else if (last_r->_prev.elem_id == r->_id) {
				if (l->_id != last->_prev) {
					continue;
				}
			}
			else continue;
		}
		lane->last = l;
		return lane;
	}
	// we need to create a new one
	auto* lane = new hdmap_lane();
	if (nullptr == lane) {
		return nullptr;
	}
	lane->last = l;
	// add to road info
	listnode_add(lanes, lane->ownerlist);
	return lane;
}

curve_segment_ref* hdmap_roadinfo::append_boundary(
	hdmap_boundary& boundary, odr_lane* lane, double eps, bool inv,
	curve_segment* cseg, curve_segment* use_cseg_blkid)
{
	if (nullptr == cseg) {
		// create the curve for this lane
		cseg = new curve_segment(lane);
		odr_map_check(nullptr != cseg);
		if (use_cseg_blkid) {
			cseg->blockid = use_cseg_blkid->blockid;
		}
		cseg->generate_points(this, blockmgr, eps);
	}

	// update reference line
	if (cseg->s_vals.size()) {
		assert(reference_line.size());
		auto iter = prev(reference_line.end());
		// update the approximate linear
		(*iter)->refline_seg.s_vals.insert(
			cseg->s_vals.begin(), cseg->s_vals.end());
	}
	cseg->finalize(blockmgr, eps);

	// create the curve segment reference
	auto csegref = new curve_segment_ref(blockmgr, lane, cseg);
	odr_map_check(nullptr != csegref);

	// add to boundary
	if (((inv) ? -lane->_id : lane->_id) < 0) {
		listnode_add(boundary.seg_list, csegref->ownerlist);
	} else {
		listnode_insertfirst(boundary.seg_list, csegref->ownerlist);
	}
	return csegref;
}

double hdmap_roadinfo::get_refline_length(void)
{
	assert(reference_line.size() > 0);
	// get the last reference fragment
	auto refl = reference_line.rbegin();
	return (*refl)->s0 + (*refl)->length();
}

refline_fragment* hdmap_roadinfo::get_refline(void)
{
	assert(reference_line.size());
	return *prev(reference_line.end());
}

void hdmap_roadinfo::finalize_reference_line(double eps)
{
	auto refl = get_refline();
	refl->finalize(blockmgr, eps);
}

hdmap_junction::hdmap_junction()
: hdmap_container(hdmap_container_junction)
, odr_junc(nullptr)
{
	listnode_init(road_pairs);
	listnode_init(connections);
}

hdmap_junction::~hdmap_junction()
{
	auto* i = road_pairs.next;
	for (; i != &road_pairs; i = i->next) {
		auto* link = list_entry(hdmap_junction_roadpair, ownerlist, i);
		listnode_del(link->ownerlist);
		delete link;
	}
	i = connections.next;
	for (; i != &connections; i = i->next) {
		auto* conn = list_entry(hdmap_junction_connection, ownerlist, i);
		listnode_del(conn->ownerlist);
		delete conn;
	}
}

hdmap_junction_connection::hdmap_junction_connection(uint32_t id, int jun_id)
: hdmap_container(hdmap_container_connecting)
{
	listnode_init(ownerlist);
	junction_id = jun_id;
	road_id = id;
}

hdmap_roadinfo::hdmap_roadinfo(hdmap_blockmgr* blkmgr)
: container(nullptr), blockmgr(blkmgr)
{
	listnode_init(lanes);
	listnode_init(boundaries);
}

hdmap_roadinfo::~hdmap_roadinfo()
{
	//todo:
	/*
	// reference lane
	vector<refline_fragment*> reference_line;

	// all lanes belongs to the road
	listnode_t lanes;

	// all boundary belongs to the road
	listnode_t boundaries;
	*/
}

hdmap_road::hdmap_road(uint32_t id)
: hdmap_container(hdmap_container_road)
, prev(nullptr), next(nullptr)
, road_id(id)
{
	listnode_init(ownerlist);
}

hdmap_road::~hdmap_road()
{
}

refline_fragment::refline_fragment(odr_road* r, double s,
	hdmap_blockmgr* blkmgr, double eps)
: attrs(0), road(r), s0(s)
, refline_seg(r, blkmgr, eps) {
	odr_map_check(nullptr != r && s0 >= 0);
}

double refline_fragment::length(void) {
	return road->_odrv_road->ref_line->length;
}

void refline_fragment::finalize(hdmap_blockmgr* blkmgr, double eps)
{
	refline_seg.finalize(blkmgr, eps);

	// update the refline data size in the block
	if (road->get_serialized_size()) {
		return;
	}
	assert(refline_seg.blockid >= 0);
	auto& blkinfo = blkmgr->blkinfo_map[refline_seg.blockid];
	size_t sz = road->_odrv_road->ref_line->getsize();
	road->set_serialized_size(sz);
	sz += sizeof(hdmap_datafile_reflseg_data_v1);
	blkinfo.reflseg_total_datasz += sz;
}

curve_segment::curve_segment(odr_road* r, hdmap_blockmgr* blkmgr, double eps)
: attrs(0), road(r), length(0.)
, blockid(-1), datafile_index(-1)
, refline_pts(new vector<refline_point>())
{
	assert(nullptr != refline_pts);
	a.type = curveseg_type_refline;
	listnode_init(reflist);
	if (eps < 1e-3) {
		calculate_blockid(blkmgr);
	}
}

curve_segment::curve_segment(odr_lane* l)
: attrs(0), lane(l), length(0.)
, blockid(-1), datafile_index(-1)
, pts(new vector<lane_point>())
, center_pts(new vector<lane_point>())
{
	assert(nullptr != pts);
	a.type = curveseg_type_lanesec;
	listnode_init(reflist);
}

curve_segment::~curve_segment()
{
	if (a.type == curveseg_type_refline) {
		delete refline_pts;
	} else if (a.type == curveseg_type_lanesec) {
		delete pts;
	}
	else assert(0);
	delete center_pts;
}

int curve_segment::generate_points(hdmap_roadinfo* rinfo,
	hdmap_blockmgr* blkmgr, double eps, bool outer)
{
	if (eps < 1e-3) {
		assert(a.type != curveseg_type_refline);
		if (blockid < 0) {
			blockid = rinfo->get_refline()->refline_seg.blockid;
			assert(blockid >= 0);
		}
		// get and update the blockinfo
		update_blockinfo(blkmgr, blockid);
		return blockid;
	}

	if (a.type == curveseg_type_refline) {
		assert(0);
	}
	else if (a.type == curveseg_type_lanesec) {
		auto odrvlane = lane->_odrv_lane.lock();
		assert(nullptr != odrvlane);
		auto ls = odrvlane->lane_section.lock();
		assert(nullptr != ls);

		s_vals = odrvlane->approximate_border_linear(ls->s0, ls->get_end(), eps, outer);
		gen_approx_linear_lane_points(blkmgr, outer);
	}
	return blockid;
}

double hdmap_lane::calculate_length(hdmap_roadinfo* rinfo)
{
	double ret = 0.;
	auto i = outer.seg_list.next;
	odr_road* prev_road = nullptr;

	for (; i != &outer.seg_list; i = i->next) {
		auto cref = list_entry(curve_segment_ref, ownerlist, i);
		assert(cref->cseg->a.type == curveseg_type_lanesec);

		if (cref->cseg->length < 1e-3) {
			odr_lane_section* ls;
			auto r = rinfo->get_lane_road(cref->lane, ls);
			if (r != prev_road) {
				ret += r->_odrv_road->length;
				prev_road = r;
			}
		}
		else ret += cref->cseg->length;
	}
	return ret;
}

void hdmap_lane::calculate_endpoints(Vec3D& start, Vec3D& end)
{
	assert(!listnode_isempty(outer.seg_list));
	auto sref = list_entry(curve_segment_ref, ownerlist, outer.seg_list.next);
	auto cseg_s = sref->cseg;
	assert(cseg_s->a.type == curveseg_type_lanesec);

	// starting point of the lane
	auto odrvlane = cseg_s->lane->_odrv_lane.lock();
	assert(nullptr != odrvlane);
	auto ls = odrvlane->lane_section.lock();
	double t = odrvlane->outer_border.get(ls->s0);
	t -= odrvlane->lane_width.get(ls->s0) / 2.0;
	start = odrvlane->get_surface_pt(ls->s0, t);

	// ending point of the lane
	auto eref = list_entry(curve_segment_ref, ownerlist, outer.seg_list.prev);
	auto cseg_e = eref->cseg;
	assert(cseg_e->a.type == curveseg_type_lanesec);

	ls = odrvlane->lane_section.lock();
	double s_end = ls->get_end();
	t = odrvlane->outer_border.get(s_end);
	t -= odrvlane->lane_width.get(s_end) / 2.0;
	end = odrvlane->get_surface_pt(s_end, t);	
}

int curve_segment::blockinfo_getmax(void)
{
	int id = -1;
	int max_cnt = -1;
	for (auto i : cseg_blkinfo) {
		if (i.second.count > max_cnt) {
			max_cnt = i.second.count;
			id = i.first;
		}
	}
	assert(id >= 0);
	return id;
}

void curve_segment::blockinfo_addpoint(hdmap_blockmgr* blkmgr,
	int x, int y, int ptidx)
{
	int blkid = y * blkmgr->cols + x;
	auto& blkinfo = cseg_blkinfo[blkid];
	if (blkinfo.ptsegs.size()) {
		auto it = prev(blkinfo.ptsegs.end());
		if (it->pt_start + it->count == ptidx) {
			it->count++;
		}
		else blkinfo.ptsegs.push_back(ptidx);
	}
	else blkinfo.ptsegs.push_back(ptidx);
	blkinfo.count++;
}

int curve_segment::gen_approx_linear_lane_points(hdmap_blockmgr* blkmgr, bool outer)
{
	Vec3D p[2];
	auto odrvlane = lane->_odrv_lane.lock();
	assert(nullptr != odrvlane);

	bool multiblk = blkmgr->is_multiple_blocks();

	uint32_t i = 0; for (const double& s : s_vals)
	{
		double t, w = 0.;
		auto& curr = p[i & 1];
		auto& prev = p[1 - (i & 1)];

		if (outer) {
			t = odrvlane->outer_border.get(s);
			w = odrvlane->lane_width.get(s);
		}
		else t = odrvlane->inner_border.get(s);
		curr = odrvlane->get_surface_pt(s, t);

		// update counter
		if (blockid < 0 && multiblk) {
			int x = int((curr[0] - blkmgr->x0) / blkmgr->block_width);
			int y = int((curr[1] - blkmgr->y0) / blkmgr->block_height);
			blockinfo_addpoint(blkmgr, x, y, int(i));
		}
		// save the point
		pts->push_back({curr[0], curr[1], curr[2], w, s});
		point3d pp3d(curr[0] + _s_xoffset, curr[1] + _s_yoffset, curr[2]);
		if (nullptr != _s_proj) {
			pp3d = _s_proj->transform(pp3d, true);
		}
		printf("%.6f, %.6f, %.6f\n", pp3d.xyz.x, pp3d.xyz.y, pp3d.xyz.z);

		if (outer) {
			curr = odrvlane->get_surface_pt(s, t + w / 2.0);
			center_pts->push_back({curr[0], curr[1], curr[2], w, s});
		}
		
		// calcuate the delta distance
		if (!i++) { continue; }
		double dx = curr[0] - prev[0];
		double dy = curr[1] - prev[1];
		double dz = curr[2] - prev[2];
		length += sqrt(dx * dx + dy * dy + dz * dz);
    }
	printf("---\n");
	// get and update the blockinfo
	if (blockid < 0) {
		blockid = (multiblk) ? blockinfo_getmax() : 0;
	}
	update_blockinfo(blkmgr, blockid);
	return 0;
}

int curve_segment::gen_approx_linear_refline_points(hdmap_blockmgr* blkmgr)
{
	auto refline = road->_odrv_road->ref_line;
	bool multiblk = blkmgr->is_multiple_blocks();

	int i = 0; for (const double& s : s_vals)
	{
		auto p = refline->get_xyz(s);
		// update counter
		if (blockid < 0 && multiblk) {
			int x = int((p[0] - blkmgr->x0) / blkmgr->block_width);
			int y = int((p[1] - blkmgr->y0) / blkmgr->block_height);
			blockinfo_addpoint(blkmgr, x, y, i);
		}
		// save the point
		refline_pts->push_back({p[0], p[1], p[2], s});
		++i;
	}
	// get the block
	if (blockid < 0) {
		blockid = (multiblk) ? blockinfo_getmax() : 0;
	}
	update_blockinfo(blkmgr, blockid);
	return 0;
}

void curve_segment::update_blockinfo(hdmap_blockmgr* blkmgr, int blkid)
{
	assert(blkid >= 0);
	auto& blkinfo = blkmgr->blkinfo_map[blkid];
	if (a.type == curveseg_type_refline) {
		blkinfo.refline_point_count += s_vals.size();
		blkinfo.refline_curveseg_count++;
	}
	else {
		blkinfo.lane_point_count += s_vals.size();
		blkinfo.lane_curveseg_count++;
	}
	// save the block ID
	blockid = blkid;

	if (!cseg_blkinfo.size()) {
		return;
	}

	// handle all non-master blocks after
	// removing master block
	cseg_blkinfo.erase(blkid);
	for (auto& cbi : cseg_blkinfo) {
		auto it = blkmgr->blkinfo_map.find(cbi.first);
		if (it == blkmgr->blkinfo_map.end()) {
			continue;
		}
		auto& bi = it->second;
		auto& cseg_bi = cbi.second;
		if (a.type == curveseg_type_refline) {
			bi.refline_point_count += cseg_bi.count;
			bi.refline_curveseg_count++;
		}
		else {
			bi.lane_point_count += cseg_bi.count;
			bi.lane_curveseg_count++;
		}
	}
}

static int blkmap_getmax(map<int, int>& blkmap)
{
	int id = -1;
	int max_cnt = -1;
	for (auto i : blkmap) {
		if (i.second > max_cnt) {
			max_cnt = i.second, id = i.first;
		}
	}
	assert(id >= 0);
	return id;
}

int curve_segment::calculate_blockid(hdmap_blockmgr* blkmgr)
{
	assert(a.type == curveseg_type_refline);
	map<int, int> blkmap;
	bool multiblk = blkmgr->is_multiple_blocks();
	auto refline = road->_odrv_road->ref_line;

	if (multiblk) for (auto& itr : refline->s0_to_geometry) {
		auto& geometry = itr.second;
		int x = int((geometry->x0 - blkmgr->x0) / blkmgr->block_width);
		int y = int((geometry->y0 - blkmgr->y0) / blkmgr->block_height);
		blkmap[y * blkmgr->cols + x]++;
	}
	blockid = (multiblk) ? blkmap_getmax(blkmap) : 0;
	return 0;
}

int curve_segment::get_lsegid_count(void)
{
	int ret = 0;
	auto i = reflist.next;
	for (; i != &reflist; i = i->next, ++ret);
	return ret;
}

void curve_segment::finalize(hdmap_blockmgr* blkmgr, double eps)
{
	if (a.type == curveseg_type_refline) {
		if (eps > 1e-3) {
			gen_approx_linear_refline_points(blkmgr);
			assert(blockid >= 0);
		}
	}
}

int odr_map::write_hdmap(void)
{
	auto hdmcfg = _psr.get_hdmap_info();
	fprintf(stdout, "\nwriting target file ... ");
	fflush(nullptr);

	// create the file header
	_hdmap = hfm().allocate<hdmap_fileheader_v1>(nullptr,
		(_georef.size() + RMAP_ALIGN_BYTES) & ~(RMAP_ALIGN_BYTES - 1));
	odr_map_check(nullptr != _hdmap);
	memset((void*)_hdmap, 0, sizeof(hdmap_fileheader_v1)
		+ (_georef.size() + RMAP_ALIGN_BYTES) & ~(RMAP_ALIGN_BYTES - 1));
	_hfm.m->set_fixlist(&_hdmap->offset_fixlist);

	// fill in the intial info
	memcpy(_hdmap->magic, HDMAP_MAGIC, sizeof(_hdmap->magic));
	_hdmap->version = HDMAP_VERSION;

	// attributes
	_hdmap->a.persistence = 1;
	_hdmap->a.points_avail = (hdmcfg->hdmap.epsilon > 1e-3) ? 1 : 0;
	_hdmap->a.geometries_avail = (hdmcfg->hdmap.a.gen_geometry) ? 1 : 0;
	_hdmap->a.separate_datafile = (hdmcfg->hdmap.a.use_extdata_file) ? 1 : 0;

	// uuid
	zas::utils::uuid uid;
	uid.generate();
	if (!uid.valid()) {
		return -1;
	}
	_hdmap->uuid = uid;
	_hdmap->size = 0;
	_hdmap->offset_fixlist = 0;

	_hdmap->xoffset = _xoffset;
	_hdmap->yoffset = _yoffset;

	_hdmap->xmin = _x0;
	_hdmap->ymin = _y0;
	_hdmap->xmax = _x1;
	_hdmap->ymax = _y1;
	_hdmap->block_width = _hdm_blkmgr.block_width;
	_hdmap->block_height = _hdm_blkmgr.block_height;
	_hdmap->cols = _hdm_blkmgr.cols;
	_hdmap->rows = _hdm_blkmgr.map_height / _hdm_blkmgr.block_height;
	if ((_hdmap->rows * _hdm_blkmgr.block_height) < _hdm_blkmgr.map_height) {
		_hdmap->rows += 1;
	}
	_hdmap->header_sz = sizeof(hdmap_fileheader_v1) + (_georef.size() + RMAP_ALIGN_BYTES) & ~(RMAP_ALIGN_BYTES - 1);
	_hdmap->roadseg_count = _hdm_road_count;
	_hdmap->lane_count = hdmap_lane::count;
	_hdmap->laneseg_count = hdmap_laneseg::count;
	_hdmap->junction_count = _hdm_junc_count;

	strcpy(_hdmap->proj, _georef.c_str());

	// write it to file
	string path = _psr.get_targetdir();
	path += "hdmap";

	int ret = write_roads();
	if (ret) return ret;

	FILE *fp = fopen(path.c_str(), "wb");
	if (nullptr == fp) {
		fprintf(stderr, "fail to create hdmap file: %s\n", path.c_str());
		return -2;
	}

	ret = hfm().write_data(_hdmap, fp);
	if (ret) return ret;

	if (hdmcfg->hdmap.a.use_extdata_file) {
		ret = write_multiple_data();
		if (ret) return ret;
	}; 
	
	printf("[success]\n");
	return ret;
}

int odr_map::write_multiple_data(void)
{
	int i = 1;
	// write it to file
	string tmp, path = _psr.get_targetdir();
	file_memory* prev = nullptr;
	for (auto& bi : _hdm_blkmgr.blkinfo_map) {
		auto& blkinfo = bi.second;

		if (blkinfo.m->m != prev) {
			char fn[128];
			// remove the file if necessary
			sprintf(fn, "%d.dat", i);
			tmp = path + fn;
			zas::utils::deletefile(tmp.c_str());
			sprintf(fn, "%d-last.dat", i);
			tmp = path + fn;
			zas::utils::deletefile(tmp.c_str());

			sprintf(fn, "%d%s.dat", i, (blkinfo.m->m == _hdm_blkmgr.last_memory)
				? "-last" : "");
			tmp = path + fn;

			FILE *fp = fopen(tmp.c_str(), "wb");
			if (nullptr == fp) {
				fprintf(stderr, "fail to create data file: %s\n", tmp.c_str());
				return -1;
			}

			auto hdr = blkinfo.m->m->get_first<hdmap_datafile_header_v1>();
			hdr->size = blkinfo.m->m->total_size();
			if (blkinfo.m->m->write_file(fp)) {
				return -2;
			}

			prev = blkinfo.m->m, ++i;
		}
	}
	return 0;
}

template <typename T> static void set_arraychunk(
	hdmap_memory* m, chunk<T>& ref, T* arraychunk, int count)
{
	ref.size = sizeof(T) * count;
	m->m->register_fixlist(ref.off, m->m->find_id_byaddr(arraychunk));
}

int odr_map::write_new_curve_segment(curve_segment_ref* csegref, chunk<hdmap_lane_boundary_point_v1>& ref)
{
	auto cseg = csegref->cseg;
	auto& blkinfo = _hdm_blkmgr.blkinfo_map[cseg->blockid];
	auto m = blkinfo.m;
	int sz = cseg->s_vals.size();

	if (cseg->datafile_index >= 0) {
		set_arraychunk(m, ref, &m->lane_pts[cseg->datafile_index], sz);
		return 0;
	}

	// write points
	int i = 0;
	cseg->datafile_index = m->lane_pts_curpos;
	for (auto s : cseg->s_vals) {
		auto& sp = (*cseg->pts)[i++];
		auto& dp = m->lane_pts[m->lane_pts_curpos++];
		dp.pt = sp;
		dp.width = sp.width;
		dp.s = sp.s;
		dp.laneseg_index = csegref->laneseg->index;
	}
	// write the curve segment reference
	auto f_cseg_pts = &m->lane_pts[cseg->datafile_index];
	auto& f_csegref = m->lane_csegs[m->lane_csegs_curpos++];
	set_arraychunk(m, f_csegref.pts, f_cseg_pts, sz);

	int csegref_cnt = cseg->get_lsegid_count();
	f_csegref.laneseg_idcount = csegref_cnt;
	if (csegref_cnt == 1) {
		auto csegref = list_entry(curve_segment_ref, cseg_ownerlist, cseg->reflist.next);
		f_csegref.laneseg_id = csegref->laneseg->index;
	} else {
		auto nd = cseg->reflist.next;
		m->m->register_fixlist(f_csegref.laneseg_ids.off,
			m->m->find_id_byaddr(&m->laneseg_ids[m->laneseg_ids_curpos]));
		for (; nd != &cseg->reflist; nd = nd->next) {
			auto csegref = list_entry(curve_segment_ref, cseg_ownerlist, nd);
			m->laneseg_ids[m->laneseg_ids_curpos++] = csegref->laneseg->index;
		}
	}
	// finally, update the reference
	set_arraychunk(m, ref, f_cseg_pts, sz);
	return 0;
}

int odr_map::write_new_curve_center(curve_segment_ref* csegref, chunk<hdmap_lane_boundary_point_v1>& ref)
{
	auto cseg = csegref->cseg;
	auto& blkinfo = _hdm_blkmgr.blkinfo_map[cseg->blockid];
	auto m = blkinfo.m;
	int sz = cseg->s_vals.size();
	auto f_cseg_pts = m->m->allocate<hdmap_lane_boundary_point_v1>(nullptr,
			sizeof(hdmap_lane_boundary_point_v1) * (sz - 1));

	// write points
	int index = 0;
	for (auto s : cseg->s_vals) {
		auto& sp = (*cseg->center_pts)[index];
		auto& dp = f_cseg_pts[index];
		dp.pt = sp;
		dp.width = sp.width;
		dp.s = sp.s;
		dp.laneseg_index = csegref->laneseg->index;
		index++;
	}

	// finally, update the reference
	set_arraychunk(m, ref, f_cseg_pts, sz);
	return 0;
}

int odr_map::write_new_refline_segment(curve_segment* cseg, chunk<hdmap_refline_point_v1>& ref)
{
	auto& blkinfo = _hdm_blkmgr.blkinfo_map[cseg->blockid];
	auto m = blkinfo.m;

	int sz = cseg->s_vals.size();
	if (sz <= 0) {
		return -EBADPARM;
	}

	auto ref_point = m->m->allocate<hdmap_refline_point_v1>(nullptr,
	sizeof(hdmap_refline_point_v1) * (sz - 1));
	odr_map_check(nullptr != ref_point);

	int i = 0;
	for (auto s : cseg->s_vals) {
		ref_point[i].pt = (*cseg->refline_pts)[i];
		ref_point[i++].s = (*cseg->refline_pts)[i].s;
	}

	set_arraychunk(m, ref, ref_point, sz);
	return 0;
}

int odr_map::write_road_laneseg_data(hdmap_laneseg* lseg, hdmap_file_laneseg_v1* laneseg)
{
	assert(lseg->inner->cseg->blockid == lseg->outer->cseg->blockid);
	int blkid = lseg->inner->cseg->blockid;
	auto& blkinfo = _hdm_blkmgr.blkinfo_map[blkid];
	auto lane = lseg->outer->cseg->lane->_odrv_lane.lock();

	size_t pos, extsz = lseg->outer->cseg->lane->get_serialized_size();
	if (!extsz) { extsz = lane->getsize(); }
	auto lseg_data = blkinfo.m->m->allocate<hdmap_datafile_laneseg_data_v1>(&pos, extsz);
	odr_map_check(nullptr != lseg_data);

	// write the inner and outer curve segment
	write_new_curve_segment(lseg->inner, lseg_data->extinfo.inner_pts);
	write_new_curve_segment(lseg->outer, lseg_data->extinfo.outer_pts);
	write_new_curve_center(lseg->outer, lseg_data->extinfo.center_pts);
	lseg_data->size_inbytes = lane->getsize();
	lane->serialize(lseg_data->sdata);
	hfm().register_fixlist(laneseg->data.off,
			blkinfo.m->m->find_id_byaddr(lseg_data));
	// todo: block info not set
	return 0;
}

int odr_map::write_road_laneseg_info(hdmap_lane *hl, hdmap_file_lane_v1* lane)
{
	auto i = hl->lanesegs.next;
	for (; i != &hl->lanesegs; i = i->next) {
		auto lseg = list_entry(hdmap_laneseg, ownerlist, i);
		auto& laneseg = _hfm.lanesegs[lseg->index];

		// get the starting and ending point
		Vec3D pts, pte;
		hl->calculate_endpoints(pts, pte);
		laneseg.start.pt.set(pts[0], pts[1], pts[2]);
		laneseg.end.pt.set(pte[0], pte[1], pte[2]);
		laneseg.start.laneseg_index = lseg->index;
		laneseg.end.laneseg_index = lseg->index;
		// todo: add kd-tree

		// get the insider and outsider
		if (lseg->insider) {
			hfm().register_fixlist(laneseg.insider.off,
			hfm().find_id_byaddr(&_hfm.lanesegs[lseg->insider->index]));
		}
		else laneseg.insider.set_null();

		if (lseg->outsider) {
			hfm().register_fixlist(laneseg.outsider.off,
			hfm().find_id_byaddr(&_hfm.lanesegs[lseg->outsider->index]));
		}
		else laneseg.outsider.set_null();

		// set the parents
		hfm().register_fixlist(laneseg.lane.off, hfm().find_id_byaddr(lane));

		// create the lane segment data
		write_road_laneseg_data(lseg, &laneseg);
	}
	return 0;
}

int odr_map::write_road_lane_info(hdmap_file_roadseg_v1& rseg, hdmap_roadinfo* rinfo)
{
	Vec3D pts, pte;
	auto j = rinfo->lanes.next;
	size_t lane_cnt = 0;
	for (; j != &rinfo->lanes; j = j->next) { lane_cnt++;}
	if (lane_cnt == 0) { 
		return -ENOTAVAIL;
	}
	auto* rlanes = hfm().allocate<pointer<hdmap_file_lane_v1>>(nullptr,
		sizeof(pointer<hdmap_file_lane_v1>) * (lane_cnt - 1));
	odr_map_check(nullptr != rlanes);
	set_arraychunk(&_hfm, rseg.lanes, rlanes, lane_cnt);

	j = rinfo->lanes.next;
	int lane_index = 0;
	for (; j != &rinfo->lanes; j = j->next, lane_index++) {
		auto hl = list_entry(hdmap_lane, ownerlist, j);

		// allocate the lane object
		auto& lane = _hfm.lanes[hl->index];
		hfm().register_fixlist(lane.roadseg.off, hfm().find_id_byaddr(&rseg));

		if (lane_index < lane_cnt) {
			hfm().register_fixlist(rlanes[lane_index].off,
				hfm().find_id_byaddr(&lane));
		}

		// predecessors
		if (hl->prevs.size() != 0) {
			auto ptbl = (offset*)hfm().allocate(hl->prevs.size(), lane.predecessors);
			odr_map_check(nullptr != ptbl);
			int i = 0; for (auto p : hl->prevs) {
				ptbl[i++].offset = hfm().find_id_byaddr(&_hfm.lanes[p->index]);
			}
		} else lane.predecessors.clear();

		// successors
		if (hl->nexts.size() != 0) {
			auto stbl = (offset*)hfm().allocate(hl->nexts.size(), lane.successors);
			odr_map_check(nullptr != stbl);
			int i = 0; for (auto p : hl->nexts) {
				stbl[i++].offset = hfm().find_id_byaddr(&_hfm.lanes[p->index]);
			}
		} else lane.successors.clear();

		// lane segments
		odr_lane_section* ls;
		auto r = rinfo->get_lane_road(hl->last, ls);
		lane.id = hl->last->_id;
		lane.lanesegs.size = r->_lanes->_lane_sects.getsize()
			* sizeof(hdmap_file_laneseg_v1);

		assert(!listnode_isempty(hl->lanesegs));
		auto fst_lseg = list_entry(hdmap_laneseg, ownerlist, hl->lanesegs.next);
		hfm().register_fixlist(lane.lanesegs.off,
			hfm().find_id_byaddr(&_hfm.lanesegs[fst_lseg->index]));

		// the length of the lane
		lane.length = hl->calculate_length(rinfo);

		// process all lane segments
		write_road_laneseg_info(hl, &lane);
	}
	return 0;
}

int odr_map::write_boundary_points_kdtree(void)
{
	for (auto& bi : _hdm_blkmgr.blkinfo_map) {
		auto& blkinfo = bi.second;
		hdmap_lane_boundary_point_v1* tree = nullptr;
		auto** exm_set = new hdmap_lane_boundary_point_v1* [blkinfo.lane_point_count];
		assert(nullptr != exm_set);

		for (int i = 0; i < blkinfo.lane_point_count; ++i) {
			exm_set[i] = &blkinfo.m->lane_pts[i];
		}
		if (build_kdtree(tree, exm_set,
			blkinfo.lane_point_count, blkinfo.m->m)) {
			return -1;
		}

		// save the kd-tree
		if (_hdm_blkmgr.master_memory == blkinfo.m) {
			hfm().register_fixlist(_hdmap->boundary_points_kdtree.off,
				hfm().find_id_byaddr(tree));
		} else {
			// todo:
		}
		delete []exm_set;
	}
	return 0;
}

int odr_map::write_laneseg_kdtree(void)
{
	assert(hdmap_laneseg::count > 0);
	// create the exam set table
	auto** exm_set = new hdmap_laneseg_point_v1* [hdmap_laneseg::count * 2];
	assert(nullptr != exm_set);

	for (int i = 0; i < hdmap_laneseg::count; ++i) {
		exm_set[i * 2] = &_hfm.lanesegs[i].start;
		exm_set[i * 2 + 1] = &_hfm.lanesegs[i].end;
	}

	hdmap_laneseg_point_v1* tree = nullptr;
	if (build_kdtree(tree, exm_set,
		hdmap_laneseg::count * 2, &hfm())) {
		return -1;
	}

	// set as the kd-tree header
	hfm().register_fixlist(_hdmap->endpoints_kdtree.off,
		hfm().find_id_byaddr(tree));

	delete [] exm_set;
	return 0;	
}

int odr_map::write_road_refline_info(hdmap_file_roadseg_v1& roadseg, hdmap_roadinfo* rinfo)
{
	assert(nullptr != rinfo);
	auto* prefl = (offset*)hfm().allocate<hdmap_datafile_reflseg_data_v1>
		(rinfo->reference_line.size(), roadseg.reflines);

	int i = 0;
	for (auto& refl : rinfo->reference_line) {
		auto& blkinfo = _hdm_blkmgr.blkinfo_map[refl->refline_seg.blockid];
		auto road = refl->refline_seg.road->_odrv_road;

		size_t pos, extsz = refl->refline_seg.road->get_serialized_size();
		if (!extsz) { road->ref_line->getsize(); }
		auto ref_data = blkinfo.m->m->allocate<hdmap_datafile_reflseg_data_v1>(&pos, extsz);

		write_new_refline_segment(&refl->refline_seg, ref_data->extinfo.pts);
		ref_data->size_inbytes = road->ref_line->getsize();
		road->ref_line->serialize(ref_data->sdata);

		hfm().register_fixlist(ref_data->extinfo.roadseg.off,
			hfm().find_id_byaddr(&roadseg));
		prefl[i++].offset = pos;
	}
	return 0;
}

int odr_map::write_roads(void)
{
	allocate_roads();
	auto n = _hdm_roads.next;
	for (int i = 0; n != &_hdm_roads; n = n->next, ++i)
	{
		auto hr = list_entry(hdmap_road, ownerlist, n);
		auto rinfo = hr->roadinfo;
		assert(nullptr != rinfo);

		auto& roadseg = _hfm.roadsegs[i];
		roadseg.s_length = rinfo->get_refline_length();
		if (hr->a.type == hdmap_container_connecting) {
			auto conn = zas_downcast(hdmap_container,
				hdmap_junction_connection, hr);
			roadseg.junction_id = conn->junction_id;
			roadseg.rd_id = conn->road_id;
		} else {
			roadseg.junction_id = -1;
			roadseg.rd_id = hr->road_id;
		}
		roadseg.road_id = rinfo->container->id;
		// rd_id = rinfo->

		if (write_road_refline_info(roadseg, rinfo)) {
			return -1;
		}
		if (write_road_lane_info(roadseg, rinfo)) {
			return -2;
		}
	}
	int ret = write_laneseg_kdtree();
	if (ret) return ret;

	ret = write_boundary_points_kdtree();
	return ret;
}

void odr_map::allocate_block_memory(void)
{
	_hdm_blkmgr.master_memory = &_hfm;
	// if we are not using "extdata-file", create all data memory here
	auto hdmcfg = _psr.get_hdmap_info();
	if (!hdmcfg->hdmap.a.use_extdata_file) {
		// use the master file memory as the block memory
		_hdm_blkmgr.blkinfo_map[0].m = &_hfm;
	}

	size_t total_sz = 0;
	file_memory* prev = nullptr;
	vector<int> datafile_blkids;
	for (auto& blkinfo : _hdm_blkmgr.blkinfo_map) {
		auto& bi = blkinfo.second;
		if (nullptr == bi.m) {
			bool exceed_limit = false;
			total_sz += bi.get_size();
			if (total_sz > hdmcfg->hdmap.filesize_limit) {
				exceed_limit = true, total_sz = 0;
			}
			auto fm = (prev && !exceed_limit) ? prev : nullptr;
			bi.m = new hdmap_memory(fm);
			if (!fm) { init_hdmap_datafile(bi.m); }
			odr_map_check(nullptr != bi.m);
		}
		prev = bi.m->m;

		// curve segment reference object buffer
		if (bi.lane_curveseg_count) {
			bi.m->lane_csegs = bi.m->m->allocate<hdmap_datafile_lane_cseg_v1>(nullptr,
			sizeof(hdmap_datafile_lane_cseg_v1) * (bi.lane_curveseg_count - 1));
			odr_map_check(nullptr != bi.m->lane_csegs);
		}
		// lane points array
		if (bi.lane_point_count) {
			bi.m->lane_pts = bi.m->m->allocate<hdmap_lane_boundary_point_v1>(nullptr,
			sizeof(hdmap_lane_boundary_point_v1) * (bi.lane_point_count - 1));
			odr_map_check(nullptr != bi.m->lane_pts);
		}
		// laneseg ID array
		if (bi.laneseg_id_count) {
			bi.m->laneseg_ids = bi.m->m->allocate<uint64_t>(nullptr,
				sizeof(uint64_t) * (bi.laneseg_id_count - 1));
			odr_map_check(nullptr != bi.m->laneseg_ids);
		}
		// save the last memory allocator
		_hdm_blkmgr.last_memory = bi.m->m;
	}
}

void odr_map::allocate_roads(void)
{
	if (_hdm_road_count) {
		_hfm.roadsegs = hfm().allocate<hdmap_file_roadseg_v1>(nullptr,
			sizeof(hdmap_file_roadseg_v1) * (_hdm_road_count - 1));
		odr_map_check(nullptr != _hfm.roadsegs);
	}
	if (hdmap_lane::count) {
		_hfm.lanes = hfm().allocate<hdmap_file_lane_v1>(nullptr,
			sizeof(hdmap_file_lane_v1) * (hdmap_lane::count - 1));
		odr_map_check(nullptr != _hfm.lanes);
	}

	assert(hdmap_laneseg::count > 0);
	size_t pos;
	_hfm.lanesegs = hfm().allocate<hdmap_file_laneseg_v1>(nullptr,
		sizeof(hdmap_file_laneseg_v1) * (hdmap_laneseg::count - 1));
	odr_map_check(nullptr != _hfm.lanesegs);
	// laneseg存在不被使用的情况，初始化lane为nullptr
	for(int i = 0; i < hdmap_laneseg::count; i++) {
		_hfm.lanesegs[i].lane.ptr = nullptr;
	}
	// allocate memory for all blocks
	allocate_block_memory();
}

void odr_map::hdmap_prepare(void)
{
	_hdm_blkmgr.x0 = _x0, _hdm_blkmgr.y0 = _y0;
	_hdm_blkmgr.map_width = _hdm_blkmgr.block_width = _x1 - _x0;
	_hdm_blkmgr.map_height = _hdm_blkmgr.block_height = _y1 - _y0;
	auto hdmcfg = _psr.get_hdmap_info();
	if (hdmcfg->hdmap.a.use_extdata_file && hdmcfg->hdmap.block_width > 1e-3
		&& hdmcfg->hdmap.block_height > 1e-3) {
		int rows = int(round((_y1 - _y0) / hdmcfg->hdmap.block_height));
		int cols = int(round((_x1 - _x0) / hdmcfg->hdmap.block_width));
		_hdm_blkmgr.block_width /= double(cols);
		_hdm_blkmgr.block_height /= double(rows);

		_hdm_blkmgr.cols = cols;
	}
}

void odr_map::init_hdmap_datafile(hdmap_memory* m)
{
	// create the file header
	auto hdr = m->m->allocate<hdmap_datafile_header_v1>();
	odr_map_check(nullptr != hdr);
	m->m->set_fixlist(&hdr->offset_fixlist);

	// fill in the intial info
	memcpy(hdr->magic, HDMAP_DATA_MAGIC, sizeof(hdr->magic));
	hdr->version = HDMAP_DATA_VERSION;

	// uuid
	hdr->uuid = _hdmap->uuid;
	hdr->size = 0;
	hdr->offset_fixlist = 0;
}

curve_segment_ref::curve_segment_ref(hdmap_blockmgr* blkmgr,
	odr_lane* l, curve_segment* cs)
: lane(l), cseg(cs), laneseg(nullptr) {
	assert(nullptr != l && nullptr != cs);
	listnode_init(ownerlist);

	// increase the laneseg_id count of master block
	assert(cs->blockid >= 0);
	auto blkinfo = &blkmgr->blkinfo_map[cs->blockid];

	int lsegid_count = cs->get_lsegid_count();
	if (lsegid_count == 1) {
		blkinfo->laneseg_id_count += 2;		
	}
	else if (lsegid_count > 1) {
		blkinfo->laneseg_id_count++;
	}

	// increase the laneseg_id count of non-master block
	for (auto& bi : cs->cseg_blkinfo) {
		blkinfo = &blkmgr->blkinfo_map[bi.first];
		if (lsegid_count == 1) {
			blkinfo->laneseg_id_count += 2;		
		}
		else if (lsegid_count > 1) {
			blkinfo->laneseg_id_count++;
		}
	}
	listnode_add(cs->reflist, cseg_ownerlist);
}

hdmap_memory::hdmap_memory(file_memory* memory)
: m(memory), roadsegs(nullptr)
, lanesegs(nullptr)
, lanes(nullptr)
, lane_csegs(nullptr), lane_csegs_curpos(0)
, lane_pts(nullptr), lane_pts_curpos(0)
, laneseg_ids(nullptr), laneseg_ids_curpos(0)
{
	if (nullptr == m) {
		m = new file_memory();
		odr_map_check(nullptr != m);
	}
}

hdmap_blockmgr::~hdmap_blockmgr()
{
	for (auto blkinfo : blkinfo_map) {
		auto& bi = blkinfo.second;
		if (bi.m && bi.m != master_memory) {
			delete bi.m;
			bi.m = nullptr;
		}
	}
}

size_t hdmap_blockinfo::get_size(void)
{
	size_t ret = 0;
	ret += sizeof(hdmap_datafile_refl_cseg_v1) * refline_curveseg_count;
	ret += sizeof(hdmap_datafile_lane_cseg_v1) * lane_curveseg_count;
	ret += sizeof(hdmap_lane_boundary_point_v1) * lane_point_count;
	ret += sizeof(uint64_t) * laneseg_id_count;
	ret += laneseg_total_datasz;
	ret += reflseg_total_datasz;
	return ret;
}

void hdmap_point_v1::set_left(file_memory* m, hdmap_point_v1* l)
{
	assert(nullptr != m);
	if (nullptr != l) {
		m->register_fixlist(left.off, m->find_id_byaddr(l));
	}
	else left.off.reset();
}

void hdmap_point_v1::set_right(file_memory* m, hdmap_point_v1* r)
{
	assert(nullptr != m);
	if (nullptr != r) {
		m->register_fixlist(right.off, m->find_id_byaddr(r));
	}
	else right.off.reset();
}

int hdmap_point_v1::kdtree_x_compare(const void* a, const void* b)
{
	auto* aa = *((hdmap_point_v1**)a);
	auto* bb = *((hdmap_point_v1**)b);
	if (aa->pt.xyz.x > bb->pt.xyz.x) {
		return 1;
	} else if (aa->pt.xyz.x < bb->pt.xyz.x) {
		return -1;
	} else return 0;
}

int hdmap_point_v1::kdtree_y_compare(const void* a, const void* b)
{
	auto* aa = *((hdmap_point_v1**)a);
	auto* bb = *((hdmap_point_v1**)b);
	if (aa->pt.xyz.y > bb->pt.xyz.y) {
		return 1;
	} else if (aa->pt.xyz.y < bb->pt.xyz.y) {
		return -1;
	} else return 0;
}

} // end of namespace osm_parser1
/* EOF */
