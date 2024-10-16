#ifndef __zeromq_msg_H_INCLUDED__
#define __zeromq_msg_H_INCLUDED__
#include "webcore/webapp-backend-zmq.h"

#include "std/zasbsc.h"
#include "zmq.hpp"
#include <vector>
#include <string>

namespace zas {
namespace webcore
{

void s_version_assert (int want_major, int want_minor);

class zeromq_msg : public webapp_zeromq_msg
{
public:
	zeromq_msg();
	zeromq_msg(char const *body, size_t sz);
	zeromq_msg(char const *body, size_t sz, zmq::socket_t &socket);
	zeromq_msg(zmq::socket_t &socket);
	zeromq_msg(zeromq_msg &msg);
	zeromq_msg(webapp_zeromq_msg *msg);

	virtual ~zeromq_msg();

	bool recv(zmq::socket_t & socket);
	int send(zmq::socket_t & socket);
	int sendparts(zmq::socket_t & socket,
		int start, size_t count, bool bfinished);
	// total message frame
	size_t parts(void);
	// total message body frame
	size_t body_parts(void);

	// clear all message frame
	int clear(void);
	// change msg frame by part_nbr
	int set_part(size_t part_nbr, char *data, size_t sz);
	// clear body msg and set body msg with body&sz
	int body_set(const char *body, size_t sz);
	// clear body msg and set body msg with format
	int body_fmt(const char *format, ...);

	//get body msg frame by index after empty delimiter frame
	std::string body(int index);
	//get & remove frist msg frame
	std::string pop_front(void);
	//get first msg frame
	char* get_front(void);
	std::string get_part(int index);
	int erase_part(int index);

	// insert msg frame by index after empty delimiter frame
	int body_insert(int index, const char *body, size_t sz);
	// insert msg frame after empty delimiter frame
	int push_body_front(char *part, size_t sz);
	// insert msg frame front of all msg frame
	int push_front(char *part, size_t sz);
	// insert msg frame back
	int push_back(char *part, size_t sz);
	// insert msg frame back
	int append(const char *part, size_t sz);

	//print all msg frame
	int dump(void);

private:
	int 	_body_index;
	int 	_refcnt;
	std::vector<std::string> _part_data;
};

}}
#endif /* zeromq_msg_H_ */
