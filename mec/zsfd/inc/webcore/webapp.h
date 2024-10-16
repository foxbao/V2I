#ifndef __CXX_ZAS_WEBAPP_H__
#define __CXX_ZAS_WEBAPP_H__

#include <string>

#include "webcore/webcore.h"
#include "utils/buffer.h"
#include "utils/timer.h"
#include "utils/uri.h"

namespace zas {
namespace webcore {

using namespace zas::utils;
class webapp_backend;

enum protocol_type {
	protocol_type_unknown = 0,
	protocol_type_ztcp,
	protocol_type_http,
};

enum wa_request_type {
	ztcp_req_type_unknown = 0,
	ztcp_req_type_http_get,
	ztcp_req_type_http_post,
	ztcp_req_type_ztcp_no_rep,
	ztcp_req_type_ztcp_req_rep,
};

struct basicinfo_frame_uri
{
	uint32_t uri_length;
	char uri[0];
} PACKED;

struct basicinfo_frame_content
{
	uint32_t sequence_id;
	uint32_t timeout;
	union {
		struct {
			uint32_t pattern : 4;
			uint32_t type : 4;
		} a;
		uint32_t attr;
	};
	int32_t body_frames;
	int32_t body_size;
} PACKED;

class WEBCORE_EXPORT wa_request
{
public:
	/**
	 * create a ztcp request
	 * note that ztcp supports both http and the
	 * customized "ztcp" protocol
	 * @param uri the request uri, the protocol will be
	 * 		automatically determined by the uri schema
	 * 		eg., "http://xxxx" means http protocol
	 * 		while "ztcp://yyyy" means ztcp protocol
	 */
	wa_request(const uri& u);

	virtual ~wa_request();

	/**
	 * get the uri object that could be used for adding
	 * or removing the queries
	 * @return uri object
	 */
	uri* url(void);

	/*
	 * send data via the request
	 * @param data the data to be transfered
	 * @param size the data size to be transferred
	 * @param type the type of the request
	 * 		eg., for "http", GET/POST could be specified
	 * 		while for "ztcp", REQ-REP could be specified
	 * 		check more from man
	 * @return 0 for success
	 */
	int send(const uint8_t *data = nullptr,
		size_t sz = 0,
		const char* type = nullptr); // todo: sendmore
	
	// todo:
	int send(zas::utils::fifobuffer *data);

	/*
	 * add uri header param key&value
	 * @param key the key of header param
	 * @param val the value of header param
	 * @param clear_all clear current header param
	 * @return 0 for success
	 */
	int set_header(const char* key, const char* val, bool clear_all);

	/**
	 * Get the running protocol of this
	 * ztcp object
	 */
	protocol_type get_protocol(void);

	/**
	 * get the time consumption of the request
	 * this will count ticks (milliseconds) from
	 * sending out the request to having reply
	 * if the request is something without a reply
	 * the method will return 0
	 * @return ticks
	 */
	long get_escaped_ticks(void);
	
	virtual int on_reply(void* context, void* data, size_t sz);
	virtual void on_finish(void* context, int code);

private:
	void* _data;
};

class WEBCORE_EXPORT wa_response
{
public:
	wa_response();
	virtual ~wa_response();
	virtual int on_request(void* data, size_t sz);
	int response(void* data, size_t sz);
};

class WEBCORE_EXPORT wa_response_factory
{
public:
	virtual wa_response* create_instance(void) = 0;
	virtual void destory_instance(wa_response *reply) = 0;
};

class WEBCORE_EXPORT webapp_callback
{
public:
	webapp_callback();
	virtual ~webapp_callback();
	virtual int oninit(void);
	virtual int onexit(void);
};

class WEBCORE_EXPORT webapp_context
{
public:
	webapp_context() = delete;
	~webapp_context() = delete;
	void* get_sock_context(void);

	ZAS_DISABLE_EVIL_CONSTRUCTOR(webapp_context);
};

class WEBCORE_EXPORT webapp
{
public:
	webapp() = delete;
	~webapp() = delete;
	static webapp* inst(void);
	static webapp_context* get_context(void);
	static timermgr* get_timermgr(void);
	int run(const char* service_name, bool septhd = true);
	int set_backend(webapp_backend *backend);
	int set_app_callback(webapp_callback *cb);
	int set_response_factory(wa_response_factory* factory);
	ZAS_DISABLE_EVIL_CONSTRUCTOR(webapp);
};


}};	//namespace zas::webcore
#endif