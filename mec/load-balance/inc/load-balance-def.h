#ifndef __CXX_LOAD_BALANCE_DEF_H__
#define __CXX_LOAD_BALANCE_DEF_H__

#include <string>
#include "std/zasbsc.h"
#include "std/list.h"
#include "utils/avltree.h"

namespace zas {
namespace load_balance {

#define LOAD_BALANCE_CLIENT_DEFAULT_PORT			(5555)
#define LOAD_BALANCE_SERVER_ENDPOINT_DEFAULT_PORT		(5556)
#define LOAD_BALANCE_DISTRIBUTE_DEFAULT_PORT		(6000)
#define LOAD_BALANCE_DISTRIBUTE_DEFAULT_IPADDR		"localhost"
#define LOAD_BALANCE_SOCKET_MAX_COUNT_DEFAULT		(15)
#define LOAD_BALANCE_TIMER_MIN_INTERVAL		        (10)
#define LOAD_BALANCE_SERVER_ENDPOINT_MAX_CNT		(10000)

#define HEARTBEAT_LIVENESS_DEFAULT				(3)	//  3-5 is reasonable
#define HEARTBEAT_INTERVAL_DEFAULT				(2500)    //  msecs
#define HEARTBEAT_EXPIRY_DEFAULT		\
	HEARTBEAT_LIVENESS_DEFAULT * HEARTBEAT_INTERVAL_DEFAULT

//for log
#define LOAD_BALANCE_FORWRD_TAG				"LBS_FORWORD"
#define LOAD_BALANCE_MGR_TAG				"LBS_MGR"

static char *servcie_cmds [] = {
	(char*)"INITIALIZE", 
	(char*)"HEARTBEAT",
	(char*)"DISCONNECT"
};

#define within(num) (int) ((float)((num) * random ()) / (RAND_MAX + 1.0))

enum msg_pattern
{
	msgptn_unknown = 0,
	msgptn_req_rep,
	msgptn_sub_pub,
};

enum msg_type
{
	msgtype_unknown = 0,
	msgtype_request,
	msgtype_reply,
	msgtype_sub,
	msgtype_pub,
};

enum service_msg_type
{
	service_msg_unknonw = 0,
	service_msg_forward,
	service_msg_request,
	service_msg_register,
	service_msg_heartbeat,
	service_msg_deregister,
	service_msg_reply,
};

struct server_header
{
	service_msg_type svc_type;
};

struct server_request_info
{
	uint32_t veh_cnt;
	size_t name_len;
	char buf[0];
};

struct server_reply_info
{
	int result;
};

}}	//	zas::load_balance

#endif  /*__CXX_LOAD_BALANCE_DEF_H__*/