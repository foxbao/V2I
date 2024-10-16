#include <string>
#include <sys/timerfd.h>
#include <unistd.h>

#include "webcore/logger.h"
#include "inc/webapp-impl.h" 

namespace zas {
namespace webcore {

using namespace std;

extern __thread void* _context;
extern __thread void* _ws_context;
const char* TAG = "backend-zmq-http";

http_global_dispatcher::
webapp_socket_http::webapp_socket_http(int fd, CURL* e,
	http_global_dispatcher* dp)
: _refcnt(1), _sockfd(fd), _easy(e)
, _dispatcher(dp) {
	assert(fd >= 0 && dp);
}

http_global_dispatcher::
webapp_socket_http::~webapp_socket_http()
{
}

int http_global_dispatcher::
	webapp_socket_http::addref(void) {
	return __sync_add_and_fetch(&_refcnt, 1);
}

int http_global_dispatcher::
	webapp_socket_http::release(void)
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0) {
		delete this;
	}
	return cnt;
}

int http_global_dispatcher::
	webapp_socket_http::send(void* data, size_t sz, bool bfinished)
{
	return -1;
}

int http_global_dispatcher::
	webapp_socket_http::reply(void* data, size_t sz)
{
	return -1;
}

webapp_socket_type http_global_dispatcher::
	webapp_socket_http::gettype(void)
{
	return webapp_socket_client;
}

#define mycase(code) \
	case code: s = __STRING(code)

static bool fatal_error(CURLMcode code)
{
	if(CURLM_OK == code) {
		return false;
	}
	
	switch(code) {
		case CURLM_BAD_HANDLE:			break;
		case CURLM_BAD_EASY_HANDLE:		break;
		case CURLM_OUT_OF_MEMORY:		break;
		case CURLM_INTERNAL_ERROR:		break;
		case CURLM_UNKNOWN_OPTION:		break;
		case CURLM_LAST:				break;
		default:						break;
		case CURLM_BAD_SOCKET:			return false;	// ignore this error
	}
	return true;
}

int http_global_dispatcher::
	webapp_socket_http::http_timer_cb(int fd, int revents)
{
	CURLMcode rc;
	uint64_t count = 0;
	ssize_t err = 0;
	
	err = read(fd, &count, sizeof(uint64_t));
	if(err == -1) {
		/* Note that we may call the timer callback even if the timerfd is not
		 * readable. It's possible that there are multiple events stored in the
		 * epoll buffer (i.e. the timer may have fired multiple times). The
		 * event count is cleared after the first call so future events in the
		 * epoll buffer will fail to read from the timer. */
		if(errno == EAGAIN) {
			return -1;
		}
	}
	assert(err == sizeof(uint64_t));
	rc = curl_multi_socket_action(_dispatcher->_http_curlm,
		CURL_SOCKET_TIMEOUT, 0, &_dispatcher->_http_still_running);
  	assert(!fatal_error(rc));
  	
	assert(nullptr != _dispatcher);
	_dispatcher->check_http_multi_info();
	return 0;
}

int http_global_dispatcher::event_cb(int fd, int revents, void* data)
{
	auto* sock = (webapp_socket_http*)data;
	auto* dispatcher = sock->_dispatcher;
	if (fd == dispatcher->_http_timersock->getfd()) {
		return sock->http_timer_cb(fd, revents);
	}
	
	// hancle fd events
	int action = ((revents & BACKEND_POLLIN) ? CURL_CSELECT_IN : 0)
		| ((revents & BACKEND_POLLOUT) ? CURL_CSELECT_OUT : 0);
		
	CURLMcode rc = curl_multi_socket_action(dispatcher->_http_curlm,
		fd, action, &dispatcher->_http_still_running);
	assert(!fatal_error(rc));
	
	dispatcher->check_http_multi_info();
	if(dispatcher->_http_still_running <= 0) {
		struct itimerspec its;
		memset(&its, 0, sizeof(struct itimerspec));
		// disarms the timer (stop timer)
		int tmrfd = dispatcher->_http_timersock->getfd();
		timerfd_settime(tmrfd, 0, &its, nullptr);
	}
	return 0;
}

void http_global_dispatcher::setsock(webapp_socket_http* sock,
	curl_socket_t s, CURL *e, int act)
{
	int kind = ((act & CURL_POLL_IN) ? BACKEND_POLLIN : 0)
		| ((act & CURL_POLL_OUT) ? BACKEND_POLLOUT : 0);
	
	assert(nullptr != _backend);
	if (sock->_sockfd) {
		_backend->removefd(_ws_context, sock->_sockfd);
	}
 
	sock->_sockfd = s;
	sock->_easy = e;

	// add the fd to backend poller
	_backend->addfd(_ws_context, sock->_sockfd,
		kind, event_cb, (void*)sock);
}

void http_global_dispatcher::addsock(
	curl_socket_t s, CURL *easy, int action)
{
	auto* sock = new webapp_socket_http(s, easy, this);
	assert(nullptr != sock);

	setsock(sock, s, easy, action);
	curl_multi_assign(_http_curlm, s, sock);
}

void http_global_dispatcher::remsock(webapp_socket_http* sock)
{
	if (nullptr == sock) {
		return;
	}

	if (sock->_sockfd) {
		// use the backend to remove this sockfd
		assert(nullptr != _backend);
		_backend->removefd(_ws_context, sock->_sockfd);
	}
	sock->release();
}

http_request_item* http_global_dispatcher::pop_http_request(void)
{
	if (!listnode_isempty(_http_curl_list)
		&& _http_curl_unused_cnt > 0) {
		auto* http_item = LIST_ENTRY(http_request_item, \
			ownerlist, _http_curl_list.next);
		listnode_del(http_item->ownerlist);
		_http_curl_unused_cnt--;
		return http_item;
	}

	auto* http_item = new http_request_item();
	assert(nullptr != http_item);

	if (async_http_request(http_item)) {
		delete http_item;
		return nullptr;
	}
	return http_item;
}

int http_global_dispatcher::push_http_request(http_request_item* http_item)
{
	if (!http_item) {
		return -EBADPARM;
	}
	http_item->reset();

	if (_http_curl_unused_cnt >= _http_curl_reused_max_cnt) {
		delete http_item;
		return 0;
	}

	listnode_add(_http_curl_list, http_item->ownerlist);
	_http_curl_unused_cnt++;
	return 0;
}

void http_global_dispatcher::check_http_multi_info(void)
{
	CURLMsg *msg;
	int msgs_left;
 
	while((msg = curl_multi_info_read(_http_curlm, &msgs_left)))
	{
		if(msg->msg != CURLMSG_DONE) {
			continue;
		}

		CURL* easy = msg->easy_handle;
		CURLcode res = msg->data.result;

		http_request_item* reqitem;
		curl_easy_getinfo(easy, CURLINFO_PRIVATE, &reqitem);
		curl_multi_remove_handle(_http_curlm, easy);

		listnode_del(reqitem->ownerlist);
		auto* req = reqitem->sender;
		assert(nullptr != req);

		get_context()->set_pending(reqitem->prev_reply);

		auto* rep_data = reqitem->reply_data;
		if (rep_data) {
			req->on_reply(_context, 
				(void*)rep_data->c_str(), rep_data->length());
			delete rep_data;
			reqitem->reply_data = nullptr;
		}
		req->on_finish(_context, (int)res);

		reqitem->prev_reply = nullptr;
		get_context()->set_pending(nullptr);
	
		reqitem->sender = nullptr;
		push_http_request(reqitem);
	}
}

/* CURLMOPT_SOCKETFUNCTION */
int http_global_dispatcher::sock_cb(CURL *e,
	curl_socket_t s, int what, void *cbp, void *sockp)
{
	auto* dispatcher = (http_global_dispatcher*)cbp;
	auto* sock = (webapp_socket_http*)sockp;

	if(what == CURL_POLL_REMOVE) {
		dispatcher->remsock(sock);
		return 0;
	}
	
	if(nullptr == sock) {
		dispatcher->addsock(s, e, what);
	}
	else {
		dispatcher->setsock(sock, s, e, what);
	}
	return 0;
}

void http_global_dispatcher::http_create_timerfd(void)
{
	itimerspec its;

	// create the http timer fd
	int tmrfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	assert(tmrfd >= 0);
	_http_timersock = new webapp_socket_http(tmrfd, nullptr, this);
	assert(nullptr != _http_timersock);

	memset(&its, 0, sizeof(itimerspec));
	its.it_interval.tv_sec = 0;
	its.it_value.tv_sec = 1;
	timerfd_settime(tmrfd, 0, &its, nullptr);
 
	// add the fd to backend poller
	_backend->addfd(_ws_context, tmrfd,
		BACKEND_POLLIN, event_cb, (void*)_http_timersock);
}

/* Update the timer after curl_multi library does it's thing. Curl will
 * inform us through this callback what it wants the new timeout to be,
 * after it does some work. */
int http_global_dispatcher::multi_timer_cb(CURLM *multi,
	long timeout_ms, http_global_dispatcher* d)
{
	itimerspec its;
  
	if(timeout_ms > 0) {
		its.it_interval.tv_sec = 0;
		its.it_interval.tv_nsec = 0;
		its.it_value.tv_sec = timeout_ms / 1000;
		its.it_value.tv_nsec = (timeout_ms % 1000) * 1000 * 1000;
	}
	else if(timeout_ms == 0) {
		/* libcurl wants us to timeout now, however setting both fields of
		 * new_value.it_value to zero disarms the timer. The closest we can
		 * do is to schedule the timer to fire in 1 ns. */
		its.it_interval.tv_sec = 0;
		its.it_interval.tv_nsec = 0;
		its.it_value.tv_sec = 0;
		its.it_value.tv_nsec = 1;
	}
	else {
		// remove the timer
		memset(&its, 0, sizeof(struct itimerspec));
	}
 
	timerfd_settime(d->_http_timersock->getfd(),
		/*flags=*/0, &its, nullptr);
	return 0;
}

int http_global_dispatcher::async_http_init(void)
{
	_http_curlm = curl_multi_init();
	assert(nullptr != _http_curlm);

	http_create_timerfd();

	/* setup the generic multi interface options we want */
	curl_multi_setopt(_http_curlm, CURLMOPT_SOCKETFUNCTION, sock_cb);
	curl_multi_setopt(_http_curlm, CURLMOPT_SOCKETDATA, this);
	curl_multi_setopt(_http_curlm, CURLMOPT_TIMERFUNCTION, multi_timer_cb);
	curl_multi_setopt(_http_curlm, CURLMOPT_TIMERDATA, this);

	return 0;
}

int http_global_dispatcher::async_http_destroy(void)
{
	if (_http_curlm) {
		curl_multi_cleanup(_http_curlm);
		_http_curlm = nullptr;
	}	
	return 0;
}

int http_global_dispatcher::async_http_request(
	http_request_item* reqitem)
{	
	auto* easy = curl_easy_init();
	if(!easy) {
		return -1;
	}
	reqitem->easy = easy;
	curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(easy, CURLOPT_WRITEDATA, reqitem);
//	curl_easy_setopt(easy, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(easy, CURLOPT_PRIVATE, reqitem);
	curl_easy_setopt(easy, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(easy, CURLOPT_PROGRESSFUNCTION, prog_cb);
	curl_easy_setopt(easy, CURLOPT_PROGRESSDATA, reqitem);
	curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(easy, CURLOPT_LOW_SPEED_TIME, 3L);
	curl_easy_setopt(easy, CURLOPT_LOW_SPEED_LIMIT, 10L);
	return 0;
}

int http_global_dispatcher::async_http_execute(
	webapp_request_socket_item* req_sock,
	uri* url, void* data, size_t sz)
{
	if (nullptr == _http_curlm) {
		return -1;
	}

	// http request
	auto* req_item = pop_http_request();
	assert(nullptr != req_item && nullptr != req_item->easy);
	req_item->sender = req_sock->ztcp_req;

	auto* req = zas_downcast(wa_request_impl,\
		http_request, req_sock->ztcp_req);
	assert(nullptr != req);

	// todo: to wyq: move to pop_http_request()
	req_item->prev_reply = get_context()->get_pending();

	CURL* curl = req_item->easy;

	// update the url
	assert(nullptr != url);
	curl_easy_setopt(curl, CURLOPT_URL, url->tostring().c_str());

	auto reqtype = req->get_request_type();
	assert(ztcp_req_type_unknown != reqtype);

	// add data if we have
	if (data != nullptr) {
		if (sz) {
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, sz);
		}
		curl_easy_setopt(curl,
			CURLOPT_COPYPOSTFIELDS, (char*)data);
	}

	// add headers
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER,
		req->update_headers(req_item->headers));
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS,2000L);
	// handle post
	if (ztcp_req_type_http_post == reqtype) {
		// set as a POST request
		curl_easy_setopt(curl, CURLOPT_POST, 1L);

		// check if this is a EMPTY data body
		if (!data && !sz) {
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
		}
	} else if (ztcp_req_type_http_get == reqtype) {
		if (!data && !sz) {
			curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
		}
		else {
			// we still have a body, which makes the curl
			// regard this as a POST
			// we mandatory change it to "GET"
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
		}
	}
	else {
		// the req_item is useless here, we push to stack
		// instead of releasing it
		push_http_request(req_item);
		return -2;
	}

	CURLMcode rc = curl_multi_add_handle(_http_curlm, curl);
	assert(!fatal_error(rc));

	listnode_add(req_sock->request_list, req_item->ownerlist);
	// note that the add_handle() will set a time-out to trigger very soon so
	// that the necessary socket_action() call will be called by this app
	return 0;
}

/* CURLOPT_WRITEFUNCTION */
size_t http_global_dispatcher::write_cb(
	void *ptr, size_t size, size_t nmemb, void *data)
{
	auto* req_item = (http_request_item*)data;
	
	assert(nullptr != req_item && nullptr != _context);
	assert(nullptr != req_item->sender);
	if (!req_item->reply_data) {
		req_item->reply_data = new std::string((char*)ptr, size * nmemb);
	} else {
		req_item->reply_data->append((char*)ptr, size * nmemb);
	}
	return size * nmemb;
}
 
 
/* CURLOPT_PROGRESSFUNCTION */
int http_global_dispatcher::prog_cb(
	void *p, double dltotal, double dlnow, double ult,
	double uln)
{
	auto* dispatcher = (http_global_dispatcher *)p;
	assert(nullptr != dispatcher);
	(void)ult;
	(void)uln;

//	fprintf(MSG_OUT, "Progress: %s (%g/%g)\n", conn->url, dlnow, dltotal);
	return 0;
}

int http_global_dispatcher::create_request(wa_request_impl *req)
{
	// create wa_request_impl object
	auto* req_item = new webapp_request_socket_item();
	assert(nullptr != req_item);

	req_item->ztcp_req = req;
	req_item->wa_socket = nullptr;
	listnode_init(req_item->request_list);

	// add wa_request_impl to webapp socket list
	if (add_request_socket(req_item)) {
		delete req_item;
		return -ELOGIC;
	}
	return 0;
}

int http_global_dispatcher::remove_request(wa_request_impl *req)
{
	if (!req) {
		return -EBADPARM;
	}
	auto* req_item = find_request_sock(req);
	if (!req_item) {
		return -ENOTFOUND;
	}
	remove_request_socket(req_item);
	return 0;
}

int http_global_dispatcher::send(wa_request_impl *req,
	uri* url, const uint8_t *data, size_t sz)
{
	assert(nullptr != req);
	assert(nullptr != url);

	auto* item = find_request_sock(req);
	if (!item) {
		return -ENOTFOUND;
	}
	// only ztcp is support here in webapp_socket_dispatcher
	assert(protocol_type_http == req->get_protocol());
	return async_http_execute(item, url, (void*)data, sz);
}


int http_global_dispatcher::add_request_socket(
	webapp_request_socket_item *req_sock)
{
	if (!req_sock) {
		return -EBADPARM;
	}

	assert(protocol_type_http == req_sock->ztcp_req->get_protocol());
	assert(req_sock->ztcp_req != nullptr);

	// add to list and avl trees
	int ret = avl_insert(&_httpreq_socket_tree, &req_sock->req_avlnode,
		webapp_request_socket_item::req_avl_compare);
	assert(ret == 0);
	listnode_add(_httpreq_socket_list, req_sock->ownerlist);
	return 0;
}

int http_global_dispatcher::remove_request_socket(
	webapp_request_socket_item* item)
{
	if (!item) {
		return -EBADPARM;
	}
	auto* req = item->ztcp_req;
	assert(nullptr != req);

	listnode_t *nd = item->request_list.next;
	if (protocol_type_http == req->get_protocol()) {
		for (; nd != &(item->request_list);) {
			auto* req_item = LIST_ENTRY(http_request_item, ownerlist, nd);
			nd = nd->next;
			listnode_del(req_item->ownerlist);
			assert(nullptr != req_item);
			curl_multi_remove_handle(_http_curlm, req_item->easy);
			push_http_request(req_item);
		}
		avl_remove(&_httpreq_socket_tree, &item->req_avlnode);
		listnode_del(item->ownerlist);
		delete item;
	} else {
		log.e(WEB_DISPATCH_TAG, "http remove request impl error.");
		return -ELOGIC;
	}
	return 0;
}

webapp_request_socket_item* 
http_global_dispatcher::find_request_sock(wa_request_impl* req)
{
	if (!req) {
		return nullptr;
	}
	// create a dummy search node
	webapp_request_socket_item dummy_sock;
	dummy_sock.ztcp_req = req;
	auto* ret = avl_find(_httpreq_socket_tree, &dummy_sock.req_avlnode,
		webapp_request_socket_item::req_avl_compare);
	if (nullptr == ret) {
		return nullptr;
	}
	return AVLNODE_ENTRY(webapp_request_socket_item, req_avlnode, ret);
}

webapp_context_impl* http_global_dispatcher::get_context(void)
{
	auto* ret = reinterpret_cast<webapp_context_impl*>(_context);
	assert(nullptr != ret);
	return ret;
}

http_request_item::http_request_item()
: easy(nullptr)
, reply_data(nullptr)
, prev_reply(nullptr)
, sender(nullptr)
, headers(nullptr)
{
}

http_request_item::~http_request_item()
{
	reset();
	if (easy) {
		curl_easy_cleanup(easy);
		easy = nullptr;
	}
}

void http_request_item::reset()
{
	if (prev_reply) {
		prev_reply = nullptr;
	}
	
	sender = nullptr;

	if (reply_data) {
		delete reply_data;
		reply_data = nullptr;
	}

	// release the headers
	if (headers) {
		curl_slist_free_all(headers);
		headers = nullptr;
	}
}

http_request::http_request(const uri& u, wa_request* req)
: wa_request_impl(u, req)
, _headers(nullptr)
, _flags(0)
{
	if (!init()) {
		wa_request_impl::_f.valid = 1;
	}
}

http_request::~http_request()
{
	if (_headers) {
		curl_slist_free_all(_headers);
		_headers = nullptr;
	}
	auto* dispatcher = get_dispatcher();
	if (nullptr == dispatcher) {
		return;
	}
	if (dispatcher->remove_request(this)) {
		log.w("wa_request_impl", "fail to remove request\n");
	}
}

int http_request::init(void)
{
	// initialize the flags
	wa_request_impl::_f.must_response = 1;
	wa_request_impl::_f.request_type = ztcp_req_type_unknown;

	// check if the input url is valid
	if (!_uri.valid()) {
		return -1;
	}

	wa_request_impl::_f.protocol = protocol_type_http;
	auto* dispatcher = get_dispatcher();
	if (nullptr == dispatcher) {
		return -2;
	}
	if (dispatcher->create_request(this)) {
		return -3;
	}
	return 0;
}

int http_request::send(const char* type, const uint8_t *data, size_t sz)
{
	bool has_data = (!data || !sz) ? false : true;
	if (set_type(type, has_data)) {
		return -EINVALID;
	}
	assert(protocol_type_http == wa_request_impl::_f.protocol);
	auto* dispatcher = get_dispatcher();
	if (nullptr == dispatcher) {
		return -ENOTAVAIL;
	}

	update_tick();
	return dispatcher->send(this, &_uri, data, sz);
}

int http_request::set_header(const char* key, const char* val, bool clear_all)
{
	if (nullptr == key || *key == '\0') {
		return -EBADPARM;
	}
	if (nullptr == val) {
		return -EBADPARM;
	}
	// handle some of the key
	if (!strcmp(key, "Content-Type")) {
		_f.content_type_assigned = 1;
	}

	string header(key);
	header += ": ";
	header += val;
	_headers = curl_slist_append(_headers, header.c_str());
	return 0;
}

int http_request::set_type(const char* type, bool has_data)
{
	assert(wa_request_impl::_f.protocol\
		 == protocol_type_http);
	// check the type
	if (nullptr == type) {
		wa_request_impl::_f.request_type = (has_data)
		? ztcp_req_type_http_post
		: ztcp_req_type_http_get;
	}
	else if (!strcmp(type, "GET")) {
		wa_request_impl::_f.request_type = ztcp_req_type_http_get;
	}
	else if (!strcmp(type, "POST")) {
		wa_request_impl::_f.request_type = ztcp_req_type_http_post;
	}
	else {
		wa_request_impl::_f.request_type = ztcp_req_type_unknown;
		return -1;
	}
	return 0;
}

curl_slist* http_request::update_headers(curl_slist*& header_list)
{
	assert(nullptr == header_list);
	if (!_f.content_type_assigned) {
		_headers = curl_slist_append(_headers,
		"Content-Type: application/json");
	}
	
	// add normal items
	_headers = curl_slist_append(_headers,
		"Connection: Keep-alive");
	_headers = curl_slist_append(_headers,
		"Keep-Alive: timeout=20");
	header_list = _headers;

	_headers = nullptr;
	return header_list;
}

}} // end of namespace zas::webcore
/* EOF */
