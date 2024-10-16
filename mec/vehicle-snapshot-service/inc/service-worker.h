
#ifndef __CXX_SNAPSHOT_SERVICE_WORKER_H__
#define __CXX_SNAPSHOT_SERVICE_WORKER_H__

#include "snapshot-service-def.h"
#include "webcore/webapp-backend-zmq.h"

#include <string.h>

#include "std/list.h"
#include "utils/avltree.h"
#include "utils/mutex.h"
#include "utils/uri.h"

namespace zas {
namespace vehicle_snapshot_service {

using namespace zas::webcore;
using namespace zas::utils;
struct forward_service_node;

class kafka_data_callback
{
public:
	virtual int on_kafka_data_send(const char* data, size_t sz) = 0;
};

// internal communication function
class service_worker : public zmq_backend_worker
{
public:
	service_worker(zmq::context_t* context, uint32_t timeout);
	virtual ~service_worker();
	webapp_backend_callback* set_callback(webapp_backend_callback *cb);
	int handle_zmq_msg(zmq_backend_socket* wa_sock);
	zmq_backend_socket* connect(const char* uri, webapp_socket_type type);
	int init_worker(void);
	bool has_worker(void);

	int create_kafka_sender(const char* name);
	int create_kafka_receiver(const char* name);
	int send_kafka_data(const char* data, size_t sz);
	int set_kafka_callback(kafka_data_callback* cb);
private:
	int handle_worker_info(webapp_zeromq_msg* msg);

private:
	zmq::context_t*		_zmq_context;
	int				_force_exit;
	uint32_t		_timeout;
	webapp_backend_callback* _cb;

	zmq_backend_socket*	_worker;
	zmq_backend_socket*	_sender;
	zmq_backend_socket*	_receiver;
	kafka_data_callback* _kafka_cb;
};

class service_backend : public webapp_zmq_backend
{
public:
	service_backend();
	virtual ~service_backend();
	static service_backend* inst();

public:
	zmq_backend_worker* create_backend_worker(
		zmq::context_t* zmq_context);
	webapp_backend_callback* set_callback(
			void* context, webapp_backend_callback *cb);
	timermgr* get_timermgr(void);

	int create_kafka_sender(const char* name);
	int create_kafka_receiver(const char* name);
	int send_kafka_data(const char* data, size_t sz);
	int set_kafka_callback(kafka_data_callback* cb);

public:
	bool has_worker(void);
	int init_worker(void);
	
};

}}

#endif /* __CXX_SNAPSHOT_SERVICE_WORKER_H__*/