#ifndef __CXX_ZAS_WEBAPP_BACKEND_ZMQ_H__
#define __CXX_ZAS_WEBAPP_BACKEND_ZMQ_H__

#include "webcore/webapp-backend.h"
#include "zmq.hpp"

namespace zas {
namespace webcore {

class WEBCORE_EXPORT webapp_zeromq_msg {
public:
	virtual bool recv(zmq::socket_t & socket) = 0;
	virtual int send(zmq::socket_t & socket) = 0;
	virtual int sendparts(zmq::socket_t & socket,
		int start, size_t count, bool bfinished) = 0;
	virtual size_t parts(void) = 0;
	virtual size_t body_parts(void) = 0;
	virtual int clear(void) = 0;
	virtual int set_part(size_t part_nbr, char *data, size_t sz) = 0;
	virtual int body_set(const char *body, size_t sz) = 0;
	virtual int body_fmt(const char *format, ...) = 0;
	virtual std::string body(int index) = 0;
	virtual std::string pop_front(void) = 0;
	virtual char* get_front(void) = 0;
	virtual std::string get_part(int index) = 0;
	virtual int erase_part(int index) = 0;
	virtual int body_insert(int index, const char *body, size_t sz) = 0;
	virtual int push_body_front(char *part, size_t sz) = 0;
	virtual int push_front(char *part, size_t sz) = 0;
	virtual int push_back(char *part, size_t sz) = 0;
	virtual int append(const char *part, size_t sz) = 0;
	virtual int dump(void) = 0;
};

WEBCORE_EXPORT webapp_zeromq_msg* create_webapp_zeromq_msg(
	zmq::socket_t & socket);

WEBCORE_EXPORT webapp_zeromq_msg* create_webapp_zeromq_msg(
	webapp_zeromq_msg* msg);

WEBCORE_EXPORT void release_webapp_zeromq_msg(webapp_zeromq_msg* msg);

class WEBCORE_EXPORT zmq_backend_socket: public webapp_socket
{
public:
	zmq_backend_socket();
	virtual ~zmq_backend_socket();
	virtual zmq::socket_t* get_socket(void) = 0;
	virtual bool need_msg_context(void) = 0;
	virtual zmq_backend_socket* duplicate(void) = 0;
	virtual int set_zmq_context(webapp_zeromq_msg* msg) = 0;
};

class WEBCORE_EXPORT zmq_backend_worker
{
public:
	zmq_backend_worker(zmq::context_t* context, uint32_t timeout);
	virtual ~zmq_backend_worker();
	timermgr* get_timermgr(void);
	webapp_backend_callback* set_callback(webapp_backend_callback *cb);
	virtual zmq_backend_socket* connect(const char* uri,
		webapp_socket_type type);
	virtual zmq_backend_socket* bind(const char* uri,
		webapp_socket_type type);
	int addfd(int fd, int action, webapp_fdcb cb, void* data);
	int removefd(int fd);
	int release_webapp_socket(webapp_socket* socket);
	int dispatch(void);
	zmq_backend_socket* get_socket_zmq(const char* uri,
		webapp_socket_type type);
	virtual int handle_zmq_msg(zmq_backend_socket* wa_sock);

private:
	void* _data;
};

class WEBCORE_EXPORT webapp_zmq_backend : public webapp_backend
{
public:
	webapp_zmq_backend();
	~webapp_zmq_backend();
	void* create_context(void);
	void destory_context(void* context);
	virtual zmq_backend_worker* create_backend_worker(
		zmq::context_t* zmq_context);

	webapp_backend_callback* set_callback(
		void* context, webapp_backend_callback *cb);

	timermgr* get_timermgr(void* context);
	webapp_socket* connect(void* context, const char* uri,
		webapp_socket_type type);
	webapp_socket* bind(void* context, const char* uri,
		webapp_socket_type type);
	int release_webapp_socket(void* context, webapp_socket* socket);

	int addfd(void* context, int fd,
		int action, webapp_fdcb cb, void* data);
	int removefd(void* context, int fd);
	int dispatch(void* context);

private:
	void* _data;
};


}};	// namespace zas::webcore
#endif