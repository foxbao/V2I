#include "utils/http.h"
#include "inc/http-impl.h"

namespace zas {
namespace utils {

httprequest_impl::httprequest_impl()
: flags(0)
, data_sz(0)
, buf_sz(0)
, offset(0)
, curl_handle(NULL)
, headers(NULL)
, buffer(NULL)
, readystate(0)
, httpstatus(0)
, resptype(response_type_unknown)
{
}

httprequest_impl::~httprequest_impl()
{
	if (buffer) free(buffer);
	http_cleanup_curl();
}

int httprequest_impl::request_type(const char* mode)
{
	if (NULL == curl_handle)
		return -1;
	if (!strcmp(mode, "GET"))
		flags |= ZAS_HTTP_FLAG_GET;
	else if (!strcmp(mode, "POST"))
		flags |= ZAS_HTTP_FLAG_POST;
	else return -2;
	return 0;
}

int httprequest_impl::open(const char* mode, const char* url)
{
	if (!url || *url == '\0' || !mode || *mode == '\0')
		return -1;

	if (http_init_curl(url)) return -2;
	if (request_type(mode))
		return -3;

//	if (fp) { fclose(fp); fp = NULL; }
	httpstatus = 0;
	resptype = response_type_unknown;
	readystate = ZAS_HTTP_READYSTATE_OPENED;
	return 0;
}

int httprequest_impl::set_request_header(const char* header, const char* content)
{
	char* tmp;

	if (!header || *header == '\0'
		|| !content || *content == '\0') return -1;
	
	size_t s1 = strlen(header);
	size_t s2 = strlen(content);

	tmp = (char*)alloca(s1 + s2 + 2);
	memcpy(tmp, header, s1);
	memcpy(tmp + s1 + 1, content, s2);
	tmp[s1] = ':'; tmp[s1 + s2 + 1] = '\0';

	headers = curl_slist_append(headers, tmp);
	return 0;
}

int httprequest_impl::range(long offset, long length)
{
	if (!curl_handle) return -1;
	if (offset < 0 || length <= 0) return -2;
	char tmp[64];
	sprintf(tmp, "%lu-%lu", offset, offset + length - 1);

	curl_easy_setopt(curl_handle, CURLOPT_RANGE, tmp);
	flags |= ZAS_HTTP_FLAG_USE_RANGE;
	return 0;
}

int httprequest_impl::send(const char* content)
{
	CURLcode result;
	if (!curl_handle) return -1;

	offset = 0;
	// fp = f;

	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);

	if (flags & ZAS_HTTP_FLAG_POST)
	{
		size_t sz = content ? strlen(content) : 0;
		curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, content);
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, sz);
	}
	else if (content) {
		fprintf(stderr, "Httprequest_impl: not allowed to send data in GET command\n");
		return -2;
	}

	if (headers)
        curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);

    result = curl_easy_perform(curl_handle);
    readystate = ZAS_HTTP_READYSTATE_DONE;
	
	if (!httpstatus)
		curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &httpstatus);
	
	if (buffer) buffer[data_sz] = '\0';
	return (CURLE_OK == result) ? 0 : -3;
}

int httprequest_impl::response_type(void)
{
	if (readystate != ZAS_HTTP_READYSTATE_DONE)
		return response_type_unavail;
	return resptype;
}

long httprequest_impl::response_length(void)
{
	if (readystate != ZAS_HTTP_READYSTATE_DONE)
		return 0;
	return (long)data_sz;
}

const char* httprequest_impl::response_text(size_t* sz)
{
	if (readystate != ZAS_HTTP_READYSTATE_DONE)
		return NULL;
	if (sz) *sz = data_sz;
	if (!buffer || !data_sz)
		return "";
	return buffer;
}

char* httprequest_impl::killspace(char* s)
{
	for (; *s == ' ' || *s == '\t'; ++s);
	return s;
}

size_t httprequest_impl::http_header_func(void *ptr, size_t s, size_t n, void *data)
{
	uint32_t len;
	char *content = (char*)ptr;
	httprequest_impl* httpobj = (httprequest_impl*)data;
	if (!httpobj) return s * n;

	httpobj->readystate = ZAS_HTTP_READYSTATE_HEADERS_RECEIVED;
	if (!strncmp(content, "HTTP/", 5))
	{
		char c = content[5], d = content[7];
		if (!((c >= '0' && c <= '9') && (d >= '0' && d <= '9')
			&& content[6] == '.')) return s * n;
		content = killspace(content + 8);
		httpobj->httpstatus = 0;
		for (; *content >= '0' && *content <= '9'; content++)
			httpobj->httpstatus = httpobj->httpstatus * 10 + *content - '0';
	}
	else if (!strncmp(content, "Content-Length:", 15))
	{
		len = atoi(killspace(content + 15));
		httpobj->data_sz = len;

		if (!httpobj->buffer || httpobj->buf_sz <= len)
		{
			if (httpobj->buffer) free(httpobj->buffer);
			httpobj->buffer = (char*)malloc(len + 1);
			httpobj->buf_sz = len + 1;
		}
	}
	else if (!strncmp(content, "Content-Type:", 13))
	{
		content = killspace(content + 13);
		if (strstr(content, "application/json"))
			httpobj->resptype = response_type_json;
		else if (strstr(content, "text/plain"))
			httpobj->resptype = response_type_json;
		else if (strstr(content, "text/html"))
			httpobj->resptype = response_type_document;
		else if (strstr(content, "application/octet-stream"))
			httpobj->resptype = response_type_binary_stream;
		else if (strstr(content, "image/x-icon"))
			httpobj->resptype = response_type_image;
		else if (strstr(content, "application/vnd.android.package-archive"))
			httpobj->resptype = response_type_android_package;
		// todo: others
		else httpobj->resptype = response_type_unknown;
	}
	return s * n;
}

size_t httprequest_impl::http_write_func(void *buf, size_t s, size_t n, void *data)
{
	uint32_t tmp, sz = (uint32_t)(s * n);
	httprequest_impl* httpobj = (httprequest_impl*)data;
	if (!httpobj) return 0;

	httpobj->readystate = ZAS_HTTP_READYSTATE_LOADING;

	if (!httpobj->buf_sz || !httpobj->buffer)
	{
		if (httpobj->flags & ZAS_HTTP_FLAG_RESP_UNKNOWN_LEN)
			return 0;
		httpobj->flags |= ZAS_HTTP_FLAG_RESP_UNKNOWN_LEN;
	}

	tmp = httpobj->offset + sz;
	if (tmp > httpobj->buf_sz)
	{
		if (httpobj->flags & ZAS_HTTP_FLAG_RESP_UNKNOWN_LEN)
		{
            char* tmpbuf;
			uint32_t bsz = httpobj->buf_sz;
			for (; bsz < tmp; bsz += ZAS_HTTP_BUF_MIN_LEN);
            tmpbuf = (char*)malloc(bsz);
			if (NULL == tmpbuf) return 0;
			if (httpobj->buffer)
			{
				memcpy(tmpbuf, httpobj->buffer, httpobj->buf_sz);
				free(httpobj->buffer);
			}
			httpobj->buffer = tmpbuf;
			httpobj->buf_sz = bsz;
		}
		else return 0;
	}

	memcpy(&httpobj->buffer[httpobj->offset], buf, sz);
	httpobj->offset += sz;
	if (httpobj->flags & ZAS_HTTP_FLAG_RESP_UNKNOWN_LEN)
		httpobj->data_sz += sz;
	return sz;
}

int httprequest_impl::http_init_curl(const char* url)
{
	http_cleanup_curl();
	curl_handle = curl_easy_init();
	if (NULL == curl_handle)
	{
		fprintf(stderr, "fail in calling curl_easy_init()\n");
		return -1;
	}
		
	curl_easy_setopt(curl_handle, CURLOPT_URL, url);
//	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
//	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
//	curl_easy_setopt(curl_handle, CURLOPT_USERPWD, "test:test");
	curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 8);
	curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, http_header_func);
	curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, this);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, http_write_func);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, this);
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1);
	return 0;
}

int httprequest_impl::http_cleanup_curl(void)
{
	if (headers)
	{
		curl_slist_free_all(headers);
		headers = NULL;
	}
	if (curl_handle)
	{
		curl_easy_cleanup(curl_handle);
		curl_handle = NULL;
	}
	return 0;
}

http::http()
: _data(NULL)
{
	auto* impl = new httprequest_impl();
	assert(NULL != impl);
	_data = reinterpret_cast<void*>(impl);
}

http::~http()
{
	auto* impl = reinterpret_cast<httprequest_impl*>(_data);
	if (impl) delete impl;
	_data = NULL;
}

int http::set_request_header(const char* header, const char* content)
{
	if (!_data) return -ENOTAVAIL;
	auto* impl = reinterpret_cast<httprequest_impl*>(_data);
	return impl->set_request_header(header, content);
}

int http::open(const char* mode, const char* url)
{
	if (!_data) return -ENOTAVAIL;
	auto* impl = reinterpret_cast<httprequest_impl*>(_data);
	return impl->open(mode, url);
}

int http::send(const char* content)
{
	if (!_data) return -ENOTAVAIL;
	auto* impl = reinterpret_cast<httprequest_impl*>(_data);
	return impl->send(content);
}

int http::response_type(void)
{
	if (!_data) return -ENOTAVAIL;
	auto* impl = reinterpret_cast<httprequest_impl*>(_data);
	return impl->response_type();
}

long http::response_length(void)
{
	if (!_data) return -ENOTAVAIL;
	auto* impl = reinterpret_cast<httprequest_impl*>(_data);
	return impl->response_length();
}

int http::range(long offset, long length)
{
	if (!_data) return -ENOTAVAIL;
	auto* impl = reinterpret_cast<httprequest_impl*>(_data);
	return impl->range(offset, length);
}

const char* http::response_text(size_t* sz)
{
	if (!_data) return NULL;
	auto* impl = reinterpret_cast<httprequest_impl*>(_data);
	return impl->response_text(sz);
}

int http::ready_state(void)
{
	if (!_data) return -ENOTAVAIL;
	auto* impl = reinterpret_cast<httprequest_impl*>(_data);
	return impl->ready_state();
}

uint32_t http::status(void)
{
	if (!_data) return -ENOTAVAIL;
	auto* impl = reinterpret_cast<httprequest_impl*>(_data);
	return impl->status();
}

}} // end of namespace zas::utils
/* EOF */
