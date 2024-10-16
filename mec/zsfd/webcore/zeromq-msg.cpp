#include "zeromq-msg.h"

#include <zmq.hpp> 
#include <iostream>
#include <iomanip>
#include <sstream>

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>

#include <unistd.h>
#include <vector>
#include <string>

namespace zas {
namespace webcore
{

void s_version_assert (int want_major, int want_minor)
{
	int major, minor, patch;
	zmq_version (&major, &minor, &patch);
	if (major < want_major
	|| (major == want_major && minor < want_minor)) {
		fprintf(stdout, "current 0MQ version is %d.%d\n", major, minor);
		fprintf(stdout, "application needs at least %d.%d - cannot continue\n", want_major, want_minor);
		exit (EXIT_FAILURE);
	}
}

zeromq_msg::zeromq_msg()
: _body_index(0)
, _refcnt(1)
{
}

zeromq_msg::zeromq_msg(char const *body, size_t sz)
: _body_index(0)
, _refcnt(1)
{
	body_set(body, sz);
}

zeromq_msg::zeromq_msg(char const *body, size_t sz, zmq::socket_t &socket)
: _body_index(0)
, _refcnt(1)
{
	body_set(body, sz);
	send(socket);
}

zeromq_msg::zeromq_msg(zmq::socket_t &socket)
: _body_index(0)
, _refcnt(1)
{
	recv(socket);
}

zeromq_msg::zeromq_msg(webapp_zeromq_msg *msg)
: _body_index(0)
, _refcnt(1)
{
	auto* zmsg = zas_downcast(webapp_zeromq_msg, zeromq_msg, msg);
	_part_data.resize(zmsg->_part_data.size());
	_body_index = zmsg->_body_index;
	std::copy(zmsg->_part_data.begin(), zmsg->_part_data.end(),
		_part_data.begin());
}

zeromq_msg::zeromq_msg(zeromq_msg &msg)
: _body_index(0)
, _refcnt(1)
{
	_part_data.resize(msg._part_data.size());
	_body_index = msg._body_index;
	std::copy(msg._part_data.begin(), msg._part_data.end(), _part_data.begin());
}

zeromq_msg::~zeromq_msg()
{
	clear();
}

int zeromq_msg::clear(void)
{
	_part_data.clear();
	_body_index = 0;
	return 0;
}

int zeromq_msg::set_part(size_t part_nbr, char *data, size_t sz)
{
	if (part_nbr < _part_data.size()) {
		_part_data[part_nbr].clear();
		_part_data[part_nbr].assign(data, sz);
	}
	return sz;
}

bool zeromq_msg::recv(zmq::socket_t & socket)
{
	clear();
	while(1) {
		zmq::message_t message(0);
		try {
			if (!socket.recv(message, ::zmq::recv_flags::none)) {
				return false;
			}
		} catch (zmq::error_t error) {
			fprintf(stderr, "E: %s\n", error.what());
			return false;
		}
		_part_data.push_back(
			std::string((char*)message.data(),
			message.size()));
		// is empty delimiter frame
		if (message.size() == 0)
			_body_index = _part_data.size();

		if (!message.more())
			break;
	}
	return true;
}

int zeromq_msg::send(zmq::socket_t & socket)
{
	for (size_t part_nbr = 0; part_nbr < _part_data.size(); part_nbr++) {
		zmq::message_t message;
		std::string data = _part_data[part_nbr];
		message.rebuild(data.c_str(), data.size());

		try {
			socket.send(message, 
				part_nbr < _part_data.size() - 1
					 ? (::zmq::send_flags::sndmore)
					 : (::zmq::send_flags::none));
		} catch (zmq::error_t error) {
			assert(error.num()!=0);
		}
	}
	return 0;
}

int zeromq_msg::sendparts(zmq::socket_t & socket,
	int start, size_t count, bool bfinished)
{
	if (start >= _part_data.size()) {
		return -EBADPARM;
	}
	if (start + count > _part_data.size() || count == 0) {
		count = _part_data.size() - start;
	}

	for (size_t part_nbr = start; part_nbr < start + count; part_nbr++) {
		zmq::message_t message;
		std::string data = _part_data[part_nbr];
		message.rebuild(data.c_str(), data.length());
		try {
			socket.send(message, 
				(part_nbr >= (start + count - 1) && bfinished)
					 ? (::zmq::send_flags::none)
					 : (::zmq::send_flags::sndmore));
		} catch (zmq::error_t error) {
			assert(error.num()!=0);
		}
	}
	return 0;
}

size_t zeromq_msg::parts(void)
{
	return _part_data.size();
}

size_t zeromq_msg::body_parts(void)
{
	if (_body_index < 0) {
		return 0;
	}
	if (_part_data.size() < _body_index) {
		return 0;
	}
	return _part_data.size() - _body_index;
}

int zeromq_msg::body_set(const char *body, size_t sz)
{
	if (_part_data.size() == 0 || _body_index == 0) {
		fprintf(stderr, "err: zeromq_msg nobody\n");
		return -ENOTAVAIL;
	}
	if (_part_data.size() > 0) {
		std::vector<std::string>::iterator it = _part_data.begin();
		advance(it, _body_index);
		_part_data.erase(it, _part_data.end());
	}
	push_back((char*)body, sz);
	return 0;
}

int zeromq_msg::body_insert(int index, const char *body, size_t sz)
{
	if (_part_data.size() == 0 || _body_index == 0) {
		fprintf(stderr, "err: zeromq_msg nobody\n");
		return -ENOTAVAIL;
	}
	if (_body_index + index >= _part_data.size()) {
		fprintf(stderr, "err: zeromq_msg invailed index\n");
		return -EBADPARM;
	}

	std::vector<std::string>::iterator it = _part_data.begin();
	advance(it, _body_index);
	_part_data.insert(it, std::string(body, sz));
	return 0;
}

int zeromq_msg::push_body_front(char *part, size_t sz) {
	body_insert(0, part, sz);
	return 0;
}


int zeromq_msg::body_fmt(const char *format, ...)
{
	char value [255 + 1];
	va_list args;

	va_start (args, format);
	vsnprintf (value, 255, format, args);
	va_end (args);

	body_set(value, strlen(value));
	return 0;
}

std::string zeromq_msg::body(int index)
{
	if (_part_data.size() == 0
		|| (_body_index + index) >= _part_data.size())
		return nullptr;
	return _part_data[_body_index + index];
}

int zeromq_msg::push_front(char *part, size_t sz) {
	_part_data.insert(_part_data.begin(), std::string(part, sz));
	return 0;
}

int zeromq_msg::push_back(char *part, size_t sz) {
	_part_data.push_back(std::string(part, sz));
	return 0;
}

std::string zeromq_msg::pop_front(void) {
	if (_part_data.size() == 0) {
		return "";
	}
	std::string part = _part_data.front();
	_part_data.erase(_part_data.begin());
	if (_body_index > 0) {
		_body_index--;
	}
	return part;
}

int zeromq_msg::append(const char *part, size_t sz)
{
	assert (part);
	push_back((char*)part, sz);
	return 0;
}

char* zeromq_msg::get_front(void) {
	if (_part_data.size() > 0) {
		return (char*)_part_data[0].c_str();
	} else {
		return nullptr;
	}
}

std::string zeromq_msg::get_part(int index) {
	if (_part_data.size() > index) {
		return _part_data[index];
	} else {
		return nullptr;
	}
}

int zeromq_msg::erase_part(int index) {
	if (index >= _part_data.size()) {
		return -EBADPARM;
	}
	std::vector<std::string>::iterator it = _part_data.begin();
	advance(it, index);
	_part_data.erase(it);
	return 0;
}

int zeromq_msg::dump(void)
{
	for (int i = 0; i < _part_data.size(); i ++) {
		fprintf(stdout, "msgdata %d:	%s\n", i, _part_data[i].c_str());
	}	
	return 0;
}

webapp_zeromq_msg* create_webapp_zeromq_msg(zmq::socket_t & socket)
{
	return new zeromq_msg(socket);
}

webapp_zeromq_msg* create_webapp_zeromq_msg(webapp_zeromq_msg* msg)
{
	return new zeromq_msg(msg);
}

void release_webapp_zeromq_msg(webapp_zeromq_msg* msg)
{
	auto* zmqmsg = zas_downcast(webapp_zeromq_msg, zeromq_msg, msg);
	delete zmqmsg;
}

}}
