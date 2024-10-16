/*
 * implementation of all predefined events
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_EVENTLOOP

#ifdef QNX_PLATEFORM
#include "qnx-poller.h"
#else
#include <sys/epoll.h>
#include <sys/eventfd.h>
#endif

#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>

#include <new>
#include "utils/timer.h"
#include "inc/eventloop-impl.h"

#ifndef QNX_PLATEFORM
#define __close ::close
#define __write ::write
#define __read ::read
#endif

namespace zas {
namespace utils {
using namespace std;

event_base::event_base()
: _refcnt(0) {
}

event_base::~event_base() {
}

int event_base::addref(void) {
	return __sync_add_and_fetch(&_refcnt, 1);
}

int event_base::release(void)
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0) {
		destroy();
	}
	return cnt;
}

int event_base::getref(void) {
	return _refcnt;
}

void event_base::on_hungup(void) {
}

void event_base::on_error(void) {
}

int event_base::on_input(void) {
	return 1;
}

int event_base::on_output(void) {
	return 1;
}

void event_base::on_default(uint32_t events) {
}

int event_base::getfd(void)
{
	fprintf(stderr, "error: fall into event_base::getfd()\n");
	return -1;
}

int event_base::detach(void)
{
	if (nullptr == _evl) {
		return -ENOTAVAIL;
	}
	event_ptr ev(this);
	auto ret = _evl->detach_event(ev);
	if (ret < 0) {
		return ret;
	}

	// get the current refcnt
	ret = ev->getref();
	assert(ret > 0);
	return ret - 1;
}

void event_base::destroy(void) {
	delete this;
}

size_t event_base::read(void *vptr, size_t n)
{
	int nread;
	size_t nleft = n;
	char *ptr = (char*)vptr;
	int fd = getfd();
	if (fd < 0) {
		return 0;
	}

    while (nleft > 0)
	{
		nread = (int)__read(fd, ptr, nleft);
		if (nread < 0) {
			if (errno == EINTR)
				nread = 0; /* call was interrupted, call read() again */
            else
				return (n - nleft); /* maybe errno == EAGAIN */
		} else if (0 == nread) {
			break; /* EOF */
		}
		nleft -= nread;
		ptr += nread;
	}
	return (n - nleft); /* return >= 0 */
}

size_t event_base::write(const void *vptr, size_t n)
{
	int nwritten;
	size_t nleft = n;
	const char *ptr = (char*)vptr;
	int fd = getfd();
	if (fd < 0) {
		return 0;
	}

	while (nleft > 0)
	{
		nwritten = __write(fd, ptr, nleft);
		if (nwritten <= 0) {
			if (nwritten < 0 && errno == EINTR)
				nwritten = 0; /* call was interrupted, call write() again */
			else
				return (n - nleft); /* error */
		}
		nleft -= nwritten;
		ptr += nwritten;
	}
    return n;
}

/* async_client_listener */

void async_client_listener::on_activated(async_event*) {
}

void async_client_listener::on_deactivated(async_event*) {
}

void async_client_listener::on_connected(void) {
}

void async_client_listener::on_disconnected() {
}

void async_client_listener::on_data(std::stringstream& stream) {
}

bool async_client_listener::on_data_fragment(const uint8_t* buf, size_t sz) {
	return false;
}

void async_client_listener::on_error(client_errcode errcode) {
}

/* async event */

struct async_event_output_fragment
{
	async_event_output_fragment(uint8_t* buf, size_t sz)
	: start_pos(0) {
		listnode_init(ownerlist);
		buffer.assign((char*)buf, sz);
	}
	listnode_t ownerlist;
	int start_pos;
	string buffer;
};

async_event::async_event() {
	listnode_init(_outputs);
}

async_event::~async_event()
{
	// release all output fragments
	while (!listnode_isempty(_outputs)) {
		auto of = list_entry(
			async_event_output_fragment, ownerlist, _outputs.next);
		listnode_del(of->ownerlist);
		delete of;
	}
}

void async_event::on_data(stringstream& stream)
{
	auto lnr = _listener.lock();
	if (nullptr != lnr) {
		lnr->on_data(stream);
	}
}

bool async_event::on_data_fragment(const uint8_t* buf, size_t sz)
{
	bool ret = false;
	auto lnr = _listener.lock();
	if (nullptr != lnr) {
		ret = lnr->on_data_fragment(buf, sz);
	}
	return ret;
}

int async_event::on_input(void)
{
	size_t sz;
	const int BUFSZ = 256;
	uint8_t buf[BUFSZ];
	bool handle_fragment = true;

	for (;;) {
		sz = event_base::read(buf, BUFSZ);
		if (sz <= 0) break;

		if (handle_fragment) {
			handle_fragment = on_data_fragment(buf, sz);
		}
		if (!handle_fragment) {
			_inputs.write((const char*)buf, sz);
			assert(!_inputs.bad());
		}
	}

	if (sz == 0 && !handle_fragment) {
		on_data(_inputs);
		_inputs.clear();
	}
	return 1;
}

void async_event::set_listener(shared_ptr<async_client_listener> lnr)
{
	for (;;) {
		auto prev = _listener.lock();
		if (nullptr != prev) {
			prev->on_deactivated(this);
		} break;
	}
	_listener = lnr;
	if (likely(nullptr != lnr)) {
		lnr->on_activated(this);
	}
}

shared_ptr<async_client_listener> async_event::get_listener(void)
{
	auto ret = _listener.lock();
	return ret;
}

std::stringstream& async_event::inputstream(void) {
	return _inputs;
}

bool async_event::pending_output(void) {
	return (output_pending_datasize() > 0) ? true : false;
}

size_t async_event::write(const void* buf, size_t sz)
{
	// check parameter
	if (nullptr == buf || !sz) {
		return 0;
	}

	if (listnode_isempty(_outputs)) {
		// directly write data to fd
		size_t wsz = event_base::write((void*)buf, sz);
		assert(wsz <= sz);
		// still remaining
		if (wsz < sz) {
			auto of = new (nothrow) async_event_output_fragment(
				((uint8_t*)buf) + wsz, sz - wsz);
			assert(nullptr != of);
			listnode_add(_outputs, of->ownerlist);
		}
	} else {
		// get the last fragment and append data to it
		auto of = list_entry(async_event_output_fragment,
			ownerlist, _outputs.prev);
		assert(nullptr != of);
		of->buffer.append((char*)buf, sz);
	}
	return sz;
}

size_t async_event::output_pending_datasize(void)
{
	size_t ret = 0;
	auto node = _outputs.next;
	for (; node != &_outputs; node = node->next) {
		auto of = list_entry(async_event_output_fragment,
			ownerlist, _outputs.next);
		ret += of->buffer.size() - of->start_pos;
	}
	return ret;
}

int async_event::on_output(void)
{
	while (!listnode_isempty(_outputs))
	{
		auto of = list_entry(async_event_output_fragment,
			ownerlist, _outputs.next);
		size_t sz = of->buffer.size() - of->start_pos;
	
		// try writing data to kernel buffer
		auto wsz = event_base::write((void*)(of->buffer.c_str()
			+ of->start_pos), sz);
		of->start_pos += wsz;
		if (wsz < sz) {
			// kernel write buffer is full
			break;
		}

		// pop the front fragment since it drained
		listnode_del(of->ownerlist);
		delete of;
	}
	return 1;
}

/* timermgr_event */

timermgr_event::timermgr_event(int min_interval)
{
	// create the timer manager
	_tmrmgr = create_timermgr(min_interval);
	assert(nullptr != _tmrmgr);
}

timermgr_event::~timermgr_event()
{
	if (nullptr == _tmrmgr) {
		return;
	}
	_tmrmgr->release();
	_tmrmgr = nullptr;
}

int timermgr_event::getfd(void)
{
	if (nullptr == _tmrmgr) {
		return -1;
	}
	return _tmrmgr->getfd();
}

int timermgr_event::on_input(void)
{
	// drain the eventfd
	uint64_t val = 0;
	int fd = _tmrmgr->getfd();

	for (;;) {
		int ret = __read(fd, &val, sizeof(uint64_t));
		if (ret <= 0 && (errno == EINTR || errno == EAGAIN)) {
			continue;
		}
		assert(ret == 8);
		break;
	}
	_tmrmgr->periodic_runner();
	return 1;
}

/* generic event */
generic_event::generic_event()
: _eventfd(-1) {
	_eventfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	assert(_eventfd >= 0);
}

generic_event::~generic_event()
{
	if (_eventfd >= 0) {
		__close(_eventfd); _eventfd = -1;
	}
}

int generic_event::on_input(void)
{
	// drain the eventfd
	uint64_t val = 0;
	for (;;) {
		int ret = __read(_eventfd, &val, sizeof(uint64_t));
		if (ret <= 0 && errno == EINTR) {
			continue;
		}
		assert(ret == 8);
		break;
	}
	on_alarm();
	return 1;
}

int generic_event::getfd(void) {
	return _eventfd;
}

void generic_event::on_alarm(void) {
}

int generic_event::alarm(void)
{
	if (_eventfd < 0) {
		return -EINVALID;
	}
	uint64_t val = 1;
	for (;;) {
		int ret = __write(_eventfd, &val, sizeof(uint64_t));
		if (ret <= 0) {
			if (errno == EINTR) continue;
			else return -1;
		}
		assert(ret == 8);
		break;
	}
	return 0;
}

// socket listener
class socket_listener_event_impl : public socket_listener_event
{
public:
	socket_listener_event_impl(const char* unix_domain_sock_name) {
		init_unix_domain_sock(unix_domain_sock_name);
	}

	socket_listener_event_impl(uint32_t addr, int port) {
		init_tcpip_sock(addr, port);
	}

	~socket_listener_event_impl()
	{
		if (_fd >= 0) {
			__close(_fd); _fd = -1;
		}
	}

	int getfd(void) {
		return _fd;
	}

protected:
	int on_input(void)
	{
		if (_fd < 0) {
			return -EINVALID;
		}

		for (;;) {
			int connfd;
			if ((connfd = accept4(_fd, (sockaddr*)nullptr,
				nullptr, SOCK_NONBLOCK)) == -1) {
				auto eno = errno;
				if (eno == EAGAIN || eno == EWOULDBLOCK) {
					break;
				}
				else if (eno == EINTR) {
					continue;
				}
				return -1;
			}

			// create the connection object
			assert(connfd >= 0);
			if (_acceptor) {
				_acceptor->accept(connfd, get_eventloop());
			} else {
				__close(connfd);
			}
		}
		return 1;
	}

private:
	int init_unix_domain_sock(const char* filename)
	{
		size_t len;
		sockaddr_un un;

		/* create a UNIX domain stream socket */
		if ((_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
			return -1;
		}
		unlink(filename);   /* in case it already exists */

		/* fill in socket address structure */
		memset(&un, 0, sizeof(un));
		un.sun_family = AF_UNIX;
		strcpy(un.sun_path, filename);
		len = offsetof(struct sockaddr_un, sun_path) + strlen(filename);

		/* bind the name to the descriptor */
		if (bind(_fd, (struct sockaddr *)&un, len) < 0) {
			__close(_fd); _fd = -1;
			return -2;
		}
		_a.domain = sockd_unix;
		return 0;
	}

	int init_tcpip_sock(uint32_t addr, int port)
	{
		sockaddr_in in;

		if (port > 0xFFFF) {
			return -EINVALID;
		}
		
		/* create a tcpip socket */
		if ((_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
			return -1;
		}

		/* fill in socket address structure */
		memset(&in, 0, sizeof(in));
		in.sin_family = AF_INET;
		in.sin_addr.s_addr = htonl(addr);
		in.sin_port = htons(port);
	
		// bind socket id
		if (bind(_fd, (sockaddr*)&in, sizeof(in)) == -1) {
			__close(_fd); _fd = -1;
			return -EINVALID;
		}
		_a.domain = sockd_tcpip;
		return 0;
	}

private:
	int _fd = -1;

	enum sock_domain {
		sockd_unknown = 0,
		sockd_unix,
		sockd_tcpip,
	};

	union {
		uint32_t _attrs = 0;
		struct {
			int domain : 2;
		} _a;
	};
};

#define SOCK_QLEN	16

int socket_listener_event::listen(void)
{
	auto fd = getfd();
	if (fd < 0) {
		return -EINVALID;
	}
	if (::listen(fd, SOCK_QLEN) < 0) { /* tell kernel we're a server */
		return -EINVALID;
	}
	return 0;
}

socket_acceptor* socket_listener_event::set_acceptor(socket_acceptor* sa)
{
	auto ret = _acceptor;
	_acceptor = sa;
	return ret;
}

socket_listener_event_ptr socket_listener_event::create(
	const char* unix_domain_sock_name)
{
	if (nullptr == unix_domain_sock_name
		|| *unix_domain_sock_name == '\0') {
		return nullptr;
	}
	return socket_listener_event_ptr(new (std::nothrow)
		socket_listener_event_impl(unix_domain_sock_name));
}

socket_listener_event_ptr socket_listener_event::create(
	uint32_t addr, int port)
{
	if (port > 0xFFFF) {
		return nullptr;
	}
	return socket_listener_event_ptr(new (std::nothrow)
		socket_listener_event_impl(addr, port));
}

class async_client_event_impl : public async_client_event
{
public:
	async_client_event_impl(const char* unix_domain_sock_name) {
		init_unix_domain_client(unix_domain_sock_name);
	}

	async_client_event_impl(void) {
		init_tcpip_client();
	}

	~async_client_event_impl()
	{
		if (nullptr != _timeout) {
			_timeout->stop();
			delete _timeout; _timeout = nullptr;
		}
		disconnect_immediately();
		if (_fd >= 0) {
			__close(_fd); _fd = -1;
		}
	}

	int connect_tcpip(uint32_t addr, int port)
	{
		if (_a.domain != sockd_tcpip) {
			return -EINVALID;
		}
		if (_fd < 0 || port > 0xFFFF) {
			return -EINVALID;
		}

		sockaddr_in in;
		memset(&in, 0, sizeof(in));
	
		in.sin_family = AF_INET;
		in.sin_addr.s_addr = htonl(addr);
		in.sin_port = htons(port);
		
		if (::connect(_fd, (sockaddr*)&in, sizeof(in)) == -1) {
			auto err = errno;
			if (err != EINPROGRESS) {
				// no release of fd, let user handle it
				return -EINVALID;
			}
		}
		// start timeout count
		start_timeout_counter();
		return 0;
	}

	int connect_tcpip(const char* hostname, int port)
	{
		if (_a.domain != sockd_tcpip || _fd < 0) {
			return -EINVALID;
		}
		if (!hostname || !*hostname || port > 0xFFFF) {
			return -EBADPARM;
		}
		
		char port_str[16];
		addrinfo *result, hints = {0};
	
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		snprintf(port_str, 16, "%d", port);
	
		if (getaddrinfo(hostname, port_str, &hints, &result)) {
			return -EINVALID;
		}
		if (nullptr == result) {
			return -ENOTAVAIL;
		}
		if (::connect(_fd, result->ai_addr, result->ai_addrlen) == -1) {
			auto err = errno;
			if (err != EINPROGRESS) {
				// no close of fd, let user handle it
				freeaddrinfo(result);
				return -EINVALID;
			}
		}
		freeaddrinfo(result);
		
		// start timeout count
		start_timeout_counter();
		return 0;
	}

	int connect_unix(const char* unix_domain_svrname)
	{
		if (_a.domain != sockd_tcpip) {
			return -EINVALID;
		}
		if (!unix_domain_svrname || !*unix_domain_svrname) {
			return -EINVALID;
		}

		sockaddr_un un;
		memset(&un, 0, sizeof(un));

		un.sun_family = AF_UNIX;
		strcpy(un.sun_path, unix_domain_svrname);	
		size_t len = offsetof(sockaddr_un, sun_path) + strlen(un.sun_path);

		if (::connect(_fd, (struct sockaddr *)&un, len) == -1) {
			auto err = errno;
			if (err != EINPROGRESS) {
				// no close of fd, let user handle it
				return -EINVALID;
			}
		}
		// start timeout count
		start_timeout_counter();
		return 0;
	}

	void disconnect(void)
	{
		if (check_set_disconnection_in_progress()) {
			return;
		}
		if (!pending_output() && _fd >= 0) {
			do_disconnect_immediately();
		}

		// wait for data drain, and the connection will be
		// released in that time
	}

protected:

	int on_output(void)
	{
		if (_a.connected) {
			auto ret = async_event::on_output();
			if (_a.disconnecting && !pending_output() && _fd >= 0) {
				do_disconnect_immediately();
			}
			return ret;
		}
		// check if we connected or not
		int err;
		socklen_t errlen = sizeof(err);
		getsockopt(_fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
		if (err == 0) {
			// set flag indicating we are connected
			_a.connected = 1;
			stop_timeout_counter();
			// callback of listener
			auto lnr = _listener.lock();
			if (nullptr != lnr) {
				lnr->on_connected();
			}
		}
		return 1;
	}

	void on_hungup(void)
	{
		if (!_a.connected)
		{
			// set error flag and stop timer
			_a.connect_fail = 1;
			stop_timeout_counter();
			
			// callback of listener
			auto lnr = _listener.lock();
			if (nullptr != lnr) {
				lnr->on_error(client_errcode::fail_to_connect);
			}
		}
		disconnect_immediately();
	}

	int getfd(void) {
		return _fd;
	}

private:
	bool check_set_disconnection_in_progress(void)
	{
		if (_a.disconnecting) {
			return true;
		}
		// set it as "progressing"
		_a.disconnecting = 1;
		return false;
	}

	void disconnect_immediately(void)
	{
		if (check_set_disconnection_in_progress()) {
			return;
		}
		do_disconnect_immediately();
	}

	void do_disconnect_immediately(void)
	{
		for (;;) {
			auto lnr = _listener.lock();
			if (nullptr != lnr) {
				lnr->on_disconnected();
			} break;
		}
		// remove listener so we'll no longer handling any data
		set_listener(nullptr);

		// detach the pollee. note that delete operation 
		// may happen in detach operation. so don't operate
		// anything in the object after this call
		if (detach() > 0) {
			assert(_fd >= 0);
			__close(_fd); _fd = -2;
		}
		
		// if detach() <= 0, the whole object may already released
		// so don't do anything here (no code)
	}

	int init_unix_domain_client(const char* filename)
	{
		/* create a UNIX domain stream socket */
		if ((_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
			return -1;

		/* fill socket address structure with our address */
		sockaddr_un un;
		memset(&un, 0, sizeof(un));
		un.sun_family = AF_UNIX;

		strcpy(un.sun_path, filename);	
		int len = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);

		unlink(un.sun_path);        /* in case it already exists */
		if (bind(_fd, (struct sockaddr *)&un, len) < 0) {
			__close(_fd); _fd = -1;
			return -2;
		}
		_a.domain = sockd_unix;
		return 0;
	}

	int init_tcpip_client(void)
	{
		if ((_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
			return -1;
		}
		_a.domain = sockd_tcpip;
		return 0;
	}

	int start_timeout_counter(void)
	{
		_a.timeout = 0;
		if (_timeout) {
			_timeout->stop();
		} else {
			_timeout = new (std::nothrow) connstate_timer(this);
			if (nullptr == _timeout) {
				return -ENOMEMORY;
			}
		}
		_timeout->start();
		return 0;
	}

	int stop_timeout_counter(void)
	{
		if (nullptr == _timeout) {
			return 0;
		}
		_a.timeout = 0;
		// release the connstate timer
		_timeout->stop();
		delete _timeout;
		_timeout = nullptr;
		return 0;
	}

private:
	int _fd = -1;
	enum sock_domain {
		sockd_unknown = 0,
		sockd_unix,
		sockd_tcpip,
	};

	union {
		uint32_t _attrs = 0;
		struct {
			uint32_t domain : 2;
			uint32_t connected : 1;
			uint32_t timeout : 1;
			uint32_t connect_fail : 1;
			uint32_t disconnecting : 1;
		} _a;
	};

	class connstate_timer : public timer
	{
		friend class async_client_event_impl;
	public:
		connstate_timer(async_client_event_impl* cli)
		: timer(cli->get_eventloop()->get_timermgr(), 5000)
		, _client(cli) {
			assert(nullptr != _client);
		}

	protected:
		void on_timer(void)
		{
			assert(nullptr != _client);
			_client->_a.timeout = 1;
			auto lnr = _client->_listener.lock();
			if (nullptr != lnr) {
				lnr->on_error(client_errcode::connection_timeout);
			}
		}

	private:
		async_client_event_impl* _client;
	}
	*_timeout = nullptr;
};

async_client_event_ptr async_client_event::create(
	const char* unix_domain_client_name)
{
	if (nullptr == unix_domain_client_name
		|| *unix_domain_client_name == '\0') {
		return nullptr;
	}
	return async_client_event_ptr(new (std::nothrow)
		async_client_event_impl(unix_domain_client_name));
}

async_client_event_ptr async_client_event::create(void)
{
	return async_client_event_ptr(new (std::nothrow)
		async_client_event_impl());
}

int async_client_event::connect(const char* svrname)
{
	if (nullptr == this) {
		return -EINVALID;
	}

	// check connection prerequisite
	if (nullptr == get_eventloop()) {
		return -ENOTALLOWED;
	}
	auto client = zas_downcast(async_client_event, async_client_event_impl, this);
	return client->connect_unix(svrname);
}

int async_client_event::connect(uint32_t addr, int port)
{
	if (nullptr == this) {
		return -EINVALID;
	}

	// check connection prerequisite
	if (nullptr == get_eventloop()) {
		return -ENOTALLOWED;
	}
	auto client = zas_downcast(async_client_event, async_client_event_impl, this);
	return client->connect_tcpip(addr, port);
}

int async_client_event::connect(const char* hostname, int port)
{
	if (nullptr == this) {
		return -EINVALID;
	}

	// check connection prerequisite
	if (nullptr == get_eventloop()) {
		return -ENOTALLOWED;
	}
	auto client = zas_downcast(async_client_event, async_client_event_impl, this);
	return client->connect_tcpip(hostname, port);
}

void async_client_event::disconnect(void) {
}

}} // end of namespace zas::utils
#endif // UTILS_ENABLE_FBLOCK_EVENTLOOP
/* EOF */
