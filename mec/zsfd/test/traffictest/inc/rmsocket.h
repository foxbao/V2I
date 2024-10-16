#ifndef __ZAS_UTILS_SOCKET_H__
#define __ZAS_UTILS_SOCKET_H__

#include <sys/socket.h>
#include <netinet/in.h>

namespace zas {
namespace traffic {

class rmsocket
{
public:
	rmsocket();
	virtual ~rmsocket();
public:
	// socket initialization
	bool init_tcpip(void);
	bool init_udp(void);

	sockaddr_in* get_addr();
	
	bool bind(int port);
	bool listen() const;
	bool accept( rmsocket*& new_socket) const;

	// client initialization
	bool connect( const char* host, int port);

	// data transimission
	int send(const char* s ) const;
	bool recv(char* buf,unsigned int lenth);
	int recvfrom(char* buf,unsigned int lenth, 
		sockaddr* addr, socklen_t* addrLen);
	int sendto(char* buf,unsigned int lenth, 
		sockaddr* addr, socklen_t* addrLen);
	bool send(char* buf,unsigned int lenth);

	void set_non_blocking (bool b);
	bool is_valid(void) const;
	bool is_connected(void) const;
	bool close(void);

private:
	void* _data;
};

}};	//zas::traffic
#endif //__ZAS_UTILS_SOCKET_H__
