
#include "forward.h"
#include "forward_arbitrate.h"
#include "unistd.h"
#include "utils/uri.h"
#include "webcore/logger.h"
#include "webcore/sysconfig.h"
#include "webcore/webapp.h"
#include "client-mgr.h"

#include <sys/time.h>

namespace zas {
namespace load_balance {

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

int forward_backend::set_distribute_info(const char* durl,
	webapp_socket_type dtype)
{
	if (!_forward_context) {
		return -EBADPARM;
	}
	return _forward_context->set_distribute_info(durl, dtype);
}

int forward_backend::set_forward_callback(forward_mgr_callback* cb)
{
	if (!_forward_context) {
		return -EBADPARM;
	}
	return _forward_context->set_forward_callback(cb);
}

int forward_backend::forward_to_service(webapp_zeromq_msg* data,
	server_endpoint_node node)
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
, _fowardclient(nullptr)
, _forward_mgr(nullptr)
, _distribute(nullptr)
{
}

forward::~forward()
{
	_client = nullptr;
	_fowardclient = nullptr;
	_distribute = nullptr;
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
		log.e(LOAD_BALANCE_FORWRD_TAG,
			"handle message no callback");
		return -ENOTAVAIL;
	}
	
	if (!wa_sock) {
		log.e(LOAD_BALANCE_FORWRD_TAG,
			"handle message socket error is nullptr.");
		return -EBADPARM;
	}
	int ret = -1;
	auto *msg = create_webapp_zeromq_msg(*wa_sock->get_socket());
	assert(nullptr != msg);
	// msg->dump();
	if (_forward_mgr) {
		if (wa_sock == _client) {
			ret = transfer_to_server(msg);
		} else if (wa_sock == _fowardclient) {
			ret = transfer_to_client(wa_sock, msg);
		} else if (wa_sock == _distribute) {
			ret = distribute_msg(msg);
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
	_fowardclient = get_socket_zmq(svc_uri, svc_type);
	assert(nullptr != _fowardclient);
	return 0;
}

int forward::set_distribute_info(const char* durl, webapp_socket_type dtype)
{
	if (!durl || !*durl) {
		return -EBADPARM;
	}
	_distribute = get_socket_zmq(durl, dtype);
	assert(nullptr != _distribute);
	return 0;
}

int forward::set_forward_callback(forward_mgr_callback* cb)
{
	_forward_mgr = cb;
	return 0;
}

int forward::transfer_to_server(webapp_zeromq_msg* data)
{
	if (!_fowardclient || !_forward_mgr) {
		return -ENOTAVAIL;
	}
	//add master uri
	assert(nullptr != data);
	std::string identity = data->get_part(0);
	if (identity.length() == 0) {
		log.e(LOAD_BALANCE_FORWRD_TAG,
			"load balance transto server no identity\n");
		return -EBADPARM;
	}

	client_mgr::inst()->add_client(identity);
	std::string addr = data->body(0);
	if (addr.length() < 1) {
		log.e(LOAD_BALANCE_FORWRD_TAG,
			"load balance transto server recv error message\n");
		return -EBADPARM;
	}
	auto* frame_uri = (basicinfo_frame_uri*)(addr.c_str());
	auto* frame_content = (basicinfo_frame_content*)(addr.c_str()
		+ sizeof(basicinfo_frame_uri) + frame_uri->uri_length);
	std::string add_uri(frame_uri->uri, frame_uri->uri_length);
	uri addr_info(add_uri);
	addr_info.add_query("cli_identity", identity.c_str());
	addr_info.add_query("issued_ipaddr",
		client_mgr::inst()->get_distribute_ipaddr());
	addr_info.add_query("issued_port",
		client_mgr::inst()->get_distribute_port());
	timeval tv;
	gettimeofday(&tv, nullptr);
	uint64_t times = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	addr_info.add_query("start_time", std::to_string(times).c_str());
	add_uri = addr_info.tostring();

	size_t hdr_sz = sizeof(basicinfo_frame_uri)
		+ sizeof(basicinfo_frame_content) + add_uri.length();
	char* basic_info = (char*)(alloca(hdr_sz));
	auto* basic_uri = reinterpret_cast<basicinfo_frame_uri*>(basic_info);
	basic_uri->uri_length = add_uri.length();
	if (nullptr == basic_uri->uri || add_uri.length() == 0) {
		return -EBADPARM;
	}
	strncpy(basic_uri->uri, add_uri.c_str(), add_uri.length());
	auto* basic_content = reinterpret_cast<basicinfo_frame_content*>(
		basic_info + sizeof(basicinfo_frame_uri) + add_uri.length());
	basic_content->sequence_id = frame_content->sequence_id;
	basic_content->attr = frame_content->attr;
	basic_content->body_frames = frame_content->body_frames;
	basic_content->body_size = frame_content->body_size;
	int bodyindex = data->parts() - data->body_parts();
	data->set_part(bodyindex, basic_info, hdr_sz);
	return _forward_mgr->forward_to_service(data, addr_info);
}

int forward::distribute_msg(webapp_zeromq_msg* data)
{
	if (!data) {
		return -ENOTAVAIL;
	}
	//add master uri
	assert(nullptr != data);

	std::string addr = data->body(0);
	if (addr.length() < 1) {
		log.e(LOAD_BALANCE_FORWRD_TAG,
			"load balance distribute recv error message\n");
		return -EBADPARM;
	}
	auto* frame_uri = (basicinfo_frame_uri*)(addr.c_str());
	auto* frame_content = (basicinfo_frame_content*)(addr.c_str()
		+ sizeof(basicinfo_frame_uri) + frame_uri->uri_length);
	std::string add_uri(frame_uri->uri, frame_uri->uri_length);
	uri addr_info(add_uri);
	std::string identity = addr_info.query_value("identity");
	std::string str_time = addr_info.query_value("start_time");
	uint64_t st_time = 0;
	if (str_time.length() > 0) {
		st_time = std::stoull(str_time.c_str(), nullptr, 10);
	}
	if (client_mgr::inst()->check_identity(identity)) {
		log.e(LOAD_BALANCE_FORWRD_TAG,
			"load balance distribute not find identity\n");
		return -EBADPARM;
	}
	timeval tv;
	gettimeofday(&tv, nullptr);
	uint64_t times = tv.tv_sec * 1000 + tv.tv_usec / 1000 - st_time;
	addr_info.add_query("processing-time", std::to_string(times).c_str());
	add_uri = addr_info.tostring();

	size_t hdr_sz = sizeof(basicinfo_frame_uri)
		+ sizeof(basicinfo_frame_content) + add_uri.length();
	char* basic_info = (char*)(alloca(hdr_sz));
	auto* basic_uri = reinterpret_cast<basicinfo_frame_uri*>(basic_info);
	log.i(LOAD_BALANCE_FORWRD_TAG, "loadblance distribute %s\n", add_uri.c_str());
	basic_uri->uri_length = add_uri.length();
	if (nullptr == basic_uri->uri || add_uri.length() == 0) {
		return -EBADPARM;
	}
	strncpy(basic_uri->uri, add_uri.c_str(), add_uri.length());
	auto* basic_content = reinterpret_cast<basicinfo_frame_content*>(
		basic_info + sizeof(basicinfo_frame_uri) + add_uri.length());
	basic_content->sequence_id = frame_content->sequence_id;
	basic_content->attr = frame_content->attr;
	basic_content->body_frames = frame_content->body_frames;
	basic_content->body_size = frame_content->body_size;
	int bodyindex = data->parts() - data->body_parts();
	data->set_part(bodyindex, basic_info, hdr_sz);
	data->set_part(0, (char*)identity.c_str(), identity.length());
	// send all data
	data->send(*_client->get_socket());
	return 0;
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
	server_endpoint_node node)
{
	if (!data || !node.get()) {
		return -EBADPARM;
	}
	assert(nullptr != _fowardclient);
	// data->dump();
	auto* msg = reinterpret_cast<webapp_zeromq_msg*>(data);

	_fowardclient->send((char*)(node->_identity.c_str()),
		node->_identity.length(), false);
	// send all data
	msg->send(*_fowardclient->get_socket());
	return 0;
}

int forward::transfer_to_client(zmq_backend_socket* wa_sock,
	webapp_zeromq_msg* data)
{
	assert(nullptr != data);
	if (data->parts() < 2) {
		log.e(LOAD_BALANCE_FORWRD_TAG,
			"load balance transfor to client data error\n");
		return -EBADPARM;
	}
	std::string node_id = data->get_part(0);
	if (data->get_part(1).length() == 0) {
		assert(nullptr != _forward_mgr);
		// data->dump();
		return _forward_mgr->service_request(wa_sock, data);
	} else {
		// from service message
		// 0 frame: service node id
		// 1 frame: client node id
		// 2 frame: empty
		// 3 frame: result
		if (!_client) {
			return -ENOTAVAIL;
		}
		data->sendparts(*_client->get_socket(), 1, data->parts(), true);
	}
	return 0;
}

}}
