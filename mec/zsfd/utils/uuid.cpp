/** @file wait.cpp
 * implementation of the uuid object
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_UUID

#include <uuid/uuid.h>
#include "utils/uuid.h"

namespace zas {
namespace utils {

uuid::uuid()
{
	memset(&_uid, 0, sizeof(uint128_t));
}

bool uuid::valid(void)
{
	static const uint128_t zero = {0};
	return memcmp(&_uid, &zero, sizeof(uint128_t))
		? true : false;
}

void uuid::generate(void)
{
	uuid_t tmp;
	uuid_generate(tmp);
	memcpy(&_uid, tmp, sizeof(uuid_t));
}

int uuid::set(const char* uid)
{
	uint32_t a, b, c, d, e, f, g, h, i, j, k;
	sscanf(uid,"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k);
	_uid.data1 = (uint32_t)a;
	_uid.data2 = (uint16_t)b;
	_uid.data3 = (uint16_t)c;
	_uid.data4[0] = (uint8_t)d;
	_uid.data4[1] = (uint8_t)e;
	_uid.data4[2] = (uint8_t)f;
	_uid.data4[3] = (uint8_t)g;
	_uid.data4[4] = (uint8_t)h;
	_uid.data4[5] = (uint8_t)i;
	_uid.data4[6] = (uint8_t)j;
	_uid.data4[7] = (uint8_t)k;
	return 0;
}

int uuid::to_hex(std::string &str)
{
	char buf[64] = {0};
	sprintf(buf, "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		_uid.data1,
		_uid.data2,
		_uid.data3,
		_uid.data4[0],
		_uid.data4[1],
		_uid.data4[2],
		_uid.data4[3],
		_uid.data4[4],
		_uid.data4[5],
		_uid.data4[6],
		_uid.data4[7]
	);
	str = buf;
	return 0;
}

}} // end of namespace zas::utils
#endif // UTILS_ENABLE_FBLOCK_UUID
/* EOF */
