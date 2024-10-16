
#include "service-worker.h"
#include "snapshot-service-def.h"
#include "unistd.h"
#include "utils/uri.h"
#include "webcore/logger.h"
#include "webcore/sysconfig.h"
#include "webcore/webapp.h"

namespace zas {
namespace vehicle_snapshot_service {

using namespace zas::webcore;
using namespace zas::utils;

__thread service_worker* _service_worker_context = nullptr;
static mutex swmut;

struct service_worker_item {
	listnode_t ownerlist;
	service_worker* bker;
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

service_backend* service_backend::inst()
{
	static service_backend* _inst = NULL;
	if (_inst) return _inst;
	auto_mutex am(swmut);
	if (_inst) return _inst;
	_inst = new service_backend();
	assert(NULL != _inst);
	return _inst;
}

service_backend::service_backend()
{

}

service_backend::~service_backend()
{
}

zmq_backend_worker*
service_backend::create_backend_worker(zmq::context_t* zmq_context)
{
	if (!zmq_connect) {
		return nullptr;
	}
	auto* sock_worker =  new service_worker(zmq_context, 1000);
	_service_worker_context = sock_worker;
	return _service_worker_context;
}

webapp_backend_callback* service_backend::set_callback(
	void* context, webapp_backend_callback *cb) 
{
	if (!context || _service_worker_context != context || !cb) {
		return nullptr;
	}
	return _service_worker_context->set_callback(cb);
}

timermgr* service_backend::get_timermgr(void)
{
	if (!_service_worker_context) {
		return nullptr;
	}
	return _service_worker_context->get_timermgr();
}

int service_backend::create_kafka_sender(const char* name)
{
	if (!_service_worker_context) {
		return -ENOTAVAIL;
	}
	if (!name) { return -EBADPARM; }
	return _service_worker_context->create_kafka_sender(name);
}

int service_backend::create_kafka_receiver(const char* name)
{
	if (!_service_worker_context) {
		return -ENOTAVAIL;
	}
	if (!name) { return -EBADPARM; }
	return _service_worker_context->create_kafka_receiver(name);
}

int service_backend::send_kafka_data(const char* data, size_t sz)
{
	if (!_service_worker_context) {
		return -ENOTAVAIL;
	}
	if (!data || !sz) { return -EBADPARM; }
	// printf("send to kafka\n");
	return _service_worker_context->send_kafka_data(data, sz);
}

int service_backend::set_kafka_callback(kafka_data_callback* cb)
{
	if (!_service_worker_context) {
		return -ENOTAVAIL;
	}
	if (!cb) { return -EBADPARM; }
	return _service_worker_context->set_kafka_callback(cb);
}

int service_backend::init_worker(void)
{
	if (!_service_worker_context) {
		log.e(SNAPSHOT_WORKER_TAG,
			"backend no sock_woker\n");
		return -ENOTAVAIL;
	}
	_service_worker_context->init_worker();
	return 0;
}

bool service_backend::has_worker(void)
{
	if (!_service_worker_context) {
		log.e(SNAPSHOT_WORKER_TAG,
			"backend no sock_woker\n");
		return false;
	}
	return _service_worker_context->has_worker();
}

service_worker::service_worker(zmq::context_t* context, uint32_t timeout)
: zmq_backend_worker(context, timeout)
, _zmq_context(context)
, _force_exit(false)
, _cb(nullptr)
, _timeout(timeout)
, _worker(nullptr)
, _sender(nullptr)
, _receiver(nullptr)
, _kafka_cb(nullptr)
{
}

service_worker::~service_worker()
{
	_zmq_context = nullptr;
	_worker = nullptr;
}

webapp_backend_callback*
service_worker::set_callback(webapp_backend_callback *cb)
{
	auto* ret = _cb;
	_cb = cb;
	return ret;
}

int service_worker::handle_zmq_msg(zmq_backend_socket* wa_sock)
{
	if (!_cb) {
		log.e(SNAPSHOT_WORKER_TAG,
			"handle message no callback");
		return -ENOTAVAIL;
	}
	
	if (!wa_sock) {
		log.e(SNAPSHOT_WORKER_TAG,
			"handle message socket error is nullptr.");
		return -EBADPARM;
	}
	int ret = -1;
	auto *msg = create_webapp_zeromq_msg(*wa_sock->get_socket());
	assert(nullptr != msg);
	// msg->dump();

	if (wa_sock == _receiver) {
		// msg->dump();
		size_t part_cnt = msg->parts();
		std::string recvdata;
		recvdata.clear();
		for (int i = 0; i < part_cnt; i++) {
			recvdata += msg->get_part(i);
		}
		if (_kafka_cb) {
			_kafka_cb->on_kafka_data_send(recvdata.c_str(), recvdata.length());
		}
		release_webapp_zeromq_msg(msg);
		return 0;
	}

	if (wa_sock == _worker) {
		ret = handle_worker_info(msg);
	}

	if (wa_sock != _worker || ret) {
		std::string type = msg->pop_front();
		if (!type.compare("MASTER")) {
			// log.d(SNAPSHOT_WORKER_TAG, "master handle\n");
			// master can reply
		} else {
			// todo  slaver can not reply
		}
		size_t bodycnt = msg->body_parts();
		std::string recvdata;
		recvdata.clear();
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

zmq_backend_socket* service_worker::connect(const char* uri,
	webapp_socket_type type)
{
	if (type == webapp_socket_worker && _worker) {
		log.e(SNAPSHOT_WORKER_TAG, "woker exist\n");
		return nullptr;
	}
	auto* sock = zmq_backend_worker::connect(uri, type);
	if (type == webapp_socket_worker) {
		_worker = sock;
	}
	return sock;
}

int service_worker::init_worker(void)
{
	if (nullptr == _worker) {
		return -ENOTAVAIL;
	}
	server_header hdr;
	hdr.svc_type = service_msg_register;
	size_t infosz = strlen("snapshot") + sizeof(server_request_info);
	server_request_info* info = (server_request_info*)(alloca(infosz));
	info->veh_cnt = 0;
	info->name_len = strlen("snapshot");
	strncpy(info->buf, "snapshot", info->name_len);
	_worker->send((void*)&hdr, sizeof(server_header), false);
	_worker->send((void*)info, infosz, true);
	return 0;
}

bool service_worker::has_worker(void)
{
	if (nullptr == _worker) {
		return false;
	}
	return true;
}

int service_worker::handle_worker_info(webapp_zeromq_msg* msg)
{
	assert(nullptr != msg);
	if (msg->get_part(0).length() != 0) {
		return -ENOTHANDLED;
	}
	std::string data = msg->get_part(1);
	auto* hdr = reinterpret_cast<server_header*>((void*)data.c_str());
	log.d(SNAPSHOT_WORKER_TAG,
		"service header type %d\n", hdr->svc_type);
	return 0;
}

int service_worker::create_kafka_sender(const char* name)
{
	if (_sender) {
		return -EEXISTS;
	}
	std::string url = "inproc://";
	url += name;
	_sender = zmq_backend_worker::connect(url.c_str(), webapp_socket_pair);
	if (!_sender) {
		log.e(SNAPSHOT_WORKER_TAG, 
			"create pair %s failure\n", url.c_str());
		return -ELOGIC;
	}
	return 0;
}

int service_worker::create_kafka_receiver(const char* name)
{
	if (_receiver) {
		return -EEXISTS;
	}
	std::string url = "inproc://";
	url += name;
	_receiver = zmq_backend_worker::bind(url.c_str(), webapp_socket_pair);
	if (!_receiver) {
		log.e(SNAPSHOT_WORKER_TAG, 
			"create pair %s failure\n", url.c_str());
		return -ELOGIC;
	}
	return 0;
}

int service_worker::send_kafka_data(const char* data, size_t sz)
{
	if (!_sender) {
		return -ENOTAVAIL;
	}
	_sender->send((void*)data, sz);
	return 0;
}
int service_worker::set_kafka_callback(kafka_data_callback* cb)
{
	if (_kafka_cb) {
		return -EEXISTS;
	}
	_kafka_cb = cb;
	return 0;
}

}}
