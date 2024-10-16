#ifndef __CXX_WASM_MAPI_H__
#define __CXX_WASM_MAPI_H__

#include <emscripten/val.h>
#include <emscripten/bind.h>
#include "tilecache.h"
#include "mapcore/hdmap.h"
#include "mapcore/mapcore.h"

namespace mapi {
using namespace std;

class tilecache : public zas::mapcore::tilecache_impl
{
public:
	tilecache() = default;
	~tilecache() = default;

	int bind_map(const string& mapinfo);
	int add_tile(const string& buffer);
	odr::RoadNetworkMesh get_mesh(uint32_t blkid);
	int release_mesh(uint32_t blkid);
	emscripten::val query_unloaded_blocks(emscripten::val pts);
	string get_mapid(void);
	const emscripten::val get_map_georef(void);
	const emscripten::val get_mapoffset(void);
	bool map_available(void);
	emscripten::val filter_loaded_blocks(emscripten::val blkids, bool lock);
	int lock_blocks(const emscripten::val blkids);
	int unlock_blocks(const emscripten::val blkids);
	int release_blocks(const emscripten::val blkids);
	emscripten::val get_singlelight_blocks(emscripten::val blkids);
	double get_road_height(double x, double y);
	emscripten::val get_junctioncenter_byappr(const string uuid);
};

} // end of namespace mapi
#endif // __CXX_WASM_MAPI_H__
/* EOF */
