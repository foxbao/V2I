#ifndef __CXX_INDEXING_RABBITMQ_H__
#define __CXX_INDEXING_RABBITMQ_H__
#include <string>
#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>
#include "utils/timer.h"
#include <vector>

namespace zas {
namespace vehicle_indexing {

using namespace zas::utils;

class forward_backend;
class rabbitmq_init_retry_timer;

enum rabbitmq_role {
	rabbitmq_role_unknown = 0,
	rabbitmq_role_producing,
	rabbitmq_role_consuming,
};

struct rabbitmq_exchange_info {
	std::string exchange;
	std::string exchange_type;
	std::vector<std::string> routekeys;
};

class rabbitmq_callback {
public:
	virtual int on_consuming(char* routingkey, size_t rsz,
		void* data, size_t dsz) = 0;
	virtual void on_consuming_error(int errid) = 0;
};

class indexing_rabbitmq
{
public:
	indexing_rabbitmq(rabbitmq_role erole, const char* name,
		forward_backend* backend, void* context, timermgr* mgr);
	virtual ~indexing_rabbitmq();

public:
	int init(void);
	int publishing(const char* exchange, std::string &routingkey,
		void* data, size_t sz);
	int set_consuming_callback(rabbitmq_callback* cb);
	void on_timer(void);

private:
	int load_config(void);
	int init_producing(void);
	int init_consuming(void);
	int retry_init_consuming(void);
	int retry_init_producing(void);
	int amqp_return_result(amqp_rpc_reply_t &x, char const *desc);
	static int consume_cb(int fd, int revents, void* data);
	int on_consume(int fd, int revents);

private:
	void amqp_dump(void const *buffer, size_t len);

private:
	rabbitmq_role _rabbitmq_role;
	forward_backend* _backend;
	amqp_connection_state_t _conn;
	rabbitmq_callback*	_cb;
	rabbitmq_init_retry_timer* _retry_timer;
	uint16_t  _channel_id;
	int _sock_fd;
	void* _context;
	int _port;
	std::string _host;
	std::string _username;
	std::string _password;
	std::string _mq_name;
	std::vector<rabbitmq_exchange_info> _exchange_info;
};

}}	//zas::vehicle_indexing

#endif /* __CXX_INDEXING_RABBITMQ_H__*/