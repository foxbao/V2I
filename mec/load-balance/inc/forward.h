
#ifndef __CXX_BROKDER_H__
#define __CXX_BROKDER_H__

#include "load-balance-def.h"
#include "webcore/webapp-backend-zmq.h"
#include "server-endpoint-mgr.h"

#include <string.h>

#include "std/list.h"
#include "utils/avltree.h"
#include "utils/mutex.h"
#include "utils/uri.h"

namespace zas {
namespace load_balance {

using namespace zas::webcore;
using namespace zas::utils;
struct forward_service_node;

class forward_mgr_callback
{
public:
	// msgdata: forward server node;
	// URL: client send url
	// int : [return value], < 0: error; >= 0: service type
	virtual int forward_to_service(webapp_zeromq_msg* data, uri &url) = 0;
	virtual int service_request(zmq_backend_socket* wa_sock, 
		webapp_zeromq_msg* data) = 0;
};

// internal communication function
class forward : public zmq_backend_worker
{
public:
	forward(zmq::context_t* context, uint32_t timeout);
	virtual ~forward();
	webapp_backend_callback* set_callback(webapp_backend_callback *cb);
	int handle_zmq_msg(zmq_backend_socket* wa_sock);

public:
	int set_forward_info(const char* cli_uri, webapp_socket_type cli_type,
		const char* svc_uri, webapp_socket_type svc_type);
	int set_distribute_info(const char* durl, webapp_socket_type dtype);
	int set_forward_callback(forward_mgr_callback* cb);
	int forward_to_service(webapp_zeromq_msg* data, server_endpoint_node node);
	int send_to_service(zmq_backend_socket* wa_sock, webapp_zeromq_msg* data);

private:
	int transfer_to_server(webapp_zeromq_msg* data);
	int distribute_msg(webapp_zeromq_msg* data);
	int transfer_to_client(zmq_backend_socket* wa_sock,
		webapp_zeromq_msg* data);

private:
	zmq::context_t*		_zmq_context;
	int				_force_exit;
	uint32_t		_timeout;
	webapp_backend_callback* _cb;

	zmq_backend_socket*	_client;
	zmq_backend_socket*	_fowardclient;	
	zmq_backend_socket*	_distribute;;	
	forward_mgr_callback*	_forward_mgr;
};

class forward_backend : public webapp_backend
{
public:
	forward_backend();
	virtual ~forward_backend();
	static forward_backend* inst();
public:
	// create msg communication context
	void* create_context();
	void destory_context(void* context);
	webapp_backend_callback* set_callback(void* context,
		webapp_backend_callback *cb);
	timermgr* get_timermgr(void* context);
	webapp_socket* connect(void* context, const char* uri,
		webapp_socket_type type);
	webapp_socket* bind(void* context, const char* uri,
		webapp_socket_type type);
	int release_webapp_socket(void* context, webapp_socket* socket);
	int dispatch(void* context);
	int addfd(void* context, int fd, int action, webapp_fdcb cb, void* data);
	int removefd(void* context, int fd);

public:	
	int set_forward_info(const char* cli_uri, webapp_socket_type cli_type,
		const char* svc_uri, webapp_socket_type svc_type);
	int set_distribute_info(const char* durl, webapp_socket_type dtype);
	int set_forward_callback(forward_mgr_callback* cb);
	int forward_to_service(webapp_zeromq_msg* data, server_endpoint_node node);
	int send_to_service(zmq_backend_socket* wa_sock, webapp_zeromq_msg* data);

private:
	int add_forward_lock(forward* forward);
	int remove_forward_lock(forward* forward);
	int release_all_forward(void);

private:
	zmq::context_t*		_zmq_context;
	listnode_t 			_forward_list;
};

}}

#endif /* __CXX_BROKDER_H__*/