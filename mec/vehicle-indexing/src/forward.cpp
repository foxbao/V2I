
#include "forward.h"
#include "forward_arbitrate.h"
#include "unistd.h"
#include "utils/uri.h"
#include "webcore/logger.h"
#include "webcore/sysconfig.h"
#include "webcore/webapp.h"

namespace zas {
namespace vehicle_indexing {

using namespace zas::webcore;
using namespace zas::utils;

__thread forward* _forward_context = nullptr;
static mutex forwardmut;

struct forward_item {
	listnode_t ownerlist;
	forward* bker;
};

static int s_interrupted = 0;

inline static void s_signal_handler (int signal_value)
{
	s_interrupted = 1;
}

inline static void s_catch_signals ()
{
	struct sigaction action;
	action.sa_handler = s_signal_handler;
	action.sa_flags = 0;
	sigemptyset (&action.sa_mask);
	sigaction (SIGINT, &action, NULL);
	sigaction (SIGTERM, &action, NULL);
}

forward_backend* forward_backend::inst()
{
	static forward_backend* _inst = NULL;
	if (_inst) return _inst;
	auto_mutex am(forwardmut);
	if (_inst) return _inst;
	_inst = new forward_backend();
	assert(NULL != _inst);
	return _inst;
}

forward_backend::forward_backend()
: _zmq_context(nullptr)
{
	_zmq_context = new zmq::context_t(1);
	listnode_init(_forward_list);
	s_catch_signals();
}

forward_backend::~forward_backend()
{
	release_all_forward();
	if (!_zmq_context) {
		delete _zmq_context;
		_zmq_context = nullptr;
	}
}

void* forward_backend::create_context()
{
	if (!_zmq_context) {
		return nullptr;
	}
	if (_forward_context) {
		return reinterpret_cast<void*>(_forward_context);
	}
	_forward_context = new forward(_zmq_context, 1000);
	assert(nullptr != _forward_context);
	add_forward_lock(_forward_context);
	return reinterpret_cast<void*>(_forward_context);
}

void forward_backend::destory_context(void* context)
{
	if (_forward_context == context) {
		remove_forward_lock(_forward_context);
		delete _forward_context;
		_forward_context = nullptr;
	}
}

webapp_backend_callback* forward_backend::set_callback(void* context, 
	webapp_backend_callback *cb)
{
	if (!context || _forward_context != context || !cb) {
		return nullptr;
	}
	return _forward_context->set_callback(cb);
}

timermgr* forward_backend::get_timermgr(void* context)
{
	if (!context || context != _forward_context) {
		return nullptr;
	}
	return _forward_context->get_timermgr();
}

webapp_socket* forward_backend::connect(void* context, const char* uri,
	webapp_socket_type type)
{
	if (!context || _forward_context != context || !uri) {
		return nullptr;
	}
	return _forward_context->connect(uri, type);
}

webapp_socket* forward_backend::bind(void* context, const char* uri,
	webapp_socket_type type)
{
	if (!context || _forward_context != context || !uri) {
		return nullptr;
	}
	printf("binding service %s, type %d\n", uri, type);
	return _forward_context->bind(uri, type);
}

int forward_backend::release_webapp_socket(void* context,
	webapp_socket *socket)
{
	if (!context || _forward_context != context || !socket) {
		return -EBADPARM;
	}
	return _forward_context->release_webapp_socket(socket);
}

int forward_backend::dispatch(void* context)
{
	if (!context || _forward_context != context) {
		return -EBADPARM;
	}
	return _forward_context->dispatch();
}

int forward_backend::addfd(void* context,
	int fd, int action, webapp_fdcb cb, void* data)
{
	if (!context || _forward_context != context) {
		return -EBADPARM;
	}
	return _forward_context->addfd(fd, action, cb, data);
}

int forward_backend::removefd(void* context, int fd)
{
	if (!context || _forward_context != context) {
		return -EBADPARM;
	}
	return _forward_context->removefd(fd);
}

int forward_backend::set_forward_info(const char* cli_uri,
	webapp_socket_type cli_type,
	const char* svc_uri,
	webapp_socket_type svc_type)
{
	if (!_forward_context) {
		return -EBADPARM;
	}
	return _forward_context->set_forward_info(cli_uri,
		cli_type, svc_uri, svc_type);
}

int forward_backend::set_forward_callback(forward_mgr_callback* cb)
{
	if (!_forward_context) {
		return -EBADPARM;
	}
	return _forward_context->set_forward_callback(cb);
}

int forward_backend::forward_to_service(webapp_zeromq_msg* data,
	forward_service_node* node)
{
	if (!_forward_context) {
		return -EBADPARM;
	}
	return _forward_context->forward_to_service(data, node);
}

int forward_backend::send_to_service(zmq_backend_socket* wa_sock,
	webapp_zeromq_msg* data)
{
	if (!_forward_context) {
		return -EBADPARM;
	}
	return _forward_context->send_to_service(wa_sock, data);
}

int forward_backend::release_all_forward(void)
{
	auto_mutex am(forward_backend);
	while (!listnode_isempty(_forward_list)) {
		auto* vb = list_entry(forward_item,	\
			ownerlist, _forward_list.next);
		listnode_del(vb->ownerlist);
		delete vb->bker;
		delete vb;
	}
	return 0;
}

int forward_backend::add_forward_lock(forward* bker)
{
	if (!bker) {
		return -EBADPARM;
	}
	auto_mutex am(forwardmut);
	listnode_t *nd = _forward_list.next;
	for (; nd != &_forward_list; nd = nd->next) {
		auto* vb = list_entry(forward_item, ownerlist, nd);
		if (vb->bker == bker) {
			return -EEXISTS;
		}
	}
	auto* vb = new forward_item();
	assert(nullptr != vb);
	vb->bker = bker;
	listnode_add(_forward_list, vb->ownerlist);
	return 0;
}

int forward_backend::remove_forward_lock(forward* bker)
{
	if (!bker) return -EBADPARM;
	auto_mutex am(forwardmut);
	listnode_t *nd = _forward_list.next;
	for (; nd != &_forward_list; nd = nd->next) {
		auto* vb = list_entry(forward_item, ownerlist, nd);
		if (vb->bker == bker) {
			listnode_del(vb->ownerlist);
			delete vb;
			return 0;
		}
	}
	return -ENOTFOUND;
}

forward::forward(zmq::context_t* context, uint32_t timeout)
: zmq_backend_worker(context, timeout)
, _zmq_context(context)
, _force_exit(false)
, _cb(nullptr)
, _timeout(timeout)
, _client(nullptr)
, _snapshot(nullptr)
, _forward_mgr(nullptr)
{
}

forward::~forward()
{
	_client = nullptr;
	_snapshot = nullptr;
}

webapp_backend_callback* forward::set_callback(webapp_backend_callback *cb)
{
	auto* ret = _cb;
	_cb = cb;
	return ret;
}

int forward::handle_zmq_msg(zmq_backend_socket* wa_sock)
{
	if (!_cb) {
		log.e(INDEXING_FORWRD_TAG,
			"handle message no callback");
		return -ENOTAVAIL;
	}
	
	if (!wa_sock) {
		log.e(INDEXING_FORWRD_TAG,
			"handle message socket error is nullptr.");
		return -EBADPARM;
	}
	int ret = -1;
	auto *msg = create_webapp_zeromq_msg(*wa_sock->get_socket());
	assert(nullptr != msg);
	// msg->dump();
	if (_forward_mgr) {
		// handle register or heartbeat
		// heartbeat not impl
		if (wa_sock == _client) {
			ret = handle_worker_info(msg);
		}
		// handle msg forward
		if (wa_sock == _client && ret) {
			ret = transfer_to_server(msg);
		} else if (wa_sock == _snapshot) {
			ret = transfer_to_client(wa_sock, msg);
		} else {
		}
	}

	if (ret) {
		size_t bodycnt = msg->body_parts();
		std::string recvdata;
		for (int i = 0; i < bodycnt; i++) {
			recvdata += msg->body(i);
		}
		if (wa_sock->need_msg_context()) {
			auto* zsock = wa_sock->duplicate();
			zsock->set_zmq_context(msg);
			_cb->on_recv(reinterpret_cast<void*>(this), zsock,
				(void*)(recvdata.c_str()), recvdata.length());
			zsock->release();
		} else {
			_cb->on_recv(reinterpret_cast<void*>(this), wa_sock,
				(void*)(recvdata.c_str()), recvdata.length());
			release_webapp_zeromq_msg(msg);
		}
	} else {
		release_webapp_zeromq_msg(msg);
	}
	return 0;
}

int forward::set_forward_info(const char* cli_uri, webapp_socket_type cli_type,
	const char* svc_uri, webapp_socket_type svc_type)
{
	if (!cli_uri || !*cli_uri || !svc_uri || !svc_uri) {
		return -EBADPARM;
	}
	_client = get_socket_zmq(cli_uri, cli_type);
	assert(nullptr != _client);
	_snapshot = get_socket_zmq(svc_uri, svc_type);
	assert(nullptr != _snapshot);
	init_client();
	return 0;
}

int forward::set_forward_callback(forward_mgr_callback* cb)
{
	_forward_mgr = cb;
	return 0;
}

int forward::transfer_to_server(webapp_zeromq_msg* data)
{
	if (!_snapshot || !_forward_mgr) {
		return -ENOTAVAIL;
	}
	//add master uri
	assert(nullptr != data);
	std::string addr = data->body(0);
	if (addr.length() < 1) {
		log.e(INDEXING_FORWRD_TAG,
			"vehicle indexing recv error message\n");
		return -EBADPARM;
	}
	auto* frame_uri = (basicinfo_frame_uri*)(addr.c_str());
	std::string add_uri(frame_uri->uri, frame_uri->uri_length);
	uri addr_info(add_uri);

	return _forward_mgr->forward_to_service(data, addr_info);
}

int forward::send_to_service(zmq_backend_socket* wa_sock,
	webapp_zeromq_msg* data)
{
	assert(nullptr != wa_sock);
	assert(nullptr != data);
	data->send(*wa_sock->get_socket());
	return 0;
}

int forward::forward_to_service(webapp_zeromq_msg* data,
	forward_service_node* node)
{
	if (!data || !node) {
		return -EBADPARM;
	}
	assert(nullptr != _snapshot);
	// data->dump();
	auto* msg = reinterpret_cast<webapp_zeromq_msg*>(data);
	// set master service name
	_snapshot->send((char*)(node->master.identity.c_str()),
		node->master.identity.length(), false);
	// send master type
	_snapshot->send(snapshot_type[0], strlen(snapshot_type[0]), false);
	// send all data
	msg->send(*_snapshot->get_socket());
	// msg->sendparts(*_snapshot->get_socket(), 0, msg->parts(), true);

	if (node->slaver.endpoint.get() > 0) {
		//set slave service name
		_snapshot->send((char*)(node->slaver.identity.c_str()),
			node->slaver.identity.length(), false);
		//set slave service type;
		// send master type
		_snapshot->send(snapshot_type[1], strlen(snapshot_type[1]), false);
		// send all data
		msg->sendparts(*_snapshot->get_socket(), 0, msg->parts(), true);
	}
	return 0;
}

int forward::transfer_to_client(zmq_backend_socket* wa_sock,
	webapp_zeromq_msg* data)
{
	assert(nullptr != data);
	if (data->parts() < 2) {
		log.e(INDEXING_FORWRD_TAG, "recv snapshort data error\n");
		return -EBADPARM;
	}
	std::string node_id = data->get_part(0);
	if (data->get_part(1).length() == 0) {
		assert(nullptr != _forward_mgr);
		// data->dump();
		return _forward_mgr->service_request(wa_sock, data);
	} else {
		// from snapshot message
		// 1st frame: snapshot node id
		// 2 frame: client node id
		// 3 frame: empty
		// 4 frame: result
		if (!_client) {
			return -ENOTAVAIL;
		}
		data->sendparts(*_client->get_socket(), 1, data->parts(), true);
	}
	return 0;
}


int forward::init_client(void)
{
	server_header hdr;
	hdr.svc_type = service_msg_register;
	size_t infosz = strlen("indexing-service") + sizeof(server_request_info);
	server_request_info* info = (server_request_info*)(alloca(infosz));
	info->veh_cnt = 0;
	info->name_len = strlen("indexing-service");
	strncpy(info->buf, "indexing-service", info->name_len);
	_client->send((void*)&hdr, sizeof(server_header), false);
	_client->send((void*)info, infosz, true);
	return 0;
}

int forward::handle_worker_info(webapp_zeromq_msg* msg)
{
	assert(nullptr != msg);
	if (msg->get_part(0).length() != 0) {
		return -ENOTHANDLED;
	}
	std::string data = msg->get_part(1);
	auto* hdr = reinterpret_cast<server_header*>((void*)data.c_str());
	log.d(INDEXING_FORWRD_TAG,
		"service header type %d\n", hdr->svc_type);
	return 0;
}

}}
