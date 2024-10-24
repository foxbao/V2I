/******************************************************************************
 * Copyright 2022 The CIVIP Authors. All Rights Reserved.
 *
 *****************************************************************************/

#include "inc/hdmap-impl.h"
#include <map>
#include <set>
#include <memory>
#include "mapcore/hdmap.h"
#include <sys/time.h>
#include "RefLine.h"
#include "Lanes.h"
#include "Viewer/RoadNetworkMesh.h"
#include "inc/kdtree.h"
#include "utils/timer.h"

namespace zas {
namespace mapcore {

#define CROSS_MULTI(x1, y1, x2, y2) ((x1)*(y2) - (x2)*(y1))
#define LINE_SLOPE(x1, y1, x2, y2) (((y2)-(y1))/((x2)-(x1)))
#define POINT_DISTANCE(x, y) (sqrt((x)*(x) + (y)*(y)))
#define SING(x) (((x) < 0) ? -1 : ((x) > 0))
#define DEG2RAD M_PI / 180
#define RAD2DEG 180 / M_PI


double NormalizeAngle(const double angle) {
  double a = std::fmod(angle + M_PI, 2.0 * M_PI);
  if (a < 0.0) {
    a += (2.0 * M_PI);
  }
  return a - M_PI;
}

double AngleDiff(const double from, const double to) {
  return NormalizeAngle(to - from);
}


using odrview_refline = odr::RefLine;
using odrview_lane = odr::Lane;

hdmap_impl::hdmap_impl()
: _refcnt(0), _buffer(nullptr), _buffer_end(0), _flags(0)
, _roadsegs(nullptr), _lanes(nullptr), _lanesegs(nullptr)
, _laneseg_kdtree(nullptr), _lanebdy_kdtree(nullptr)
{
	listnode_init(_blocklist);
}

hdmap_impl::~hdmap_impl() { release_all(); }

void hdmap_impl::release_all(void)
{
	remove_hdmap_date(-1);
	delete[] _buffer;
	_buffer = nullptr;

	_roadsegs = nullptr;
	_lanes = nullptr;
	_lanesegs = nullptr;
	_laneseg_kdtree = nullptr;
	_lanebdy_kdtree = nullptr;

	_wgs84prj.reset(nullptr, nullptr);
	_f.valid = 0;
}

int hdmap_impl::load_fromfile(const char* filename)
{
	if (!filename || !*filename) {
		return -ENOTFOUND;
	}

	if (nullptr != _buffer) {
		return -ENOTALLOWED;
	}

	// clean the old one and start
	// loading the new one
	reset();
	int ret = loadfile(filename);
	if (ret) {
		reset();
		return ret;
	}
	if (check_validity()) {
		reset();
		return -EINVALID;
	}
	if (fix_ref()) {
		reset();
		return -EINVALID;
	}
	if (update_hdmap_info()) {
		reset();
		return -EINVALID;
	}
	if (update_hdmap_data(-1)) {
		reset();
		return -EINVALID;
	}

	if (finalize()) {
		reset();
		return -EINVALID;
	}

	// drawmap();

	// set success
	_f.valid = 1;
	return 0;
}

int hdmap_impl::finalize(void)
{
	// init the projection
	_wgs84prj.reset(GREF_WGS84, get_georef());
	return 0;
}

int hdmap_impl::drawmap(void)
{
	auto* hdr = header();
	if (nullptr == hdr) {
		return -ENOTAVAIL;
	}
	for (int road_index = 0; road_index < hdr->roadseg_count; road_index++) {
		auto reftbl = _roadsegs[road_index].reflines;
		for (int ref_index = 0; ref_index < reftbl.count; ref_index++) {
			auto& ref_info = reftbl.indices[ref_index]->extinfo;
			size_t pts_sz = ref_info.pts.size
				/ sizeof(hdmap_refline_point_v1);
			for (int i = 0; i < pts_sz; i++) {
				auto& point = ref_info.pts.ptr[i].pt;
				if (point.v[0] > 10000.0 || point.v[0] < -10000.0
					|| point.v[1] > 10000.0 || point.v[1] < -10000.0
					|| point.v[2] > 10000.0 || point.v[2] < -10000.0)
					continue;
				printf("ref point %.6f, %.6f, %.6f\n", point.v[0], point.v[1], point.v[2]);
			}

			auto* odrref = reinterpret_cast<odrview_refline*>
				(reftbl.indices[ref_index]->ptr);
			for (auto it = odrref->s0_to_geometry.begin();
				it!=odrref->s0_to_geometry.end(); ++it) {
				printf("geometry %f, s %f, x %f, y %f,"
					" hdr %f, len %f, type %d\n", it->first,
					it->second->s0, it->second->x0,
					it->second->y0, it->second->hdg0,
					it->second->length, (int)it->second->type);
			}
		}
		size_t road_la_size = _roadsegs[road_index].lanes.size / sizeof(point_hdmap_file_lane_v1);
		for (int la_index = 0; la_index < road_la_size; la_index++) {
			auto& lanesegs = _roadsegs[road_index].lanes.ptr[la_index].ptr->lanesegs;
			size_t road_ls_sz = lanesegs.size / sizeof(hdmap_file_laneseg_v1);
			for (int ls_index = 0; ls_index < road_ls_sz; ls_index++) {
				if (nullptr == lanesegs.ptr[ls_index].lane.ptr) {
					continue;
				}
				auto& seg_info = lanesegs.ptr[ls_index].data.ptr->extinfo;
				size_t inner_sz = seg_info.inner_pts.size / sizeof(hdmap_lane_boundary_point_v1);
				for (int i = 0; i < inner_sz; i++) {
					auto& point = seg_info.inner_pts.ptr[i].pt;
					if (point.v[0] > 10000.0 || point.v[0] < -10000.0
						|| point.v[1] > 10000.0 || point.v[1] < -10000.0
						|| point.v[2] > 10000.0 || point.v[2] < -10000.0)
						continue;
					printf("inner point %.6f, %.6f, %.6f\n", point.v[0], point.v[1], point.v[2]);
				}
				size_t outer_sz = seg_info.outer_pts.size / sizeof(hdmap_lane_boundary_point_v1);
				for (int i = 0; i < outer_sz; i++) {
					auto& point = seg_info.outer_pts.ptr[i].pt;
					if (point.v[0] > 10000.0 || point.v[0] < -10000.0
						|| point.v[1] > 10000.0 || point.v[1] < -10000.0
						|| point.v[2] > 10000.0 || point.v[2] < -10000.0)
						continue;
					printf("outer point %.6f, %.6f, %.6f\n", point.v[0], point.v[1], point.v[2]);
				}
				auto* odrlane = reinterpret_cast<odrview_lane*>
					(lanesegs.ptr[ls_index].data.ptr->ptr);
				auto& lanew = odrlane->lane_width.s0_to_poly;
				for (auto it = lanew.begin(); it != lanew.end(); ++it) {
					printf("lane %f, a %f, b %f, c %f, d %f\n", it->first,
						it->second.a, it->second.b, it->second.c, it->second.d);
				}
			}
		}
	}
	return 0;
}

int hdmap_impl::check_validity(void)
{
	auto* hdr = header();
	if (nullptr == hdr) {
		return -ENOTAVAIL;
	}
	if (memcmp(hdr->magic, HDMAP_MAGIC, sizeof(hdr->magic))) {
		return -1;
	}
	// check version
	if (hdr->version > HDMAP_VERSION) {
		return -2;
	}
	if (hdr->a.persistence) {
		// hdrmap_file_rendermap_v1
		_buffer_end = ((size_t)hdr) + hdr->size;
	} else {
		// todo
	}
	return 0;
}

int hdmap_impl::fix_ref(void)
{
	auto* hdr = header();
	if (nullptr == hdr) {
		return -ENOTAVAIL;
	}
	uint32_t link = hdr->offset_fixlist;
	while (0 != link) {
		auto* offobj = (offset*)&_buffer[link];
		link = offobj->link;
		auto* fixobj = (uint64_t*)offobj;
		*fixobj = (uint64_t)&_buffer[offobj->offset];
	}
	return 0;
}

int hdmap_impl::update_hdmap_info(void)
{
	auto* hdr = header();
	if (nullptr == hdr) {
		return -ENOTAVAIL;
	}
	_roadsegs = reinterpret_cast<hdmap_file_roadseg_v1*>(_buffer + hdr->header_sz);
	_lanes = reinterpret_cast<hdmap_file_lane_v1*>(_roadsegs
		+ hdr->roadseg_count);
	_lanesegs = reinterpret_cast<hdmap_file_laneseg_v1*>(_lanes
		+ hdr->lane_count);
	_laneseg_kdtree = hdr->endpoints_kdtree.ptr;
	_lanebdy_kdtree = hdr->boundary_points_kdtree.ptr;
	return 0;
}

int hdmap_impl::update_hdmap_data(int blockid)
{
	if (nullptr == _roadsegs || nullptr == _lanesegs) {
		return -ENOTAVAIL;
	}

	// todo check block id

	auto* hdr = header();
	if (nullptr == hdr) {
		return -ENOTAVAIL;
	}
	for (int road_index = 0; road_index < hdr->roadseg_count; road_index++) {
		auto reftbl = _roadsegs[road_index].reflines;
		for (int ref_index = 0; ref_index < reftbl.count; ref_index++) {
			size_t datasz = reftbl.indices[ref_index]->size_inbytes;
			void* buf = reinterpret_cast<void*>(reftbl.indices[ref_index]->sdata);
			reftbl.indices[ref_index]->ptr = 
				reinterpret_cast<void*>(new odrview_refline(&buf));
		}
	}
	for (int ls_index = 0; ls_index < hdr->laneseg_count; ls_index++) {
		auto& laneseg = _lanesegs[ls_index];
		if (laneseg.lane.ptr == nullptr) {
			continue;;
		}
		size_t datasz = laneseg.data.ptr->size_inbytes;
		void* buf = reinterpret_cast<void*>(&laneseg.data.ptr->sdata);
		laneseg.data.ptr->ptr = reinterpret_cast<void*>(new odrview_lane(&buf));
	}
	// for (int ls_index = 0; ls_index < hdr->laneseg_count; ls_index++) {	
	// 	auto& laneseg = _lanesegs[ls_index];
	// 	if (laneseg.lane.ptr == nullptr) {
	// 		continue;;
	// 	}
	// 	if (ls_index == 865) {
	// 		printf("test\n");
	// 	}
	// 	auto* tmplane = reinterpret_cast<odr_lane*>(laneseg.data.ptr->ptr);
	// 	int tmpid = tmplane->id;
	// 	auto* pts = laneseg.data.ptr->extinfo.outer_pts.ptr;
	// 	auto ptsz = laneseg.data.ptr->extinfo.outer_pts.size / sizeof(hdmap_lane_boundary_point_v1);
	// 	auto* rseg = laneseg.lane.ptr->roadseg.ptr;
	// 	if (nullptr == rseg) { continue;}

	// 	point3d centerpt;
	// 	auto* ls_v = new std::map<double, point3d>();
	// 	for (int outer_index = 0; outer_index < ptsz; outer_index ++) {
	// 		auto* bp = &pts[outer_index];
	// 		point3d rpt;
	// 		double s = bp->s;
	// 		if (get_refline_point(rseg, bp->s, rpt)) {
	// 			continue;
	// 		}
	// 		if (get_center_point(bp, rpt, centerpt)) {
	// 			continue;
	// 		}
	// 		ls_v->insert(std::pair<double,point3d>(s, centerpt));
	// 	}
	// 	_lanesegs_center[ls_index] = ls_v;
	// }
	return 0;
}

int hdmap_impl::get_center_point(hdmap_lane_boundary_point_v1* bpt, point3d& rpt, point3d& pt)
{
	double w = bpt->width;
	if (fabs(bpt->pt.xyz.x - rpt.xyz.x) < 10E-15 && fabs(bpt->pt.xyz.y - rpt.xyz.y) < 10E-15 ) {
		pt.xyz.x = rpt.xyz.x;
		pt.xyz.y = rpt.xyz.y;
	} else {
		double lendis = POINT_DISTANCE(bpt->pt.xyz.x - rpt.xyz.x, bpt->pt.xyz.y - rpt.xyz.y);
		double dlenx = bpt->pt.xyz.x - rpt.xyz.x;
		pt.xyz.x = rpt.xyz.x + SING(dlenx)*(fabs(dlenx) - fabs((bpt->pt.xyz.x - rpt.xyz.x)* w / 2 / lendis));
		double dleny = bpt->pt.xyz.y - rpt.xyz.y;
		pt.xyz.y = rpt.xyz.y + SING(dleny)*(fabs(dleny) - fabs((bpt->pt.xyz.y - rpt.xyz.y)* w / 2 / lendis));
	}
	return 0;
}

int hdmap_impl::remove_hdmap_date(int blockid)
{
	if (nullptr == _roadsegs || nullptr == _lanesegs) {
		return -ENOTAVAIL;
	}

	// todo check block id

	auto* hdr = header();
	if (nullptr == hdr) {
		return -ENOTAVAIL;
	}
	for (int road_index = 0; road_index < hdr->roadseg_count; road_index++) {
		auto reftbl = _roadsegs[road_index].reflines;
		for (int ref_index = 0; ref_index < reftbl.count; ref_index++) {
			auto* refline = reinterpret_cast<odrview_refline*>
				(reftbl.indices[ref_index]->ptr);
			delete refline;
		}
	}
	for (int ls_index = 0; ls_index < hdr->laneseg_count; ls_index++) {
		auto& laneseg = _lanesegs[ls_index];
		if (laneseg.lane.ptr == nullptr) {
			continue;;
		}
		size_t datasz = laneseg.data.ptr->size_inbytes;
		auto* lane = reinterpret_cast<odrview_lane*>(laneseg.data.ptr->ptr);
		delete lane;
	}
	return 0;
}

int hdmap_impl::loadfile(const char* filename)
{
	FILE* fp = fopen(filename, "rb");
	if (nullptr == fp) {
		return -EFAILTOLOAD;
	}

	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);
	if (sz < sizeof(hdmap_fileheader_v1)) {
		fclose(fp);
		return -EINVALID;
	}

	// load the file to memory
	assert(nullptr == _buffer);
	_buffer = new uint8_t[sz];
	if (nullptr == _buffer) {
		fclose(fp);
		return -ENOMEMORY;
	}

	rewind(fp);
	if (sz != fread(_buffer, 1, sz, fp)) {
		fclose(fp);
		return -EINVALID;
	}

	fclose(fp);
	return 0;
}

int hdmap_impl::find_laneseg(double x, double y, int count,
	vector<hdmap_nearest_laneseg>& set)
{
	if (nullptr == _laneseg_kdtree) {
		return -ENOTAVAIL;
	}

	if (!count) {
		return -EBADPARM;
	}

	auto* hdr = header();
	if (nullptr == hdr) {
		return -ENOTAVAIL;
	}
	if (!hdr->a.persistence) {
		return -ENOTSUPPORT;
	}

	if (!hdr->laneseg_count) {
		return -ENOTFOUND;
	}
	if (count > hdr->laneseg_count) {
		count = hdr->laneseg_count;
	}

	if (x < hdr->xmin || x > hdr->xmax || y < hdr->ymin || y > hdr->ymax) {
		return -ENOTFOUND;
	}

	for (int i = 0; i < count; ++i) {
		hdmap_nearest_laneseg item;
		auto* langseg = kdtree2d_search(_laneseg_kdtree,
			x, y, item.distance);
		item.laneseg = reinterpret_cast<hdmap_laneseg*>(&(_lanesegs[langseg->laneseg_index]));
		set.push_back(item);
	}

	// restore "selected" flags
	for (auto& item : set) {
		if (nullptr == item.laneseg) {
			continue;
		}
		auto* langseg = reinterpret_cast<hdmap_laneseg_point_v1*>
			(item.laneseg);
		if (nullptr == langseg) {
			return -ENOTAVAIL;
		}
		langseg->attrs.KNN_selected = 0;
	}
	
	return count;
}

int hdmap_impl::get_laneseg_width(const hdmap_laneseg* from, const hdmap_laneseg *to, double s, double& fwidth, double& twidth, double& bwidth)
{
	if (nullptr == from && nullptr == to) {
		return -EBADPARM;
	}
	auto* fls = reinterpret_cast<const hdmap_file_laneseg_v1*>(from);
	auto* tls = reinterpret_cast<const hdmap_file_laneseg_v1*>(to);
	
	if (fls) {
		fls = get_laneseg_center_width(fls, s, fwidth);
	}
	if (tls) {
		tls = get_laneseg_center_width(tls, s, twidth);
	}

	if (fls && tls) {
		auto* fodrlane = reinterpret_cast<odrview_lane*>(fls->data.ptr->ptr);
		int fodrid = fodrlane->id;
		auto* todrlane = reinterpret_cast<odrview_lane*>(tls->data.ptr->ptr);
		int todrid = todrlane->id;
		if (fodrid * todrid < 0) {
			return -ENOTALLOWED;
		}
		bwidth = 0.0;
		if (fodrid == todrid) {
			bwidth = 0.0;
		}
		else if (((fodrid > 0 || todrid > 0) && (fodrid > todrid))
		|| ((fodrid < 0 || todrid < 0) && (fodrid < todrid))) {
			int odrid = fodrid;
			double tmpwidth = 0.0;
			auto* ls = fls;
			while (odrid != todrid) {
				ls = ls->insider.ptr;
				ls = get_laneseg_center_width(ls, s, tmpwidth);
				if (ls == nullptr) {
					return -ELOGIC;
				}
				auto* odrlane = reinterpret_cast<odrview_lane*>(ls->data.ptr->ptr);
				odrid = odrlane->id;
				if (odrid != todrid) {
					bwidth += tmpwidth;
				}
			}
		}
		else if (((fodrid > 0 || todrid > 0) && (fodrid < todrid))
		|| ((fodrid < 0 || todrid < 0) && (fodrid > todrid))) {
			int odrid = fodrid;
			double tmpwidth = 0.0;
			auto* ls = fls;
			while (odrid != todrid) {
				ls = ls->outsider.ptr;
				ls = get_laneseg_center_width(ls, s, tmpwidth);
				if (ls == nullptr) {
					return -ELOGIC;
				}
				auto* odrlane = reinterpret_cast<odrview_lane*>(ls->data.ptr->ptr);
				odrid = odrlane->id;
				if (odrid != todrid) {
					bwidth += tmpwidth;
				}
			}
		}
		
	}

	return 0;
}

const hdmap_road* hdmap_impl::get_road_by_id(uint32_t rid)
{
	auto* hdr = header();
	if (nullptr == hdr) {
		return nullptr;
	}
	auto rd_cnt = hdr->roadseg_count;
	for (int i = 0; i < rd_cnt; i++) {
		if (rid == _roadsegs[i].rd_id) {
			return reinterpret_cast<const hdmap_road*>(&_roadsegs[i]);
		}
	}
	return nullptr;
}

int hdmap_impl::generate_lanesect_transition(const char* filepath) {
	zas::utils::uri jsonuri(filepath);
	zas::utils::jsonobject& lanetransjobj = zas::utils::json::loadfromfile(jsonuri, nullptr);
	if (lanetransjobj.is_array()) {
		for (size_t i = 0; i < lanetransjobj.count(); i++) {
			std::shared_ptr<hdmap_lanesect_transition> lscts = std::make_shared<hdmap_lanesect_transition>();

			zas::utils::jsonobject& itemjobj = lanetransjobj.get(i);
			int lanesec_index = itemjobj.get("lanesec_index").to_number();
			lscts->lanesec_index = lanesec_index;
			int road_id = itemjobj.get("road_id").to_number();
			lscts->road_id = road_id;
			double length = itemjobj.get("length").to_double();
			lscts->length = length;

			zas::utils::jsonobject& prevjobj = itemjobj.get("lsc_prev_index");
			if (!prevjobj.is_null()) {
				for (size_t j = 0; j < prevjobj.count(); j ++) {
					int previndex = prevjobj.get(j).get("prev_index").to_number();
					lscts->lsc_prev_index.insert(previndex);
				}
			}

			zas::utils::jsonobject& nextjobj = itemjobj.get("lsc_next_index");
			if (!nextjobj.is_null()) {
				for (size_t j = 0; j < nextjobj.count(); j ++) {
					int nextindex = nextjobj.get(j).get("next_index").to_number();
					lscts->lsc_next_index.insert(nextindex);
				}
			}

			zas::utils::jsonobject& leftjobj = itemjobj.get("lsc_left_index");
			if (!leftjobj.is_null() && leftjobj.is_array()) {
				for (size_t j = 0; j < leftjobj.count(); j++) {
					int key = leftjobj.get(j).get("key").to_number();
					int value = leftjobj.get(j).get("value").to_number();
					lscts->lsc_left_index[key] = value;
				}
			}

			zas::utils::jsonobject& rightjobj = itemjobj.get("lsc_right_index");
			if (!rightjobj.is_null() && rightjobj.is_array()) {
				for (size_t j = 0; j < rightjobj.count(); j++) {
					int key = rightjobj.get(j).get("key").to_number();
					int value = rightjobj.get(j).get("value").to_number();
					lscts->lsc_right_index[key] = value;
				}
			}
			_lsc_transitions[lanesec_index] = lscts;
		}
	}
	return 0;
}

bool hdmap_impl::is_in_lane(const hdmap_laneseg *hdls, double s, point3d &pt)
{
	auto* ls = reinterpret_cast<const hdmap_file_laneseg_v1*>(hdls);
	if (nullptr == ls) { return false;}
	hdmap_lane_boundary_point_v1* outstart = nullptr;
	hdmap_lane_boundary_point_v1* outend = nullptr;
	size_t lssz = ls->data.ptr->extinfo.outer_pts.size / sizeof(hdmap_lane_boundary_point_v1);
	auto* lscenter = ls->data.ptr->extinfo.outer_pts.ptr;
	int index = 0;
	for (index = 0; index < lssz; index++) {
		if (nullptr == outstart) {
			outstart = &lscenter[index];
			continue;
		}
		outend = &lscenter[index];
		if (s < outend->s) {
			break;
		}
		outstart = outend;
	}
	if (index == lssz) {
		return false;
	}
	hdmap_lane_boundary_point_v1* innerstart = nullptr;
	hdmap_lane_boundary_point_v1* innerend = nullptr;
	lssz = ls->data.ptr->extinfo.inner_pts.size / sizeof(hdmap_lane_boundary_point_v1);
	lscenter = ls->data.ptr->extinfo.inner_pts.ptr;
	for (index = 0; index < lssz; index++) {
		if (nullptr == innerstart) {
			innerstart = &lscenter[index];
			continue;
		}
		innerend = &lscenter[index];
		if (s < innerend->s) {
			break;
		}
		innerstart = innerend;
	}
	if (index == lssz) {
		return false;
	}
	double x1 = pt.xyz.x - outstart->pt.xyz.x;
	double y1 = pt.xyz.y - outstart->pt.xyz.y;
	double x2 = outend->pt.xyz.x - outstart->pt.xyz.x;
	double y2 = outend->pt.xyz.y - outstart->pt.xyz.y;
	double cross_m1 = CROSS_MULTI(x1, y1, x2, y2);
	x1 = pt.xyz.x - innerstart->pt.xyz.x;
	y1 = pt.xyz.y - innerstart->pt.xyz.y;
	x2 = innerend->pt.xyz.x - innerstart->pt.xyz.x;
	y2 = innerend->pt.xyz.y - innerstart->pt.xyz.y;
	double cross_m2 = CROSS_MULTI(x1, y1, x2, y2);
	int right1 = 0;
	if (cross_m1 > 10e-15) {
		right1 = 1;
	} else {
		right1 = -1;
	}
	int right2 = 0;
	if (cross_m2 > 10e-15) {
		right2 = 1;
	} else {
		right2 = -1;
	}
	if (right1 * right2 > 0) {
		return false;
	}
	return true;
}

bool hdmap_impl::is_same_road(const hdmap_laneseg *lss, const hdmap_laneseg *lsd) 
{
	auto* ls1 = reinterpret_cast<const hdmap_file_laneseg_v1*>(lss);
	auto* ls2 = reinterpret_cast<const hdmap_file_laneseg_v1*>(lsd);
	if (nullptr == ls1 || nullptr == ls1) { return false;}
	if (ls1->lane.ptr == nullptr || ls2->lane.ptr == nullptr) {
		return false;
	}
	auto* roads = ls1->lane.ptr->roadseg.ptr;
	auto* roadd = ls2->lane.ptr->roadseg.ptr;
	if (roads == roadd) {
		return true;
	}
	return false;
}

const hdmap_laneseg* hdmap_impl::find_laneseg(point3d& src, double &hdg,
	hdmap_segment_point& centet_line_pt, std::vector<point3d> *line)
{
	double s;
	auto* ls = get_laneseg(src, hdg, s);
	auto* laneseg = get_laneseg_center_projection(ls, src, centet_line_pt, hdg, s, line);
	return reinterpret_cast<const hdmap_laneseg*>(laneseg);
}

std::vector<uint64_t> hdmap_impl::get_lanes_near_pt_enu(Eigen::Vector3d p3d, double radius)
{
	std::vector<uint64_t> tmp;

	if (nullptr == _lanebdy_kdtree) {
		return tmp;
	}

	auto* hdr = header();
	if (nullptr == hdr) {
		return tmp;
	}
	if (!hdr->a.persistence) {
		return tmp;
	}
	if (!hdr->laneseg_count) {
		return tmp;
	}

	if (p3d[0] < hdr->xmin || p3d[0] > hdr->xmax
		|| p3d[1] < hdr->ymin || p3d[1] > hdr->ymax) {
		return tmp;
	}

	double distance = 0.0;
	point3d src(p3d[0], p3d[1]);
	point3d des;
	hdmap_lane_boundary_point_v1* lane_seg_point = nullptr;
	std::vector<hdmap_lane_boundary_point_v1*> lanept;
	for (int i = 0; i < hdr->laneseg_count; i++) {
		auto* ls = &_lanesegs[i];
		if (nullptr == ls || nullptr == ls->data.ptr) {
			continue;
		}

		if (nullptr == ls->lane.ptr) {
			continue;
		}

		size_t lssz = ls->data.ptr->extinfo.center_pts.size / sizeof(hdmap_lane_boundary_point_v1);
		auto* lscenter = ls->data.ptr->extinfo.center_pts.ptr;
		int index = 0;
		for (index = 0; index < lssz; index++) {
			if (index + 1 == lssz) {
				break;
			}
			hdmap_lane_boundary_point_v1* start = &lscenter[index];
			hdmap_lane_boundary_point_v1* end  = &lscenter[index+1];
			get_line_projection_distance(&start->pt, &end->pt, src, des, distance);
			if(distance < radius) {
				if (!std::count(tmp.begin(), tmp.end(), i)) {
					tmp.push_back(i);
				}
				break;
			}
		}
	}

	for (auto& item : lanept) {
		item->attrs.KNN_selected = 0;
	}

	return tmp;
}

std::vector<uint64_t> hdmap_impl::get_lanes_near_pt_enu(std::vector<Eigen::Vector3d> trajectory, double radius)
{
	std::vector<uint64_t> tmp;

	if (nullptr == _lanebdy_kdtree) {
		return tmp;
	}

	auto* hdr = header();
	if (nullptr == hdr) {
		return tmp;
	}
	if (!hdr->a.persistence) {
		return tmp;
	}
	if (!hdr->laneseg_count) {
		return tmp;
	}

	for (size_t sz = 0; sz < trajectory.size(); sz++) {
		Eigen::Vector3d p3d = trajectory[sz];

		std::vector<uint64_t> laneids = get_lanes_near_pt_enu(p3d, radius);
		if (laneids.size() > 0) {
			for (size_t i = 0; i < laneids.size(); i++) {
				vector<uint64_t>::iterator it;
				it=find(tmp.begin(),tmp.end(),laneids[i]);
				if (it==tmp.end()){
					tmp.push_back(laneids[i]);
				}
			}
		}
		
	}
	return tmp;
}

std::vector<uint64_t> hdmap_impl::get_lanes_near_pt_enu(std::vector<Pose> trajectory, double radius)
{
	std::vector<uint64_t> tmp;
	return tmp;
}

Eigen::MatrixXd hdmap_impl::CalculateTransitionalProbability(std::vector<uint64_t> lanes_id)
{
	Eigen::MatrixXd transition_matrix;

	size_t num_states = lanes_id.size();
	transition_matrix = Eigen::MatrixXd(num_states, num_states);
	transition_matrix.setZero(num_states, num_states);
	for (size_t i = 0; i < num_states; i++) {
		uint64_t lane_id = lanes_id[i];
		if (lane_id == 971) {
			printf("hello.\n");
		}
		for (size_t j = 0; j < num_states; j++) {
			uint64_t lane_id2 = lanes_id[j];
			if (i == j) {
				transition_matrix(i, j) = 1.;
			}
			else {
				if (_lsc_transitions.empty()) {
					transition_matrix(i, j) = 0.;
				}
				else {
					std::shared_ptr<hdmap_lanesect_transition> lanesect = _lsc_transitions[lane_id];
					if (lanesect != nullptr) {
						if (lanesect->lsc_next_index.count(lane_id2) > 0) {
							transition_matrix(i, j) = 0.7;
						} 
						if (lanesect->lsc_left_index.size() > 0) {
							if (lane_id2 == lanesect->lsc_left_index[0]) {
								transition_matrix(i, j) = 0.7;
							}
						} 
						if (lanesect->lsc_right_index.size() > 0) {
							if (lane_id2 == lanesect->lsc_right_index[0]) {
								transition_matrix(i, j) = 0.7;
							} 
						} 
					} else {
						transition_matrix(i, j) = 0.;
					}
				}
			}
		}
		double sum = transition_matrix.block(i, 0, 1, num_states).sum();
		if (sum > 0) {
			transition_matrix.block(i, 0, 1, num_states) /= sum;
		}
	}

	return transition_matrix;
}

Eigen::MatrixXd hdmap_impl::CalculateTransitionalProbability(std::vector<uint64_t> lanes_id_1,std::vector<uint64_t> lanes_id_2)
{
	Eigen::MatrixXd transition_matrix;

	size_t num_states1 = lanes_id_1.size();
	size_t num_states2 = lanes_id_2.size();
	transition_matrix = Eigen::MatrixXd(num_states1, num_states2);
	transition_matrix.setZero(num_states1, num_states2);

	for (size_t i = 0; i < num_states1; i++) {
		uint64_t lane_id1 = lanes_id_1[i];
		for (size_t j = 0; j < num_states2; j++) {
			uint64_t lane_id2 = lanes_id_2[j];
			if (lane_id1 == lane_id2) {
				transition_matrix(i, j) = 1.;
			} else {
				std::shared_ptr<hdmap_lanesect_transition> lanesect = _lsc_transitions[lane_id1];
				if (lanesect != nullptr) {
					if (lanesect->lsc_next_index.count(lane_id2) > 0) {
						transition_matrix(i, j) = 0.7;
					} 
					if (lanesect->lsc_left_index.size() > 0) {
						if (lane_id2 == lanesect->lsc_left_index[0]) {
							transition_matrix(i, j) = 0.7;
						}
					} 
					if (lanesect->lsc_right_index.size() > 0) {
						if (lane_id2 == lanesect->lsc_right_index[0]) {
							transition_matrix(i, j) = 0.7;
						} 
					} 
				} 
			}
		}
		double sum = transition_matrix.block(i, 0, 1, num_states2).sum();
		if(sum > 0) {
			transition_matrix.block(i, 0, 1, num_states2) /= sum;
		}
	}

	return transition_matrix;
}

double hdmap_impl::calcLaneProbability(double num, uint64_t lane_id1, uint64_t lane_id2)
{
	
	return 0.;
}

double hdmap_impl::get_distance_pt_curve_enu(const Eigen::Vector3d &pt_enu, uint64_t lane_id, Eigen::Vector3d &cross_pt_curve)
{
	auto* hdr = header();
	if (nullptr == hdr) {
		return -1;
	}
	if (!hdr->a.persistence) {
		return -2;
	}
	if (!hdr->laneseg_count) {
		return -3;
	}
	if (lane_id >= hdr->laneseg_count) {
		printf("lane seg point data error. point index %lu\n", lane_id);
		return -4;
	}
	auto* ls = &_lanesegs[lane_id];
	if (nullptr == ls || nullptr == ls->data.ptr) {
		return -5;
	}

	double distance = 0.;
	double s = 0.;
	point3d src(pt_enu[0],pt_enu[1]);
	point3d des;
	hdmap_lane_boundary_point_v1* start = nullptr;
	hdmap_lane_boundary_point_v1* end = nullptr;
	size_t lssz = ls->data.ptr->extinfo.center_pts.size / sizeof(hdmap_lane_boundary_point_v1);
	auto* lscenter = ls->data.ptr->extinfo.center_pts.ptr;
	int index = 0;
	for (index = 0; index < lssz; index++) {
		if (index + 1 == lssz) {
			break;
		}
		start = &lscenter[index];
		end  = &lscenter[index+1];
		get_line_projection_distance(&start->pt, &end->pt, src, des, s);
		if (index == 0) {
			distance = s;
			cross_pt_curve[0] = des.xyz.x;
			cross_pt_curve[1] = des.xyz.y;
			cross_pt_curve[2] = des.xyz.z;
		}
		else if (s < distance) {
			distance = s;
			cross_pt_curve[0] = des.xyz.x;
			cross_pt_curve[1] = des.xyz.y;
			cross_pt_curve[2] = des.xyz.z;
		}
	}
	return distance;
}

std::vector<Curve> hdmap_impl::get_curves_enu(CurveType type)
{
	std::vector<Curve> p3d_all;
	auto* hdr = header();
	if (nullptr == hdr) {
		return p3d_all;
	}
	if (!hdr->a.persistence) {
		return p3d_all;
	}
	if (!hdr->laneseg_count) {
		return p3d_all;
	}
	for (uint64_t lane_index = 0; lane_index < hdr->laneseg_count; lane_index++) {
		Curve curve;
		vector<Eigen::Vector3d> p3ds = get_curve(lane_index,type);
		int aaa=p3ds.size();
		vector<Eigen::Vector3d> p3ds_center = get_curve(lane_index,CurveType::center_curve);
		int bbb=p3ds.size();
		curve.id = lane_index;
		curve.p3d = p3ds;
		p3d_all.push_back(curve);
	}
	return p3d_all;
}

/// @brief 获取lane里面的曲线，可以设定中间线、内侧线和外侧线
/// @param lane_id 
/// @param type Center line, inner boundary or outer boundary
/// @return std::vector<Eigen::Vector3d> p3ds
std::vector<Eigen::Vector3d> hdmap_impl::get_curve(uint64_t lane_id,CurveType type)
{
	std::vector<Eigen::Vector3d> p3ds;
	auto* hdr = header();
	if (nullptr == hdr) {
		return p3ds;
	}
	if (!hdr->a.persistence) {
		return p3ds;
	}
	if (!hdr->laneseg_count) {
		return p3ds;
	}
	if (lane_id >= hdr->laneseg_count) {
		printf("lane seg point data error. point index %lu\n", lane_id);
		return p3ds;
	}
	auto* ls = &_lanesegs[lane_id];
	if (nullptr == ls || nullptr == ls->data.ptr) {
		return p3ds;
	}

	if (nullptr == ls->data.ptr) {
		return p3ds;
	}

	if (nullptr == ls->lane.ptr) {
		return p3ds;
	}
	osm_parser1::chunk<hdmap_lane_boundary_point_v1> *pts = nullptr;
	size_t ptsz = 0;

	if (type == CurveType::center_curve) {
		pts = &ls->data.ptr->extinfo.center_pts;
		ptsz = ls->data.ptr->extinfo.center_pts.size / sizeof(hdmap_lane_boundary_point_v1);
	}
	else if (type == CurveType::inner_boundary)
	{
		pts = &ls->data.ptr->extinfo.inner_pts;
		ptsz = ls->data.ptr->extinfo.inner_pts.size / sizeof(hdmap_lane_boundary_point_v1);
	}
	else if (type == CurveType::outer_boundary)
	{
		pts = &ls->data.ptr->extinfo.outer_pts;
		ptsz = ls->data.ptr->extinfo.outer_pts.size / sizeof(hdmap_lane_boundary_point_v1);
	}
	else
	{
		pts = &ls->data.ptr->extinfo.center_pts;
		ptsz = ls->data.ptr->extinfo.center_pts.size / sizeof(hdmap_lane_boundary_point_v1);
	}
	
	
	for (size_t i = 0; i < ptsz; i++) {
		Eigen::Vector3d p3d;
		p3d[0] = pts->ptr[i].pt.xyz.x;
		p3d[1] = pts->ptr[i].pt.xyz.y;
		p3d[2] = pts->ptr[i].pt.xyz.z;
		p3ds.push_back(p3d);
	}
	return p3ds;
}

double hdmap_impl::get_distance_pt_closest_central_line_enu(const Eigen::Vector3d &pt_enu, Eigen::Vector3d &cross_pt_map_enu)
{
	if (nullptr == _lanebdy_kdtree) {
		return -1;
	}

	auto* hdr = header();
	if (nullptr == hdr) {
		return -2;
	}
	if (!hdr->a.persistence) {
		return -3;
	}
	if (!hdr->laneseg_count) {
		return -4;
	}

	if (pt_enu[0] < hdr->xmin || pt_enu[0] > hdr->xmax
		|| pt_enu[1] < hdr->ymin || pt_enu[1] > hdr->ymax) {
		return -5;
	}

	double minidis = 0.0;
	double distance = 0.0;
	point3d src(pt_enu[0],pt_enu[1]);
	point3d des;
	uint64_t lane_index = 0;
	hdmap_lane_boundary_point_v1* lane_seg_point = nullptr;
	std::vector<hdmap_lane_boundary_point_v1*> lanept;
	uint64_t point_index = 0;;
	for (int i = 0; i < hdr->laneseg_count; i++) {
		auto* ls = &_lanesegs[i];
		if (nullptr == ls || nullptr == ls->data.ptr) {
			continue;
		}

		if (nullptr == ls->lane.ptr) {
			continue;
		}

		size_t lssz = ls->data.ptr->extinfo.center_pts.size / sizeof(hdmap_lane_boundary_point_v1);
		auto* lscenter = ls->data.ptr->extinfo.center_pts.ptr;
		int index = 0;
		for (index = 0; index < lssz; index++) {
			if (index + 1 == lssz) {
				break;
			}
			hdmap_lane_boundary_point_v1* start = &lscenter[index];
			hdmap_lane_boundary_point_v1* end  = &lscenter[index+1];
			get_line_projection_distance(&start->pt, &end->pt, src, des, distance);
			
			if (point_index == 0) {
				minidis = distance;
				cross_pt_map_enu[0] = des.xyz.x;
				cross_pt_map_enu[1] = des.xyz.y;
			} else if (minidis > distance) {
				minidis = distance;
				cross_pt_map_enu[0] = des.xyz.x;
				cross_pt_map_enu[1] = des.xyz.y;
			}
			point_index++;
		}
	}

	return minidis;
}

int hdmap_impl::get_neareast_points(point3d src, double hdg, std::map<uint64_t, std::shared_ptr<hdmap_segment_point>> &points, double distance)
{
	if (nullptr == _lanebdy_kdtree) {
		return -1;
	}

	auto* hdr = header();
	if (nullptr == hdr) {
		return -2;
	}
	if (!hdr->a.persistence) {
		return -3;
	}

	if (src.xyz.x < hdr->xmin || src.xyz.x > hdr->xmax
		 || src.xyz.y < hdr->ymin || src.xyz.y > hdr->ymax) {
		return -4;
	}
	hdg = NormalizeAngle((90 - hdg) * DEG2RAD);

	double calc_distance = 0.0;
	hdmap_lane_boundary_point_v1* lane_seg_point = nullptr;
	std::vector<hdmap_lane_boundary_point_v1*> lanept;
	std::map<uint64_t, double> dismap;
	for (int i = 0; i < hdr->laneseg_count; i++) {
		lane_seg_point = kdtree2d_search(_lanebdy_kdtree,
			src.xyz.x, src.xyz.y, calc_distance);
		if (nullptr == lane_seg_point) {
			return -5;
		}
		if (lane_seg_point->laneseg_index >= hdr->laneseg_count) {
			printf("lane seg point data error. point index %lu\n",
				lane_seg_point->laneseg_index);
			lanept.push_back(lane_seg_point);
			for (auto& item : lanept) {
				item->attrs.KNN_selected = 0;
			}
			return -6;
		}

		if (calc_distance > distance) {
			continue;
		}
		uint64_t laneseg_index = lane_seg_point->laneseg_index;
		std::shared_ptr<hdmap_segment_point> segpoint = std::make_shared<hdmap_segment_point>();
		segpoint->pt = lane_seg_point->pt;
		segpoint->s = lane_seg_point->s;
		if (points.count(laneseg_index) > 0 && dismap[laneseg_index] > calc_distance) {
			dismap[laneseg_index] = calc_distance;
			points[laneseg_index] = segpoint;
		} 
		else {
			dismap[laneseg_index] = calc_distance;
			points[laneseg_index] = segpoint;
		}
		lanept.push_back(lane_seg_point);
	}
	for (auto& item : lanept) {
		item->attrs.KNN_selected = 0;
	}
	return 0;
}

int hdmap_impl::get_neareast_laneses(point3d p3d, double hdg, std::map<uint64_t, std::shared_ptr<hdmap_segment_point>> points, std::map<uint64_t, double> &lanesegs)
{
	if (points.empty()) {
		return -1;
	}
	std::set<hdmap_file_laneseg_v1*> laneset;
	std::map<uint64_t, std::shared_ptr<hdmap_segment_point>>::iterator it;
	for (it = points.begin(); it != points.end(); it++) {
		uint64_t laneseg_index = it->first;
		auto* ls = &_lanesegs[laneseg_index];
		if (nullptr == ls || nullptr == ls->data.ptr) {
			continue;
		}
		auto pts = ls->data.ptr->extinfo.outer_pts;
		int ptsz = pts.size / sizeof(hdmap_lane_boundary_point_v1);
		auto* lanels = (odr_lane*)ls->data.ptr->ptr;
		if (nullptr == lanels) {
			continue;
		}
		int tmpid = lanels->id;
		int index = 0;
		for (index = 0; index < ptsz; index++) {
			if (pts.ptr[index].s == it->second->s) {
				if (pts.ptr[index].pt == it->second->pt) {
					break;
				}
			}
		}
		if (index == ptsz) {
			pts = ls->data.ptr->extinfo.inner_pts;
			ptsz = pts.size / sizeof(hdmap_lane_boundary_point_v1);
			for (index = 0; index < ptsz; index++) {
				if (pts.ptr[index].s == it->second->s) {
					if (pts.ptr[index].pt == it->second->pt) {
						break;
					}
				}
			}
		}
		if (index < ptsz) {
			hdmap_lane_boundary_point_v1* start = nullptr;
			hdmap_lane_boundary_point_v1* end = nullptr;
			start = end = &pts.ptr[index];
			for (int i = 1; index - i > 0; i++) {
				start = &pts.ptr[index - i];
				if (end->s - start->s > 0.1) {
					break;
				}
			}
			if (end->s - start->s < 0.1) {
				for (int i = 1; index + i < ptsz; i++) {
					end = &pts.ptr[index + i];
					if (end->s - start->s > 0.1) {
						break;
					}
				}
			}
			double pthdr = std::atan2((end->pt.xyz.y - start->pt.xyz.y), (end->pt.xyz.x - start->pt.xyz.x)) ;
			pthdr = NormalizeAngle(pthdr);
			double adf = AngleDiff(pthdr, hdg);
			if (adf < M_PI_2 && adf > -M_PI_2) {
				// hdg = NormalizeAngle(M_PI_2 - pthdr) * RAD2DEG;
				// double s = POINT_DISTANCE(lane_seg_point->pt.xyz.x - src.xyz.x, lane_seg_point->pt.xyz.y - src.xyz.y);
				break;
			}
		}

		// double s;
		// get_laneseg_center_projection_distance(ls, p3d, hdg, s);

	}
	if (laneset.empty()) {
		return -2;
	}
	return 0;
}

const hdmap_file_laneseg_v1* hdmap_impl::get_laneseg_center_projection_distance(const hdmap_file_laneseg_v1*ls, point3d& src, hdmap_segment_point& hspt, double &hdg, double &s)
{
	if (nullptr == ls) { return nullptr; }
	double centpts = 0;
	hdmap_lane_boundary_point_v1* start = nullptr;
	hdmap_lane_boundary_point_v1* end = nullptr;
	size_t lssz = ls->data.ptr->extinfo.outer_pts.size / sizeof(hdmap_lane_boundary_point_v1);
	auto* lscenter = ls->data.ptr->extinfo.outer_pts.ptr;
	int index = 0;
	for (index = 0; index < lssz; index++) {
		if (nullptr == start) {
			start = &lscenter[index];
			continue;
		}
		end  = &lscenter[index];
		if (!get_line_projection(&start->pt, &end->pt, src, hspt.pt, s)) {
			centpts = start->s + s;
			break;
		}
		start = end;
	}
	if (index == lssz) {
		lssz = ls->data.ptr->extinfo.inner_pts.size / sizeof(hdmap_lane_boundary_point_v1);
		lscenter = ls->data.ptr->extinfo.inner_pts.ptr;
		start = nullptr;
		for (index = 0; index < lssz; index++) {
			if (nullptr == start) {
				start = &lscenter[index];
				continue;
			}
			end  = &lscenter[index];
			if (!get_line_projection(&start->pt, &end->pt, src, hspt.pt, s)) {
				centpts = start->s + s;
				break;
			}
			start = end;
		}
	}
	if (index == lssz) {
		// printf("error point project failure\n");
		centpts = s;
	}
	lssz = ls->data.ptr->extinfo.center_pts.size / sizeof(hdmap_lane_boundary_point_v1);
	lscenter = ls->data.ptr->extinfo.center_pts.ptr;
	int ptindex;
	start = nullptr;
	while (ls != nullptr) {
		for (ptindex = 0; ptindex < lssz; ptindex++) {
			if (nullptr == start) {
				start = &lscenter[ptindex];
			}
			end = &lscenter[ptindex];
			if (end->s > centpts) {
				break;
			}
			start = end;
		}
		if (ptindex == lssz && start->s != centpts) {
			ls = get_next_laneseg(ls);
			if (nullptr == ls) {
				printf("error point not in next line: [%f], pt s [%f]\n", start->s, centpts);
				return nullptr;				
			}
			if (nullptr == ls->data.ptr) {
				auto* nextls = ls->insider.ptr;
				if (nullptr == nextls || nullptr == nextls->data.ptr) {
					nextls = ls->outsider.ptr;
					if (nullptr == nextls || nullptr == nextls->data.ptr) {
						printf("error point not find neighbouring line\n");
						return nullptr;
					}
				}
				ls = nextls;
			}
			lssz = ls->data.ptr->extinfo.center_pts.size / sizeof(hdmap_lane_boundary_point_v1);
			lscenter = ls->data.ptr->extinfo.center_pts.ptr;
		} else {
			break;
		}
	}

	if (ptindex == 0 || ptindex == lssz) {
		hspt.pt = start->pt;
		hspt.s = start->s;
	}
	else {
		double d_s = end->s - start->s;
		double seglen = POINT_DISTANCE(end->pt.xyz.x - start->pt.xyz.x, end->pt.xyz.y - start->pt.xyz.y);
		double left_s = centpts - start->s;
		if (d_s > 0.1) {
			left_s = left_s * seglen / d_s;
		}
		if (seglen < left_s) {
			printf("lane segment length error, len %f, left s %f\n", seglen, left_s);
			left_s = seglen;
		}
		double x = (end->pt.xyz.x - start->pt.xyz.x)*left_s / seglen + start->pt.xyz.x;
		double y = (end->pt.xyz.y - start->pt.xyz.y)*left_s / seglen + start->pt.xyz.y;
		hspt.s = centpts;
		hspt.pt.set(x, y);
		// hdg = asin((end->pt.xyz.x - start->pt.xyz.x) / seglen) * RAD2DEG;
		s = POINT_DISTANCE(src.xyz.x - x, src.xyz.y - y);
	}
	return ls;
}

const hdmap_file_laneseg_v1* hdmap_impl::get_laneseg_center_projection(const hdmap_file_laneseg_v1* ls, point3d& src, hdmap_segment_point& hspt, double &hdg, double spt, std::vector<point3d>* line)
{
	if (nullptr == ls) { return nullptr; }
	double centpts = 0;
	double s = 0;
	hdmap_lane_boundary_point_v1* start = nullptr;
	hdmap_lane_boundary_point_v1* end = nullptr;
	size_t lssz = ls->data.ptr->extinfo.outer_pts.size / sizeof(hdmap_lane_boundary_point_v1);
	auto* lscenter = ls->data.ptr->extinfo.outer_pts.ptr;
	int index = 0;
	for (index = 0; index < lssz; index++) {
		if (nullptr == start) {
			start = &lscenter[index];
			continue;
		}
		end  = &lscenter[index];
		if (!get_line_projection(&start->pt, &end->pt, src, hspt.pt, s)) {
			centpts = start->s + s;
			break;
		}
		start = end;
	}
	if (index == lssz) {
		lssz = ls->data.ptr->extinfo.inner_pts.size / sizeof(hdmap_lane_boundary_point_v1);
		lscenter = ls->data.ptr->extinfo.inner_pts.ptr;
		start = nullptr;
		for (index = 0; index < lssz; index++) {
			if (nullptr == start) {
				start = &lscenter[index];
				continue;
			}
			end  = &lscenter[index];
			if (!get_line_projection(&start->pt, &end->pt, src, hspt.pt, s)) {
				centpts = start->s + s;
				break;
			}
			start = end;
		}
	}
	if (index == lssz) {
		// printf("error point project failure\n");
		centpts = spt;
	}
	lssz = ls->data.ptr->extinfo.center_pts.size / sizeof(hdmap_lane_boundary_point_v1);
	lscenter = ls->data.ptr->extinfo.center_pts.ptr;
	int ptindex;
	start = nullptr;
	while (ls != nullptr) {
		for (ptindex = 0; ptindex < lssz; ptindex++) {
			if (nullptr == start) {
				start = &lscenter[ptindex];
			}
			end = &lscenter[ptindex];
			if (end->s > centpts) {
				break;
			}
			start = end;
		}
		if (ptindex == lssz && start->s != centpts) {
			ls = get_next_laneseg(ls);
			if (nullptr == ls) {
				printf("error point not in next line: [%f], pt s [%f]\n", start->s, centpts);
				return nullptr;				
			}
			if (nullptr == ls->data.ptr) {
				auto* nextls = ls->insider.ptr;
				if (nullptr == nextls || nullptr == nextls->data.ptr) {
					nextls = ls->outsider.ptr;
					if (nullptr == nextls || nullptr == nextls->data.ptr) {
						printf("error point not find neighbouring line\n");
						return nullptr;
					}
				}
				ls = nextls;
			}
			lssz = ls->data.ptr->extinfo.center_pts.size / sizeof(hdmap_lane_boundary_point_v1);
			lscenter = ls->data.ptr->extinfo.center_pts.ptr;
		} else {
			break;
		}
	}

	if (ptindex == 0 || ptindex == lssz) {
		hspt.pt = start->pt;
		hspt.s = start->s;
	}
	else {
		double d_s = end->s - start->s;
		double seglen = POINT_DISTANCE(end->pt.xyz.x - start->pt.xyz.x, end->pt.xyz.y - start->pt.xyz.y);
		double left_s = centpts - start->s;
		if (d_s > 0.1) {
			left_s = left_s * seglen / d_s;
		}
		if (seglen < left_s) {
			printf("lane segment length error, len %f, left s %f\n", seglen, left_s);
			left_s = seglen;
		}
		double x = (end->pt.xyz.x - start->pt.xyz.x)*left_s / seglen + start->pt.xyz.x;
		double y = (end->pt.xyz.y - start->pt.xyz.y)*left_s / seglen + start->pt.xyz.y;
		hspt.s = centpts;
		hspt.pt.set(x, y);
		// hdg = asin((end->pt.xyz.x - start->pt.xyz.x) / seglen) * RAD2DEG;
	}
	if (nullptr != line) {
		for (int i = 0; i < lssz; i++) {
			line->push_back(lscenter[i].pt);
		}
	}
	return ls;
}

const hdmap_file_laneseg_v1* hdmap_impl::get_laneseg_center_width(const hdmap_file_laneseg_v1* ls, double s, double& width)
{
	if (nullptr == ls) { return nullptr; }
	double centpts = 0;
	hdmap_lane_boundary_point_v1* start = nullptr;
	hdmap_lane_boundary_point_v1* end = nullptr;
	size_t lssz = ls->data.ptr->extinfo.center_pts.size / sizeof(hdmap_lane_boundary_point_v1);
	auto* lscenter = ls->data.ptr->extinfo.center_pts.ptr;
	int ptindex = 0;
	while (ls != nullptr) {
		for (ptindex = 0; ptindex < lssz; ptindex++) {
			if (nullptr == start) {
				start = &lscenter[ptindex];
			}
			end = &lscenter[ptindex];
			if (end->s > s) {
				width = end->width;
				break;
			}
			start = end;
		}
		if (ptindex == lssz && start->s != s) {
			while (ls != nullptr) {
				ls = get_next_laneseg(ls);
				if (nullptr == ls || nullptr == ls->data.ptr) {
					printf("error point not in next line: [%f], pt s [%f]\n", start->s, s);
					return nullptr;				
				}
				lssz = ls->data.ptr->extinfo.center_pts.size / sizeof(hdmap_lane_boundary_point_v1);
				lscenter = ls->data.ptr->extinfo.center_pts.ptr;
			}
		} else {
			break;
		}
	}
	return ls;
}

int hdmap_impl::get_neareast_laneseg(point3d &src,double &hdg, std::set<hdmap_file_laneseg_v1*> &lanesegs)
{
	hdmap_lane_boundary_point_v1* bpt;
	if (nullptr == _lanebdy_kdtree) {
		return -1;
	}

	auto* hdr = header();
	if (nullptr == hdr) {
		return -1;
	}
	if (!hdr->a.persistence) {
		return -1;
	}
	if (!hdr->laneseg_count) {
		return -1;
	}

	if (src.xyz.x < hdr->xmin || src.xyz.x > hdr->xmax
		 || src.xyz.y < hdr->ymin || src.xyz.y > hdr->ymax) {
		return -1;
	}
	hdg = NormalizeAngle((90 - hdg) * DEG2RAD);

	double distance = 10.0;
	hdmap_lane_boundary_point_v1* lane_seg_point = nullptr;
	std::vector<hdmap_lane_boundary_point_v1*> lanept;
	for (int i = 0; i < hdr->laneseg_count; i++) {
		lane_seg_point = kdtree2d_search(_lanebdy_kdtree,
			src.xyz.x, src.xyz.y, distance);
		if (nullptr == lane_seg_point) {
			return -1;
		}
		if (lane_seg_point->laneseg_index >= hdr->laneseg_count) {
			printf("lane seg point data error. point index %lu\n",
				lane_seg_point->laneseg_index);
			lanept.push_back(lane_seg_point);
			for (auto& item : lanept) {
				item->attrs.KNN_selected = 0;
			}
			return -1;
		}
		auto* ls = &_lanesegs[lane_seg_point->laneseg_index];
		if (nullptr == ls || nullptr == ls->data.ptr) {
			return -1;
		}
		auto pts = ls->data.ptr->extinfo.outer_pts;
		int ptsz = pts.size / sizeof(hdmap_lane_boundary_point_v1);
		auto* lanels = (odr_lane*)ls->data.ptr->ptr;
		if (nullptr == lanels) {
			return -1;
		}
		int tmpid = lanels->id;
		int index = 0;
		for (index = 0; index < ptsz; index++) {
			if (pts.ptr[index].s == lane_seg_point->s) {
				if (pts.ptr[index].pt == lane_seg_point->pt) {
					break;
				}
			}
		}
		if (index == ptsz) {
			pts = ls->data.ptr->extinfo.inner_pts;
			ptsz = pts.size / sizeof(hdmap_lane_boundary_point_v1);
			for (index = 0; index < ptsz; index++) {
				if (pts.ptr[index].s == lane_seg_point->s) {
					if (pts.ptr[index].pt == lane_seg_point->pt) {
						break;
					}
				}
			}
		}
		if (index < ptsz) {
			hdmap_lane_boundary_point_v1* start = nullptr;
			hdmap_lane_boundary_point_v1* end = nullptr;
			start = end = &pts.ptr[index];
			for (int i = 1; index - i > 0; i++) {
				start = &pts.ptr[index - i];
				if (end->s - start->s > 0.1) {
					break;
				}
			}
			if (end->s - start->s < 0.1) {
				for (int i = 1; index + i < ptsz; i++) {
					end = &pts.ptr[index + i];
					if (end->s - start->s > 0.1) {
						break;
					}
				}
			}
			double pthdr = std::atan2((end->pt.xyz.y - start->pt.xyz.y), (end->pt.xyz.x - start->pt.xyz.x)) ;
			pthdr = NormalizeAngle(pthdr);
			double adf = AngleDiff(pthdr, hdg);
			if (adf < M_PI_2 && adf > -M_PI_2) {
				hdg = NormalizeAngle(M_PI_2 - pthdr) * RAD2DEG;
				double s = POINT_DISTANCE(lane_seg_point->pt.xyz.x - src.xyz.x, lane_seg_point->pt.xyz.y - src.xyz.y);
				lanept.push_back(lane_seg_point);
				break;
			}
		}
		lanept.push_back(lane_seg_point);
		// todo 查找与hdg相符的lane
		bpt = lane_seg_point;
		auto *lls = &_lanesegs[lane_seg_point->laneseg_index];
		int right = 0;
		int boundary_type = find_boundary(lls, bpt, src, right);

		if (0 == boundary_type) {
			printf("laneseg not find boduary point\n");
			return -1;
		}
		auto olane = (odr_lane*)lls->data.ptr->ptr;
		int tmpindex = lls - _lanesegs;
		right *= olane->id;
		right *= boundary_type;
		hdmap_file_laneseg_v1* llaneseg = nullptr;
		if (right > 0) {
			if (boundary_type > 0) {
				llaneseg = lls->insider.ptr; 
			} else {
				llaneseg = lls->outsider.ptr; 
			}
		} else {
			llaneseg = lls;
		}
		if (llaneseg == nullptr) {
			llaneseg = lls;
		}
		lanesegs.insert(llaneseg);
	}
	for (auto& item : lanept) {
		item->attrs.KNN_selected = 0;
	}
	return lanesegs.size();
}

hdmap_file_laneseg_v1* hdmap_impl::get_laneseg(point3d& src, double &hdg, double &s)
{
	hdmap_lane_boundary_point_v1* bpt;
	auto* ls = find_laneseg_v1(src, hdg, &bpt);
	if (nullptr == ls) {
		printf("laneseg not find lang segment\n");
		return nullptr;
	}
	int right = 0;
	int boundary_type = find_boundary(ls, bpt, src, right);

	if (0 == boundary_type) {
		printf("laneseg not find boduary point\n");
		return nullptr;
	}
	auto olane = (odr_lane*)ls->data.ptr->ptr;
	int tmpindex = ls - _lanesegs;
	right *= olane->id;
	right *= boundary_type;
	hdmap_file_laneseg_v1* llaneseg = nullptr;
	if (right > 0) {
		if (boundary_type > 0) {
			llaneseg = ls->insider.ptr; 
		} else {
			llaneseg = ls->outsider.ptr; 
		}
	} else {
		llaneseg = ls;
	}
	if (llaneseg == nullptr) {
		llaneseg = ls;
	}
	s = bpt->s;
	return llaneseg;
}

hdmap_file_laneseg_v1* hdmap_impl::get_prev_laneseg(hdmap_file_laneseg_v1* ls)
{
	if (nullptr == ls) {
		return nullptr;
	}
	auto* lane = ls->lane.ptr;
	if (nullptr == lane) {
		return nullptr;
	}
	auto* lanesegs = lane->lanesegs.ptr;
	size_t lssz = lane->lanesegs.size / sizeof(hdmap_file_laneseg_v1);
	int index = (ls - lanesegs) / sizeof(hdmap_file_laneseg_v1);
	if (index < 0 || index >= lssz) {
		printf("lane calc laneseg error\n");
		return nullptr;
	}
	if (index == 0) {
		if (lane->predecessors.count == 0) {
			return nullptr;
		}
		auto* lane_prev = lane->predecessors.indices[0];
		if (nullptr == lane_prev) {
			return nullptr;
		}
		auto* planesegs = lane_prev->lanesegs.ptr;
		if (nullptr == planesegs) {
			return nullptr;
		}
		size_t plssz = lane_prev->lanesegs.size / sizeof(hdmap_file_laneseg_v1);
		return &planesegs[plssz - 1];
	}
	return &lanesegs[index - 1];
}

const hdmap_file_laneseg_v1*
hdmap_impl::get_next_laneseg(const hdmap_file_laneseg_v1* ls)
{
	if (nullptr == ls) {
		return nullptr;
	}
	auto* lane = ls->lane.ptr;
	if (nullptr == lane) {
		return nullptr;
	}
	auto* lanesegs = lane->lanesegs.ptr;
	size_t lssz = lane->lanesegs.size / sizeof(hdmap_file_laneseg_v1);
	int index = (ls - lanesegs) / sizeof(hdmap_file_laneseg_v1);
	if (index < 0 || index >= lssz) {
		printf("lane calc laneseg error\n");
		return nullptr;
	}
	if (index == lssz - 1) {
		if (lane->successors.count == 0) {
			return nullptr;
		}
		auto* lane_next = lane->successors.indices[0];
		if (nullptr == lane_next) {
			return nullptr;
		}
		if (lane_next->lanesegs.ptr == nullptr) {
			return nullptr;
		}
		auto* nlanesegs = lane_next->lanesegs.ptr;
		return &nlanesegs[0];
	}
	auto* next_l = ls + 1;
	return next_l;
}

hdmap_file_laneseg_v1* hdmap_impl::find_laneseg_v1(point3d& src, double &hdg,
	hdmap_lane_boundary_point_v1** bpt)
{
	if (nullptr == _lanebdy_kdtree) {
		return nullptr;
	}

	auto* hdr = header();
	if (nullptr == hdr) {
		return nullptr;
	}
	if (!hdr->a.persistence) {
		return nullptr;
	}
	if (!hdr->laneseg_count) {
		return nullptr;
	}

	if (src.xyz.x < hdr->xmin || src.xyz.x > hdr->xmax
		 || src.xyz.y < hdr->ymin || src.xyz.y > hdr->ymax) {
		return nullptr;
	}
	hdg = NormalizeAngle((90 - hdg) * DEG2RAD);

	double distance = 0.0;
	hdmap_lane_boundary_point_v1* lane_seg_point = nullptr;
	std::vector<hdmap_lane_boundary_point_v1*> lanept;
	for (int i = 0; i < hdr->laneseg_count; i++) {
		lane_seg_point = kdtree2d_search(_lanebdy_kdtree,
			src.xyz.x, src.xyz.y, distance);
		if (nullptr == lane_seg_point) {
			return nullptr;
		}
		if (lane_seg_point->laneseg_index >= hdr->laneseg_count) {
			printf("lane seg point data error. point index %lu\n",
				lane_seg_point->laneseg_index);
			lanept.push_back(lane_seg_point);
			for (auto& item : lanept) {
				item->attrs.KNN_selected = 0;
			}
			return nullptr;
		}
		auto* ls = &_lanesegs[lane_seg_point->laneseg_index];
		if (nullptr == ls || nullptr == ls->data.ptr) {
			return nullptr;
		}
		auto pts = ls->data.ptr->extinfo.outer_pts;
		int ptsz = pts.size / sizeof(hdmap_lane_boundary_point_v1);
		auto* lanels = (odr_lane*)ls->data.ptr->ptr;
		if (nullptr == lanels) {
			return nullptr;
		}
		int tmpid = lanels->id;
		int index = 0;
		for (index = 0; index < ptsz; index++) {
			if (pts.ptr[index].s == lane_seg_point->s) {
				if (pts.ptr[index].pt == lane_seg_point->pt) {
					break;
				}
			}
		}
		if (index == ptsz) {
			pts = ls->data.ptr->extinfo.inner_pts;
			ptsz = pts.size / sizeof(hdmap_lane_boundary_point_v1);
			for (index = 0; index < ptsz; index++) {
				if (pts.ptr[index].s == lane_seg_point->s) {
					if (pts.ptr[index].pt == lane_seg_point->pt) {
						break;
					}
				}
			}
		}
		if (index < ptsz) {
			hdmap_lane_boundary_point_v1* start = nullptr;
			hdmap_lane_boundary_point_v1* end = nullptr;
			start = end = &pts.ptr[index];
			for (int i = 1; index - i > 0; i++) {
				start = &pts.ptr[index - i];
				if (end->s - start->s > 0.1) {
					break;
				}
			}
			if (end->s - start->s < 0.1) {
				for (int i = 1; index + i < ptsz; i++) {
					end = &pts.ptr[index + i];
					if (end->s - start->s > 0.1) {
						break;
					}
				}
			}
			double pthdr = std::atan2((end->pt.xyz.y - start->pt.xyz.y), (end->pt.xyz.x - start->pt.xyz.x)) ;
			pthdr = NormalizeAngle(pthdr);
			double adf = AngleDiff(pthdr, hdg);
			if (adf < M_PI_2 && adf > -M_PI_2) {
				hdg = NormalizeAngle(M_PI_2 - pthdr) * RAD2DEG;
				double s = POINT_DISTANCE(lane_seg_point->pt.xyz.x - src.xyz.x, lane_seg_point->pt.xyz.y - src.xyz.y);
				lanept.push_back(lane_seg_point);
				break;
			}
		}
		lanept.push_back(lane_seg_point);
	}
	for (auto& item : lanept) {
		item->attrs.KNN_selected = 0;
	}
	// todo 查找与hdg相符的lane
	*bpt = lane_seg_point;
	return &_lanesegs[lane_seg_point->laneseg_index];
}

int hdmap_impl::find_point_post_with_segment(hdmap_lane_boundary_point_v1* pt1, hdmap_lane_boundary_point_v1* pt2, point3d& pt, int &right)
{
	if (nullptr == pt1 || nullptr == pt2) {
		return -EBADPARM;
	}
	double x = pt.v[0];
	double y = pt.v[1];
	double x1 = pt1->pt.v[0];
	double y1 = pt1->pt.v[1];
	double x2 = pt2->pt.v[0];
	double y2 = pt2->pt.v[1];
	double cross = (x2 - x1) * (x - x1) + (y2 - y1) * (y - y1);
	if (cross <= 0) {
		return -ENOTFOUND;
	}
	double d2 = (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1);
	if (cross >= d2) {
		return -ENOTFOUND;
	}
	double r = cross / d2;
	double px = x1 + (x2 - x1) * r;
	double py = y1 + (y2 - y1) * r;
	point3d pt_bp;
	pt_bp.set(px, py);
	if (px == x && py == y) {
		right = 0;
	} else {
		double cross_m = CROSS_MULTI((x-px), (y-py), (x2-px), (y2-py));
		if (cross_m > 10e-15) {
			right = 1;
		} else {
			right = -1;
		}
	}
	return 0;
}

int hdmap_impl::get_line_projection(point3d* pt1, point3d* pt2, point3d& pt, point3d& ppt, double &s)
{
	if (nullptr == pt1 || nullptr == pt2) {
		return -EBADPARM;
	}
	double x = pt.v[0];
	double y = pt.v[1];
	double x1 = pt1->v[0];
	double y1 = pt1->v[1];
	double x2 = pt2->v[0];
	double y2 = pt2->v[1];
	double cross = (x2 - x1) * (x - x1) + (y2 - y1) * (y - y1);
	if (cross <= 0) {
		return -ENOTFOUND;

	}
	double d2 = (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1);
	if (cross >= d2) {
		return -ENOTFOUND;

	}
	double r = cross / d2;
	double px = x1 + (x2 - x1) * r;
	double py = y1 + (y2 - y1) * r;
	ppt.set(px, py);
 	s = POINT_DISTANCE(px - x1, py - y1);
	return 0;
}

int hdmap_impl::get_line_projection_distance(point3d* pt1, point3d* pt2, point3d& pt, point3d& ppt, double &s)
{
	if (nullptr == pt1 || nullptr == pt2) {
		return -EBADPARM;
	}
	double x = pt.v[0];
	double y = pt.v[1];
	double x1 = pt1->v[0];
	double y1 = pt1->v[1];
	double x2 = pt2->v[0];
	double y2 = pt2->v[1];
	double cross = (x2 - x1) * (x - x1) + (y2 - y1) * (y - y1);
	if (cross <= 0) {
		ppt.set(x1, y1);
 		s = POINT_DISTANCE(x - x1, y - y1);
		return 0;
	}
	double d2 = (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1);
	if (cross >= d2) {
		ppt.set(x2, y2);
 		s = POINT_DISTANCE(x - x2, y - y2);
		return 0;
	}
	double r = cross / d2;
	double px = x1 + (x2 - x1) * r;
	double py = y1 + (y2 - y1) * r;
	ppt.set(px, py);
 	s = POINT_DISTANCE(px - x, py - y);
	return 0;
}

int hdmap_impl::find_boundary(hdmap_file_laneseg_v1* ls,
	hdmap_lane_boundary_point_v1* bpt, point3d& pt, int &right)
{
	if (nullptr == ls || nullptr == ls->data.ptr) {
		return -ENOTAVAIL;
	}
	int boundary_type = 0;
	int bpt_index = 0;
	auto* lanels = (odr_lane*)ls->data.ptr->ptr;
	int tmpid = lanels->id;

	auto* pts = ls->data.ptr->extinfo.inner_pts.ptr;
	int ptsz = ls->data.ptr->extinfo.inner_pts.size / sizeof(hdmap_lane_boundary_point_v1);
	for (int i = 0; i < ptsz; i++, bpt_index++) {
		if (pts[i].s == bpt->s) {
			if (pts[i].pt == bpt->pt) {
				boundary_type = 1;
				break;
			}
		}
	}
	if (0 == boundary_type) {
		bpt_index = 0;
		pts = ls->data.ptr->extinfo.outer_pts.ptr;
		ptsz = ls->data.ptr->extinfo.outer_pts.size / sizeof(hdmap_lane_boundary_point_v1);
		for (int i = 0; i < ptsz; i++, bpt_index++) {
			if (pts[i].s == bpt->s) {
				if (pts[i].pt == bpt->pt) {
					boundary_type = -1;
					break;
				}
			}
		}
	}
	if (boundary_type == 0) {
		return -ENOTFOUND;
	}
	hdmap_lane_boundary_point_v1* start;
	hdmap_lane_boundary_point_v1* end;
	auto* lspts = pts;
	auto lsptsz = ptsz;
	if (0 == bpt_index) {
		auto* laneprev = get_prev_laneseg(ls);
		if (laneprev) {
			hdmap_lane_boundary_point_v1* prevpts;
			size_t prevptsz = 0;
			if (boundary_type > 0) {
				prevpts = laneprev->data.ptr->extinfo.inner_pts.ptr;
				prevptsz = laneprev->data.ptr->extinfo.inner_pts.size / sizeof(hdmap_lane_boundary_point_v1);	
				start = &prevpts[prevptsz - 1];	
			} else {
				prevpts = laneprev->data.ptr->extinfo.outer_pts.ptr;
				prevptsz = laneprev->data.ptr->extinfo.outer_pts.size / sizeof(hdmap_lane_boundary_point_v1);	
				start = &prevpts[prevptsz - 1];	
			}
		} else {
			start = nullptr;
		}
		end = &(pts[bpt_index]);
	}
	else {
		start = &(pts[bpt_index - 1]);
		end = &(pts[bpt_index]);
	}

	int ret = find_point_post_with_segment(start, end, pt, right);
	if (ret) {
		if (bpt_index == (ptsz - 1)) {
			start = &(pts[bpt_index]);
			auto* lanenext = get_next_laneseg(ls);
			if (lanenext) {
				hdmap_lane_boundary_point_v1* prevpts;
				size_t prevptsz = 0;
				if (nullptr == lanenext->data.ptr) {
					printf("next lane data is null\n");
					return -ENOTFOUND;
				}
				if (boundary_type > 0) {
					prevpts = lanenext->data.ptr->extinfo.inner_pts.ptr;
					end = &prevpts[0];
				} else {
					prevpts = lanenext->data.ptr->extinfo.outer_pts.ptr;
					end = &prevpts[0];
				}
			} else {
				end = nullptr;
			}			
		}
		else {
			start = &(pts[bpt_index]);
			end = &(pts[bpt_index + 1]);
		}
		ret = find_point_post_with_segment(start, end, pt, right);
		if (ret) {
			start = &(pts[0]);
			int ptindex = 1;
			for (; ptindex < ptsz; ptindex++) {
				end = &(pts[ptindex]);
				if (!find_point_post_with_segment(start, end, pt, right)) {
					return boundary_type;
				}
				start = end;
			}
			auto* laneprev = get_prev_laneseg(ls);
			if (laneprev && laneprev->data.ptr) {
				hdmap_lane_boundary_point_v1* prevpts;
				size_t prevptsz = 0;
				if (boundary_type > 0) {
					pts = laneprev->data.ptr->extinfo.inner_pts.ptr;
					ptsz = laneprev->data.ptr->extinfo.inner_pts.size / sizeof(hdmap_lane_boundary_point_v1);	
				} else {
					pts = laneprev->data.ptr->extinfo.outer_pts.ptr;
					ptsz = laneprev->data.ptr->extinfo.outer_pts.size / sizeof(hdmap_lane_boundary_point_v1);	
				}
				start = &(pts[0]);
				for (ptindex = 1; ptindex < ptsz; ptindex++) {
					end = &(pts[ptindex]);
					if (!find_point_post_with_segment(start, end, pt, right)) {
						return boundary_type;
					}
					start = end;
				}
			}
			end = &(lspts[0]);
			if (!find_point_post_with_segment(start, end, pt, right)) {
				return boundary_type;
			}
			start = &(lspts[lsptsz - 1]);
			auto* lanenext = get_next_laneseg(ls);
			if (lanenext && lanenext->data.ptr) {
				if (boundary_type > 0) {
					pts = lanenext->data.ptr->extinfo.inner_pts.ptr;
					ptsz = lanenext->data.ptr->extinfo.inner_pts.size / sizeof(hdmap_lane_boundary_point_v1);	
				} else {
					pts = lanenext->data.ptr->extinfo.outer_pts.ptr;
					ptsz = lanenext->data.ptr->extinfo.outer_pts.size / sizeof(hdmap_lane_boundary_point_v1);	
				}
				for (ptindex = 0; ptindex < ptsz; ptindex++) {
					end = &(pts[ptindex]);
					if (!find_point_post_with_segment(start, end, pt, right)) {
						return boundary_type;
					}
					start = end;
				}
			}
			// printf("point project to lanesegment failure\n");
			return boundary_type;
		}
	}
	return boundary_type;
}


const char* hdmap_impl::get_georef(void)
{
	auto hdr = header();
	if (nullptr == hdr) {
		return nullptr;
	}
	if (hdr->a.persistence) {
		return (*hdr->proj) ? hdr->proj : nullptr;
	} else {
		// todo
		return nullptr;
	}
	// shall never be here
	return nullptr;
}

int hdmap_impl::get_offset(double& dx, double& dy)
{
	auto hdr = header();
	if (nullptr == hdr) {
		return -ENOTAVAIL;
	}
	if (hdr->a.persistence) {
		dx = hdr->xoffset;
		dy = hdr->yoffset;
	} else {
		// todo
	}
	return 0;
}

int hdmap_impl::get_mapinfo(hdmap_info& mapinfo) {
	auto hdr = header();
	if (nullptr == hdr) {
		return -ENOTAVAIL;
	}
	if (hdr->a.persistence) {
		mapinfo.uuid = hdr->uuid;
		mapinfo.rows = hdr->rows;
		mapinfo.cols = hdr->cols;
		mapinfo.xmin = hdr->xmin;
		mapinfo.ymin = hdr->ymin;
		mapinfo.xmax = hdr->xmax;
		mapinfo.ymax = hdr->ymax;
		mapinfo.junction_count = hdr->junction_count;
		mapinfo.road_count = hdr->roadseg_count;
	} else {
		// todo
	}
	return 0;
}

hdmap::hdmap() : _data(nullptr)
{
	auto* map = new hdmap_impl();
	if (nullptr == map) return;
	_data = reinterpret_cast<void*>(map);
}

hdmap::~hdmap()
{
	if (_data) {
		auto* map = reinterpret_cast<hdmap_impl*>(_data);
		delete map;
		_data = nullptr;
	}
}

int hdmap::load_fromfile(const char* filename)
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	return map->load_fromfile(filename);
}

int hdmap::find_laneseg(double x, double y, int count,
	vector<hdmap_nearest_laneseg>& set) const
{
	if (nullptr == _data) {
		return -ENOTALLOWED;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return -ENOTAVAIL;
	}
	return map->find_laneseg(x, y, count, set);
}

int hdmap::get_laneseg_width(const hdmap_laneseg* from, const hdmap_laneseg *to, double s, double& fwidth, double& twidth, double& bwidth) const
{
	if (nullptr == _data) {
		return -ENOTALLOWED;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return -ENOTAVAIL;
	}
	return map->get_laneseg_width(from, to, s, fwidth, twidth, bwidth);
}

int hdmap:: get_neareast_points(point3d p3d, double hdg, std::map<uint64_t, std::shared_ptr<hdmap_segment_point>> &points, double distance) const
{
	if (nullptr == _data) {
		return -ENOTALLOWED;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return -ENOTAVAIL;
	}
	return map->get_neareast_points(p3d, hdg, points, distance);
}

int hdmap::get_neareast_laneses(point3d p3d, double hdg, std::map<uint64_t, std::shared_ptr<hdmap_segment_point>> points, std::map<uint64_t, double> &lanesegs) const
{
	if (nullptr == _data) {
		return -ENOTALLOWED;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return -ENOTAVAIL;
	}
	return map->get_neareast_laneses(p3d, hdg, points, lanesegs);
}

int hdmap::generate_lanesect_transition(const char* filepath)
{
	if (nullptr == _data) {
		return -ENOTALLOWED;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return -ENOTAVAIL;
	}
	return map->generate_lanesect_transition(filepath);
}

const hdmap_road* hdmap::get_road_by_id(uint32_t rid) const
{
	if (nullptr == _data) {
		return nullptr;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return nullptr;
	}
	return map->get_road_by_id(rid);
}

bool hdmap::is_in_lane(const hdmap_laneseg *ls, double s, point3d &pt) const
{
	if (nullptr == _data) {
		return -ENOTALLOWED;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return -ENOTAVAIL;
	}
	return map->is_in_lane(ls, s, pt);
}

bool hdmap::is_same_road(const hdmap_laneseg *lss, const hdmap_laneseg *lsd) const
{
	if (nullptr == _data) {
		return -ENOTALLOWED;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return -ENOTAVAIL;
	}
	return map->is_same_road(lss, lsd);
}

const hdmap_laneseg* hdmap::find_laneseg(point3d& src, double &hdg,
	hdmap_segment_point& centet_line_pt, std::vector<point3d> *line) const
{
	if (nullptr == _data) {
		return nullptr;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return nullptr;
	}
	return map->find_laneseg(src, hdg, centet_line_pt, line);
}

const char* hdmap::get_georef(void) const
{
	if (nullptr == _data) {
		return nullptr;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return nullptr;
	}
	return map->get_georef();
}

int hdmap::get_offset(double& dx, double& dy) const
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return -EINVALID;
	}
	return map->get_offset(dx, dy);
}

int hdmap::get_mapinfo(hdmap_info& mapinfo) const
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return -EINVALID;
	}
	return map->get_mapinfo(mapinfo);
}

std::vector<uint64_t> hdmap::get_lanes_near_pt_enu(Eigen::Vector3d p3d, double radius)
{
	std::vector<uint64_t> tmp;
	if (nullptr == _data) {
		return tmp;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return tmp;
	}
	return map->get_lanes_near_pt_enu(p3d, radius);
}

std::vector<uint64_t> hdmap::get_lanes_near_pt_enu(std::vector<Eigen::Vector3d> trajectory, double radius)
{
	std::vector<uint64_t> tmp;
	if (nullptr == _data) {
		return tmp;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return tmp;
	}
	return map->get_lanes_near_pt_enu(trajectory, radius);
}

std::vector<uint64_t> hdmap::get_lanes_near_pt_enu(std::vector<Pose> trajectory, double radius)
{
	std::vector<uint64_t> tmp;
	if (nullptr == _data) {
		return tmp;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return tmp;
	}
	return map->get_lanes_near_pt_enu(trajectory, radius);
}

Eigen::MatrixXd hdmap::CalculateTransitionalProbability(std::vector<uint64_t> lanes_id)
{
	Eigen::MatrixXd tmp;
	if (nullptr == _data) {
		return tmp;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return tmp;
	}
	return map->CalculateTransitionalProbability(lanes_id);
}

Eigen::MatrixXd hdmap::CalculateTransitionalProbability(std::vector<uint64_t> lanes_id_1,std::vector<uint64_t> lanes_id_2)
{
	Eigen::MatrixXd tmp;
	if (nullptr == _data) {
		return tmp;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return tmp;
	}
	return map->CalculateTransitionalProbability(lanes_id_1, lanes_id_2);
}


double hdmap::get_distance_pt_curve_enu(const Eigen::Vector3d &pt_enu, uint64_t lane_id, Eigen::Vector3d &cross_pt_curve)
{
	if (nullptr == _data) {
		return -EINVALID;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return -EINVALID;
	}
	return map->get_distance_pt_curve_enu(pt_enu, lane_id, cross_pt_curve);
}

std::vector<Curve> hdmap::get_curves_enu(CurveType type)
{
	std::vector<Curve> tmp;
	if (nullptr == _data) {
		return tmp;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return tmp;
	}
	return map->get_curves_enu(type);
}

std::vector<Curve> hdmap::get_inner_boundaries_enu(void)
{
	return get_curves_enu(CurveType::inner_boundary);
}
std::vector<Curve> hdmap::get_central_curves_enu(void)
{
	return get_curves_enu(CurveType::center_curve);
}

std::vector<Curve> hdmap::get_outer_boundaries_enu(void)
{
	return get_curves_enu(CurveType::outer_boundary);
}

std::vector<Eigen::Vector3d> hdmap::get_curve(uint64_t lane_id)
{
	std::vector<Eigen::Vector3d> tmp;
	if (nullptr == _data) {
		return tmp;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return tmp;
	}
	return map->get_curve(lane_id);
}

double hdmap::get_distance_pt_closest_central_line_enu(const Eigen::Vector3d &pt_enu, Eigen::Vector3d &cross_pt_map_enu)
{
	if (nullptr == _data) {
		return -1;
	}
	auto* map = reinterpret_cast<hdmap_impl*>(_data);
	if (!map->valid()) {
		return -2;
	}
	return map->get_distance_pt_closest_central_line_enu(pt_enu, cross_pt_map_enu);
}

int hdmap_laneseg::get_id() const
{
	auto* laneseg = reinterpret_cast<const hdmap_file_laneseg_v1*>(this);
	if (nullptr == laneseg) {
		return 0;
	}
	assert(nullptr != laneseg->data.ptr);
	auto plane = (odr_lane*)laneseg->data.ptr->ptr;
	return plane->id;
}

int hdmap_laneseg::get_endpoint(point3d& start, point3d& end) const
{
	auto* laneseg = reinterpret_cast<const hdmap_file_laneseg_v1*>(this);
	if (nullptr == laneseg) {
		return -ENOTAVAIL;
	}
	start = laneseg->start.pt;
	end = laneseg->end.pt;
	return 0;
}

int hdmap_laneseg::get_outsider_point(std::map<double, hdmap_segment_point>& opoints) const
{
	auto* laneseg = reinterpret_cast<const hdmap_file_laneseg_v1*>(this);
	if (nullptr == laneseg) {
		return -ENOTAVAIL;
	}
	if (nullptr == laneseg->data.ptr) {
		return -ENOTAVAIL;
	}
	auto pts = laneseg->data.ptr->extinfo.inner_pts;
	int ptsz = pts.size / sizeof(hdmap_lane_boundary_point_v1);
	
	for (int i = 0; i < ptsz; i++) {
		hdmap_segment_point pt;
		pt.s = pts.ptr[i].s;
		pt.pt = pts.ptr[i].pt;
		opoints[pt.s] = pt; 
	}
	return 0;
}

int hdmap_laneseg::get_insider_point(std::map<double, hdmap_segment_point>& ipoints) const
{
	auto* laneseg = reinterpret_cast<const hdmap_file_laneseg_v1*>(this);
	if (nullptr == laneseg) {
		return -ENOTAVAIL;
	}
	if (nullptr == laneseg->data.ptr) {
		return -ENOTAVAIL;
	}
	auto pts = laneseg->data.ptr->extinfo.outer_pts;
	int ptsz = pts.size / sizeof(hdmap_lane_boundary_point_v1);
	
	for (int i = 0; i < ptsz; i++) {
		hdmap_segment_point pt;
		pt.s = pts.ptr[i].s;
		pt.pt = pts.ptr[i].pt;
		ipoints[pt.s] = pt; 
	}
	return 0;
}

int hdmap_laneseg::get_center_line(std::vector<point3d>& line) const
{
	auto* laneseg = reinterpret_cast<const hdmap_file_laneseg_v1*>(this);
	if (nullptr == laneseg->data.ptr) {
		return -ENOTAVAIL;
	}
	auto& pts = laneseg->data.ptr->extinfo.center_pts;
	int ptsz = pts.size / sizeof(hdmap_lane_boundary_point_v1);
	
	for (int i = 0; i < ptsz; i++) {
		line.push_back(pts.ptr[i].pt);
	}
	return 0;
}

hdmap_laneseg* hdmap_laneseg::get_next_laneseg_by_hdg(const hdmap_laneseg* hdls, double hdg) const
{
	auto* ls = reinterpret_cast<const hdmap_file_laneseg_v1*>(hdls);
	if (nullptr == ls) {return nullptr;}
	auto* hlane = ls->lane.ptr;
	if (nullptr == hlane) {
		printf("lane segment not find lane\n");
		return nullptr;
	}

	if (0 == hlane->successors.count) {
		return nullptr;
	}
	double minadf = M_PI_2;
	hdmap_file_laneseg_v1 *newls = nullptr;
	for (int l_index = 0; l_index < hlane->successors.count; l_index++) {
		auto* tmplane = hlane->successors.indices[l_index];
		if (nullptr == tmplane || nullptr == tmplane->lanesegs.ptr) {
			continue;
		}

		for (int ls_index = 0; ls_index < tmplane->lanesegs.size / sizeof(hdmap_file_laneseg_v1); ls_index ++ ) {
			if (nullptr == tmplane->lanesegs.ptr[ls_index].data.ptr) {
				continue;
			}
			auto* lanestart = &(tmplane->lanesegs.ptr[ls_index].data.ptr->extinfo.center_pts.ptr[0]);
			auto* start = &(tmplane->lanesegs.ptr[ls_index].data.ptr->extinfo.center_pts.ptr[0].pt);
			if (lanestart->s > 1e-7) {
				continue;
			}
			auto* end = &(tmplane->lanesegs.ptr[ls_index].data.ptr->extinfo.center_pts.ptr[1].pt);
			double pthdr = std::atan2((end->xyz.y - start->xyz.y), (end->xyz.x - start->xyz.x)) ;
			pthdr = NormalizeAngle(pthdr);
			double adf = AngleDiff(pthdr, hdg);
			if (nullptr == newls) {
				newls = &tmplane->lanesegs.ptr[ls_index];
			}
			if (std::abs(adf) < minadf) {
				newls = &tmplane->lanesegs.ptr[ls_index];
				minadf = adf;
			}
		}
	}
	return reinterpret_cast<hdmap_laneseg*>(newls);
}

const hdmap_laneseg* hdmap_laneseg::get_next_laneseg(const hdmap_laneseg* hdls, int direct) const
{
	auto* ls = reinterpret_cast<const hdmap_file_laneseg_v1*>(hdls);
	if (nullptr == ls) {return nullptr;}

	if (0 == direct) {
		auto* hlane = ls->lane.ptr;
		if (nullptr == hlane) {
			printf("lane segment not find lane\n");
			return nullptr;
		}
		auto hlsz = hlane->lanesegs.size / sizeof(hdmap_file_laneseg_v1);
		if (0 == hlsz) {
			printf("laneseg not find lane\n");
			return nullptr;
		}
		int index = ls - hlane->lanesegs.ptr;
		if (index >= hlsz) {
			printf("lanseg index is over lane, lane index %d, total %lu\n",
				index, hlsz);
			return nullptr;
		}

		if (index < hlsz - 1) {
			auto* lsn = ls + 1;
			if (lsn->data.ptr == nullptr) {
				return nullptr;
			}
			return reinterpret_cast<const hdmap_laneseg*>(lsn);
		}
	}
	else if (direct > 0) {
		auto* lsn = ls->outsider.ptr;
		if (nullptr == lsn || lsn->data.ptr == nullptr) {
			return nullptr;
		}
		return reinterpret_cast<const hdmap_laneseg*>(lsn);
	} else {
		auto* lsn = ls->insider.ptr;
		if (nullptr == lsn || lsn->data.ptr == nullptr) {
			return nullptr;
		}
		return reinterpret_cast<const hdmap_laneseg*>(lsn);
	}
	return nullptr;
}

const hdmap_laneseg* hdmap_laneseg::get_adc_and_point(double s, hdmap_segment_point& spt, hdmap_segment_point& nextpt, double &hdg, double& w, std::vector<const hdmap_laneseg*> &route) const
{
	auto* laneseg = reinterpret_cast<const hdmap_file_laneseg_v1*>(this);
	if (nullptr == laneseg) {
		return nullptr;
	}
	double left_s = s + spt.s;
	route.clear();
	
	hdmap_lane_boundary_point_v1* start = nullptr;
	hdmap_lane_boundary_point_v1* end = nullptr;
	hdg = (90 - hdg) * DEG2RAD;
	int direct = 0;
	auto* origin_ls = laneseg;
	while (left_s > 0) {
		int ptindex = 0;
		if (nullptr == laneseg || nullptr == laneseg->data.ptr) {
			return nullptr;
		}
		auto* lscenter = laneseg->data.ptr->extinfo.center_pts.ptr;
		auto lscsz = laneseg->data.ptr->extinfo.center_pts.size / sizeof(hdmap_lane_boundary_point_v1);
		start = nullptr;
		if (lscenter[lscsz -1].s < left_s) {
			ptindex = lscsz;
			start = end = &lscenter[lscsz -1];
		} else {
			for (ptindex = 0; ptindex < lscsz; ptindex++) {
				if (nullptr == start) {
					start = &lscenter[ptindex];
					continue;
				}
				end = &lscenter[ptindex];
				if (end->s > left_s) {
					break;
				}
				start = end;
			}
		}
		if (ptindex < lscsz) {
			left_s -= start->s;
			double d_s = end->s - start->s;
			double seglen = POINT_DISTANCE(end->pt.xyz.x - start->pt.xyz.x, end->pt.xyz.y - start->pt.xyz.y);
			if (d_s > seglen) {
				left_s = left_s * seglen / d_s;
			}
			if (seglen < left_s) {
				printf("adc lane segment length error need %f, real %f\n", left_s, seglen);
				left_s = seglen;
			}

			nextpt.s = start->s + left_s;
			if (nextpt.s < 0) {
				printf("error\n");
			}
			double dx = end->pt.xyz.x - start->pt.xyz.x;
			double dy = end->pt.xyz.y - start->pt.xyz.y;
			double x = (dx)*left_s / seglen + start->pt.xyz.x;
			double y = (dy)*left_s / seglen + start->pt.xyz.y;
			if (isnan(x) || isnan(y)) {
				printf("error\n");
			}
			nextpt.pt.set(x, y);
			w = start->width;
			if (dx == 0) {
				if (dy > 0) {
					hdg = M_PI_2;
				}else {
					hdg = M_PI_2;
				}
			} else {
				hdg = atan2(dy, dx);
			}
			hdg = NormalizeAngle(M_PI_2 - hdg);
			hdg = hdg * RAD2DEG;
			route.push_back(reinterpret_cast<const hdmap_laneseg*>(laneseg));
			return reinterpret_cast<const hdmap_laneseg*>(laneseg);
		}

		route.push_back(reinterpret_cast<const hdmap_laneseg*>(laneseg));

		auto* or_ls = laneseg;
		laneseg = reinterpret_cast<const hdmap_file_laneseg_v1*>(get_next_laneseg(reinterpret_cast<const hdmap_laneseg*>(laneseg), 0));

		while (nullptr == laneseg) {
			if (direct == 0) {
				direct = -1;
			}
			else if (direct == -1) {
				direct = 1;
			} else {
				left_s -= start->s;
				laneseg = reinterpret_cast<const hdmap_file_laneseg_v1*>(get_next_laneseg_by_hdg(reinterpret_cast<const hdmap_laneseg*>(origin_ls), hdg));
				if (nullptr == laneseg) {
					printf("lane segment not next ls of other lane\n");
					return nullptr;
				}
				origin_ls = laneseg;
				direct = 0;
				break;
			}

			laneseg = reinterpret_cast<const hdmap_file_laneseg_v1*>(get_next_laneseg(reinterpret_cast<const hdmap_laneseg*>(or_ls), direct));
			if (nullptr != laneseg) {
				laneseg = reinterpret_cast<const hdmap_file_laneseg_v1*>(get_next_laneseg(reinterpret_cast<const hdmap_laneseg*>(laneseg), 0));
				if (nullptr != laneseg) {
					direct = 0;
				}
			}
		}
	}
	return nullptr;
}

hdmap_laneseg* hdmap_laneseg::get_insider(void) const
{
	auto* laneseg = reinterpret_cast<const hdmap_file_laneseg_v1*>(this);
	if (nullptr == laneseg) {
		return nullptr;
	}
	return reinterpret_cast<hdmap_laneseg*>(laneseg->insider.ptr);
}

hdmap_laneseg* hdmap_laneseg::get_outsider(void) const
{
	auto* laneseg = reinterpret_cast<const hdmap_file_laneseg_v1*>(this);
	if (nullptr == laneseg) {
		return nullptr;
	}
	return reinterpret_cast<hdmap_laneseg*>(laneseg->outsider.ptr);
}

hdmap_lane* hdmap_laneseg::get_lane(void) const
{
	auto* laneseg = reinterpret_cast<const hdmap_file_laneseg_v1*>(this);
	if (nullptr == laneseg) {
		return nullptr;
	}
	return reinterpret_cast<hdmap_lane*>(laneseg->lane.ptr);
}

int hdmap_lane::get_predecessors(vector<hdmap_lane*>& set) const
{
	auto* lane = reinterpret_cast<const hdmap_file_lane_v1*>(this);
	if (nullptr == lane) {
		return -ENOTFOUND;
	}
	auto predtbl = lane->predecessors;
	set.clear();
	for (int i = 0; i < predtbl.count; i++) {
		set.push_back(reinterpret_cast<hdmap_lane*>(predtbl.indices[i]));
	}
	return 0;
}

int hdmap_lane::get_successors(vector<hdmap_lane*>& set) const
{
	auto* lane = reinterpret_cast<const hdmap_file_lane_v1*>(this);
	if (nullptr == lane) {
		return -ENOTFOUND;
	}
	auto suctbl = lane->successors;
	set.clear();
	for (int i = 0; i < suctbl.count; i++) {
		set.push_back(reinterpret_cast<hdmap_lane*>(suctbl.indices[i]));
	}
	return 0;
}

int hdmap_lane::get_lanesegs(vector<hdmap_laneseg*>& set) const
{
	auto* lane = reinterpret_cast<const hdmap_file_lane_v1*>(this);
	if (nullptr == lane) {
		return -ENOTFOUND;
	}
	auto lanesegs = lane->lanesegs;
	set.clear();
	for (int i = 0; i < lanesegs.size; i++) {
		set.push_back(reinterpret_cast<hdmap_laneseg*>(&lanesegs.ptr[i]));
	}
	return 0;
}

const hdmap_laneseg* hdmap_lane::get_lane_first_segment(void) const
{
	auto* lane = reinterpret_cast<const hdmap_file_lane_v1*>(this);
	if (nullptr == lane) {
		return nullptr;
	}
	auto lanesegs = lane->lanesegs;
	// printf("get lane first segement %f\n", lanesegs.ptr[0].data.ptr->extinfo.inner_pts.ptr[0].s);
	return reinterpret_cast<const hdmap_laneseg*>(&lanesegs.ptr[0]);
}

const hdmap_laneseg* hdmap_lane::get_lane_last_segment(void) const
{
	auto* lane = reinterpret_cast<const hdmap_file_lane_v1*>(this);
	if (nullptr == lane) {
		return nullptr;
	}
	auto& lanesegs = lane->lanesegs;
	size_t sz = lanesegs.size / sizeof(hdmap_file_laneseg_v1);
	// printf("get lane last segement %f\n", lanesegs.ptr[sz -1].data.ptr->extinfo.inner_pts.ptr[0].s);
	return reinterpret_cast<const hdmap_laneseg*>(&lanesegs.ptr[sz -1]);
}

hdmap_road* hdmap_lane::get_road(void) const
{
	auto* lane = reinterpret_cast<const hdmap_file_lane_v1*>(this);
	if (nullptr == lane) {
		return nullptr;
	}
	auto roadseg = lane->roadseg;
	return reinterpret_cast<hdmap_road*>(roadseg.ptr);
}

double hdmap_lane::get_length(void) const
{
	auto* lane = reinterpret_cast<const hdmap_file_lane_v1*>(this);
	if (nullptr == lane) {
		return 0;
	}
	return lane->length;
}

int hdmap_lane::get_id(void) const
{
	auto* lane = reinterpret_cast<const hdmap_file_lane_v1*>(this);
	if (nullptr == lane) {
		return 0;
	}
	return lane->id;
}

int hdmap_road::get_lanes(vector<hdmap_lane*>& set) const
{
	auto* road = reinterpret_cast<const hdmap_file_roadseg_v1*>(this);
	if (nullptr == road) {
		return -EBADPARM;
	}
	auto lanes = road->lanes;
	set.clear();
	for (int i = 0; i < lanes.size; i++) {
		set.push_back(reinterpret_cast<hdmap_lane*>(lanes.ptr[i].ptr));
	}
	return 0;
}

const hdmap_lane* hdmap_road::get_lane_by_id(int laneid) const
{
	auto* road = reinterpret_cast<const hdmap_file_roadseg_v1*>(this);
	if (nullptr == road) {
		return nullptr;
	}
	auto lanes = road->lanes;
	uint32_t lane_cnt = lanes.size / sizeof(point_hdmap_file_lane_v1);
	for (int i = 0; i < lane_cnt; i++) {
		if (lanes.ptr[i].ptr->id == laneid) {
			return reinterpret_cast<const hdmap_lane*>(lanes.ptr[i].ptr);
		}
	}
	return nullptr;
}

int hdmap_impl::get_refline_point(hdmap_file_roadseg_v1* rs, double s, point3d& pt)
{
	if (nullptr == rs) {
		return -EBADPARM;
	}
	auto reftbl = rs->reflines;
	for (int i = 0; i < reftbl.count; i++) {
		size_t ptsz = reftbl.indices[i]->extinfo.pts.size / sizeof(hdmap_refline_point_v1);
		auto* pts = reftbl.indices[i]->extinfo.pts.ptr;
		for (int pt_index = 0; pt_index < ptsz; pt_index++) {
			if (pts[pt_index].s == s) {
				pt = pts[pt_index].pt;
				return 0;
			}
		}
		
	}
	return -ENOTFOUND;
}

double hdmap_road::get_length(void) const
{
	auto* road = reinterpret_cast<const hdmap_file_roadseg_v1*>(this);
	if (nullptr == road) {
		return 0;
	}
	return road->s_length;
}

uint32_t hdmap_road::get_road_id(void) const
{
	auto* road = reinterpret_cast<const hdmap_file_roadseg_v1*>(this);
	if (nullptr == road) {
		return -1;
	}
	return road->rd_id;
}

int hdmap_road::get_junctionid(void) const
{
	auto* road = reinterpret_cast<const hdmap_file_roadseg_v1*>(this);
	if (nullptr == road) {
		return 0;
	}
	return road->junction_id;
}

}}  // namespace zas::mapcore

/* EOF */
