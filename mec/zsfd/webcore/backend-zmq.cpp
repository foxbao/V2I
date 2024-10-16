#include <unistd.h>
#include "webcore/sysconfig.h"
#include "webapp.h"

#include "backend-zmq-impl.h"
#include "zeromq-msg.h"
#include "webapp-def.h"
#include "utils/uuid.h"

namespace zas {
namespace webcore {

#define ZMQ_CONNECT_DEVICE_IF_PAHT "webcore.ztcp-ifname"

using namespace zas::webcore;

__thread zmq_backend_worker* _worker_context = nullptr;
static int _test_timer_count = 0;

struct worker_item {
	listnode_t ownerlist;
	zmq_backend_worker* wker;
};

webapp_socket_zmq::webapp_socket_zmq(zmq::socket_t* socket,
	const char* uri,
	webapp_socket_type type)
: _socket(socket)
, _socket_type(type)
, _zmqmsg_context(nullptr)
, _isnew(true)
, _refcnt(1)
{
	_uri = uri;
}

webapp_socket_zmq::webapp_socket_zmq(webapp_socket_zmq& zmq)
: _socket(zmq._socket)
, _socket_type(zmq._socket_type)
, _zmqmsg_context(nullptr)
, _isnew(true)
, _refcnt(1)
{
	_uri = zmq._uri;
}

webapp_socket_zmq::~webapp_socket_zmq()
{
	if (!_socket) {
		delete _socket;
		_socket = nullptr;
	}
	if (!_zmqmsg_context) {
		delete _zmqmsg_context;
		_zmqmsg_context = nullptr;
	}
}

int webapp_socket_zmq::send(void* data, size_t sz, bool bfinished)
{
	if (!_socket) {
		return -ENOTAVAIL;
	}
	::zmq::send_flags flags = ::zmq::send_flags::none | ::zmq::send_flags::dontwait;
	if (!bfinished) {
		flags = ::zmq::send_flags::sndmore | ::zmq::send_flags::dontwait;
	}
	try {
		if (webapp_socket_worker == _socket_type
			|| webapp_socket_client == _socket_type) {
			if (_isnew) {
				zmq::message_t emptymsg(0);
				_socket->send(emptymsg, ::zmq::send_flags::sndmore | ::zmq::send_flags::dontwait);
				_isnew = false;
			}
			zmq::message_t datamsg(data, sz);
			_socket->send(datamsg, flags);
			if (bfinished) {
				_isnew = true;
			}
			return sz;
		} else if (webapp_socket_router == _socket_type
			|| webapp_socket_dealer == _socket_type) {
			if (0 == sz || !data) {
				zmq::message_t datamsg(0);
				_socket->send(datamsg, flags);
			} else {
				zmq::message_t datamsg(data, sz);
				_socket->send(datamsg, flags);
			}
			return sz;
		} else if (webapp_socket_pair == _socket_type) {
			zmq::message_t datamsg(data, sz);
			_socket->send(datamsg, flags);
		}
	} catch(zmq::error_t error) {
		fprintf(stderr, "webapp_socket_zmq::send E: %s\n", error.what());
		return -EINVALID;
	}
	return -ENOTALLOWED;
}

int webapp_socket_zmq::reply(void* data, size_t sz)
{
	if (webapp_socket_worker != _socket_type
		&& webapp_socket_dealer != _socket_type
		&& webapp_socket_router != _socket_type) {
		return -ENOTALLOWED;
	}
	if (!_zmqmsg_context) {
		fprintf(stderr, "zeromq message reply, but no msg context\n");
		return -ENOTALLOWED;
	}

	std::string basic_frame = _zmqmsg_context->body(0);
	auto* hdr_data = (char*)basic_frame.c_str();
	auto* hdr_uri = reinterpret_cast<basicinfo_frame_uri*>(hdr_data);
	auto* hdr_ct = reinterpret_cast<basicinfo_frame_content*>((uint8_t*)hdr_data
		+ sizeof(basicinfo_frame_uri) + hdr_uri->uri_length);
	if (msgptn_req_rep != hdr_ct->a.pattern) {
		fprintf(stderr, "zeromq message reply, context type is error\n");
		return -ELOGIC;
	}
	hdr_ct->a.type = msgtype_reply;
	try {
	_zmqmsg_context->body_set((const char*)hdr_data, basic_frame.length());
	_zmqmsg_context->append((char*)data, sz);
	_zmqmsg_context->send(*_socket);
	} catch(zmq::error_t error) {
		fprintf(stderr, "webapp_socket_zmq::reply E: %s\n", error.what());
		return -EINVALID;
	}
	return sz;
}

int webapp_socket_zmq::addref(void) {
	return __sync_add_and_fetch(&_refcnt, 1);
}

int webapp_socket_zmq::release(void)
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0 && need_msg_context()) {
		delete this;
	}
	return cnt;
}

webapp_socket_type webapp_socket_zmq::gettype(void)
{
	return _socket_type;
}

zmq::socket_t* webapp_socket_zmq::get_socket(void)
{
	return _socket;
}

bool webapp_socket_zmq::need_msg_context(void)
{
	if (webapp_socket_router == _socket_type
		|| webapp_socket_dealer == _socket_type
		|| webapp_socket_worker == _socket_type) {
		return true;
	}
	return false;
}

int webapp_socket_zmq::set_zmq_context(webapp_zeromq_msg* msg)
{
	if (_zmqmsg_context)
		delete _zmqmsg_context;
	_zmqmsg_context = msg;
	return 0;
}

zmq_backend_socket* webapp_socket_zmq::duplicate()
{
	auto* zsock = new webapp_socket_zmq(*this);
	return zsock;
}

bool webapp_socket_zmq::is_same_websocket(const char* uri,
	webapp_socket_type type)
{
	if (!_uri.compare(uri) && type == _socket_type) {
		return true;
	}
	return false;
}

worker_backend::worker_backend()
: _zmq_context(nullptr)
, _extern_backend(nullptr)
{
	_zmq_context = new zmq::context_t(1);
	listnode_init(_worker_list);
}

worker_backend::~worker_backend()
{
	release_all_worker();
	if (!_zmq_context) {
		delete _zmq_context;
		_zmq_context = nullptr;
	}
	_extern_backend = nullptr;
}

void* worker_backend::create_context(void)
{
	if (!_zmq_context) { return nullptr; }
	if (_worker_context) { return reinterpret_cast<void*>(_worker_context); }
	if (_extern_backend) {
		_worker_context = _extern_backend->create_backend_worker(_zmq_context);
	} else {
		_worker_context = new zmq_backend_worker(_zmq_context, 1000);
	}
	add_worker_lock(_worker_context);
	return reinterpret_cast<void*>(_worker_context);
}

void worker_backend::destory_context(void* context)
{
	if (_worker_context == context) {
		remove_worker_lock(_worker_context);
		delete _worker_context;
		_worker_context = nullptr;
	}
}

webapp_backend_callback* worker_backend::set_callback(
	void* context, 
	webapp_backend_callback *cb)
{
	if (!context || _worker_context != context || !cb) {
		return nullptr;
	}
	return _worker_context->set_callback(cb);
}

timermgr* worker_backend::get_timermgr(void* context)
{
	if (!context || context != _worker_context) {
		return nullptr;
	}
	return _worker_context->get_timermgr();
}

webapp_socket* worker_backend::connect(void* context, const char* uri,
	webapp_socket_type type)
{
	if (!context || context != _worker_context || !uri) {
		return nullptr;
	}
	// printf("connect %s\n", uri);
	return _worker_context->connect(uri, type);
}

webapp_socket* worker_backend::bind(void* context, const char* uri,
	webapp_socket_type type)
{
	if (!context || _worker_context != context || !uri) {
		return nullptr;
	}
	printf("bind %s\n", uri);
	return _worker_context->bind(uri, type);
}

int worker_backend::release_webapp_socket(void* context,
	webapp_socket *socket)
{
	if (!context || _worker_context != context || !socket) {
		return -EBADPARM;
	}
	return _worker_context->release_webapp_socket(socket);
}

int worker_backend::addfd(void* context, int fd, int action,
	webapp_fdcb cb, void* data)
{
	if (fd < 0) {
		return -EINVALID;
	}
	if (!context || _worker_context != context) {
		return -EBADPARM;
	}
	return _worker_context->addfd(fd, action, cb, data);
}

int worker_backend::removefd(void* context, int fd)
{
	if (fd < 0) {
		return -EINVALID;
	}
	if (!context || _worker_context != context) {
		return -EBADPARM;
	}
	return _worker_context->removefd(fd);
}

int worker_backend::dispatch(void* context)
{
	if (!context || _worker_context != context) {
		return -EBADPARM;
	}
	return _worker_context->dispatch();
}
int worker_backend::set_external_backend(webapp_zmq_backend* ext_backend)
{
	_extern_backend = ext_backend;
	return 0;
}

int worker_backend::release_all_worker(void)
{
	auto_mutex am(_mut);
	while (!listnode_isempty(_worker_list)) {
		auto* vb = list_entry(worker_item,	\
			ownerlist, _worker_list.next);
		listnode_del(vb->ownerlist);
		delete vb->wker;
		delete vb;
	}
	return 0;
}

int worker_backend::add_worker_lock(zmq_backend_worker* wker)
{
	if (!wker) {
		return -EBADPARM;
	}
	auto_mutex am(_mut);
	listnode_t *nd = _worker_list.next;
	for (; nd != &_worker_list; nd = nd->next) {
		auto* vb = list_entry(worker_item, ownerlist, nd);
		if (vb->wker == wker) return -EEXISTS;
	}
	auto* vb = new worker_item();
	vb->wker = wker;
	listnode_add(_worker_list, vb->ownerlist);
	return 0;
}

int worker_backend::remove_worker_lock(zmq_backend_worker* wker)
{
	if (!wker) {
		return -EBADPARM;
	}
	auto_mutex am(_mut);
	listnode_t *nd = _worker_list.next;
	for (; nd != &_worker_list; nd = nd->next) {
		auto* vb = list_entry(worker_item, ownerlist, nd);
		if (vb->wker == wker) {
			listnode_del(vb->ownerlist);
			delete vb;
			return 0;
		}
	}
	return -ENOTFOUND;
}

poller_items::poller_items()
: _items(WEB_SOCKET_MAX_COUNT_INIT)
, _websocks(WEB_SOCKET_MAX_COUNT_INIT)
, _sock_tree(nullptr)
{
	_freelist.clear();
}

poller_items::~poller_items()
{
	release_all();
}

websocket_node* poller_items::get_websock(int i, zmq::pollitem_t** item)
{
	int cnt = _websocks.getsize();
	if (i >= cnt) {
		return nullptr;
	}
	auto* pollitem = &_items.buffer()[i];
	if (pollitem->fd < 0 && !pollitem->socket) {
		return nullptr;
	}
	auto& websock = _websocks.buffer()[i];
	if (item) *item = pollitem;

	return &websock;
}

webapp_socket_zmq* poller_items::get_websock(const char* uri,
	webapp_socket_type type)
{
	int cnt = _websocks.getsize();
	for (int i = 0; i < cnt; i++) {
		auto& ws = _websocks.buffer()[i];
		if (ws.f.is_fd || !ws.sock) {
			continue;
		}
		if (ws.sock->is_same_websocket(uri, type)) {
			return ws.sock;
		}
	}
	return nullptr;
}

int websocket_avlnode::compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(websocket_avlnode, avlnode, a);
	auto* bb = AVLNODE_ENTRY(websocket_avlnode, avlnode, b);
	if (aa->key > bb->key) {
		return 1;
	} else if (aa->key < bb->key) {
		return -1;
	} else return 0;
}

int poller_items::additem(webapp_socket_zmq* sock,
	int fd, int events,
	webapp_fdcb fdcb, void* data)
{
	if (nullptr == sock && fd < 0) {
		return -1;
	}

	int ret = -4;
	zmq::pollitem_t* item = nullptr;
	websocket_node* wsn = nullptr;

	if (!_freelist.empty()) {
		ret = _freelist.back();
		_freelist.pop_back();
		assert(ret < _items.getsize());
		item = &_items.buffer()[ret];
		wsn = &_websocks.buffer()[ret];
	}
	else {
		// allocate a new one
		item = _items.new_object();
		wsn = _websocks.new_object();
		if (wsn) {
			// before doing anything else, we make
			// sure the avl_entry is allocated
			wsn->avl_entry = new websocket_avlnode;
			if (nullptr == wsn->avl_entry) {
				_websocks.remove(1);
				if (item) _items.remove(1);
				return -2;
			}
		}

		ret = _items.getsize();
		assert(ret == _websocks.getsize());
		--ret; // the last one
	}
	
//	printf("add fd: %u(%u), wsn: %lx, _websocks: %lx\n",
//		fd, _websocks.getsize(), wsn, _websocks.buffer());

	if (nullptr == item || nullptr == wsn) {
		return -3;
	}

	if (sock) {
		item->socket = (void*)*sock->get_socket();
		wsn->f.is_fd = 0;
		wsn->sock = sock;
	} else {
		item->socket = nullptr;
		wsn->f.is_fd = 1;
		wsn->fd = fd;
	}
	wsn->fdcb = fdcb;
	wsn->data = data;
	item->fd = fd;
	item->events = events;

	wsn->avl_entry->index = ret;
	if (0 == wsn->f.is_fd) {
		wsn->avl_entry->key = (size_t)wsn->sock;
	} else {
		wsn->avl_entry->key = (size_t)wsn->fd;
	}
	int val = avl_insert(&_sock_tree,
		&wsn->avl_entry->avlnode, websocket_avlnode::compare);
	assert(val == 0);
	return ret;
}

int poller_items::release(webapp_socket_zmq* sock)
{
	int id = get_websock_id(sock);
	if (id < 0) return id;

	release_byid(id);
	return id;
}

int poller_items::release(int fd)
{
	int id = get_websock_id(fd);
	if (id < 0) return id;

	release_byid(id);
	return id;
}

void poller_items::release_byid(int id)
{
	// clear and add item to freelist
	auto* wsn = &_websocks.buffer()[id];
	avl_remove(&_sock_tree, &wsn->avl_entry->avlnode);
	wsn->sock = nullptr;
	wsn->flags = 0;

	auto& item = _items.buffer()[id];
	item.socket = nullptr;
	item.fd = -1;
	item.events = 0;
	item.revents = 0;
	_freelist.push_back(id);
}

int poller_items::get_websock_id(webapp_socket_zmq* s)
{
	if (nullptr == s) {
		return -1;
	}

	auto* avl = avl_find(_sock_tree,
		MAKE_FIND_OBJECT(s, websocket_avlnode, key, avlnode),
		websocket_avlnode::compare);
	if (nullptr == avl) {
		return -2;
	}

	auto* node = AVLNODE_ENTRY(websocket_avlnode, avlnode, avl);
	return node->index;
}

int poller_items::get_websock_id(int fd)
{
	if (fd < 0) {
		return -1;
	}

	websocket_avlnode dummy;
	dummy.key = fd;

	avl_node_t* avl = avl_find(_sock_tree, &dummy.avlnode,
		websocket_avlnode::compare);
	if (nullptr == avl) {
		return -2;
	}

	auto* node = AVLNODE_ENTRY(websocket_avlnode, avlnode, avl);
	return node->index;
}

void poller_items::release_all(void)
{
	int i, cnt = _items.getsize();
	for (i = 0; i < cnt; i++) {
		auto& wsn = _websocks.buffer()[i];
		if (nullptr != wsn.sock) {
			delete wsn.sock;
		}
	}
	for (i = 0; i < cnt; ++i) {
		int fd = _items.buffer()[i].fd;
		if (fd >= 0) close(fd);
	}
	// release socket tree
	for (auto* node = avl_first(_sock_tree); node;) {
		avl_remove(&_sock_tree, node);
		auto* avl = AVLNODE_ENTRY(websocket_avlnode, avlnode, node);
		delete avl;
	}
}

bool poller_items::empty_slot(int slot)
{
	int cnt = _items.getsize();
	assert(cnt == _websocks.getsize());
	
	if (slot >= cnt) {
		return true;
	}

	zmq_pollitem_t& item = _items.buffer()[slot];
	if (!item.socket && item.fd < 0) {
		return true;
	}
	return false;
}

worker::worker(zmq::context_t* context, uint32_t timeout)
: _zmq_context(context)
, _cb(nullptr)
, _tmrmgr(nullptr)
, _force_exit(false)
, _timeout(timeout)
, _external_worker(nullptr)
, _client_count(0)
{
	int tmr_min_interval = WEB_TIMER_MIN_INTERVAL;
	tmr_min_interval = 
		get_sysconfig("webcore.timer-min-interval",
			tmr_min_interval);
	_tmrmgr = create_timermgr(tmr_min_interval);
	assert(nullptr != _tmrmgr);
	int ret =_items.additem(nullptr, _tmrmgr->getfd(),
		ZMQ_POLLIN, nullptr, nullptr);
	assert(ret >= 0);
}

worker::~worker()
{
	// release timer manager
	if (nullptr != _tmrmgr) {
		_tmrmgr->release();
		_tmrmgr = nullptr;
	}
	_external_worker = nullptr;
}


webapp_backend_callback* worker::set_callback(webapp_backend_callback *cb)
{
	auto* ret = _cb;
	_cb = cb;
	return ret;
}

webapp_backend_callback* worker::get_callback(void)
{
	return _cb;
}

timermgr* worker::get_timermgr(void)
{
	return _tmrmgr;
}

webapp_socket_zmq* worker::connect(const char* uri,
	webapp_socket_type type)
{
	webapp_socket_zmq* ret = nullptr;
	if (webapp_socket_router == type || webapp_socket_pub == type
		|| !uri || !*uri) {
		return ret;
	}

	ret = _items.get_websock(uri, type);
	if (ret) return ret;

	zmq::socket_t* zmq_socket = nullptr;
	if (webapp_socket_sub == type) {
		zmq_socket = new zmq::socket_t(*_zmq_context, ZMQ_SUB);
	} else if (webapp_socket_worker == type
		|| webapp_socket_client == type) {
		int linger = 0;
		int hwml = 0;
		zmq_socket = new zmq::socket_t(*_zmq_context, ZMQ_DEALER);
		std::string client_identity;
		int iret = 0;
		client_identity = 
		get_sysconfig("webcore.name", client_identity.c_str(), &iret);
		if (iret) {
			zas::utils::uuid tmpuid;
			tmpuid.generate();
			tmpuid.to_hex(client_identity);
		} else {
			client_identity += std::to_string(_client_count);
			_client_count++;
		}
		zmq_socket->set(zmq::sockopt::routing_id, client_identity.c_str());
		zmq_socket->set(zmq::sockopt::tcp_keepalive, 1);
		zmq_socket->set(zmq::sockopt::tcp_keepalive_cnt, 10);
		zmq_socket->set(zmq::sockopt::tcp_keepalive_idle, 120);
		zmq_socket->set(zmq::sockopt::tcp_keepalive_intvl, 1);
		zmq_socket->set(zmq::sockopt::sndhwm, 500);
		zmq_socket->set(zmq::sockopt::rcvhwm, 500);
		zmq_socket->set(zmq::sockopt::linger, 0);
	} else if (webapp_socket_pair == type) {
		zmq_socket = new zmq::socket_t(*_zmq_context, ZMQ_PAIR);
	} else {
		return nullptr;
	}
	std::string dev_ifname;
	int iret = 0;
	dev_ifname = get_sysconfig(ZMQ_CONNECT_DEVICE_IF_PAHT,
		dev_ifname.c_str(), &iret);
	if (!iret && dev_ifname.length() > 0) {
		printf("set zmq bind device %s\n", dev_ifname.c_str());
		zmq_socket->set(zmq::sockopt::bindtodevice, dev_ifname.c_str());
	}
	try {
		zmq_socket->connect(uri);
	} catch (zmq::error_t error) {
		fprintf(stderr, "connect E: %s\n", error.what());
		return nullptr;
	}
	auto* wa_socket = new webapp_socket_zmq(zmq_socket, uri, type);
	assert(nullptr != wa_socket);

	int id = _items.additem(wa_socket, -1, ZMQ_POLLIN, nullptr, nullptr);
	assert(id >= 0);
	return wa_socket;
}

webapp_socket_zmq* worker::bind(const char* uri,
	webapp_socket_type type)
{
	webapp_socket_zmq* ret = nullptr;
	if (webapp_socket_sub == type || webapp_socket_client == type
		|| webapp_socket_worker == type || !uri || !*uri) {
		return nullptr;
	}

	ret = _items.get_websock(uri, type);
	if (ret) return ret;

	zmq::socket_t* zmq_socket = nullptr;
	if (webapp_socket_router == type) {
		int linger = 0;
		int hwml = 0;
		zmq_socket = new zmq::socket_t(*_zmq_context, ZMQ_ROUTER);
		zmq_socket->set(zmq::sockopt::tcp_keepalive, 1);
		zmq_socket->set(zmq::sockopt::tcp_keepalive_cnt, 10);
		zmq_socket->set(zmq::sockopt::tcp_keepalive_idle, 120);
		zmq_socket->set(zmq::sockopt::tcp_keepalive_intvl, 1);
		zmq_socket->set(zmq::sockopt::sndhwm, 500);
		zmq_socket->set(zmq::sockopt::rcvhwm, 500);
		zmq_socket->set(zmq::sockopt::linger, 0);
	} else if (webapp_socket_dealer == type) {
		int linger = 0;
		int hwml = 0;
		zmq_socket = new zmq::socket_t(*_zmq_context, ZMQ_DEALER);
		zmq_socket->set(zmq::sockopt::tcp_keepalive, 1);
		zmq_socket->set(zmq::sockopt::tcp_keepalive_cnt, 10);
		zmq_socket->set(zmq::sockopt::tcp_keepalive_idle, 120);
		zmq_socket->set(zmq::sockopt::tcp_keepalive_intvl, 1);
		zmq_socket->set(zmq::sockopt::sndhwm, 500);
		zmq_socket->set(zmq::sockopt::rcvhwm, 500);
		zmq_socket->set(zmq::sockopt::linger, 0);
	} else if (webapp_socket_pub == type) {
		zmq_socket = new zmq::socket_t(*_zmq_context, ZMQ_PUB);
	} else if (webapp_socket_pair == type) {
		zmq_socket = new zmq::socket_t(*_zmq_context, ZMQ_PAIR);
	} else {
		return nullptr;
	}
	try {
		zmq_socket->bind(uri);
	} catch (zmq::error_t error) {
		fprintf(stderr, "bind E: %s\n", error.what());
		return nullptr;
	}

	auto* wa_socket = new webapp_socket_zmq(zmq_socket, uri, type);
	int id = _items.additem(wa_socket, -1, ZMQ_POLLIN, nullptr, nullptr);
	assert(id >= 0);
	return wa_socket;
}

int worker::release_webapp_socket(webapp_socket* socket)
{
	auto* s = zas_downcast(webapp_socket, webapp_socket_zmq, socket);
	return _items.release(s);
}

poller_items* worker::get_poller_items(void)
{
	return &_items;
}

webapp_socket_zmq* worker::get_socket_zmq(const char* uri,
	webapp_socket_type type)
{
	return _items.get_websock(uri, type);
}

int worker::set_external_worker(zmq_backend_worker* external_worker)
{
	_external_worker = external_worker;
	return 0;
}

int worker::handle_timer(int tfd)
{
	// drain the eventfd
	uint64_t val = 0;
	int fd = _tmrmgr->getfd();
	if (tfd != fd) {
		return -ENOTALLOWED;
	}
	_test_timer_count++;
	if (_test_timer_count > 100) {
		printf("zmq worker handle_timer 100 times\n");
		_test_timer_count = 0;
	}
	for (;;) {
		int ret = read(fd, &val, sizeof(uint64_t));
		if (ret <= 0 && errno == EINTR)
			continue;
		assert(ret == 8);
		break;
	}
	_tmrmgr->periodic_runner();
	return 0;
}

int worker::handle_zmq_msg(webapp_socket_zmq* wa_sock)
{
	if (!_cb) {
		fprintf(stderr, "E: worker: no callback");
		return -ENOTAVAIL;
	}

	// recv all message;
	zeromq_msg *msg = new zeromq_msg(*(wa_sock->get_socket()));
	if (nullptr == msg) {
		return -ENOMEMORY;
	}

	size_t bodycnt = msg->body_parts();
	std::string recvdata;
	recvdata.clear();
	for (int i = 0; i < bodycnt; i++) {
		recvdata.append(msg->body(i).c_str(), msg->body(i).length());
	}
	if (wa_sock->need_msg_context()) {
		auto* recwebsocket = new webapp_socket_zmq(*wa_sock);
		recwebsocket->set_zmq_context(msg);
		_cb->on_recv((void*)this, recwebsocket,(void*)recvdata.c_str(),
			recvdata.length());
		recwebsocket->release();
	} else {
		_cb->on_recv((void*)this, wa_sock, (void*)recvdata.c_str(),
			recvdata.length());
		delete msg;
	}
	return 0;
}

int worker::addfd(int fd, int action, webapp_fdcb cb, void* data)
{
	int events = 0;
	if (action & BACKEND_POLLIN) {
		events |= ZMQ_POLLIN;
	}
	else if (action & BACKEND_POLLOUT) {
		events |= ZMQ_POLLOUT;
	}
	return _items.additem(nullptr, fd, events, cb, data);
}

int worker::removefd(int fd)
{
	return _items.release(fd);
}

int worker::convert_revents(int revents)
{
	int ret = 0;
	if (revents & ZMQ_POLLIN) {
		ret |= BACKEND_POLLIN;
	}
	if (revents & ZMQ_POLLOUT) {
		ret |= BACKEND_POLLOUT;
	}
	if (revents & ZMQ_POLLERR) {
		ret |= BACKEND_POLLERR;
	}
	if (revents & ZMQ_POLLPRI) {
		ret |= BACKEND_POLLPRI;
	}
	return ret;
}

int worker::dispatch(void)
{
	while (!_force_exit) {
		int cnt = _items.count();
		try{
			int ret = zmq::poll(_items.get_pollset(), cnt,
				std::chrono::milliseconds(_timeout));
			if (ret < 0) {
				return -EINVALID; 
			}
			else if (ret == 0) continue;
		} catch(zmq::error_t error) {
			fprintf(stderr, "dispatch E: %s\n", error.what());
			return -EINVALID;
		}


		for (int i = 0; i < cnt; ++i) {
			zmq::pollitem_t* item;
			auto* wsn = _items.get_websock(i, &item);
			if (nullptr == wsn) {
				// this is an empty poll item
				continue;
			}

			if (item->fd >= 0) {
				if (item->revents & ZMQ_POLLIN) {
					if (!handle_timer(item->fd)) {
						continue;
					}
					// check external callback
					if (wsn->fdcb) {
						wsn->fdcb(wsn->fd, convert_revents(item->revents), wsn->data);
					}
				}
			}
			else if (item->revents & ZMQ_POLLIN) {
				assert(nullptr != wsn->sock);
				if (!_external_worker) {
					if (handle_zmq_msg(wsn->sock)) {
						return -EINVALID;
					}
				} else {
					_external_worker->handle_zmq_msg(wsn->sock);
				}
			}
		}
	}
	return 0;
}

webapp_zmq_backend::webapp_zmq_backend()
: _data(nullptr)
{
	auto* backend = new worker_backend();
	_data = reinterpret_cast<void*>(backend);
	backend->set_external_backend(this);
}

webapp_zmq_backend::~webapp_zmq_backend()
{
	if (!_data) {
		auto* backend = reinterpret_cast<worker_backend*>(_data);
		delete backend;
		_data = nullptr;
	}
}
void* webapp_zmq_backend::create_context(void)
{
	auto* backend = reinterpret_cast<worker_backend*>(_data);
	if (!backend) {
		return nullptr;
	}
	return backend->create_context();
}

zmq_backend_worker*
webapp_zmq_backend::create_backend_worker(zmq::context_t* zmq_context)
{
	return new zmq_backend_worker(zmq_context, 1000);
}

void webapp_zmq_backend::destory_context(void* context)
{
	auto* backend = reinterpret_cast<worker_backend*>(_data);
	if (!backend) {
		return ;
	}
	backend->destory_context(context);	
}

webapp_backend_callback* webapp_zmq_backend::set_callback(
	void* context,
	webapp_backend_callback *cb)
{
	auto* backend = reinterpret_cast<worker_backend*>(_data);
	if (!backend) {
		return nullptr;
	}
	return backend->set_callback(context, cb);
}

timermgr* webapp_zmq_backend::get_timermgr(void* context)
{
	auto* backend = reinterpret_cast<worker_backend*>(_data);
	if (!backend) {
		return nullptr;
	}
	return backend->get_timermgr(context);
}

webapp_socket* webapp_zmq_backend::connect(void* context,
	const char* uri,
	webapp_socket_type type)
{
	auto* backend = reinterpret_cast<worker_backend*>(_data);
	if (!backend) {
		return nullptr;
	}
	return backend->connect(context, uri, type);
}

webapp_socket* webapp_zmq_backend::bind(void* context,
	const char* uri,
	webapp_socket_type type)
{
	auto* backend = reinterpret_cast<worker_backend*>(_data);
	if (!backend) {
		return nullptr;
	}
	return backend->bind(context, uri, type);
}

int webapp_zmq_backend::release_webapp_socket(void* context,
	webapp_socket* socket)
{
	auto* backend = reinterpret_cast<worker_backend*>(_data);
	if (!backend) {
		return -ENOTAVAIL;
	}
	return backend->release_webapp_socket(context, socket);
}

int webapp_zmq_backend::addfd(void* context, int fd,
	int action, webapp_fdcb cb, void* data)
{
	auto* backend = reinterpret_cast<worker_backend*>(_data);
	if (!backend) {
		return -ENOTAVAIL;
	}
	return backend->addfd(context, fd, action, cb, data);
}

int webapp_zmq_backend::removefd(void* context, int fd)
{
	auto* backend = reinterpret_cast<worker_backend*>(_data);
	if (!backend) {
		return -ENOTAVAIL;
	}
	return backend->removefd(context, fd);
}

int webapp_zmq_backend::dispatch(void* context)
{
	auto* backend = reinterpret_cast<worker_backend*>(_data);
	if (!backend) {
		return -ENOTAVAIL;
	}
	return backend->dispatch(context);
}

zmq_backend_worker::zmq_backend_worker(zmq::context_t* context,
	uint32_t timeout)
: _data(nullptr)
{
	auto* wker = new worker(context, timeout);
	assert(nullptr != wker);
	_data = reinterpret_cast<void*>(wker);
	wker->set_external_worker(this);
}

zmq_backend_worker::~zmq_backend_worker()
{
	if (!_data) {
		auto* wker = reinterpret_cast<worker*>(_data);
		delete wker;
		_data = nullptr;
	}
}

timermgr* zmq_backend_worker::get_timermgr(void)
{
	if (!_data) return nullptr;
	auto* wker = reinterpret_cast<worker*>(_data);
	return wker->get_timermgr();
}

webapp_backend_callback*
zmq_backend_worker::set_callback(webapp_backend_callback *cb)
{
	if (!_data) return nullptr;
	auto* wker = reinterpret_cast<worker*>(_data);
	return wker->set_callback(cb);
}

zmq_backend_socket* zmq_backend_worker::connect(const char* uri,
	webapp_socket_type type)
{
	if (!_data) return nullptr;
	auto* wker = reinterpret_cast<worker*>(_data);
	return wker->connect(uri, type);
}

zmq_backend_socket* zmq_backend_worker::bind(const char* uri,
	webapp_socket_type type)
{
	if (!_data) return nullptr;
	auto* wker = reinterpret_cast<worker*>(_data);
	return wker->bind(uri, type);
}

int zmq_backend_worker::release_webapp_socket(webapp_socket* socket)
{
	if (!_data) return -ENOTAVAIL;
	auto* wker = reinterpret_cast<worker*>(_data);
	return wker->release_webapp_socket(socket);	
}

int zmq_backend_worker::addfd(int fd, int action,
	webapp_fdcb cb, void* data)
{
	if (!_data) return -ENOTAVAIL;
	auto* wker = reinterpret_cast<worker*>(_data);
	return wker->addfd(fd, action, cb, data);	
}

int zmq_backend_worker::removefd(int fd)
{
	if (!_data) return -ENOTAVAIL;
	auto* wker = reinterpret_cast<worker*>(_data);
	return wker->removefd(fd);	
}

int zmq_backend_worker::dispatch(void)
{
	if (!_data) return -ENOTAVAIL;
	auto* wker = reinterpret_cast<worker*>(_data);
	return wker->dispatch();	
}

zmq_backend_socket* zmq_backend_worker::get_socket_zmq(const char* uri,
	webapp_socket_type type)
{
	if (!_data) return nullptr;

	auto* wker = reinterpret_cast<worker*>(_data);

	if (webapp_socket_sub == type || webapp_socket_client == type
		|| webapp_socket_worker == type) {
		return wker->connect(uri, type);
	} else {
		return wker->bind(uri, type);
	}	
	return nullptr;
}

int zmq_backend_worker::handle_zmq_msg(zmq_backend_socket* zmq_sock)
{
	if (!_data || !zmq_sock) return -EBADPARM;
	auto* wker = reinterpret_cast<worker*>(_data);
	auto* cb = wker->get_callback();

	if (!cb) {
		fprintf(stderr, "E: zmq_backend_worker: no callback");
		return -ENOTAVAIL;
	}

	auto* wa_sock = zas_downcast(zmq_backend_socket,	\
		webapp_socket_zmq, zmq_sock);

	// recv all message;
	zeromq_msg *msg = new zeromq_msg(*(wa_sock->get_socket()));
	if (nullptr == msg) {
		return -ENOMEMORY;
	}

	size_t bodycnt = msg->body_parts();
	std::string recvdata;
	recvdata.clear();
	for (int i = 0; i < bodycnt; i++) {
		recvdata.append(msg->body(i).c_str(), msg->body(i).length());
	}
	if (wa_sock->need_msg_context()) {
		auto* recwebsocket = new webapp_socket_zmq(*wa_sock);
		recwebsocket->set_zmq_context(msg);
		cb->on_recv((void*)this, recwebsocket,(void*)recvdata.c_str(),
			recvdata.length());
		recwebsocket->release();
	} else {
		cb->on_recv((void*)this, wa_sock, (void*)recvdata.c_str(),
			recvdata.length());
		delete msg;
	}
	return 0;
}

zmq_backend_socket::zmq_backend_socket()
{
}

zmq_backend_socket::~zmq_backend_socket()
{
}

}} // end of namespace zas::webcore
/* EOF */
