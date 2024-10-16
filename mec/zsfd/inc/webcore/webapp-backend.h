#ifndef __CXX_ZAS_WEBAPP_BACKEND_H__
#define __CXX_ZAS_WEBAPP_BACKEND_H__

#include "webcore/webcore.h"
#include "utils/buffer.h"
#include "utils/timer.h"

namespace zas {
namespace webcore {

using namespace zas::utils;

enum webapp_socket_type
{
	webapp_socket_unknown = 0,
	webapp_socket_worker,
	webapp_socket_client,
	webapp_socket_router,
	webapp_socket_dealer,
	webapp_socket_pub,
	webapp_socket_sub,
	webapp_socket_pair,
};

class WEBCORE_EXPORT webapp_socket
{
public:
	webapp_socket();
	virtual ~webapp_socket();
	virtual int addref(void) = 0;
	virtual int release(void) = 0;
	virtual int send(void* data, size_t sz, bool bfinished = true) = 0;
	virtual int reply(void* data, size_t sz) = 0;
	virtual webapp_socket_type gettype(void) = 0;
};

class WEBCORE_EXPORT webapp_backend_callback
{
public:
	webapp_backend_callback();
	virtual ~webapp_backend_callback();
	virtual int on_recv(void* context, webapp_socket* socket, void* data, size_t sz) = 0;
};

enum {
	BACKEND_POLLIN = 1,
	BACKEND_POLLOUT = 2,
	BACKEND_POLLERR = 4,
	BACKEND_POLLPRI = 8,
};

typedef int (*webapp_fdcb)(int fd, int revents, void* data);

class WEBCORE_EXPORT webapp_backend
{
public:
	webapp_backend();
	virtual ~webapp_backend();
	virtual void* create_context(void) = 0;
	virtual void destory_context(void* context) = 0;
	
	/**
	  Set the callback
	  @param context the backend context
	  @param cb the callback
	  	passing nullptr is regarded as remove callback
	  @return the previous callback set
	 */
	virtual webapp_backend_callback* set_callback(
		void* context, webapp_backend_callback *cb) = 0;
	
	virtual timermgr* get_timermgr(void* context) = 0;
	virtual webapp_socket* connect(void* context, const char* uri,
		webapp_socket_type type) = 0;
	virtual webapp_socket* bind(void* context, const char* uri,
		webapp_socket_type type) = 0;
	virtual int release_webapp_socket(void* context, webapp_socket* socket) = 0;

	/**
	 * offer capabilities of adding a pure fd
	 * to the backend
	 * @param context the context
	 * @param fd the OS fd to be removed
	 * @param action the action to this fd
	 * @param cb the fd callback
	 * @param data the callback data
	 * @return 0 for success
	 */
	virtual int addfd(void* context, int fd,
		int action, webapp_fdcb cb, void* data) = 0;

	/**
	 * offer capabilities of removing a pure fd
	 * from the backend
	 * @param context the context
	 * @param fd the OS fd to be removed
	 * @return 0 for success
	 */
	virtual int removefd(void* context, int fd) = 0;

	virtual int dispatch(void* context) = 0;
};

}};	// namespace zas::webcore
#endif