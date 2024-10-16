#ifndef ___CXX_ZAS_RPC_BRIDGE_MSG_DEF_H__
#define ___CXX_ZAS_RPC_BRIDGE_MSG_DEF_H__

#include "std/zasbsc.h"

namespace zas {
namespace servicebridge {

#define BRIDGE_SERVICE_PKG "zas.system"
#define BRIDGE_SERVICE_NAME "bridge"
#define HOST_CONTAINER_ID "hostid"

struct request_topic
{
	uint16_t type;
	uint16_t clientid;
	uint16_t pkg;
	uint16_t name;
	uint16_t naming;
	uint32_t securecode;
	uint16_t token;
	char	buf[0];
}PACKED;

struct topic_reply
{
	uint16_t code;
	uint16_t pkg;
	uint16_t name;
	uint16_t naming;
	uint16_t topic;
	uint16_t subtopic;
	uint16_t token;
	char	buf[0];
}PACKED;

struct request_sequence
{
	uint64_t evlid;
	uint32_t seqid;
	uint32_t retained;
}PACKED;

struct request_method_invoke
{
	uint8_t msgid;
	uint8_t token;
	uint8_t clslen;
	uint8_t methodid;
	uint8_t flag;
	request_sequence seqid;
	uint64_t instancid;
	uint16_t inputsz;
	uint16_t outputsz;
	char	buf[0];
}PACKED;

struct reply_method_invoke
{
	uint8_t msgid;
	uint8_t code;
	uint8_t errlen;
	uint8_t clslen;
	uint8_t methodid;
	uint8_t flag;
	request_sequence seqid;
	uint64_t instanceid;
	uint16_t outputsz;
	char	buf[0];
}PACKED;

struct send_event_invoke
{
	uint8_t msgid;
	uint8_t token;
	uint16_t namelen;
	request_sequence seqid;
	uint8_t action;
	char buf[0];
}PACKED;

struct recv_event_invoke
{
	uint8_t msgid;
	uint16_t namelen;
	uint16_t inputlen;
	char	buf[0];
}PACKED;

struct observable_called
{
	uint8_t msgid;
	uint8_t classlen;
	uint8_t uuid;
	int64_t instanceid;
	uint16_t client_name;
	uint16_t inst_name;
	uint16_t inputsz;
	uint16_t outputsz;
	char	buf[0];
}PACKED;

struct observable_return
{
	uint8_t msgid;
	uint8_t code;
	uint8_t errmsg;
	uint8_t token;
	uint8_t clslen;
	uint8_t uuid;
	int64_t instanceid;
	uint16_t outputsz;
	char	buf[0];
}PACKED;


}} // end of namespace zas::rpcbridge
/* EOF */

#endif