#ifndef __CXX_ZAS_UTILS_HTTP_H__
#define __CXX_ZAS_UTILS_HTTP_H__

#include "utils/utils.h"
#include "curl/curl.h"

namespace zas {
namespace utils {

enum http_response_type
{
	response_type_unknown = 0,
	response_type_unavail,
	response_type_json,
	response_type_document,
	response_type_binary_stream,
	response_type_image,
	response_type_android_package,
};

enum http_readystate
{
	readystate_unsend = 1,
	readystate_opened,
	readystate_headers_received,
	readystate_loading,
	readystate_done,
};

class UTILS_EXPORT http
{
public:
	http();
	~http();
	int set_request_header(const char* header, const char* content);
	int open(const char* mode, const char* url);
	int send(const char* content = NULL);	
	int response_type(void);
	long response_length(void);
	int range(long offset, long length);
	const char* response_text(size_t* sz = NULL);	
	int ready_state(void);
	uint32_t status(void);

private:
	void*	_data;
};

}}	// end of namespace zas::utils
#endif // GPIO_HTTP_H
/* EOF */
