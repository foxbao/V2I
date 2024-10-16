#ifndef __CXX_OTAPACK_CONFIG_H__
#define __CXX_OTAPACK_CONFIG_H__

#include <string>
#include <stdint.h>

using namespace std;

struct otapack_config
{
	otapack_config();
	uint32_t full_package : 1;
	uint32_t need_compress : 1;
	uint32_t need_full_compress : 1;
	uint32_t force : 1;
	uint32_t compress_level : 4;

	uint32_t prev_version;
	uint32_t curr_version;
	uint32_t compression_block_size;

	string src_dir;
	string dst_dir;
	string otapack_name;
};

#endif /* __CXX_OTAPACK_CONFIG_H__ */
/* EOF */
