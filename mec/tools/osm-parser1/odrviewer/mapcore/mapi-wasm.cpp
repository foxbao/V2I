#include "inc/mapi-wasm.h"
#include "utils/uuid.h"
#include "../Math.hpp"
namespace mapi {

using namespace zas::mapcore;
using namespace odr;

int tilecache::bind_map(const string& mapinfo)
{
	if (!mapinfo.size()) {
		return -1;
	}
	return tilecache_impl::bind_map(mapinfo.data(), mapinfo.size());
}

int tilecache::add_tile(const string& buffer)
{
	if (!buffer.size()) {
		return -1;
	}
	int ret = tilecache_impl::add_tile(buffer.data(), buffer.size());
	printf(".. add tile %d, size %lu\n", ret, buffer.size());
	return ret;
}

RoadNetworkMesh tilecache::get_mesh(uint32_t blkid)
{
	bool temp;
	auto tilemesh = tilecache_impl::get_mesh(blkid, &temp);
	if (tilemesh) {
		return *tilemesh;
	}
	RoadNetworkMesh emptyobj;
	return emptyobj;
}

int tilecache::release_mesh(uint32_t blkid)
{
	int ret = tilecache_impl::release_mesh(blkid);
	return ret;
}

emscripten::val tilecache::query_unloaded_blocks(emscripten::val pts)
{
	emscripten::val retarr = emscripten::val::array();
	uint32_t p2len = pts["length"].as<uint32_t>();
	std::vector<point2d> p2ds;
	std::set<uint32_t> blks;
	if(p2len > 0) {
		for(uint32_t i = 0; i < p2len; i++) {
			double x = pts[i]["x"].as<double>();
			double y = pts[i]["y"].as<double>();
			point2d p2d;
			p2d.x = x;
			p2d.y = y;
			p2ds.push_back(p2d);
		}
	}
	int ret = tilecache_impl::query_unloaded_blocks(p2ds, blks);
	if(ret == 0) {
		uint32_t index = 0;
		for(auto v : blks) {
			retarr.set(index, v);
			index++;
		}
	}
	return retarr;
}

string tilecache::get_mapid(void)
{
	uint128_t id;
	bool ret = tilecache_impl::get_mapid(id);
	if (!ret) {
		return "";
	}
	uuid uid(id);
	std::string result;
	uid.to_hex(result);
	return result;
}

const emscripten::val tilecache::get_map_georef()
{
	const char* ret = tilecache_impl::get_map_georef();
	emscripten::val result(ret);
	return result;
}

const emscripten::val tilecache::get_mapoffset()
{
	const point3d p3d = tilecache_impl::get_mapoffset();
	emscripten::val ret = emscripten::val::object();
	ret.set("x", p3d.v[0]);
	ret.set("y", p3d.v[1]);
	ret.set("z", p3d.v[2]);
	return ret;
}

bool tilecache::map_available()
{
	bool ret = tilecache_impl::map_available();
	return ret;
}

emscripten::val tilecache::filter_loaded_blocks(emscripten::val blkids, bool lock)
{
	emscripten::val retarr = emscripten::val::array();
	uint32_t blklen = blkids["length"].as<uint32_t>();
	if(blklen > 0) {
		std::set<uint32_t> blks;
		for(uint32_t i = 0; i < blklen; i++) {
			blks.insert(blkids[i].as<uint32_t>());
		}
		int ret = tilecache_impl::filter_loaded_blocks(blks, lock);
		if(ret == 0) {
			uint32_t index = 0;
			for(auto v : blks) {
				retarr.set(index, v);
				index++;
			}
		}
	}
	return retarr;
}

int tilecache::lock_blocks(const emscripten::val blkids)
{
	int ret = 0;
	uint32_t blklen = blkids["length"].as<uint32_t>();
	if(blklen > 0) {
		std::set<uint32_t> blks;
		for(uint32_t i = 0; i < blklen; i++) {
			blks.insert(blkids[i].as<uint32_t>());
		}
		ret = tilecache_impl::lock_blocks(blks);
	}
	return ret;
}

int tilecache::unlock_blocks(const emscripten::val blkids)
{
	int ret = 0;
	uint32_t blklen = blkids["length"].as<uint32_t>();
	if(blklen > 0) {
		std::set<uint32_t> blks;
		for(uint32_t i = 0; i < blklen; i++) {
			blks.insert(blkids[i].as<uint32_t>());
		}
		ret = tilecache_impl::unlock_blocks(blks);
	}
	return ret;
}

int tilecache::release_blocks(const emscripten::val blkids)
{
	int ret = 0;
	uint32_t blklen = blkids["length"].as<uint32_t>();
	if(blklen > 0) {
		std::set<uint32_t> blks;
		for(uint32_t i = 0; i < blklen; i++) {
			blks.insert(blkids[i].as<uint32_t>());
		}
		ret = tilecache_impl::release_blocks(blks);
	}
	return ret;
}

emscripten::val tilecache::get_singlelight_blocks(emscripten::val blkids) {
	emscripten::val retarr = emscripten::val::array();
	uint32_t blklen = blkids["length"].as<uint32_t>();
	if(blklen > 0) {
		std::set<uint32_t> blks;
		int siglightindex = 0;
		for(uint32_t i = 0; i < blklen; i++) {
			uint32_t blkid = blkids[i].as<uint32_t>();
			set<const rendermap_roadobject*> roret;
			tilecache_impl::extract_roadobjects(blkid, {hdrm_robjtype_signal_light}, roret);

			set<const rendermap_roadobject*>::iterator it;
			for (it = roret.begin(); it != roret.end(); it++) {
				const hdrmap_ro_signal_light* light = (*it)->as_siglight();
				point3d p3d;
				light->getpos(p3d);
				const double *mtx = light->get_rotate_matrix();
				emscripten::val mtxarr = emscripten::val::array();
				for(int k = 0; k < 9; k++) {
					mtxarr.set(k, *(mtx + k));
				}
				// double mtx;
				// light->get_rotate_matrix(&mtx);

				emscripten::val ret = emscripten::val::object();
				ret.set("blkid", blkid);
				ret.set("id", to_string(light->getuid()));
				ret.set("x", p3d.xyz.x);
				ret.set("y", p3d.xyz.y);
				ret.set("z", p3d.xyz.z);
				ret.set("rotate", mtxarr);
				retarr.set(siglightindex, ret);
				siglightindex++;
			}
		}
	}
	return retarr;
}

double tilecache::get_road_height(double x, double y)
{	
	double height = tilecache_impl::get_road_height(x, y);
	return height;
}

emscripten::val tilecache::get_junctioncenter_byappr(const string uuid)
{
	emscripten::val center = emscripten::val::object();
	double x,y;
	tilecache_impl::find_junction_center_byappro(x, y, uuid);
	center.set("x", (int)(x*100000)/100000.0);
	center.set("y", (int)(y*100000)/100000.0);
	return center;
}

} // end of namespace mapi
/* EOF */
