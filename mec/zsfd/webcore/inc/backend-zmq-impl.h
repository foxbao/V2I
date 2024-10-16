#ifndef __CXX_ZAS_WEBAPP_BACKEND_ZMQ_IMPL_H__
#define __CXX_ZAS_WEBAPP_BACKEND_ZMQ_IMPL_H__

#include <zmq.hpp>

#include "std/list.h"
#include "utils/mutex.h"
#include "utils/avltree.h"

// todo: re-arrange the following header
#include "./../hwgrph/inc/gbuf.h"

#include "webcore/webapp-backend-zmq.h"
#include "zeromq-msg.h"
#include <vector>

namespace zas {
namespace webcore {

using namespace zas::utils;

class webapp_socket_zmq : public zmq_backend_socket
{
public:
	webapp_socket_zmq(zmq::socket_t* socket,
		const char* uri,
		webapp_socket_type type);
	webapp_socket_zmq(webapp_socket_zmq& zmq);
	virtual ~webapp_socket_zmq();
	int send(void* data, size_t sz, bool bfinished = true);
	int reply(void* data, size_t sz);
	int addref(void);
	int release(void);
	webapp_socket_type gettype(void);

public:
	zmq::socket_t* get_socket(void);
	bool need_msg_context(void);
	zmq_backend_socket* duplicate(void);
	int set_zmq_context(webapp_zeromq_msg* msg);
	bool is_same_websocket(const char* uri, webapp_socket_type type);

private:
	zmq::socket_t* 	_socket;
	webapp_socket_type	_socket_type;
	webapp_zeromq_msg* 	_zmqmsg_context;
	std::string 	_uri;
	int 			_refcnt;
	bool		 	_isnew;
};

typedef zas::hwgrph::gbuf_<zmq::pollitem_t> zmq_pollitem_set;

struct websocket_avlnode
{
	// todo: remove the node in dtor
	avl_node_t avlnode;
	size_t key;
	int index;
	static int compare(avl_node_t*, avl_node_t*);
};

struct websocket_node
{
	websocket_avlnode* avl_entry;
	union {
		int fd;
		webapp_socket_zmq* sock;
	};
	union {
		uint32_t flags;
		struct {
			uint32_t is_fd : 1;
		} f;
	};
	webapp_fdcb fdcb;
	void* data;
};

typedef zas::hwgrph::gbuf_<websocket_node> websocket_set;

class poller_items
{
public:
	poller_items();
	~poller_items();

	websocket_node* get_websock(int i, zmq::pollitem_t** item);
	webapp_socket_zmq* get_websock(const char* uri,
		webapp_socket_type type);
	
	// add a new sock or fd item
	int additem(webapp_socket_zmq* sock, int fd,
		int events, webapp_fdcb fdcb, void* data);

	// release a sock
	int release(webapp_socket_zmq* sock);

	// release a fd
	int release(int fd);

public:

	int count(void) {
		return _items.getsize();
	}

	zmq::pollitem_t* get_pollset(void) {
		return _items.buffer();
	}

private:
	void release_all(void);
	bool empty_slot(int slot);
	int get_websock_id(webapp_socket_zmq* sock);
	int get_websock_id(int fd);

	void release_byid(int id);

private:
	std::vector<short> _freelist;

	// all poll items
	zmq_pollitem_set _items;
	websocket_set _websocks;

	// all webapp socket will be
	// added to this tree
	avl_node_t* _sock_tree;
};

// internal communication function
class worker
{
public:
	worker(zmq::context_t* context, uint32_t timeout);
	virtual ~worker();
	webapp_backend_callback* set_callback(webapp_backend_callback *cb);
	webapp_backend_callback* get_callback(void);
	timermgr* get_timermgr(void);
	webapp_socket_zmq* connect(const char* uri,
		webapp_socket_type type);
	webapp_socket_zmq* bind(const char* uri,
		webapp_socket_type type);
	int release_webapp_socket(webapp_socket* socket);

	// pure fd operations
	int addfd(int fd, int action, webapp_fdcb cb, void* data);
	int removefd(int fd);

	int dispatch();
	int set_external_worker(zmq_backend_worker* external_woker);
	
private:
	poller_items* get_poller_items(void);	
	webapp_socket_zmq* get_socket_zmq(const char* uri, webapp_socket_type type);
	int convert_revents(int revents);
	int handle_timer(int fd);
	int handle_zmq_msg(webapp_socket_zmq* wa_sock);

private:
	zmq::context_t*	_zmq_context;
	poller_items	_items;
	webapp_backend_callback* _cb;
	timermgr*		_tmrmgr;
	int				_force_exit;
	uint32_t		_timeout;
	zmq_backend_worker*		_external_worker;
	uint32_t		_client_count;
};

class worker_backend
{
public:
	worker_backend();
	virtual ~worker_backend();
	// create msg communication context
	void* create_context(void);
	void destory_context(void* context);

	webapp_backend_callback* set_callback(
		void* context, webapp_backend_callback *cb);

	timermgr* get_timermgr(void* context);
	webapp_socket* connect(void* context, const char* uri,
		webapp_socket_type type);
	webapp_socket* bind(void* context, const char* uri,
		webapp_socket_type type);
	int release_webapp_socket(void* context, webapp_socket* socket);

	// pure fd operations
	int addfd(void* context, int fd, int action,
		webapp_fdcb cb, void* data);
	int removefd(void* context, int fd);
	int dispatch(void* context);

	int set_external_backend(webapp_zmq_backend* ext_backend);

private:
	int add_worker_lock(zmq_backend_worker* worker);
	int remove_worker_lock(zmq_backend_worker* worker);
	int release_all_worker(void);

private:
	zmq::context_t*		_zmq_context;
	listnode_t 			_worker_list;
	mutex				_mut;
	webapp_zmq_backend* _extern_backend;
};

}};	// namespace zas::webcore
#endif