#ifndef __CXX_ZAS_UTILS_HTTP_IMPL_H__
#define __CXX_ZAS_UTILS_HTTP_IMPL_H__

#include <curl/curl.h>
#include "utils/http.h"

namespace zas {
namespace utils {

#define ZAS_HTTP_BUF_MIN_LEN				(256)

#define ZAS_HTTP_FLAG_GET					(1)
#define ZAS_HTTP_FLAG_POST				(2)
#define ZAS_HTTP_FLAG_RESP_UNKNOWN_LEN	(4)
#define ZAS_HTTP_FLAG_USE_RANGE			(8)

#define ZAS_HTTP_READYSTATE_UNSEND			(readystate_unsend)
#define ZAS_HTTP_READYSTATE_OPENED			(readystate_opened)
#define ZAS_HTTP_READYSTATE_HEADERS_RECEIVED	(readystate_headers_received)
#define ZAS_HTTP_READYSTATE_LOADING			(readystate_loading)
#define ZAS_HTTP_READYSTATE_DONE				(readystate_done)

class httprequest_impl
{
public:
	httprequest_impl();
	virtual ~httprequest_impl();
	int set_request_header(const char* header, const char* content);

	int open(const char* mode, const char* url);
	int send(const char* content = NULL);
	
	int response_type(void);
	long response_length(void);
	int range(long offset, long length);
	const char* response_text(size_t* sz = NULL);

public:
	void release(void) {
		delete this;
	}
	
	int ready_state(void) {
		return (int)readystate;
	}

	uint32_t status(void) {
		return httpstatus;
	}

private:
	static char* killspace(char* s);
	static size_t http_header_func(void *ptr, size_t s, size_t n, void *data);
	static size_t http_write_func(void *buf, size_t s, size_t n, void *data);
	
	int http_init_curl(const char* url);
	int http_cleanup_curl(void);
	int request_type(const char* mode);

private:
	uint32_t flags;
	uint32_t data_sz;
	uint32_t buf_sz;
	uint32_t offset;

	void* curl_handle;
	struct curl_slist* headers;
	char* buffer;
	uint32_t readystate;
	long httpstatus;
	int resptype;
};

}}	// end of namespace zas::utils
#endif // GPIO_HTTP_H
/* EOF */
