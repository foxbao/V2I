#include "inc/rmsocket.h"
#include <iostream>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <arpa/inet.h>

namespace zas {
namespace traffic {

const int MAXCLIENTCONNECTION = 10;

class rmsocket_impl
{
public:
	rmsocket_impl() 
	: _sock(-1), _connected(false) {
		memset(&_addr, 0, sizeof(_addr));
	}
	~rmsocket_impl() {
		if (is_valid()) ::close(_sock);
	}

	bool is_connected() const {
		return _connected;
	}

	bool is_valid() const {
		return (bool)(_sock != -1);
	}

	int& sock() {
		return _sock;
	}

	bool init_tcpip() {
		_sock = socket(AF_INET, SOCK_STREAM, 0);
		if (!is_valid()) return false;

		int on = 1;
		if (setsockopt(_sock, SOL_SOCKET, SO_REUSEADDR,
			(const char*)&on, sizeof(on)) == -1)
			return false;

		return true;
	}

	bool init_udp() {
		_sock = socket(AF_INET, SOCK_DGRAM, 0);
		if (!is_valid())
			return false;

		int on = 1;
		if (setsockopt(_sock, SOL_SOCKET, SO_REUSEADDR,
			(const char*)&on, sizeof(on)) == -1)
			return false;

		return true;
	}

	sockaddr_in* get_addr() {
		return &_addr;
	}

	bool bind(int port) {
		if (!is_valid()) return false;

		_addr.sin_family = AF_INET;
		_addr.sin_addr.s_addr = INADDR_ANY;
		_addr.sin_port = htons(port);
		int bind_return = ::bind(_sock, (struct sockaddr*)&_addr,
			sizeof(_addr));

		if (bind_return == -1)
			return false;

		return true;
	}

	bool listen() const {
		if (!is_valid()) return false;

		int listen_return = ::listen(_sock, MAXCLIENTCONNECTION);

		if (listen_return == -1) return false;

		return true;
	}

	bool accept(rmsocket_impl*& new_socket ) const {

		rmsocket_impl* ns = new rmsocket_impl();
		int addr_length = sizeof(_addr);
		ns->sock() = ::accept(_sock, (sockaddr*)&_addr,
			(socklen_t*)&addr_length);
		new_socket = ns;
		if ( ns->sock() <= 0 )
			return false;
		else
			return true;
	}

	int send(const char* s) const {
		if (!is_valid()) return -1;
		return ::send(_sock, s, strlen(s), MSG_NOSIGNAL);
	}

	bool send(char* buf,unsigned int len) {
		if (!is_valid()) return false;
		int number = 0;
		while(len > 0) {
			number = ::send(_sock, buf, len, MSG_DONTWAIT);
			if(number > 0) {
				len -= number;
				buf += number;
			} else {
				if(number < 0 && (errno == EINTR || errno == EWOULDBLOCK)) {
					//close_sock();
					return false;
				} else {
					return false;
				}
			}
		}
		return true;
	}

	bool recv(char* buf,unsigned int len)
	{
		if (!is_valid()) return false;
		memset(buf, 0, len);
		int number = 0;
		while( len > 0 )
		{
			number = ::recv(_sock, buf, len, 0);
			if(number > 0) {
				len -= number;
				buf += number;
			} else {
				if(number < 0 && (errno == EINTR))
					continue;
				close_sock();
				return false;
			}
		}
		return true;
	}

	int recvfrom(char* buf,unsigned int len, 
		sockaddr *addr, socklen_t *addrLen) {
		if ( ! is_valid() ) return -1;

		return ::recvfrom(_sock, buf, len, 0, addr, addrLen);
	}

	int sendto(char* buf,unsigned int len, 
		sockaddr *addr, socklen_t *addrLen) {
		if ( ! is_valid() ) return -1;

		return ::sendto(_sock, buf, len, 0, addr, *addrLen);
	}

	bool connect(const char* host, int port) {
		if ( ! is_valid() ) return false;
		_addr.sin_family = AF_INET;
		_addr.sin_port = htons(port);

		int status = inet_pton(AF_INET, host, &_addr.sin_addr);
		if ( errno == EAFNOSUPPORT ) return false;

		status = ::connect(_sock, (sockaddr*)&_addr, sizeof(_addr));
		if (status) return false;
		
		_connected = true;
		return true;
	}

	void set_non_blocking(bool b) {
		int opts;
		opts = fcntl (_sock, F_GETFL);
		if (opts < 0) return;

		if (b)
			opts = (opts | O_NONBLOCK);
		else
			opts = (opts & ~O_NONBLOCK);

		fcntl(_sock, F_SETFL, opts);
	}

	bool close_sock()
	{
		if (!is_valid())
			return false;

		::close(_sock);
		_sock = -1;
		_connected = false;
		return true;
	}

private:
	int _sock;
	sockaddr_in _addr;
	bool _connected;
};

rmsocket::rmsocket()
: _data(NULL)
{
	auto* impl = new rmsocket_impl();
	_data = reinterpret_cast<void*>(impl);
}

rmsocket::~rmsocket()
{
	auto* impl  = reinterpret_cast<rmsocket_impl*>(_data);
	delete impl;
	impl = NULL;
}

bool rmsocket::init_tcpip(void)
{
	auto* impl  = reinterpret_cast<rmsocket_impl*>(_data);
	if (!impl) return false;
	return impl->init_tcpip();
}

sockaddr_in* rmsocket::get_addr(void)
{
	auto* impl  = reinterpret_cast<rmsocket_impl*>(_data);
	if (!impl) return NULL;
	return impl->get_addr();
}

bool rmsocket::init_udp(void)
{
	auto* impl  = reinterpret_cast<rmsocket_impl*>(_data);
	if (!impl) return false;
	return impl->init_udp();
}

bool rmsocket::bind(int port)
{
	auto* impl  = reinterpret_cast<rmsocket_impl*>(_data);
	if (!impl) return false;
	return impl->bind(port);
}
bool rmsocket::listen() const
{
	auto* impl  = reinterpret_cast<rmsocket_impl*>(_data);
	if (!impl) return false;
	return impl->listen();
}
bool rmsocket::accept(rmsocket*& new_socket) const
{
	auto* impl  = reinterpret_cast<rmsocket_impl*>(_data);
	if (!impl) return false;
	rmsocket_impl* nimpl = NULL;
	bool ret = impl->accept(nimpl);
	new_socket = reinterpret_cast<rmsocket*>(nimpl);
	return ret;
}

// Client initialization
bool rmsocket::connect( const char* host, int port)
{
	auto* impl  = reinterpret_cast<rmsocket_impl*>(_data);
	if (!impl) return false;
	return impl->connect(host, port);
}

// Data Transimission
int rmsocket::send(const char* s ) const
{
	auto* impl  = reinterpret_cast<rmsocket_impl*>(_data);
	if (!impl) return false;
	return impl->send(s);
}

bool rmsocket::recv(char* buf,unsigned int lenth)
{
	auto* impl  = reinterpret_cast<rmsocket_impl*>(_data);
	if (!impl) return false;
	bool ret = impl->recv(buf, lenth);
	return ret;
}

int rmsocket::recvfrom(char* buf,unsigned int lenth,
	sockaddr *addr, socklen_t *addrLen)
{
	auto* impl  = reinterpret_cast<rmsocket_impl*>(_data);
	if (!impl) return false;
	return impl->recvfrom(buf, lenth, addr, addrLen);
}

int rmsocket::sendto(char* buf,unsigned int lenth,
	sockaddr *addr, socklen_t *addrLen)
{
	auto* impl  = reinterpret_cast<rmsocket_impl*>(_data);
	if (!impl) return false;
	return impl->sendto(buf, lenth, addr, addrLen);
}

bool rmsocket::send(char* buf,unsigned int lenth)
{
	auto* impl  = reinterpret_cast<rmsocket_impl*>(_data);
	if (!impl) return false;
	return impl->send(buf, lenth);
}
void rmsocket::set_non_blocking (bool b)
{
	auto* impl  = reinterpret_cast<rmsocket_impl*>(_data);
	if (!impl) return;
	impl->set_non_blocking(b);
}
bool rmsocket::is_valid(void) const
{
	auto* impl  = reinterpret_cast<rmsocket_impl*>(_data);
	if (!impl) return false;
	return impl->is_valid();
}

bool rmsocket::is_connected(void) const
{
	auto* impl  = reinterpret_cast<rmsocket_impl*>(_data);
	if (!impl) return false;
	return impl->is_connected();
}

bool rmsocket::close(void)
{
	auto* impl  = reinterpret_cast<rmsocket_impl*>(_data);
	if (!impl) return false;
	return impl->close_sock();
}

}}	//zas::traffic