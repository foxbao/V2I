/** @file inc/utils/uuid.h
 * definition of uuid related features
 */

#ifndef __CXX_ZAS_UTILS_UUID_H__
#define __CXX_ZAS_UTILS_UUID_H__

#include "std/zasbsc.h"
#include <string>

namespace zas {
namespace utils {

class uuid
{
public:
	uuid();
	uuid(uint128_t &uid);

	/**
	  check if the uuid is valid
	  @return true for valid
	 */
	bool valid(void);

	int set(const char* uid);
	int to_hex(std::string &str);

	operator uint128_t() {
		return _uid;
	}

	uuid& operator=(uint128_t uid) {
		_uid = uid;
		return *this;
	}

	uint128_t& getuuid(void) {
		return _uid;
	}

private:
	uint128_t _uid;
};

}} // end of namespace zas::utils
#endif // __CXX_UTILS_UUID_H__
/* EOF */
