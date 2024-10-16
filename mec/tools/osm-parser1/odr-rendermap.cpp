#include <math.h>
#include <float.h>
#include "odr-loader.h"
#include "odr-map.h"

#include "utils/uuid.h"
#include "utils/timer.h"
#include "mapcore/mapcore.h"

void* operator new(size_t sz, size_t extsz, size_t& pos, void** r) throw()
{
	sz += extsz;
	pos += sz;
	void* ret = malloc(sz);
	if (r) *r = ret;
	return ret;
}

void operator delete(void* ptr, size_t)
{
	free(ptr);
}

void* operator new[](size_t sz, size_t& pos, void** r) throw()
{
	pos += sz;
	void* ret = malloc(sz);
	if (r) *r = ret;
	return ret;
}

void operator delete[](void* ptr, size_t)
{
	free(ptr);
}

namespace osm_parser1 {
using namespace std;
using namespace zas::utils;
using namespace zas::mapcore;

#define odr_rendermap_check(expr) \
do {	\
	if (expr) break;	\
	fprintf(stderr, "rendermap: error at %s <ln:%u>: \"%s\"," \
		" abort!\n", __FILE__, __LINE__, #expr);	\
	__asm__("int $0x03");	\
	exit(114);	\
} while (0)

file_memory::~file_memory()
{
	reset();
}

void file_memory::reset(void)
{
	auto node = avl_first(_rdmap_bufnode_id_tree);
	while (node) {
		auto obj = AVLNODE_ENTRY(rdmap_bufnode, id_avlnode, node);
		avl_remove(&_rdmap_bufnode_id_tree, &obj->id_avlnode);
		free(obj->buf);
		delete obj;
		node = avl_first(_rdmap_bufnode_id_tree);
	}
	assert(nullptr == _rdmap_bufnode_id_tree);
	_rdmap_bufnode_addr_tree = nullptr;
	_databuf_sz = 0;
}

void odr_map::rendermap_release_all(void)
{
	_rfm.reset();
}

int odr_map::generate_rendermap(void)
{
	fprintf(stdout, "hdmap: generating rendering map ...\n\n");
	if (generate_mesh()) {
		return -1;
	}

	fprintf(stdout, "rendering map bounding box (%.2f, %.2f)-(%.2f, %.2f)\n",
		_x0, _y0, _x1, _y1);
	if (build_blocks()) {
		return -2;
	}

	if (write_rendermap_header()) {
		return -3;
	}

	if (write_lane_sections()) {
		return -4;
	}

	if (write_junctions()) {
		return -5;
	}

	_rfm.allocate<hdrmap_file_roadseg_v1>(
		_rendermap->roadsegs.count, _rendermap->roadsegs);
	_rfm.allocate<hdrmap_file_junction_v1>(
		_rendermap->junctions.count, _rendermap->junctions);

	fprintf(stdout, "\nwriting target file ... ");
	fflush(nullptr);
	if (write_rendermap()) {
		fprintf(stdout, "[failed]\n");
		return -6;
	}
	fprintf(stdout, "[success]\n\n");
	return 0;
}

int odr_map::write_junctions(void)
{
	auto* i = _odr_junctions.next;
	for (; i != &_odr_junctions; i = i->next) {
		auto* junc = list_entry(odr_junction, _ownerlist, i);
		generate_render_junctions(junc);
		_rendermap->junctions.count++;
	}
	return 0;
}

int odr_map::write_lane_sections(void)
{
	int lanesec_count = getloader()->_lane_section_count;
	fprintf(stdout, "generating lane sections (0/%d) ...", lanesec_count);
	fflush(stdout);

	int lanesec_idx = 0;
	long prev_tc = gettick_millisecond();
	listnode_t* i = _odr_roads.next;
	for (; i != &_odr_roads; i = i->next) {
		auto* r = list_entry(odr_road, _ownerlist, i);
		if (generate_render_lanesections(r, lanesec_idx)) {
			return -4;
		}
		if(generate_render_roadobject(r)) {
			return -5;
		}
		create_rendermap_road(r);

		// show progress
		long cur = gettick_millisecond();
		if (cur - prev_tc > 200) {
			fprintf(stdout, "\rgenerating lane sections (%d/%d) ...",
				lanesec_idx, getloader()->_lane_section_count);
			fflush(stdout);
			prev_tc = cur;
		}
	}

	fprintf(stdout, "\rgenerating lane sections (%d/%d) ... [success]\n",
		lanesec_idx, lanesec_count);
	if (lanesec_idx < lanesec_count) {
		fprintf(stdout, "%d similar lane sections merged.\n",
		lanesec_count - lanesec_idx);
	}
	return 0;
}

void odr_map::create_rendermap_road(odr_road* r)
{
	auto* rr = _rfm.allocate<hdrmap_file_roadseg_v1>();
	rr->extinfo.id = r->_id;
	rr->extinfo.length = r->_length;
	
	// create the reference line
	size_t p, s = r->_odrv_road->ref_line->getsize();
	auto* refl = _rfm.allocate<hdrmap_file_refline_v1>(&p, s);
	refl->size_inbytes = s;
	r->_odrv_road->ref_line->serialize(refl->sdata);

	rr->refline.off.offset = p;
	rr->refline.off.link = _rendermap->offset_fixlist;
	_rendermap->offset_fixlist = _rfm.find_id_byaddr(&rr->refline.off);

	// create the superevelation
	size_t elevp, elevs = r->_odrv_road->superelevation.getsize();
	auto* sperelev = _rfm.allocate<hdrmap_file_superelev_v1>(&elevp, elevs);
	sperelev->size_inbytes = elevs;
	r->_odrv_road->superelevation.serialize(sperelev->sdata);

	rr->superelev.off.offset = elevp;
	rr->superelev.off.link = _rendermap->offset_fixlist;
	_rendermap->offset_fixlist = _rfm.find_id_byaddr(&rr->superelev.off);

	// create the lane section
	auto* offs = (offset*)_rfm.allocate<hdrmap_file_lanesec_v1>(
		r->_lanes->_lane_sects.getsize(), rr->lane_secs);

	for (int i = 0; i < r->_lanes->_lane_sects.getsize(); ++i) {
		auto* ls = r->_lanes->_lane_sects.buffer()[i];
		assert(ls->_rendermap_lanesec != nullptr);
		offs[i].offset = _rfm.find_id_byaddr(ls->_rendermap_lanesec);
	}

	// create the road object
	auto* objoffs = (offset*)_rfm.allocate<hdrmap_file_road_object_v1>(
		r->_objects->_objects.getsize(), rr->road_objs);

	for (int i=0; i < r->_objects->_objects.getsize(); ++i) {
		auto* obj = r->_objects->_objects.buffer()[i];
		// all road object shall have _render_roadobj be generated
		// we make an assert here
		assert(obj->_rendermap_roadobj != nullptr);
		objoffs[i].offset = _rfm.find_id_byaddr(obj->_rendermap_roadobj);
	}

	r->_rendermap_road = rr;
	_rendermap->roadsegs.count++;
}

int odr_map::generate_mesh(void)
{
	double eps = _psr.get_hdmap_info()->eps;
	fprintf(stdout, "building mesh for all objects ... ");
	fflush(stdout);

	listnode_t* i = _odr_roads.next;
	for (; i != &_odr_roads; i = i->next)
	{
		auto* r = list_entry(odr_road, _ownerlist, i);
		// parse all lanes sections in this read
		for (int lsc = 0; lsc < r->_lanes->_lane_sects.getsize(); ++lsc)
		{
			auto* ls = r->_lanes->_lane_sects.buffer()[lsc];
			shared_ptr<const LaneSection> lanesec = ls->_odrv_lanesect.lock();
			odr_rendermap_check(nullptr != lanesec);

			// we only parse the left and right lane
			for (int leftc = 0; leftc < ls->_left_lanes.getsize(); ++leftc) {
				auto* l = ls->_left_lanes.buffer()[leftc];
				shared_ptr<const Lane> lane = l->_odrv_lane.lock();
				odr_rendermap_check(nullptr != lane);
				l->_odrv_mesh.add_mesh(lane->get_mesh(lanesec->s0, lanesec->get_end(), eps));
				update_bounding_box(r, l, ls);
			}
			for (int rightc = 0; rightc < ls->_right_lanes.getsize(); ++rightc) {
				auto* l = ls->_right_lanes.buffer()[rightc];
				shared_ptr<const Lane> lane = l->_odrv_lane.lock();
				odr_rendermap_check(nullptr != lane);
				l->_odrv_mesh.add_mesh(lane->get_mesh(lanesec->s0, lanesec->get_end(), eps));
				update_bounding_box(r, l, ls);
			}
		}

		for (int obji = 0; obji < r->_objects->_objects.getsize(); ++obji)
		{
			auto* obj = r->_objects->_objects.buffer()[obji];
			shared_ptr<const RoadObject> roadobj = obj->_odrv_object.lock();
			if (nullptr == roadobj) {
				assert(obj->_type == object_type_userdef);
				continue;
			}
			obj->_odrv_mesh.add_mesh(roadobj->get_mesh(eps));

			if(obj->_odrv_mesh.vertices.size() == 0) {
				auto road_ptr = roadobj->road.lock();
				const Vec3D p0 = road_ptr->get_xyz(roadobj->s0, roadobj->t0,
					roadobj->z0, nullptr, nullptr, nullptr);
				update_bounding_box(p0[0], p0[1]);
				obj->update_bounding_box(p0[0], p0[1]);
				continue;
			}
			for (auto& v : obj->_odrv_mesh.vertices) {
				update_bounding_box(v[0], v[1]);
				obj->update_bounding_box(v[0], v[1]);
			}
		}
	}
	fprintf(stdout, "[success]\n");
	return 0;
}

void odr_map::update_bounding_box(odr_road* r,
	odr_lane* lane, odr_lane_section* ls)
{
	for (auto& v : lane->_odrv_mesh.vertices)
	{
		update_bounding_box(v[0], v[1]);
		ls->update_bounding_box(v[0], v[1]);

		// check if we need to update junction
		if (r->_junction_id > 0) {
			uint32_t id = r->_junction_id;
			auto* node = avl_find(_odr_junction_idtree,
				MAKE_FIND_OBJECT(id, odr_junction, _id, _avlnode),
				odr_junction::avl_id_compare);
			assert(nullptr != node);
			auto* junc = AVLNODE_ENTRY(odr_junction, _avlnode, node);
			junc->update_bounding_box(v[0], v[1]);
		}
	}
}

int odr_map::build_blocks(void)
{
	_rows = round((_y1 - _y0) / _psr.get_hdmap_info()->block_height);
	_cols = round((_x1 - _x0) / _psr.get_hdmap_info()->block_width);
	fprintf(stdout, "blocks: %u x %u (width = %.2f, height = %.2f)\n",
		_cols, _rows, (_x1 - _x0) / double(_cols),
		(_y1 - _y0) / double(_rows));

	assert(nullptr == _rblks);
	_rblks = new hdmap_renderblock[_cols * _rows];
	odr_rendermap_check(nullptr != _rblks);
	return 0;
}

int odr_map::generate_render_roadobject(odr_road* r)
{
	hdmapcfg* cfg = _psr.get_hdmap_info();
	double blkw = (_x1 - _x0) / double(_cols);
	double blkh = (_y1 - _y0) / double(_rows);

	int objsz = r->_objects->_objects.getsize();
	for (int i = 0; i < objsz; ++i)
	{
		set<uint32_t> areas;
		auto* obj = r->_objects->_objects.buffer()[i];
		if (obj->_type != object_type_userdef) {
			assert(!obj->_odrv_object.expired());

			double dx0 = obj->_x0 - _x0, dx1 = obj->_x1 - _x0;
			double dy0 = obj->_y0 - _y0, dy1 = obj->_y1 - _y0;
			int x0 = int(dx0 / blkw);
			int y0 = int(dy0 / blkh);
			int x1 = int(dx1 / blkw);
			int y1 = int(dy1 / blkh);

			double ddx = obj->_x1 - obj->_x0;
			double ddy = obj->_y1 - obj->_y0;
			if (ddx < std::numeric_limits<double>::min() &&
				ddy < std::numeric_limits<double>::min()) {
				areas.insert(x0 + y0 * _cols);
			} 
			else if (ddx < std::numeric_limits<double>::min()) {
				do {
					areas.insert(x0 + y0 * _cols);
					y0++;
				} while (y0 < y1);
			}
			else if (ddy < std::numeric_limits<double>::min()) {
				do {
					areas.insert(x0 + y0 * _cols);
					x0++;
				} while (x0 < x1);
			}
			else {
				if (fabs(double(x1) * blkw - dx1) > 1E-6) ++x1;
				if (fabs(double(y1) * blkh - dy1) > 1E-6) ++y1;
				assert(x0 <= x1 && y0 <= y1);

				find_obj_owner_block(obj, x0, y0, x1, y1, areas);
			}
		}
		// generate the rendermap road object
		create_rendermap_road_object(obj, r, areas);
	}
	return 0;
}

void odr_map::create_rendermap_road_object(
	odr_object* obj, odr_road* r, set<uint32_t>& areas)
{
	init_rendermap_road_object(obj, r, areas);

	// save all referred blocks
	auto robj = obj->_rendermap_roadobj;
	uint32_t sz = areas.size() * sizeof(uint32_t);
	auto* blkids = (uint32_t*)_rfm.allocate(sz, robj->extinfo.referred_blockids);
	int idx = 0; 
	for (auto& blkid : areas) {
		blkids[idx++] = blkid;
	}
	roadobject_addto_blocks(obj, areas);
}

void odr_map::init_road_object_rsd(odr_object* obj,
	odr_road* r, hdrmap_file_road_object_v1* ro,
	exd_roadside_device_info* exd_rsdinfo, set<uint32_t>& areas)
{
	size_t pos;
	auto* rsdinfo = _rfm.allocate<roadside_device_info_v1>(&pos);
	odr_rendermap_check(rsdinfo != nullptr);

	rsdinfo->spotid = obj->_id;
	rsdinfo->devtype = exd_rsdinfo->devtype;
	rsdinfo->junctionid = exd_rsdinfo->junctionid;

	// convert the <lon, lat> to UTM coord
	rsdinfo->base_xyz = _wgs84prj.transform(llh(obj->_lon, obj->_lat, obj->_z_offset));
	rsdinfo->base_xyz -= point3d(_xoffset, _yoffset);
	auto mat = EulerAnglesToMatrix(obj->_roll, obj->_pitch, obj->_hdg);
	
	// set the block of the road side device
	int64_t blkid = find_block_by_pos(rsdinfo->base_xyz);
	odr_rendermap_check(blkid >= 0 && !areas.size());
	areas.insert((uint32_t)blkid);

	for (int idx = 0; idx < 3 * 3; idx++) {
		rsdinfo->matrix[idx] = ((double*)mat.data())[idx];
	}
	// save the road side device
	ro->extinfo.rsdinfo.off.offset = pos;
	ro->extinfo.rsdinfo.off.link = _rendermap->offset_fixlist;
	_rendermap->offset_fixlist = _rfm.find_id_byaddr(&ro->extinfo.rsdinfo.off);
}

void odr_map::init_road_object_sigboard(odr_object* obj,
	odr_road* r, hdrmap_file_road_object_v1* ro,
	exd_sigboard_info* exd_sigboardinfo, set<uint32_t>& areas)
{
	size_t pos;
	auto* sigboardinfo = _rfm.allocate<sigboard_info_v1>(&pos);
	odr_rendermap_check(sigboardinfo != nullptr);

	sigboardinfo->sigboard_uid = exd_sigboardinfo->uid;
	sigboardinfo->width = exd_sigboardinfo->width;
	sigboardinfo->length = exd_sigboardinfo->length;
	sigboardinfo->a.shape_type = exd_sigboardinfo->shape_type;
	sigboardinfo->a.kind = exd_sigboardinfo->kind;
	sigboardinfo->t.major = exd_sigboardinfo->major_type;
	sigboardinfo->t.sub = exd_sigboardinfo->sub_type;

	// save the arrow object
	ro->extinfo.sigboardinfo.off.offset = pos;
	ro->extinfo.sigboardinfo.off.link = _rendermap->offset_fixlist;
	_rendermap->offset_fixlist = _rfm.find_id_byaddr(&ro->extinfo.sigboardinfo.off);
}

void odr_map::update_sigboard_pole_object(odr_object* obj, odr_road* r, set<uint32_t>& areas)
{
	auto sigboard = obj->_pole_related_sigboard;
	assert(nullptr != sigboard);

	// calculate the absolute height of the road
	auto p0 = r->_odrv_road->get_xyz(sigboard->_s, sigboard->_t, 0.);
	double height = obj->_outline_base.xyz.z - p0[2];
	obj->_outline_base.xyz.z = p0[2];
	
	// reset all height for all points in pole surface polygon
	for (int i = 0; i < obj->_outline->_cornerlocals.getsize(); ++i) {
		auto p = obj->_outline->_cornerlocals.buffer()[i];
		p->_height = height;
	}

	// set the block of the signal board pole
	int64_t blkid = find_block_by_pos(sigboard->_outline_base);
	odr_rendermap_check(blkid >= 0 && !areas.size());
	areas.insert((uint32_t)blkid);

	auto objptr = obj->_odrv_object.lock();
	if (nullptr == objptr) {
		assert(obj->_type == object_type_userdef);
		objptr = make_shared<RoadObject>();
		odr_rendermap_check(nullptr != objptr);
	}
	r->odrv_update_roadobject(obj, objptr, RoadObjectCorner::Type::Road);
}

void odr_map::update_siglight_object(odr_object* obj, odr_road* r)
{
	auto hdmcfg = _psr.get_hdmap_info();
	// if (hdmcfg->siginfo.pole_radius < 1e-3) {
	// 	return;
	// }

	// we handle the request of creating pseudo pole
	auto objptr = obj->_odrv_object.lock();
	if (nullptr == objptr) {
		assert(obj->_type == object_type_userdef);
		objptr = make_shared<RoadObject>();
		odr_rendermap_check(nullptr != objptr);
		obj->_odrv_object = objptr;
	}

	assert(!objptr->outline.size());
	obj->_height = (obj->_z_offset > 1e-3) ? obj->_z_offset
		: hdmcfg->siginfo.zoffset;

	// set all other parameters as 0
	obj->_radius = hdmcfg->siginfo.pole_radius;
	obj->_roll = obj->_pitch = obj->_pitch =
	obj->_z_offset = obj->_width = obj->_length = 0.;

	// update the odrv_roadobject
	r->odrv_update_roadobject(obj, objptr);
}

void odr_map::init_road_object_arrow(odr_object* obj, odr_road* r,
	hdrmap_file_road_object_v1* ro,
	exd_arrow_info* exd_arrowinfo, set<uint32_t>&)
{
	size_t pos;
	auto* arrowinfo = _rfm.allocate<arrow_info_v1>(&pos);
	odr_rendermap_check(arrowinfo != nullptr);

	// udate the road object unique ID
	arrowinfo->arrow_uid = obj->_id;
	arrowinfo->type = exd_arrowinfo->type;
	arrowinfo->color = exd_arrowinfo->fillcolor;

	// save the arrow object
	ro->extinfo.siginfo.off.offset = pos;
	ro->extinfo.siginfo.off.link = _rendermap->offset_fixlist;
	_rendermap->offset_fixlist = _rfm.find_id_byaddr(&ro->extinfo.siginfo.off);
}

void odr_map::init_road_object_signal_light(odr_object* obj,
	odr_road* r, hdrmap_file_road_object_v1* ro,
	exd_siglight_info* exd_siginfo, set<uint32_t>& areas)
{
	size_t pos;
	auto* siginfo = _rfm.allocate<siglight_info_v1>(&pos);
	odr_rendermap_check(siginfo != nullptr);

	// udate the road object unique ID
	siginfo->siguid = obj->_id;

	Mat3D mat;
	Vec3D p0, e_s, e_t, e_h;
	if (obj->_type != object_type_userdef) {
		p0 = r->_odrv_road->get_xyz(obj->_s, obj->_t, obj->_z_offset, &e_s, &e_t, &e_h);
		if (nullptr == exd_siginfo) {
			// calculate x, y, z
			const Mat3D base_mat{{{ e_s[0], e_t[0], e_h[0] },
				{ e_s[1], e_t[1], e_h[1] },
				{ e_s[2], e_t[2], e_h[2] }}};

			const Mat3D rot_mat = EulerAnglesToMatrix(obj->_roll, obj->_pitch, obj->_hdg);
			MatMatMultiplication(base_mat, rot_mat, mat);
		}
		else mat = EulerAnglesToMatrix(obj->_roll, obj->_pitch, exd_siginfo->heading);
		siginfo->base_xyz = {p0[0], p0[1], p0[2]};
	}
	else {
		// this is a new added signal light
		mat = EulerAnglesToMatrix(obj->_roll, obj->_pitch, obj->_hdg);
		siginfo->base_xyz = _wgs84prj.transform(llh(obj->_lon, obj->_lat, obj->_z_offset));
		siginfo->base_xyz -= point3d(_xoffset, _yoffset);

		// set the block of the signal light
		int64_t blkid = find_block_by_pos(siginfo->base_xyz);
		odr_rendermap_check(blkid >= 0 && !areas.size());
		areas.insert((uint32_t)blkid);
	}

	for (int idx = 0; idx < 3 * 3; idx++) {
		siginfo->matrix[idx] = ((double*)mat.data())[idx];
	}
	// save the siginfo
	ro->extinfo.siginfo.off.offset = pos;
	ro->extinfo.siginfo.off.link = _rendermap->offset_fixlist;
	_rendermap->offset_fixlist = _rfm.find_id_byaddr(&ro->extinfo.siginfo.off);
}

int64_t odr_map::find_block_by_pos(const point3d& pt)
{
	if (pt.xyz.x < _x0 || pt.xyz.x > _x1
		|| pt.xyz.y < _y0 || pt.xyz.y > _y1) {
		return -1;
	}
	double blkw = (_x1 - _x0) / double(_cols);
	double blkh = (_y1 - _y0) / double(_rows);
	int x = (pt.xyz.x - _x0) / blkw;
	int y = (pt.xyz.y - _y0) / blkh;
	return (y * _cols) + x;
}

int odr_map::serialized_datasize(odr_object* obj, odr_road* r, set<uint32_t>& areas)
{
	if (obj->_name == "signalLight") {
		// handle the case of generating pseudo pole for the signal light
		// this may change the deserialize data size
		update_siglight_object(obj, r);
	}
	else if (obj->_name == "signalBoardPole") {
		// create a pole object (cylinder) for the signal board
		update_sigboard_pole_object(obj, r, areas);
	}

	shared_ptr<RoadObject> ro = obj->_odrv_object.lock();
	if (nullptr == ro) {
		return 0;
	}
	return ro->getsize();
}

void odr_map::init_rendermap_road_object(odr_object* obj,
	odr_road* r, set<uint32_t>& areas)
{
	void* ptr = (void*)obj->_rendermap_roadobj;
	auto& robj = obj->_rendermap_roadobj;

	size_t s = serialized_datasize(obj, r, areas);
	robj = _rfm.allocate<hdrmap_file_road_object_v1>(nullptr, s);
	robj->size_inbytes = s;
	robj->extinfo.id = get_road_object_index(obj->_id);
	robj->extinfo.roid = obj->_id;
	robj->extinfo.roadid = r->_id;

	if (obj->_name == "signalLight") {
		auto siginfo = (exd_siglight_info*)ptr;
		init_road_object_signal_light(obj, r, robj, siginfo, areas);
		if (siginfo) delete siginfo;
		robj->type = objtype_road_object_signal_light;
	}
	else if (obj->_name == "roadSideDevice") {
		auto rsdinfo = (exd_roadside_device_info*)ptr;
		init_road_object_rsd(obj, r, robj, rsdinfo, areas);
		if (rsdinfo) delete rsdinfo;
		robj->type = objtype_road_object_rsd;
	}
	else if (obj->_name == "arrow" && ptr != nullptr) {
		auto arrowinfo = (exd_arrow_info*)ptr;
		init_road_object_arrow(obj, r, robj, arrowinfo, areas);
		if (arrowinfo) delete arrowinfo;
		robj->type = objtype_road_object_arrow;
	}
	else if (obj->_name == "signalBoard" && ptr != nullptr) {
		auto sigboardinfo = (exd_sigboard_info*)ptr;
		init_road_object_sigboard(obj, r, robj, sigboardinfo, areas);
		if (sigboardinfo) delete sigboardinfo;
		robj->type = objtype_road_object_sigboard;
	}
	else if (obj->_name == "signalBoardPole") {
		robj->type = objtype_road_object_cared;
	}
	else if (obj->_name == "junconver") {
		robj->type = objtype_road_object_junconver;
	}

	if (s > 0) {
		shared_ptr<RoadObject> ro = obj->_odrv_object.lock();
		ro->serialize((void*)robj->sdata);
	}
}

int odr_map::generate_render_junctions(odr_junction* junc)
{
	double x = (junc->_x1 + junc->_x0) / 2.;
	double y = (junc->_y1 + junc->_y0) / 2.;
	assert(x >= _x0 && x <= _x1 && y >= _y0 && y <= _y1);

	hdmapcfg* cfg = _psr.get_hdmap_info();
	double blkw = (_x1 - _x0) / double(_cols);
	double blkh = (_y1 - _y0) / double(_rows);

	int xx = int((x - _x0) / blkw);
	int yy = int((y - _y0) / blkh);
	int blkid = yy * _cols + xx;

	auto* block = &_rblks[blkid];
	block->junctions.push_back(junc);
	return 0;
}

int odr_map::generate_render_lanesections(odr_road* r, int& count)
{
	hdmapcfg* cfg = _psr.get_hdmap_info();
	double blkw = (_x1 - _x0) / double(_cols);
	double blkh = (_y1 - _y0) / double(_rows);

	int lsc = r->_lanes->_lane_sects.getsize();
	for (int i = 0; i < lsc; ++i, ++count)
	{
		set<uint32_t> areas;
		auto ls = r->_lanes->_lane_sects.buffer()[i];
		double dx0 = ls->_x0 - _x0, dx1 = ls->_x1 - _x0;
		double dy0 = ls->_y0 - _y0, dy1 = ls->_y1 - _y0;
		int x0 = int(dx0 / blkw);
		int y0 = int(dy0 / blkh);
		int x1 = int(dx1 / blkw);
		int y1 = int(dy1 / blkh);
		if (fabs(double(x1) * blkw - dx1) > 1E-6) ++x1;
		if (fabs(double(y1) * blkh - dy1) > 1E-6) ++y1;
		assert(x0 <= x1 && y0 <= y1);

		// we only parse the left and right lane
		for (int leftc = 0; leftc < ls->_left_lanes.getsize(); ++leftc) {
			auto* l = ls->_left_lanes.buffer()[leftc];
			find_lane_owner_blocks(l, x0, y0, x1, y1, areas);
			/* todo: roadmark mesh */
		}
		for (int rightc = 0; rightc < ls->_right_lanes.getsize(); ++rightc) {
			auto* l = ls->_right_lanes.buffer()[rightc];
			find_lane_owner_blocks(l, x0, y0, x1, y1, areas);
			/* todo: roadmark mesh */
		}
		// add the lane section to the block
		lanesection_addto_blocks(ls, areas);

		// generate the rendermap lane section object
		create_rendermap_lane_section(ls, count, areas);
	}
	return 0;
}

void odr_map::create_rendermap_lane_section(
	odr_lane_section* ls, int index, set<uint32_t>& areas)
{
	shared_ptr<LaneSection> lanesec = ls->_odrv_lanesect.lock();
	assert(nullptr != lanesec);
	size_t s = lanesec->getsize();

	auto& rls = ls->_rendermap_lanesec;
	rls = _rfm.allocate<hdrmap_file_lanesec_v1>(nullptr, s);
	rls->size_inbytes = s;
	rls->extinfo.id = index + 1;
	rls->extinfo.roadid = strtol(lanesec->road.lock()->id.c_str(), nullptr, 10);
	lanesec->serialize((void*)rls->sdata);

	// save all referred blocks
	uint32_t sz = areas.size() * sizeof(uint32_t);
	auto* blkids = (uint32_t*)_rfm.allocate(sz, rls->extinfo.referred_blockids);
	int idx = 0; for (auto& blkid : areas) {
		blkids[idx++] = blkid;
	}
}

void odr_map::roadobject_addto_blocks(odr_object* obj, set<uint32_t>& areas)
{
	for (auto& blkid : areas) {
		assert(blkid >= 0 && blkid < _cols * _rows);
		hdmap_renderblock* block = &_rblks[blkid];
		block->objects.push_back(obj);
	}
}

void odr_map::lanesection_addto_blocks(
	odr_lane_section* ls, set<uint32_t>& areas)
{
	for (auto& blkid : areas) {
		assert(blkid >= 0 && blkid < _cols * _rows);
		hdmap_renderblock* block = &_rblks[blkid];
		block->lanesecs.push_back(ls);
	}
}

int odr_map::find_obj_owner_block(odr_object* obj, int blk_x0,
	int blk_y0, int blk_x1, int blk_y1, set<uint32_t>& areas)
{
	int x, y;
	vector<point> pts;

	double blkw = (_x1 - _x0) / double(_cols);
	double blkh = (_y1 - _y0) / double(_rows);

	for (y = blk_y0; y < blk_y1; ++y)
	{
		for (x = blk_x0; x < blk_x1; ++x)
		{
			double x_pos = _x0 + double(x) * blkw;
			double y_pos = _y0 + double(y) * blkh;

			pts.clear();
			pts.push_back({x_pos, y_pos});
			pts.push_back({x_pos + blkw, y_pos});
			pts.push_back({x_pos + blkw, y_pos + blkh});
			pts.push_back({x_pos, y_pos + blkh});
			if (is_intersect_with_obj(pts, obj)) {
				areas.insert(x + y * _cols);
			}
			//printf("%d,%d) s = %.2f\n", x, y, s);
		}
	}
	return 0;
}

int odr_map::find_lane_owner_blocks(odr_lane* lane, int blk_x0,
	int blk_y0, int blk_x1, int blk_y1,
	set<uint32_t>& areas)
{
	int x, y;
	vector<point> pts;

	double blkw = (_x1 - _x0) / double(_cols);
	double blkh = (_y1 - _y0) / double(_rows);

	for (y = blk_y0; y < blk_y1; ++y)
	{
		for (x = blk_x0; x < blk_x1; ++x)
		{
			double x_pos = _x0 + double(x) * blkw;
			double y_pos = _y0 + double(y) * blkh;

			pts.clear();
			pts.push_back({x_pos, y_pos});
			pts.push_back({x_pos + blkw, y_pos});
			pts.push_back({x_pos + blkw, y_pos + blkh});
			pts.push_back({x_pos, y_pos + blkh});
			if (is_intersect_with_lane(pts, lane)) {
				areas.insert(x + y * _cols);
			}
		}
	}
	return 0;
}

bool odr_map::is_intersect_with_obj(vector<point>& p1, odr_object* obj)
{
	vector<point> p2, output;
	int sz = obj->_odrv_mesh.indices.size();
	for (int i = 0; i < sz; i += 3) {
		p2.clear();
		for (int j = 0; j < 3; j++) {
			auto& v = obj->_odrv_mesh.vertices[i + j];
			p2.push_back({v[0], v[1]});
		}
		if (is_polygons_intersected(p1, p2)) {
			return true;
		}
	}
	return false;
}

bool odr_map::is_intersect_with_lane(vector<point>& p1, odr_lane* l)
{
	vector<point> p2, output;
	int sz = l->_odrv_mesh.indices.size();
	for (int i = 0; i < sz; i += 3) {
		p2.clear();
		for (int j = 0; j < 3; j++) {
			auto& v = l->_odrv_mesh.vertices[i + j];
			p2.push_back({v[0], v[1]});
		}
		if (is_polygons_intersected(p1, p2)) {
			return true;
		}
	}
	return false;
}

int odr_map::write_rendermap(void)
{
	if (write_roadseg()) {
		return -1;
	}
	if (write_blocks()) {
		return -2;
	}
	// note: must be later than calling of write_blocks()
	if (write_junction_table()) {
		return -3;
	}
	if (_rfm.write_data(_rendermap, _rendermap_fp)) {
		return -4;
	}
	return 0;
}

int odr_map::write_rendermap_header(void)
{
	size_t extsz = _georef.size() + 1;
	size_t tmp = sizeof(hdrmap_file_rendermap_v1) + extsz;
	tmp = (tmp + (RMAP_ALIGN_BYTES - 1)) & ~(RMAP_ALIGN_BYTES - 1);
	extsz = tmp - sizeof(hdrmap_file_rendermap_v1);

	_rendermap = _rfm.allocate<hdrmap_file_rendermap_v1>(nullptr, extsz);
	_rfm.set_fixlist(&_rendermap->offset_fixlist);

	memset(_rendermap, 0, tmp);
	memcpy(_rendermap->magic, HDM_RENDER_MAGIC,
		sizeof(_rendermap->magic));
	_rendermap->version = HDM_RENDER_VERSION;

	// attributes
	_rendermap->a.persistence = 1;

	// uuid
	zas::utils::uuid uid;
	uid.generate();
	if (!uid.valid()) {
		return -1;
	}
	_rendermap->uuid = uid;
	_rendermap->size = 0;

	_rendermap->offset_fixlist = 0;

	_rendermap->xoffset = _xoffset;
	_rendermap->yoffset = _yoffset;

	_rendermap->xmin = _x0;
	_rendermap->ymin = _y0;
	_rendermap->xmax = _x1;
	_rendermap->ymax = _y1;

	_rendermap->block_width = (_x1 - _x0) / double(_cols);
	_rendermap->block_height = (_y1 - _y0) / double(_rows);

	_rendermap->rows = _rows;
	_rendermap->cols = _cols;

	_rendermap->junc_kdtree = nullptr;
	if (_georef.size()) {
		fprintf(stdout, "proj: <%s>\n", _georef.c_str());
		strcpy(_rendermap->proj, _georef.c_str());
	}

	// write it to file
	string path = _psr.get_targetdir();
	path += "rendermap";

	_rendermap_fp = fopen(path.c_str(), "wb");
	if (nullptr == _rendermap_fp) {
		fprintf(stderr, "fail to create file: %s\n", path.c_str());
		return -2;
	}
	return 0;
}

int file_memory::rdmap_bufnode::id_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(rdmap_bufnode, id_avlnode, a);
	auto* bb = AVLNODE_ENTRY(rdmap_bufnode, id_avlnode, b);
	if (aa->id > bb->id) { return 1; }
	else if (aa->id < bb->id) { return -1; }
	else return 0;
}

int file_memory::rdmap_bufnode::addr_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(rdmap_bufnode, addr_avlnode, a);
	auto* bb = AVLNODE_ENTRY(rdmap_bufnode, addr_avlnode, b);
	if (aa->addr_start >= bb->addr_start
		&& aa->addr_end <= bb->addr_end) return 0;
	else if (aa->addr_start >= bb->addr_end) return 1;
	else if (aa->addr_end <= bb->addr_start) return -1;
	return 0;
}

void* file_memory::find_addr_byid(size_t _id)
{
	auto* ret = avl_find(_rdmap_bufnode_id_tree,
		MAKE_FIND_OBJECT(_id, rdmap_bufnode, id, id_avlnode),
		rdmap_bufnode::id_compare);
	odr_rendermap_check(nullptr != ret);

	return (void*)AVLNODE_ENTRY(rdmap_bufnode, \
		id_avlnode, ret)->addr_start;
}

size_t file_memory::find_id_byaddr(void* addr)
{
	rdmap_bufnode dummy;
	dummy.addr_start = (size_t)addr;
	dummy.addr_end =  (size_t)addr;
	auto* ret = avl_find(_rdmap_bufnode_addr_tree,
		&dummy.addr_avlnode, rdmap_bufnode::addr_compare);
	odr_rendermap_check(nullptr != ret);

	auto* node = AVLNODE_ENTRY(rdmap_bufnode, addr_avlnode, ret);

	assert((size_t)addr >= node->addr_start \
		&& (size_t)addr < node->addr_end);
	size_t delta = (size_t)addr - node->addr_start;
	return node->id + delta;
}

int odr_map::write_roadseg(void)
{
	int i = 0;
	auto* offs = (offset*)_rfm.find_addr_byid(
		_rendermap->roadsegs.indices_off.offset);

	auto* node = avl_first(_odr_road_idtree);
	while (node) {
		auto* r = AVLNODE_ENTRY(odr_road, _avlnode, node);
		size_t pos = _rfm.find_id_byaddr(r->_rendermap_road);
		offs[i++].offset = pos;
		node = avl_next(node);
	}
	return 0;
}

int odr_map::write_junction_table(void)
{
	int i = 0;
	auto* offs = (offset*)_rfm.find_addr_byid(
		_rendermap->junctions.indices_off.offset);

	auto* node = avl_first(_odr_junction_idtree);
	while (node) {
		auto* j = AVLNODE_ENTRY(odr_junction, _avlnode, node);
		offs[i++].offset = j->_rendermap_juncoff;
		node = avl_next(node);
	}
	return 0;
}

struct odr_roadptr
{
	odr_roadptr(odr_road* _r) : r(_r) {} 
	odr_road* r;
	bool operator<(const odr_roadptr& other) const {
		uint32_t a = r->_rendermap_road->extinfo.id;
		uint32_t b = other.r->_rendermap_road->extinfo.id;
		return (a < b);
	}
};

struct odr_junction_ptr
{
	odr_junction_ptr(odr_junction* j) : junc(j) {} 
	odr_junction* junc;
	bool operator<(const odr_junction_ptr& other) const {
		uint32_t a = junc->_id;
		uint32_t b = other.junc->_id;
		return (a < b);
	}
};

int odr_map::write_blocks(void)
{
	int cnt = _cols * _rows;
	auto* tblks = (offset*)_rfm.allocate<hdrmap_file_blockinfo_v1>
		(cnt, _rendermap->blocks);

	for (int i = 0; i < cnt; ++i)
	{
		size_t pos;
		auto& blk = _rblks[i];
		auto* tblk = _rfm.allocate<hdrmap_file_blockinfo_v1>(&pos);
		tblks[i].offset = pos;

		tblk->row = i / _cols;
		tblk->col = i % _cols;

		// add reference to all related roads
		set<odr_roadptr> roads;
		for (int j = 0; j < blk.lanesecs.size(); ++j) {
			auto* ls = blk.lanesecs[j];
			auto* p = ls->_parents->_parents;
			assert(p->get_type() == odr_elemtype_road);
			auto* r = zas_downcast(odr_element, odr_road, p);
			roads.insert({r});
		}
		// all new created road object has no related lanesection
		// we manully add its related road to the set
		for (int j = 0; j < blk.objects.size(); ++j) {
			auto* o = blk.objects[j];
			if (o->_type != object_type_userdef) {
				continue;
			}
			auto* p = o->_parents;
			assert(p->get_type() == odr_elemtype_road);
			auto* r = zas_downcast(odr_element, odr_road, p);
			roads.insert({r});
		}
		if (roads.size()) {
			int idx = 0;
			auto* rsegs = _rfm.allocate<hdrmap_file_roadseg_v1>(
				roads.size(), tblk->road_segs);

			for (auto& r : roads) {
				offset* rseg_off = (offset*)&rsegs[idx++];
				rseg_off->offset = _rfm.find_id_byaddr(r.r->_rendermap_road);
			}
		}

		// add all lane sections
		if (blk.lanesecs.size()) {
			int idx = 0;
			auto* lss = _rfm.allocate<hdrmap_file_lanesec_v1>(
				blk.lanesecs.size(), tblk->lane_secs);

			for (auto& ls : blk.lanesecs) {
				offset* lss_off = (offset*)&lss[idx++];
				lss_off->offset = _rfm.find_id_byaddr(ls->_rendermap_lanesec);
			}
		}

		// add all road object
		if (blk.objects.size()) {
			int idx = 0;
			auto* obj = _rfm.allocate<hdrmap_file_road_object_v1>(
				blk.objects.size(), tblk->road_objs);

			for(auto* o : blk.objects) {
				offset* obj_off = (offset*)&obj[idx++];
				obj_off->offset = _rfm.find_id_byaddr(o->_rendermap_roadobj);
			}
		}

		// add all junction
		if (blk.junctions.size()) {
			int idx = 0;
			auto* obj = _rfm.allocate<hdrmap_file_junction_v1>(
				blk.junctions.size(), tblk->junctions);

			for (auto junc : blk.junctions) {
				size_t off;
				auto* j = _rfm.allocate<hdrmap_file_junction_v1>(&off, 0);
				update_rendermap_junction(j, junc);
				junc->_rendermap_juncoff = off;

				offset* junc_off = (offset*)&obj[idx++];
				junc_off->offset = off;
			}
		}
	}
	return 0;
}

void odr_map::update_rendermap_junction(hdrmap_file_junction_v1* j, odr_junction* junc)
{
	j->center_x = (junc->_x1 + junc->_x0) / 2.;
	j->center_y = (junc->_y1 + junc->_y0) / 2.;
	
	double r1 = (junc->_x1 - junc->_x0) / 2.;
	double r2 = (junc->_y1 - junc->_y0) / 2.;
	j->radius = (r1 > r2) ? r1 : r2;
	j->extinfo.id = junc->_id;

	if(junc->_approach.getsize() > 0) {
		auto* obj = _rfm.allocate<hdrmap_file_approach_info_v1>(
			junc->_approach.getsize(), j->approachs);
		for (int i = 0; i < junc->_approach.getsize(); ++i) {
			size_t off;
			size_t approach_sz = junc->_approach.buffer()[i]->length();
			auto* appro = _rfm.allocate<hdrmap_file_approach_info_v1>(&off, approach_sz);
			appro->uid_sz = approach_sz;
			memcpy(appro->data, junc->_approach.buffer()[i]->c_str(), approach_sz);

			offset* appr_off = (offset*)&obj[i];
			appr_off->offset = off;
		}
	}

	memset(&j->attrs, 0, sizeof(j->attrs));
	j->left = j->right = nullptr;
}

} // end of namespace osm_parser1
/* EOF */
