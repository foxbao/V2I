#ifndef __CXX_VHIECLE_INDEXING_DEF_H__
#define __CXX_VHIECLE_INDEXING_DEF_H__

#include <string>
#include "std/zasbsc.h"
#include "std/list.h"
#include "utils/avltree.h"

namespace zas {
namespace vehicle_indexing {

#define INDEXING_CLIENT_DEFAULT_PORT			(5556)
#define INDEXING_SERVER_ENDPOINT_DEFAULT_PORT		(5557)
#define INDEXING_SOCKET_MAX_COUNT_DEFAULT		(15)
#define INDEXING_TIMER_MIN_INTERVAL		        (10)
#define INDEXING_SERVER_ENDPOINT_VEH_MAX_CNT		(10000)

#define HEARTBEAT_LIVENESS_DEFAULT				(3)	//  3-5 is reasonable
#define HEARTBEAT_INTERVAL_DEFAULT				(2500)    //  msecs
#define HEARTBEAT_EXPIRY_DEFAULT		\
	HEARTBEAT_LIVENESS_DEFAULT * HEARTBEAT_INTERVAL_DEFAULT

#define INDEXING_URI_UID_KEY			"access-token"

//for log
#define INDEXING_FORWRD_TAG				"IDX_FORWORD"
#define INDEXING_SNAPSHOT_TAG			"IDX_SNAPSHOT"

static char *veh_snapshot_cmds [] = {
	(char*)"INITIALIZE", 
	(char*)"HEARTBEAT",
	(char*)"DISCONNECT"
};

static char *snapshot_type [] = {
	(char*)"MASTER", 
	(char*)"SLAVE"
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

}}	//	zas::vehicle_indexing

#endif  /*__CXX_VHIECLE_INDEXING_H__*/