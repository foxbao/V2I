
#include "service-worker.h"
#include "fusion-service-def.h"
#include "unistd.h"
#include "utils/uri.h"
#include "webcore/logger.h"
#include "webcore/sysconfig.h"
#include "webcore/webapp.h"
#include "webcore/webapp-backend.h"

namespace zas {
namespace fusion_service {

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

service_worker::service_worker(zmq::context_t* context, uint32_t timeout)
: zmq_backend_worker(context, timeout)
, _zmq_context(context)
, _force_exit(false)
, _cb(nullptr)
, _timeout(timeout)
{
}

service_worker::~service_worker()
{
	_zmq_context = nullptr;
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
		log.e(FUSION_SNAPSHOT_TAG,
			"handle message no callback");
		return -ENOTAVAIL;
	}
	
	if (!wa_sock) {
		log.e(FUSION_SNAPSHOT_TAG,
			"handle message socket error is nullptr.");
		return -EBADPARM;
	}
	int ret = -1;
	auto *msg = create_webapp_zeromq_msg(*wa_sock->get_socket());
	assert(nullptr != msg);
	// msg->dump();
	size_t bodycnt = msg->body_parts();
	std::string recvdata;
	recvdata.clear();
	for (int i = 0; i < bodycnt; i++) {
		recvdata += msg->body(i);
	}
	wa_sock->set_zmq_context(msg);
	_cb->on_recv(reinterpret_cast<void*>(this), wa_sock,
		(void*)(recvdata.c_str()), recvdata.length());	
	// release_webapp_zeromq_msg(msg);
	return 0;
}

}} /* zas::fusion_service*/
