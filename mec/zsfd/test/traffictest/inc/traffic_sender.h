#ifndef __ZAS_TRAFFIC_SIGNAL_RECEIVE_H__
#define __ZAS_TRAFFIC_SIGNAL_RECEIVE_H__

#include "utils/buffer.h"

namespace zas {
namespace traffic {

using namespace zas::utils;

class traffic_sender
{
public:
	traffic_sender();
	virtual ~traffic_sender();
public:
	int senddata();

private:
	int refreshdata();
	
private:
	uint8_t _buffer[256];
	int _size;
	uint32_t _status;
	uint32_t _lefttime;
	uint32_t* _timestamp;
	uint8_t* _timestampms;	
};

}};	//zas::traffic
#endif //__ZAS_UTILS_SOCKET_H__
