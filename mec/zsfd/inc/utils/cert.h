/** @file cert.h
 * definition of certutils
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_CERT

#ifndef __CXX_ZAS_UTILS_CERT_H__
#define __CXX_ZAS_UTILS_CERT_H__

#include <string>

namespace zas {
namespace utils {

class UTILS_EXPORT digest
{
public:
	digest(const char* algorithm);
	~digest();

	/**
	  add data that is to be digested
	  @param data the pointer to the data
	  @param sz the size of the data
	  @return 0 for success
	 */
	int append(void* data, size_t sz);

	/**
	  Get the digest final result
	  @param sz the finial digest size
	  @return the pointer to the final digest
	 */
	const uint8_t* getresult(size_t* sz = NULL);

private:
	void* _data;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(digest);
};

/**
  conduct the base64 encode operation
  @param input the input conent
  @param output the output string
  @return 0 for success
 */
UTILS_EXPORT int base64_encode(const std::string& input, std::string& output);

/**
  conduct the base64 encode operation
  @param input the input conent
  @param sz the size of input content
  @param output the output string
  @return 0 for success
 */
UTILS_EXPORT int base64_encode(const void* input, size_t sz, std::string& output);

/**
  conduct the base64 decode operation
  @param input the input conent
  @param sz the size of input content
  @param output the output string
  @return 0 for success
 */
UTILS_EXPORT int base64_decode(const char* input, size_t sz, std::string& output);

}} // end of namespace zas::utils
#endif /* __CXX_ZAS_UTILS_CERT_H__ */
#endif // UTILS_ENABLE_FBLOCK_CERT
/* EOF */

