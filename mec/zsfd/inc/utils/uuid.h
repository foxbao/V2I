/** @file inc/utils/uuid.h
 * definition of uuid related features
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_UUID

#ifndef __CXX_UTILS_UUID_H__
#define __CXX_UTILS_UUID_H__

#include <string>

namespace zas {
namespace utils {

class UTILS_EXPORT uuid
{
public:
	uuid();

	/**
	  check if the uuid is valid
	  @return true for valid
	 */
	bool valid(void);

	/**
	  Generate a new uuid
	 */
	void generate(void);

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
#endif // UTILS_ENABLE_FBLOCK_UUID
/* EOF */
