#ifndef __ZAS_TRAFFIC_SIGNAL_RECEIVE_H__
#define __ZAS_TRAFFIC_SIGNAL_RECEIVE_H__

#include "utils/buffer.h"
#include "utils/json.h"
#include "utils/thread.h"

namespace zas {
namespace traffic {

using namespace zas::utils;

class traffic_receiver
{
public:
	traffic_receiver();
	virtual ~traffic_receiver();
public:
	int start_traffic_receiver();
	int receive_traffic_singnal();
	size_t move_to_package_start();
	int check_traffice_info();

private:
	fifobuffer*  _buff;
	looper_thread	_task_thread;
	uint8_t	_buffer[256];
};

}};	//zas::traffic
#endif //__ZAS_UTILS_SOCKET_H__
