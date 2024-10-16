#ifndef __CXX_OSM_PARSER1_OSM_LOADER_H__
#define __CXX_OSM_PARSER1_OSM_LOADER_H__

#include <stdint.h>
#include "tinyxml2.h"
#include "osm-map.h"

namespace osm_parser1 {

using namespace tinyxml2;

class osm_loader
{
	enum {
		buffer_length = 1024 * 1024,
	};
public:
	osm_loader(cmdline_parser& psr);
	~osm_loader();

	int parse(void);

public:
	osm_map* get_osm_map(void) {
		if (!map_loaded()) {
			return nullptr;
		}
		return &_map;
	}

public:
	bool ready(void) {
		return (_f.ready) ? true : false;
	}

	bool map_loaded(void) {
		return (_f.map_loaded) ? true : false;
	}

private:
	int calc_xmlraw_osm_header_length(int sz);
	int truncate_xmlbuffer(char* begin, int sz);

private:
	union {
		uint32_t _flags;
		struct {
			uint32_t ready : 1;
			uint32_t map_loaded : 1;
		} _f;
	};

	FILE*		_osmfp;
	size_t		_filesize;
	char*		_buffer;
	int 		_xmlraw_osmhdr_length;
	XMLDocument	_doc;
	osm_map		_map;
};

} // end of namespace osm_parser1
#endif // __CXX_OSM_PARSER1_OSM_LOADER_H__
/* EOF */
