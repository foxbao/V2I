#ifndef __CXX_ZAS_WEBAPP_IMPL_H__
#define __CXX_ZAS_WEBAPP_IMPL_H__

#include "std/list.h"
#include "utils/uri.h"
#include "utils/thread.h"

#include "webcore/webcore.h"
#include "webapp-backend.h"
#include "webapp.h"
#include "webapp-def.h"

namespace zas {
namespace webcore {

using namespace zas::utils;

class webapp_socket_dispatch;
class http_global_dispatcher;
class webapp_context_impl;

class wa_request_impl
{
public:
	wa_request_impl(const uri& u, wa_request* req);
	virtual ~wa_request_impl();
	
	virtual int set_header(const char* key, const char* val, bool clear_all);
	virtual int send(const char* type, const uint8_t *data, size_t sz);
	virtual int on_reply(void* context, void* data, size_t sz);
	virtual void on_finish(void* context, int code);

public:
	protocol_type get_protocol(void) {
		return (protocol_type)_f.protocol;
	}

	wa_request_type get_request_type(void) {
		return (wa_request_type)_f.request_type;
	}

	uri& geturl(void) {
		return _uri;
	}

	bool valid(void) {
		return (_f.valid) ? true : false;
	}

	long get_escaped_ticks(void) {
		long ret = _tick2 - _tick1;
		assert(ret >= 0);
		return ret;
	}

public:
	static wa_request_impl* create(const uri& u, wa_request* req);

protected:
	void update_tick(void);

protected:
	uri _uri;
	wa_request*	_req_cb;
	long _tick1, _tick2;
	union {
		struct {
			uint32_t valid : 1;
			uint32_t must_response : 1;
			uint32_t protocol : 4;
			uint32_t request_type : 4;
		} _f;
		uint32_t	_flags;
	};
};

class http_request : public wa_request_impl
{
public:
	http_request(const uri& u, wa_request* req);
	~http_request();
	
	int set_header(const char* key, const char* val, bool clear_all);
	int send(const char* type, const uint8_t *data, size_t sz);

	curl_slist* update_headers(curl_slist*& header_list);

private:
	int init(void);
	http_global_dispatcher* get_dispatcher(void);
	int set_type(const char* type, bool has_data);

private:
	curl_slist* _headers;
	union {
		uint32_t _flags;
		struct {
			uint32_t content_type_assigned : 1;
		} _f;
	};
};

class ztcp_request : public wa_request_impl
{
public:
	ztcp_request(const uri& u, wa_request* req);
	~ztcp_request();
	int set_header(const char* key, const char* val, bool clear_all);
	int send(const char* type, const uint8_t *data, size_t sz);

private:
	int init(void);
	webapp_socket_dispatch* get_dispatcher(void);
	int set_type(const char* type, bool has_data);
};

class webapp_impl
{
public:
	webapp_impl();
	~webapp_impl();

	static webapp_impl* inst(void);

	int run(const char* service_name, bool septhd);
	int set_backend(webapp_backend* backend);
	webapp_backend* get_backend(void);
	const char* get_service_name(void);
	int set_app_callback(webapp_callback* cb);
	int set_response_factory(wa_response_factory* factory);

public:
	int set_thread(uint64_t thd_id);
	wa_response_factory* get_response_factory(void);

private:
	webapp_backend* _backend;
	webapp_callback* _cb;
	wa_response_factory* _factory;
	uint64_t _thread_id;
	void* _global_context;
	std::string _service_name;
};

class webapp_socket_dispatch : public webapp_backend_callback
{
public:
	webapp_socket_dispatch(void* thd_context, webapp_impl* impl,
		void* ws_context, webapp_backend* backend);
	~webapp_socket_dispatch();

	// webapp backend callback
	int on_recv(void* context, webapp_socket* socket, void* data, size_t sz);

public:
	// get timer mgr only in zmq thread;
	timermgr* get_timermgr(void);

	int set_response_factory(wa_response_factory *factory);
	int create_request(wa_request_impl *req);
	int remove_request(wa_request_impl *req);
	int send(wa_request_impl *req, uri* url, const uint8_t *data, size_t sz);
	int reply(void* data, size_t sz);

private:
	// backend on_recv msg handle
	int dispatch(webapp_socket* socket, void* data, size_t sz);

	// wa_request_impl request info
	int add_webapp_request_unlock(webapp_request_socket_item* item,
		wa_request_impl *req, uint64_t seq_id);
	wa_request_item* find_webapp_request_unlock(uint64_t seq_id);
	int release_webapp_request_unlock(wa_request_item* item,
		wa_request_impl *req_impl);
	
	// webapp_socket
	webapp_backend_socket* add_webapp_socket_unlock(webapp_socket *wasocket);
	webapp_backend_socket* find_webapp_socket_unlock(webapp_socket *wasocket);
	int remove_all_webapp_socket(void);

	// wa_request_impl
	int add_webapp_request_socket(webapp_request_socket_item *req_sock);
	webapp_request_socket_item* find_request_sock_by_req_unlock(
		wa_request_impl* req);
	int remove_webapp_request_unlock(webapp_request_socket_item* item);

	int get_tcp_uri(const uri& u, std::string& req);
	
	webapp_context_impl* get_context(void);

private:
	void*		_thd_context;
	void*		_ws_context;
	uint64_t	_seq_id;
	webapp_impl*	_webapp;
	webapp_backend* _backend;
	wa_response_factory*	_factory;

	// all webapp_socket list
	listnode_t		_webapp_socket_list;
	avl_node_t*		_wa_sock_avltree;

	// all wa_request_impl socket list
	avl_node_t*		_wa_sock_item_avltree;
	avl_node_t*		_wa_request_avltree;
	
	std::string 	_service_name;
};

class http_global_dispatcher
{
public:
	http_global_dispatcher(webapp_backend* backend);
	~http_global_dispatcher();

	int create_request(wa_request_impl *req);
	int remove_request(wa_request_impl *req);
	int send(wa_request_impl *req, uri* url, const uint8_t *data, size_t sz);

private:

	class webapp_socket_http : public webapp_socket
	{
		friend class http_global_dispatcher;
	public:
		webapp_socket_http(int fd, CURL* e, http_global_dispatcher* dp);
		~webapp_socket_http();

		int addref(void);
		int release(void);
		int send(void* data, size_t sz, bool bfinished);
		int reply(void* data, size_t sz);
		webapp_socket_type gettype(void);

	public:
		int getfd(void) {
			return _sockfd;
		}

	private:
		int http_timer_cb(int fd, int revents);

	private:
		int _refcnt;
		curl_socket_t _sockfd;
		CURL *_easy;
		http_global_dispatcher* _dispatcher;
		ZAS_DISABLE_EVIL_CONSTRUCTOR(webapp_socket_http);
	};

	// async http methods
	void check_http_multi_info(void);

	int async_http_init(void);
	int async_http_destroy(void);

	void http_create_timerfd(void);
	int async_http_request(http_request_item* reqitem);
	int async_http_execute(webapp_request_socket_item* req_sock,
		uri* url, void* data, size_t sz);
	http_request_item* pop_http_request(void);
	int push_http_request(http_request_item* http_item);

	void remsock(webapp_socket_http* sock);
	void setsock(webapp_socket_http* sock, curl_socket_t s, CURL *e, int act);
	void addsock(curl_socket_t s, CURL *easy, int action);

	// curl callbacks
	static int sock_cb(CURL*,curl_socket_t, int, void*, void*);
	static int event_cb(int fd, int revents, void* data);
	static int multi_timer_cb(CURLM *multi, long timeout_ms,
		http_global_dispatcher* d);
	static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *data);
	static int prog_cb(void *p, double dltotal, double dlnow,
		double ult, double uln);

	int add_request_socket(webapp_request_socket_item *req_sock);
	int remove_request_socket(webapp_request_socket_item *req_sock);
	webapp_request_socket_item* find_request_sock(wa_request_impl* req);
	int release_all_request(void);

	webapp_context_impl* get_context(void);

private:
	// http async request global info
	webapp_socket_http*	_http_timersock;
	CURLM*				_http_curlm;
	int					_http_still_running;
	listnode_t			_http_curl_list;
	listnode_t			_httpreq_socket_list;
	avl_node_t*			_httpreq_socket_tree;
	int					_http_curl_unused_cnt;
	int					_http_curl_reused_max_cnt;

	webapp_backend*		_backend;

	// the http virtual socket, which is reserved
	// for linking all http request object
	webapp_backend_socket _http_virt_socket;

	ZAS_DISABLE_EVIL_CONSTRUCTOR(http_global_dispatcher);
};

class webapp_context_impl
{
public:
	webapp_context_impl();
	~webapp_context_impl();

	int set_webapp_socket_context(void* context);
	void* get_webapp_socket_context(void);
	int set_webapp_socket_dispatch(webapp_socket_dispatch* dispatch);
	int set_http_dispatch(http_global_dispatcher* dispatch);

public:
	webapp_socket_dispatch* get_webapp_socket_dispatch(void) {
		return _dispatch;
	}

	http_global_dispatcher* get_http_dispatcher(void) {
		return _http_dispatcher;
	}

	wa_response_proxy_smtptr get_pending(void) {
		return _pending;
	}

	void set_pending(wa_response_proxy_smtptr pending) {
		_pending = pending;
	}

private:
	wa_response_proxy_smtptr _pending;
	webapp_socket_dispatch* _dispatch;
	http_global_dispatcher* _http_dispatcher;
};

class webapp_thread : public thread
{
public:
	webapp_thread(const char* name, webapp_impl* wa,
		webapp_backend *backend,
		webapp_callback *cb,
		int server_id,
		int endpoint_id);
	~webapp_thread();

	int run(void);

private:
	int start_server_and_endpoint(void);

private:
	webapp_impl* _webapp;
	webapp_backend*	_backend;
	webapp_callback*	_cb;
	std::string 	_name;
	int _server_id;
	int _endpoint_id;
	webapp_socket* _server_sock;
	webapp_socket* _endpoint_sock;
};

}};	//namespace zas::webcore
#endif