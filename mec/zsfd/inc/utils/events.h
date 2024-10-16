/*
 * definition of all predefined events
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_EVENTLOOP

#ifndef __CXX_ZAS_UTILS_EVENTS_H__
#define __CXX_ZAS_UTILS_EVENTS_H__

#include <sstream>
#include <memory>
#include "std/list.h"
#include "std/smtptr.h"
#include "utils/utils.h"

namespace zas {
namespace utils {

// predefined class
class timermgr;
class eventloop;
class async_event;	
class async_client_event;

class UTILS_EXPORT event_base
{
	friend class eventloop_impl;
public:
	event_base();
	virtual ~event_base();

public:
	virtual int addref(void);
	virtual int release(void);
	virtual int getref(void);

	/**
	 * get the fd of the event
	 * @return -1 for error
	 */
	virtual int getfd(void);

	/**
	 * detach the event object from the event loop
	 * @return >= 0 for success (refcnt)
	 */
	int detach(void);

public:
	eventloop* get_eventloop(void) {
		return _evl;
	}

protected:
	virtual void on_hungup(void);
	virtual void on_error(void);
	virtual int on_input(void);
	virtual int on_output(void);

	/**
	 * if you try to have this callback, just return 0
	 * in the override of on_input() & on_output()
	 */
	virtual void on_default(uint32_t events);

	/**
	 * read n bytes to vptr from the fd
	 * @param vptr the buffer to read
	 * @param n bytes to read
	 * @return the actual size read
	 */
	size_t read(void *vptr, size_t n);

	/**
	 * write n bytes from vptr to the fd
	 * @param vptr the buffer to write
	 * @param n bytes to write
	 * @return the actual size written
	 */
	size_t write(const void *vptr, size_t n);

	/**
	 * delete the object
	 * user could define its own strategy for the deletion
	 */
	virtual void destroy(void);

private:
	int _refcnt;
	eventloop* _evl = nullptr;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(event_base);
};

typedef zas::smtptr<event_base> event_ptr;

enum class client_errcode : uint32_t {
	no_error = 0,
	connection_timeout,
	fail_to_connect,
};

struct UTILS_EXPORT async_client_listener
{
	async_client_listener() = default;
	virtual ~async_client_listener() = default;

	/**
	 * callback indicating the listener is activated
	*/
	virtual void on_activated(async_event*);

	/**
	 * callback indicating the listener is deactivated
	*/
	virtual void on_deactivated(async_event*);
	
	/**
	 * callback indicating that the connection is established
	 */
	virtual void on_connected(void);

	/**
	 * callback indicating the connection is broken
	 */
	virtual void on_disconnected();
	/**
	 * callback indicating that there is some data input
	 * @param stream the data stored as a stream
	 */
	virtual void on_data(std::stringstream& stream);

	/**
	 * callback indicating that there is some data fragment
	 * a data fragment is in the form of <buf, size>
	 * @param buf the buffer of the data
	 * @param sz the size of the buffer
	 */
	virtual bool on_data_fragment(const uint8_t* buf, size_t sz);

	/**
	 * callback indicating an error happened
	 * @param errcode the error reason code
	*/
	virtual void on_error(client_errcode errcode);
};

// the async event
class UTILS_EXPORT async_event : public event_base
{
public:
	async_event();
	~async_event();

	/**
	 * set the listener for the socket
	 * @param lnr the listener object
	*/
	void set_listener(std::shared_ptr<async_client_listener> lnr);

	/**
	 * get the listener for the socket
	 * @return the listener object
	*/
	std::shared_ptr<async_client_listener> get_listener(void);

	/**
	 * get the input stream, which is used for
	 * reading available data
	 * @return stringstream object
	 */
	std::stringstream& inputstream(void);

	/**
	 * override version of write
	 * see write() in event_base for detail
	 */
	size_t write(const void* vptr, size_t n);

	/**
	 * check if there is pending output data
	 * @return true means we have pending data 
	 */
	bool pending_output(void);

	/**
	 * get the output pending data size
	 * @return data size
	 */
	size_t output_pending_datasize(void);

protected:
	/**
	 * callback for receiving data in an
	 * asynchronized manner
	 * @param stream the input data
	 */
	virtual void on_data(std::stringstream& stream);

	/**
	 * callback for receiving data fragment
	 * @param buf the buffer for the input data
	 * @param sz the size of the buffer
	 * @return true means data has been handled
	 */
	virtual bool on_data_fragment(const uint8_t* buf, size_t sz);

	int on_input(void);
	int on_output(void);

protected:
	std::weak_ptr<async_client_listener> _listener;

private:
	std::stringstream _inputs;
	listnode_t _outputs;
};

typedef zas::smtptr<async_event> async_event_ptr;

class UTILS_EXPORT async_client_event : public async_event
{
public:
	/**
	 * create a unix domain socket client
	 * @param sock_name the path of client name
	 * @return the pointer to the event
	 */
	static zas::smtptr<async_client_event> create(const char* unix_domain_client_name);

	/**
	 * create a tcp/ip client
	 * @return the pointer to the event
	 */
	static zas::smtptr<async_client_event> create(void);

	/**
	 * connect to the server in unix domain socket
	 * @param svrname the server name
	 * @return 0 for success
	 */
	int connect(const char* svrname);

	/**
	 * connect to the server in tcpip
	 * @param addr the ip address
	 * @param port the port
	 * @return 0 for success
	 */
	int connect(uint32_t addr, int port);

	/**
	 * connect to the server in IPV4 tcpip
	 * @param hostname the host name
	 * @param port the port
	 * @return 0 for success
	 */
	int connect(const char* hostname, int port);

	/**
	 * disconnect the connection
	 */
	virtual void disconnect(void);

protected:
	async_client_event() = default;
};

typedef zas::smtptr<async_client_event> async_client_event_ptr;

// the event for timer manager
class UTILS_EXPORT timermgr_event : public event_base
{
public:
	timermgr_event(int min_interval);
	~timermgr_event();

public:
	timermgr* get_timermgr(void) {
		return _tmrmgr;
	}

public:
	int getfd(void);
	int on_input(void);

private:
	timermgr *_tmrmgr;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(timermgr_event);
};

typedef zas::smtptr<timermgr_event> timermgr_event_ptr;

// the event for eventfd
class UTILS_EXPORT generic_event : public event_base
{
public:
	generic_event();
	~generic_event();

	/**
	 * @brief alarm the event
	 * @return 0 for success
	 */
	int alarm(void);

protected:
	int on_input(void);
	int getfd(void);

	/**
	 * @brief handler for the alarm
	 */
	virtual void on_alarm(void);

private:
	int _eventfd;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(generic_event);
};

zas_interface socket_acceptor
{
	/**
	 * accept a fd from the socket listener
	 * @param fd the fd from socket listener
	 * @param evl the eventloop holding the socket listener
	 * 		typically the new event object will also be attached
	 * 		to this eventloop. User could determine to do so
	 * @return a pointer to event_base
	 */
	virtual event_ptr accept(int fd, eventloop* evl) = 0;
};

// the event for socket listener
class UTILS_EXPORT socket_listener_event : public event_base
{
public:
	/**
	 * create a unix domain socket listenfd
	 * @param sock_name the path of socket name
	 * @return the pointer to the event
	 */
	static zas::smtptr<socket_listener_event> create(const char* unix_domain_sock_name);

	/**
	 * create a TCP/IP socket listenfd
	 * @param addr the ip address
	 * @param port the port
	 * @return the pointer to the event
	 */
	static zas::smtptr<socket_listener_event> create(uint32_t addr, int port);

	/**
	 * set the acceptor of the socket listener
	 * @param socket_acceptor the socket acceptor
	 * @return the previous set socket acceptor
	 */
	socket_acceptor* set_acceptor(socket_acceptor*);

	/**
	 * start listening
	 * @return 0 for success
	 */
	int listen(void);

protected:
	socket_listener_event() = default;
	~socket_listener_event() = default;
	socket_acceptor* _acceptor = nullptr;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(socket_listener_event);
};

typedef zas::smtptr<socket_listener_event> socket_listener_event_ptr;

}} // end of namespace zas::utils
#endif // __CXX_ZAS_UTILS_EVENTS_H__
#endif // UTILS_ENABLE_FBLOCK_EVENTLOOP
/* EOF */