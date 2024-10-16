#include "utils/mutex.h"
#include "utils/thread.h"

#include "webcore/webapp.h"
#include "webcore/webapp-backend.h"
#include "webcore/sysconfig.h"
#include "webcore/logger.h"

#include "inc/webapp-impl.h"

namespace zas {
namespace webcore {

using namespace std;
using namespace zas::utils;

static mutex _webappmut;
__thread void* _context = nullptr;
__thread void* _ws_context = nullptr;

webapp_thread::webapp_thread(const char* name,
	webapp_impl* wa,
	webapp_backend *backend,
	webapp_callback *cb, 
	int server_id,
	int endpoint_id)
: thread(name)
, _webapp(wa), _backend(backend)
, _cb(cb), _server_id(server_id)
, _endpoint_id(endpoint_id)
, _server_sock(nullptr)
, _endpoint_sock(nullptr)
{
	_name.clear();
	if (name && *name) {
		_name = name;
	}
}

webapp_thread::~webapp_thread()
{
}

int webapp_thread::run()
{
	if (!_backend || !_webapp || !_cb) {
		return -EBADPARM;
	}

	// initialize the logger
	log.init();

	//generate thread context
	auto* wa_ct = new webapp_context_impl();
	_context = reinterpret_cast<void*>(wa_ct);
	//create backend context
	_ws_context = _backend->create_context();
	if (!_context || !_ws_context) {
		log.e("webapp_thread", "connext error\n");
		return -ELOGIC;
	}
	start_server_and_endpoint();

	//generate backend socket callback dispatch
	auto* wa_cb= new webapp_socket_dispatch(
		reinterpret_cast<void*>(wa_ct), _webapp, _ws_context, _backend);
	assert(nullptr != wa_cb);

	_backend->set_callback(_ws_context, wa_cb);
	// init thread context info: socket-context, socket-dispatch
	wa_ct->set_webapp_socket_context(_ws_context);
	wa_ct->set_webapp_socket_dispatch(wa_cb);

	auto* http_dispatch = new http_global_dispatcher(_backend);
	assert(nullptr != http_dispatch);
	wa_ct->set_http_dispatch(http_dispatch);

	// init disptach response factory
	wa_cb->set_response_factory(_webapp->get_response_factory());

	// notify user initilized
	_cb->oninit();

	//run backend. if dispatch return, then thread exit
	int ret = _backend->dispatch(_ws_context);

	//free backend socket callback dispatch & thread-context
	_backend->set_callback(_ws_context, nullptr);
	delete http_dispatch;
	delete wa_cb;
	if (_server_sock) {
		_server_sock->release();
		_server_sock = nullptr;
	}
	if (_endpoint_sock) {
		_endpoint_sock->release();
		_endpoint_sock = nullptr;
	}
	_backend->destory_context(_ws_context);
	delete wa_ct;
	_cb->onexit();

	log.flush();
	return 0;
}

int webapp_thread::start_server_and_endpoint(void)
{
	// this is for test
	if (_server_id >= 0) {
		std::string portpath = "webcore.servers.";
		portpath += std::to_string(_server_id);
		portpath += ".port";
		int port = -1;
		port = get_sysconfig(portpath.c_str(), port);
		if (port > 0) {
			std::string url = "tcp://*:";
			url += std::to_string(port);
			_server_sock = _backend->bind(_ws_context,
				url.c_str(), webapp_socket_router);
			if (!_server_sock) {
				log.e("webapp_thread", "bind [%s] error\n", url.c_str());
				return -ELOGIC;
			}
		}
	} 
	if (_endpoint_id >= 0) {
		std::string rootpath = "webcore.endpoints.";
		rootpath += std::to_string(_endpoint_id);
		std::string portpath = rootpath + ".port";
		int port = -1;
		port = get_sysconfig(portpath.c_str(), port);
		std::string eptype = "worker"; 
		std::string type_path = rootpath + ".type";
		eptype = get_sysconfig(eptype.c_str(), port);
		std::string addr; 
		std::string addr_path = rootpath + ".addr";
		addr = get_sysconfig(addr_path.c_str(), addr.c_str());
		if (addr.length() > 0 && port > 0) {
			std::string url = "tcp://";
			url += addr;
			url += ":";
			url += std::to_string(port);
			webapp_socket_type etype = webapp_socket_worker;
			if (!eptype.compare("client")) {
				etype = webapp_socket_client;
			}
			_endpoint_sock = _backend->connect(_ws_context,
				url.c_str(), etype);
			if (!_endpoint_sock) {
				log.e("webapp_thread", "endpoint connect [%s] error\n", url.c_str());
				return -ELOGIC;
			}
		}
	}
	return 0;
}

webapp_socket_dispatch::webapp_socket_dispatch(
	void* thd_context, webapp_impl* impl,
	void* ws_context, webapp_backend* backend)
: _thd_context(thd_context)
, _ws_context(ws_context)
, _seq_id(0)
, _webapp(impl)
, _factory(nullptr)
, _backend(backend)
, _wa_sock_avltree(nullptr)
, _wa_sock_item_avltree(nullptr)
, _wa_request_avltree(nullptr)
{
	listnode_init(_webapp_socket_list);
	_service_name.clear();
	if (_webapp && _webapp->get_service_name()) {
		_service_name = _webapp->get_service_name();
	}

}

webapp_socket_dispatch::~webapp_socket_dispatch()
{
	remove_all_webapp_socket();
}

int webapp_socket_dispatch::on_recv(void* context,
	webapp_socket* socket,
	void* data,
	size_t sz)
{
	assert (nullptr != _context);
	assert (_ws_context == context);
	assert (nullptr != _backend);
	
	if (!_factory) {
		_factory = _webapp->get_response_factory();
		if (_factory) {
			log.e("webapp","error: no ztcp response factory");
			return -ENOTAVAIL;
		}
	}
	return dispatch(socket, data, sz);
}

int webapp_socket_dispatch::get_tcp_uri(const uri& u, std::string& req)
{
	static int def_port = -1;

	// load the default port
	if (def_port < 0) {
		// todo: load default port from "maybe" sysconfig
		def_port = 5556;
	}

	req = "tcp://" + u.get_host();
	if (req.length() <= 6) {
		return -1;
	}

	char port_buf[16];
	if (u.has_port()) {
		sprintf(port_buf, ":%u", u.get_port());
	} else {
		sprintf(port_buf, ":%u", def_port);
	}
	req += port_buf;
	return 0;
}

webapp_context_impl* webapp_socket_dispatch::get_context(void)
{
	auto* ret = reinterpret_cast<webapp_context_impl*>(_context);
	assert(nullptr != ret);
	return ret;
}

int webapp_socket_dispatch::create_request(wa_request_impl *req)
{
	if (!req) {
		return -ENOTAVAIL;
	}

	//wa_request_impl req object is reqeust object unique id
	auto* item = find_request_sock_by_req_unlock(req);
	if (item) {
		return -EEXISTS;
	}

	// currently only ztcp request is handled by web_socket_dispatch
	assert(req->get_protocol() == protocol_type_ztcp);

	// create zmq style uri
	std::string req_svr;
	if (get_tcp_uri(req->geturl(), req_svr)) {
		return -EINVALID;
	}

	// create webapp socket for req-rep
	assert(nullptr != _backend);
	auto* socket = _backend->connect(_ws_context,
		req_svr.c_str(), webapp_socket_client);
	if (nullptr == socket) {
		return -ELOGIC;
	}

	// create wa_request_impl object
	auto* req_item = new webapp_request_socket_item();
	assert(nullptr != req_item);
	req_item->ztcp_req = req;
	req_item->wa_socket = socket;
	socket->addref();
	listnode_init(req_item->request_list);

	// add wa_request_impl to webapp socket list
	if (add_webapp_request_socket(req_item)) {
		socket->release();
		delete req_item;
		return -ELOGIC;
	}
	return 0;
}

int webapp_socket_dispatch::remove_request(wa_request_impl *req)
{
	if (!req) {
		return -ENOTAVAIL;
	}

	auto* item = find_request_sock_by_req_unlock(req);
	if (!item) {
		return -ENOTFOUND;
	}
	return remove_webapp_request_unlock(item);
}

int webapp_socket_dispatch::send(wa_request_impl *req,
	uri* url, const uint8_t* data, size_t sz)
{
	assert(nullptr != req);
	assert(nullptr != url);

	auto* item = find_request_sock_by_req_unlock(req);
	if (!item) {
		return -ENOTFOUND;
	}

	// only ztcp is support here in webapp_socket_dispatcher
	assert(protocol_type_ztcp == req->get_protocol());
	auto reqtype = req->get_request_type();
	
	// fill zmq basickinfo
	std::string surl = url->tostring();
	size_t hdr_sz = sizeof(basicinfo_frame_uri)
		+ sizeof(basicinfo_frame_content) + surl.length();
	uint8_t* basic_info = (uint8_t*)(alloca(hdr_sz));
	auto* basic_uri = reinterpret_cast<basicinfo_frame_uri*>(basic_info);
	basic_uri->uri_length = surl.length();
	if (nullptr == basic_uri->uri || surl.length() == 0) {
		return -EBADPARM;
	}
	strncpy(basic_uri->uri, surl.c_str(), surl.length());
	auto* basic_content = reinterpret_cast<basicinfo_frame_content*>(
		basic_info + sizeof(basicinfo_frame_uri) + surl.length());
	basic_content->sequence_id = _seq_id++;
	basic_content->a.pattern = msgptn_unknown;
	if (ztcp_req_type_ztcp_req_rep == reqtype) {
		basic_content->a.pattern = msgptn_req_rep;
		basic_content->a.type = msgtype_request;
		add_webapp_request_unlock(item, req, basic_content->sequence_id);
	}
	basic_content->body_frames = 1;
	basic_content->body_size = sz;

	auto* ws_socket = item->wa_socket;
	ws_socket->send(basic_info, hdr_sz, false);
	ws_socket->send((void*)data, sz, true);
	return 0;
}

int webapp_socket_dispatch::reply(void* data, size_t sz)
{
	auto response = get_context()->get_pending();
	if (!response.get()) {
		log.e(WEB_DISPATCH_TAG, 
			"current thread does not have message reply\n");
		return -ENOTAVAIL;
	}
	auto* web_socket = response->get_webapp_socket();
	if (nullptr == web_socket) {
		log.e(WEB_DISPATCH_TAG, 
			"current thread does not have message reply socket\n");
		return -ENOTAVAIL;
	}
	return web_socket->reply(data, sz);
}

timermgr* webapp_socket_dispatch::get_timermgr(void)
{
	// if thread is not zmq thread, timermgr return null
	if (!_backend || !_context) {
		return nullptr;
	}
	return _backend->get_timermgr(_ws_context);
}

int webapp_socket_dispatch::set_response_factory(wa_response_factory *factory)
{
	_factory = factory;
	return 0;
}

int webapp_socket_dispatch::add_webapp_request_socket(
	webapp_request_socket_item *req_sock)
{
	if (!req_sock) {
		return -EBADPARM;
	}

	assert(protocol_type_ztcp == req_sock->ztcp_req->get_protocol());

	webapp_backend_socket* wa_socket_item;	
	wa_socket_item = find_webapp_socket_unlock(req_sock->wa_socket);
	if (!wa_socket_item) {
		wa_socket_item =  add_webapp_socket_unlock(req_sock->wa_socket);
		assert(nullptr != wa_socket_item);
	}

	assert(req_sock->ztcp_req != nullptr);
	int ret = avl_insert(&_wa_sock_item_avltree, &req_sock->req_avlnode,
		webapp_request_socket_item::req_avl_compare);
	assert(ret == 0);

	// add to list and avl trees
	listnode_add(wa_socket_item->req_socket_list, req_sock->ownerlist);
	return 0;
}

http_global_dispatcher::http_global_dispatcher(webapp_backend* backend)
: _http_timersock(nullptr)
, _http_curlm(nullptr)
, _http_still_running(0)
, _httpreq_socket_tree(nullptr)
, _http_curl_unused_cnt(0)
, _http_curl_reused_max_cnt(WEB_HTTP_REQUEST_MAX_REUSED_CNT)
, _backend(backend)
{
	listnode_init(_http_curl_list);
	listnode_init(_httpreq_socket_list);

	// init async http request handler
	int ret = async_http_init();
	assert(ret == 0);

	// init the virtual socket object
	listnode_init(_http_virt_socket.ownerlist);
	listnode_init(_http_virt_socket.req_socket_list);
	_http_virt_socket.wa_socket = nullptr;
}

http_global_dispatcher::~http_global_dispatcher()
{
	// release _http_curl_list
	// check release _httpreq_socket_list
	release_all_request();
	
	// release _http_curlm
	async_http_destroy();
	
	if (_http_timersock) {
		_http_timersock->release();
		_http_timersock = nullptr;
	}
}

int http_global_dispatcher::release_all_request(void)
{
	while (!listnode_isempty(_http_virt_socket.req_socket_list))
	{
		auto* sock_item = LIST_ENTRY(webapp_request_socket_item, 	\
			ownerlist, _http_virt_socket.req_socket_list.next);
		listnode_del(sock_item->ownerlist);
		while(!listnode_isempty(sock_item->request_list)) {
			auto* req_item = LIST_ENTRY(http_request_item, 	\
				ownerlist, sock_item->request_list.next);
			listnode_del(req_item->ownerlist);
			delete req_item;
		}
		assert(nullptr != sock_item->ztcp_req);
		delete sock_item;
	}

	while (!listnode_isempty(_http_curl_list))
	{
		auto* req_item = LIST_ENTRY(http_request_item, 	\
			ownerlist, _http_curl_list.next);
		listnode_del(req_item->ownerlist);
		delete req_item;
	}

	_httpreq_socket_tree = nullptr;
	return 0;
}

webapp_backend_socket* webapp_socket_dispatch::add_webapp_socket_unlock(
	webapp_socket *wasocket)
{
	if (!wasocket) {
		return nullptr;
	}

	auto* item = find_webapp_socket_unlock(wasocket);
	if (item) {
		return item;
	}

	item = new webapp_backend_socket();
	item->wa_socket = wasocket;
	wasocket->addref();
	listnode_init(item->req_socket_list);

	int ret = avl_insert(&_wa_sock_avltree, &item->wa_sock_avlnode,
		webapp_backend_socket::wa_sock_avl_compare);
	assert(ret == 0);

	// add to list and avl trees
	listnode_add(_webapp_socket_list, item->ownerlist);
	return item;
}

int webapp_backend_socket::wa_sock_avl_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(webapp_backend_socket, wa_sock_avlnode, a);
	auto* bb = AVLNODE_ENTRY(webapp_backend_socket, wa_sock_avlnode, b);
	if ((size_t)aa->wa_socket < (size_t)bb->wa_socket) return -1;
	else if ((size_t)aa->wa_socket > (size_t)bb->wa_socket) return 1;
	else return 0;
}

int webapp_request_socket_item::req_avl_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(webapp_request_socket_item, req_avlnode, a);
	auto* bb = AVLNODE_ENTRY(webapp_request_socket_item, req_avlnode, b);
	if ((size_t)aa->ztcp_req > (size_t)bb->ztcp_req) {
		return 1;
	} else if ((size_t)aa->ztcp_req < (size_t)bb->ztcp_req) {
		return -1;
	} else return 0;
}

int wa_request_item::seq_avl_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(wa_request_item, seq_avlnode, a);
	auto* bb = AVLNODE_ENTRY(wa_request_item, seq_avlnode, b);
	if (aa->sequence_id > bb->sequence_id) {
		return 1;
	} else if (aa->sequence_id < bb->sequence_id) {
		return -1;
	} else return 0;
}

webapp_request_socket_item* 
webapp_socket_dispatch::find_request_sock_by_req_unlock(wa_request_impl* req)
{
	if (!req) {
		return nullptr;
	}

	// create a dummy search node
	webapp_request_socket_item dummy_sock;
	dummy_sock.ztcp_req = req;

	auto* ret = avl_find(_wa_sock_item_avltree, &dummy_sock.req_avlnode,
		webapp_request_socket_item::req_avl_compare);
	if (nullptr == ret) {
		return nullptr;
	}

	return AVLNODE_ENTRY(webapp_request_socket_item, req_avlnode, ret);
}

// find websocket info item by wasocket
webapp_backend_socket* 
webapp_socket_dispatch::find_webapp_socket_unlock(webapp_socket *wasocket)
{
	if (!wasocket) {
		return nullptr;
	}

	if (listnode_isempty(_webapp_socket_list)) {
		return nullptr;
	}

	// create a dummy search node
	webapp_backend_socket dummy_item;
	dummy_item.wa_socket = wasocket;

	auto* ret = avl_find(_wa_sock_avltree, &dummy_item.wa_sock_avlnode,
		webapp_backend_socket::wa_sock_avl_compare);
	if (nullptr == ret) {
		return nullptr;
	}

	return AVLNODE_ENTRY(webapp_backend_socket, wa_sock_avlnode, ret);
}

int webapp_socket_dispatch::add_webapp_request_unlock(
	webapp_request_socket_item* item,
	wa_request_impl *req,
	uint64_t seq_id)
{
	auto* req_item = find_webapp_request_unlock(seq_id);
	if (req_item) {
		return -EEXISTS;
	}

	req_item = new wa_request_item();
	if (nullptr == req_item) {
		return -ENOMEMORY;
	}

	req_item->sequence_id = seq_id;
	req_item->sender = req;
	req_item->prev_reply = get_context()->get_pending();

	listnode_add(item->request_list, req_item->ownerlist);
	int ret = avl_insert(&_wa_request_avltree, &req_item->seq_avlnode,
		wa_request_item::seq_avl_compare);
	assert(ret == 0);
	return 0;
}

wa_request_item* webapp_socket_dispatch::find_webapp_request_unlock(
	uint64_t seq_id)
{
	// create a dummy search node
	wa_request_item dummy_item;
	dummy_item.sequence_id = seq_id;

	auto* ret = avl_find(_wa_request_avltree, &dummy_item.seq_avlnode,
		wa_request_item::seq_avl_compare);
	if (nullptr == ret) {
		return nullptr;
	}

	return AVLNODE_ENTRY(wa_request_item, seq_avlnode, ret);
}

int webapp_socket_dispatch::release_webapp_request_unlock(
	wa_request_item* item, wa_request_impl *req_impl)
{
	if (!item) {
		return -EBADPARM;
	}
	
	auto* req_sock_item = find_request_sock_by_req_unlock(req_impl);
	if (!req_sock_item) {
		return -ENOTFOUND;
	}

	// pending response maybe nullptr before this wa_request_impl
	if (item->prev_reply) {
		item->prev_reply = nullptr;
	}

	listnode_del(item->ownerlist);
	avl_remove(&_wa_request_avltree, &item->seq_avlnode);
	delete item;
	return 0;
}

int webapp_socket_dispatch::remove_webapp_request_unlock(
	webapp_request_socket_item* item)
{
	if (!item) {
		return -EBADPARM;
	}
	auto* req = item->ztcp_req;
	assert(nullptr != req);

	listnode_t *nd = item->request_list.next;
	if (protocol_type_ztcp == req->get_protocol()) {
		for (; nd != &(item->request_list);) {
			auto* req_item = LIST_ENTRY(wa_request_item, ownerlist, nd);
			nd = nd->next;
			listnode_del(req_item->ownerlist);
			avl_remove(&_wa_request_avltree, &req_item->seq_avlnode);
			delete req_item;
		}
		// release wa_request_impl in webapp_socket
		auto* sock_item = find_webapp_socket_unlock(item->wa_socket);
		assert(nullptr != sock_item);

		listnode_del(item->ownerlist);
		// if webapp_socket is 0, release webapp_socket
		if (listnode_isempty(sock_item->req_socket_list)) {
			listnode_del(sock_item->ownerlist);
			sock_item->wa_socket->release();
			avl_remove(&_wa_sock_avltree, &sock_item->wa_sock_avlnode);
			delete sock_item;
		}

		item->wa_socket->release();
		item->wa_socket = nullptr;
	} else {
		log.e(WEB_DISPATCH_TAG, "remove request impl error.");
		return -ELOGIC;
	}
	avl_remove(&_wa_sock_item_avltree, &item->req_avlnode);
	delete item;
	return 0;
}

int webapp_socket_dispatch::remove_all_webapp_socket(void)
{
	while (!listnode_isempty(_webapp_socket_list)) {
		auto* item =
			LIST_ENTRY(webapp_backend_socket, ownerlist, _webapp_socket_list.next);
		listnode_del(item->ownerlist);
		while (!listnode_isempty(item->req_socket_list))
		{
			auto* sock_item = LIST_ENTRY(webapp_request_socket_item, 	\
				ownerlist, item->req_socket_list.next);
			listnode_del(sock_item->ownerlist);
			while(!listnode_isempty(sock_item->request_list)) {
				auto* req_item = LIST_ENTRY(wa_request_item, 	\
					ownerlist, sock_item->request_list.next);
				listnode_del(req_item->ownerlist);
				delete req_item;
			}
			assert(nullptr != sock_item->ztcp_req);
			assert(protocol_type_ztcp == sock_item->ztcp_req->get_protocol()); 
			sock_item->wa_socket->release();
			sock_item->wa_socket = nullptr;
			delete sock_item;
		}
		item->wa_socket->release();
		delete item;
	}
	_wa_sock_item_avltree = nullptr;
	_wa_sock_avltree = nullptr;
	_wa_request_avltree = nullptr;
	return 0;
}

int webapp_socket_dispatch::dispatch(webapp_socket* socket,
	void* data, size_t sz)
{
	assert(nullptr != _factory);
	auto* hdr_uri = reinterpret_cast<basicinfo_frame_uri*>(data);
	auto* hdr_ct = reinterpret_cast<basicinfo_frame_content*>((uint8_t*)data
		+ sizeof(basicinfo_frame_uri) + hdr_uri->uri_length);
	
	auto response = get_context()->get_pending();
	if (response.get()) {
		log.w(WEB_DISPATCH_TAG, "message reply record error\n");
	}

	if (msgptn_unknown == hdr_ct->a.pattern 
		|| (msgptn_req_rep == hdr_ct->a.pattern
		&& (msgtype_request == hdr_ct->a.type
		|| msgtype_unknown == hdr_ct->a.type))) {
		response.set(new wa_response_proxy(_factory, socket));
		assert(nullptr != response.get());
		get_context()->set_pending(response);
		
		// void* udata = reinterpret_cast<void*>((hdr_ct + 1));
		// size_t data_sz = sz - sizeof(basicinfo_frame_uri)
		// 	- hdr_uri->uri_length - sizeof(basicinfo_frame_content);

		assert(nullptr != response->get_reply());
		response->get_reply()->on_request(data, sz);
		response = nullptr;
		get_context()->set_pending(nullptr);
	}
	else {
		auto *item = find_webapp_socket_unlock(socket);
		if (!item) {
			log.e(WEB_DISPATCH_TAG, "recv invalid message\n");
			return -ENOTAVAIL;
		}
		auto* req_item = find_webapp_request_unlock(hdr_ct->sequence_id);
		if (!req_item) {
			log.e(WEB_DISPATCH_TAG, "recv incorrect reply message\n");
			return -ENOTAVAIL;
		}
		auto* req_impl = req_item->sender;
		get_context()->set_pending(req_item->prev_reply);
	
		void* udata = reinterpret_cast<void*>((hdr_ct + 1));
		size_t data_sz = sz - sizeof(basicinfo_frame_uri)
			- hdr_uri->uri_length - sizeof(basicinfo_frame_content);

		// let user handle the reply
		auto* sender = req_item->sender;
		int errcode  = sender->on_reply(_context, udata, data_sz);
		sender->on_finish(_context, errcode);
		get_context()->set_pending(nullptr);
		release_webapp_request_unlock(req_item, req_impl);
	}
	return 0;
}

webapp_impl* webapp_impl::inst()
{
	static webapp_impl* _inst = NULL;
	if (_inst) {
		return _inst;
	}

	auto_mutex am(_webappmut);
	if (_inst) {
		return _inst;
	}
	_inst = new webapp_impl();
	assert(NULL != _inst);
	return _inst;	
}

webapp_impl::webapp_impl()
: _backend(nullptr)
, _cb(nullptr)
, _factory(nullptr)
, _thread_id(0)
{

}

webapp_impl::~webapp_impl()
{
}

int webapp_impl::run(const char* service_name, bool septhd)
{
	// check current is main thread
	if (0 == _thread_id) {
		set_thread((uint64_t)pthread_self());
	}
	if (gettid() != _thread_id) {
		return -EINVALID;
	}
	if (!_backend || !_cb) {
		return -ENOTALLOWED;
	}

	int iret = 0;
	int sock_thd_cnt = 1;
	int server_cnt = -1;
	int endpoint_cnt = -1;
	int cli_thd_cnt = 1;


	sock_thd_cnt = get_sysconfig("webcore.sock-thread-maxcnt", sock_thd_cnt);

	//get server port and thread cnt from configfile
	server_cnt = get_sysconfig("webcore.server-count", server_cnt);
	endpoint_cnt = get_sysconfig("webcore.endpoint-count", endpoint_cnt);
	if (server_cnt > sock_thd_cnt) {
		sock_thd_cnt = server_cnt;
	}
	if (endpoint_cnt > sock_thd_cnt) {
		sock_thd_cnt = endpoint_cnt;
	}
	std::string thd_name;
	if (sock_thd_cnt > 0 && !septhd) {
		sock_thd_cnt--;
	}

	int endpoint_id = -1;
	int server_id = -1;
	for (int i = 0; i < sock_thd_cnt; i++) {
		thd_name = "worker_";
		thd_name += std::to_string(i);
		if (endpoint_cnt > 0) {
			endpoint_cnt--;
			endpoint_id = endpoint_cnt;
		} else {
			endpoint_id = -1;
		}
		if (server_cnt > 0) {
			server_cnt--;
			server_id = server_cnt;
		} else {
			server_id = -1;
		}
		auto* thd = new webapp_thread(thd_name.c_str(), this,
			_backend, _cb, server_id, endpoint_id);
		assert(nullptr != thd);
		thd->start();
		thd->release();
	}
	
	if (!septhd) {
		thd_name = "worker_";
		thd_name += std::to_string(sock_thd_cnt - 1);
		if (endpoint_cnt > 0) {
			endpoint_cnt--;
			endpoint_id = endpoint_cnt;
		} else {
			endpoint_id = -1;
		}
		if (server_cnt > 0) {
			server_cnt--;
			server_id = server_cnt;
		} else {
			server_id = -1;
		}
		auto* thd = new webapp_thread(thd_name.c_str(), this,
			_backend, _cb, server_id, endpoint_id);
		assert(nullptr != thd);
		thd->run();
		thd->release();
	}
	return 0;
}

int webapp_impl::set_backend(webapp_backend *backend)
{
	if (_backend) {
		//todo release ?
	}
	_backend = backend;
	return 0;
}

webapp_backend* webapp_impl::get_backend()
{
	return _backend;
}

const char* webapp_impl::get_service_name()
{
	if (_service_name.length() == 0) {
		return nullptr;
	}
	return _service_name.c_str();
}

int webapp_impl::set_app_callback(webapp_callback* cb)
{
	_cb = cb;
	return 0;
}

int webapp_impl::set_response_factory(wa_response_factory* factory)
{
	_factory = factory;
	return 0;
}

int webapp_impl::set_thread(uint64_t thd_id)
{
	_thread_id = thd_id;
	return 0;
}

wa_response_factory* webapp_impl::get_response_factory(void)
{
	return _factory;
}

wa_response_proxy::wa_response_proxy(wa_response_factory* factory,
	webapp_socket* socket)
: _factory(factory)
, _reply(nullptr)
, _socket(nullptr)
, _refcnt(1)
{
	assert(nullptr != _factory);
	_reply = _factory->create_instance();
	assert(nullptr != _reply);
	_socket = socket;
	assert(nullptr != socket);
	_socket->addref();
}

wa_response_proxy::~wa_response_proxy()
{
	if (_factory) {
		if (_reply) {
			_factory->destory_instance(_reply);
			_reply = nullptr;
		}
		_factory = nullptr;
	}

	if (_socket) {
		_socket->release();
		_socket = nullptr;
	}
}

int wa_response_proxy::addref()
{
	return __sync_add_and_fetch(&_refcnt, 1);
}

int wa_response_proxy::release()
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0) {
		delete this;
	}
	return cnt;
}

wa_response* wa_response_proxy::get_reply()
{
	return _reply;
}

webapp_socket* wa_response_proxy::get_webapp_socket()
{
	return _socket;
}

webapp_context_impl::webapp_context_impl()
: _http_dispatcher(nullptr)
, _dispatch(nullptr)
{
}

webapp_context_impl::~webapp_context_impl()
{
	if (_http_dispatcher) {
		delete _http_dispatcher;
		_http_dispatcher = nullptr;
	}
}

int webapp_context_impl::set_webapp_socket_context(void* context)
{
	_ws_context = context;
	return 0;
}

void* webapp_context_impl::get_webapp_socket_context(void)
{
	return _ws_context;
}

int webapp_context_impl::set_webapp_socket_dispatch(
	webapp_socket_dispatch* dispatch)
{
	if (!dispatch) {
		return -EBADPARM;
	}
	_dispatch = dispatch;
	return 0;
}

int webapp_context_impl::set_http_dispatch(http_global_dispatcher* dispatch)
{
	if (!dispatch) {
		return -EBADPARM;
	}
	_http_dispatcher = dispatch;
	return 0;
}

wa_request_impl::wa_request_impl(const uri& u, wa_request* req)
: _uri(u), _req_cb(req), _flags(0)
, _tick1(0), _tick2(0)
{
}

wa_request_impl* wa_request_impl::create(const uri& u, wa_request* req)
{
	wa_request_impl* ret = nullptr;
	string schema = u.get_scheme();
	if (schema == "http" || schema == "https") {
		ret = new http_request(u, req);
	}
	else if (schema == "ztcp") {
		ret = new ztcp_request(u, req);
	}
	return ret;
}

void wa_request_impl::update_tick(void)
{
	_tick1 = _tick2;
	_tick2 = gettick_millisecond();
}

wa_request_impl::~wa_request_impl()
{
}

ztcp_request::ztcp_request(const uri& u, wa_request* req)
: wa_request_impl(u, req)
{
	if (!init()) {
		_f.valid = 1;
	}
}

ztcp_request::~ztcp_request()
{
	auto* dispatcher = get_dispatcher();
	if (nullptr == dispatcher) {
		return;
	}
	if (dispatcher->remove_request(this)) {
		log.w("wa_request_impl", "fail to remove request\n");
	}
}

int ztcp_request::set_type(const char* type, bool has_data)
{
	assert(_f.protocol == protocol_type_ztcp);

	// check the type
	if (nullptr == type) {
		_f.request_type = ztcp_req_type_ztcp_no_rep;
		_f.must_response = 0;
	}
	else if (!strcmp(type, "REQ-REP")) {
		_f.request_type = ztcp_req_type_ztcp_req_rep;
	}
	else {
		_f.request_type = ztcp_req_type_unknown;
		return -2;
	}
	return 0;
}

int ztcp_request::set_header(const char* key, const char* val, bool clear_all)
{
	if (clear_all) {
		_uri.clear_query();
	}
	return _uri.add_query(key, val);
}

int ztcp_request::send(const char* type, const uint8_t *data, size_t sz)
{
	bool has_data = (!data || !sz) ? false : true;
	if (set_type(type, has_data)) {
		return -EINVALID;
	}

	assert(protocol_type_ztcp == _f.protocol);
	// get dispatcher
	auto* dispatcher = get_dispatcher();
	if (nullptr == dispatcher) {
		return -ENOTAVAIL;
	}

	msg_pattern pattern = msgptn_req_rep;
	if (!_f.must_response) {
		pattern = msgptn_unknown;
	}

	update_tick();
	return dispatcher->send(this, &_uri, data, sz);
}

int ztcp_request::init(void)
{
	// initialize the flags
	_f.must_response = 1;
	_f.request_type = ztcp_req_type_unknown;

	// check if the input url is valid
	if (!_uri.valid()) {
		return -1;
	}

	_f.protocol = protocol_type_ztcp;
	auto* dispatcher = get_dispatcher();
	if (dispatcher->create_request(this)) {
		return -3;
	}
	return 0;
}

http_global_dispatcher* http_request::get_dispatcher(void)
{
	auto* wa_ct = reinterpret_cast<webapp_context_impl*>(_context);
	if (!wa_ct) {
		return nullptr;
	}
	return wa_ct->get_http_dispatcher();
}

webapp_socket_dispatch* ztcp_request::get_dispatcher(void)
{
	auto* wa_ct = reinterpret_cast<webapp_context_impl*>(_context);
	if (!wa_ct) {
		return nullptr;
	}
	return wa_ct->get_webapp_socket_dispatch();
}

int wa_request_impl::set_header(const char* key, const char* val, bool clear_all)
{
	return -ENOTIMPL;
}

int wa_request_impl::send(const char* type, const uint8_t *data, size_t sz)
{
	update_tick();
	return -ENOTIMPL;
}

int wa_request_impl::on_reply(void* context, void* data, size_t sz)
{
	update_tick();
	if (!_f.valid) {
		return -EINVALID;
	}
	if (!_req_cb) {
		return -ENOTAVAIL;
	}
	return _req_cb->on_reply(context, data, sz);
}

void wa_request_impl::on_finish(void* context, int code)
{
	if (!_f.valid || !_req_cb) {
		return;
	}
	return _req_cb->on_finish(context, code);
}

wa_response::wa_response()
{
}

wa_response::~wa_response()
{
}

int wa_response::on_request(void* data, size_t sz)
{
	return 0;
}

int wa_response::response(void* data, size_t sz)
{
	auto* wa_ct = reinterpret_cast<webapp_context_impl*>(_context);
	if (!wa_ct) {
		log.e("wa_request_impl", "reply error: no valid context\n");
		return -ENOTAVAIL;
	}
	auto* dispatch = wa_ct->get_webapp_socket_dispatch();
	if (!dispatch) {
		log.e("wa_request_impl", "reply error: no valid dispatch\n");
		return -ENOTAVAIL;
	}
	return dispatch->reply(data, sz);
}

long wa_request::get_escaped_ticks(void)
{
	if (!_data) {
		return 0;
	}
	auto* req = reinterpret_cast<wa_request_impl*>(_data);
	if (!req->valid()) {
		return 0;
	}
	return req->get_escaped_ticks();

}

webapp_callback::webapp_callback()
{
}

webapp_callback::~webapp_callback()
{
}

int webapp_callback::oninit(void)
{
	return 0;
}

int webapp_callback::onexit(void)
{
	return 0;
}

void* webapp_context::get_sock_context(void)
{
	auto* imp = reinterpret_cast<webapp_context_impl*>(this);
	return imp->get_webapp_socket_context();
}

webapp* webapp::inst()
{
	auto* impl = webapp_impl::inst();
	assert(nullptr != impl);
	return reinterpret_cast<webapp*>(impl);
}

webapp_context* webapp::get_context(void)
{
	if (!_context) {
		return nullptr;
	}
	return reinterpret_cast<webapp_context*>(_context);
}

timermgr* webapp::get_timermgr()
{
	if (!_context || !_ws_context) {
		log.e("webapp", "get_timermgr error: connext error\n");
		return nullptr;
	}
	auto* wa_ct = reinterpret_cast<webapp_context_impl*>(_context);
	if (!wa_ct) {
		log.e("webapp", "get_timermgr error: no valid context\n");
		return nullptr;
	}
	auto* dispatch = wa_ct->get_webapp_socket_dispatch();
	if (!dispatch) {
		log.e("webapp", "get_timermgr error: no valid dispatch\n");
		return nullptr;
	}
	return dispatch->get_timermgr();
}

int webapp::run(const char* service_name, bool septhd)
{
	auto* impl = reinterpret_cast<webapp_impl*>(this);
	return impl->run(service_name, septhd);
}

int webapp::set_backend(webapp_backend *backend)
{
	auto* impl = reinterpret_cast<webapp_impl*>(this);
	return impl->set_backend(backend);
}

int webapp::set_app_callback(webapp_callback *_cb)
{
	auto* impl = reinterpret_cast<webapp_impl*>(this);
	return impl->set_app_callback(_cb);
}

int webapp::set_response_factory(wa_response_factory *factory)
{
	auto* impl = reinterpret_cast<webapp_impl*>(this);
	return impl->set_response_factory(factory);
}

wa_request::wa_request(const uri& u)
: _data(nullptr)
{
	auto* req = wa_request_impl::create(u, this);
	if (!req) return;

	if (!req->valid()) {
		delete req; return;
	}
	_data = reinterpret_cast<void*>(req);
}

wa_request::~wa_request()
{
	if (_data) {
		auto* req = reinterpret_cast<wa_request_impl*>(_data);
		delete req;
		_data = nullptr;
	}
}

uri* wa_request::url(void)
{
	if (!_data) {
		return nullptr;
	}
	auto* req = reinterpret_cast<wa_request_impl*>(_data);
	if (!req->valid()) {
		return nullptr;
	}
	return &req->geturl();
}

int wa_request::send(const uint8_t *data, size_t sz,
	const char* type)
{
	if (!_data) {
		return -ENOTAVAIL;
	}
	auto* req = reinterpret_cast<wa_request_impl*>(_data);
	if (!req->valid()) {
		return -EINVALID;
	}
	return req->send(type, data, sz);
}

int wa_request::send(zas::utils::fifobuffer *data)
{
	if (!_data) {
		return -ENOTAVAIL;
	}
	if (nullptr == data) {
		return -EBADPARM;
	}
	auto* req = reinterpret_cast<wa_request_impl*>(_data);
	if (!req->valid()) {
		return -EINVALID;
	}
	// return req->send(data);
	return 0;
}

int wa_request::set_header(const char* key, const char* val, bool clear_all)
{
	if (!_data) {
		return -ENOTAVAIL;
	}
	if (nullptr == key || nullptr == val) {
		return -EBADPARM;
	}
	auto* req = reinterpret_cast<wa_request_impl*>(_data);
	if (!req->valid()) {
		return -EINVALID;
	}
	return req->set_header(key, val, clear_all);
}

protocol_type wa_request::get_protocol(void)
{
	if (!_data) {
		return protocol_type_unknown;
	}
	auto* req = reinterpret_cast<wa_request_impl*>(_data);
	if (!req->valid()) {
		return protocol_type_unknown;
	}
	return req->get_protocol();
}

int wa_request::on_reply(void* context, void* data, size_t sz)
{
	return 0;
}

void wa_request::on_finish(void* context, int code)
{
}

}} // end of namespace zas::webcore
/* EOF */
