
#ifndef __CXX_ZAS_RPC_PKG_DEF_H__
#define __CXX_ZAS_RPC_PKG_DEF_H__

#include <string>
#include "utils/evloop.h"
#include "mware/pkg_id_def.h"

namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;

// client with server
#define REQUEST_SERVER_CTRL			\
	EVL_MAKE_PKGID(SYSTEM_RPC_PACKAGE_CLSID_CTRL, 1)
#define SERVER_REPLY_CTRL			\
	EVL_MAKE_PKGID(SYSTEM_RPC_PACKAGE_CLSID_CTRL, 2)

// server with service
#define REQUEST_START_SERVICE_CTRL			\
	EVL_MAKE_PKGID(SYSTEM_RPC_PACKAGE_CLSID_CTRL, 3)
#define REPLY_START_SERVICE_CTRL			\
	EVL_MAKE_PKGID(SYSTEM_RPC_PACKAGE_CLSID_CTRL, 4)

// rpc invoke
#define INVOKE_METHOD_REQUEST_CTRL			\
	EVL_MAKE_PKGID(SYSTEM_RPC_PACKAGE_CLSID_CTRL, 5)
#define INVOKE_METHOD_REPLY_CTRL			\
	EVL_MAKE_PKGID(SYSTEM_RPC_PACKAGE_CLSID_CTRL, 6)

// rpc observable invoke
#define OBSERVABLE_INVOKE_METHOD_REQUEST_CTRL			\
	EVL_MAKE_PKGID(SYSTEM_RPC_PACKAGE_CLSID_CTRL, 7)
#define OBSERVABLE_INVOKE_METHOD_REPLY_CTRL			\
	EVL_MAKE_PKGID(SYSTEM_RPC_PACKAGE_CLSID_CTRL, 8)

// client with service
#define CLIENT_REQUEST_SERVICE_CTRL			\
	EVL_MAKE_PKGID(SYSTEM_RPC_PACKAGE_CLSID_CTRL, 9)
#define SERVICE_REPLY_CLIENT_CTRL			\
	EVL_MAKE_PKGID(SYSTEM_RPC_PACKAGE_CLSID_CTRL, 10)

// cmd with server
#define CMD_REQUEST_SERVER_CTRL			\
	EVL_MAKE_PKGID(SYSTEM_RPC_PACKAGE_CLSID_CTRL, 11)
#define CMD_SERVER_REPLY_CTRL			\
	EVL_MAKE_PKGID(SYSTEM_RPC_PACKAGE_CLSID_CTRL, 12)

enum request_type
{
	reqtype_service_start = 0,
	reqtype_service_stop,
	reqtype_client
};

enum cmd_request_type
{
	cmd_reqtype_start_service  = 0,
	cmd_reqtype_stop_service,
	cmd_reqtype_terminate_service
};

enum reply_type
{
	reply_type_service = 0,
	reply_type_client,
	reply_type_other,
};

enum invoke_request_action
{
	req_action_call_method = 0,
	req_action_get_instance,
	req_action_get_singleton,
	req_action_destory,
};

enum invoke_reply_type
{
	reply_type_unknown = 0,
	reply_type_success,
	reply_type_cannot_create_instance,
	reply_type_no_instance,
	reply_type_no_method,
	reply_type_no_class,
	reply_type_no_service,
	reply_type_get_instance_param_err,
	reply_type_invalid_instid,
	reply_type_evl_pkg_error,
	reply_type_other_error,
};

struct client_base_info
{
	std::string client_name;
	std::string inst_name;	
};

union service_pkginfo_validity_u
{
	uint32_t all;
	struct {
		uint32_t pkgname			: 1;
		uint32_t name				: 1;
		uint32_t inst_name			: 1;
		uint32_t executive			: 1;
		uint32_t ipaddr				: 1;
		uint32_t port				: 1;
		uint32_t version			: 1;
		uint32_t id					: 1;
		uint32_t accessible			: 1;
		uint32_t shared				: 1;
		uint32_t signleton			: 1;
		uint32_t cli_name			: 1;
		uint32_t cli_inst_name		: 1;
	} m;
};

struct service_pkginfo{
	service_pkginfo();
	service_pkginfo_validity_u validity;
	uint16_t pkgname;
	uint16_t name;	
	uint16_t inst_name;	
	uint16_t executive;
	uint16_t ipaddr;
	uint16_t port;
	uint32_t version;
	uint32_t id;
	uint16_t accessible;
	uint16_t shared;
	uint16_t signleton;
	uint16_t client_name;
	uint16_t cli_inst_name;	
	uint16_t bufsize;
	char buf[0];
};

struct request_server
{
	uint16_t request;
	service_pkginfo service_info;
};

struct server_reply
{
	int result;
	uint16_t reply;
	service_pkginfo service_info;
};

enum service_request_type
{
	zrpc_svc_req_start = 0,
	zrpc_svc_start_request,
	zrpc_svc_stop_request,
	zrpc_svc_terminate_request,
};

struct request_service
{
	uint16_t request;
	service_pkginfo service_info;
};

struct service_reply
{
	int result;
	service_pkginfo service_info;
};

union invoke_reqinfo_validity_u
{
	uint32_t all;
	struct {
		uint32_t class_name			: 1;
		uint32_t renew_inst			: 1;
		uint32_t method_name		: 1;
		uint32_t method_id			: 1;
		uint32_t inparam			: 1;
		uint32_t inoutparam			: 1;
		uint32_t instid				: 1;
	} m;
};

struct invoke_reqinfo{
	invoke_reqinfo();
	invoke_reqinfo_validity_u validity;
	uint16_t class_name;
	uint16_t renew_inst;
	uint16_t method_name;
	uint16_t method_id;
	uint32_t inparam;
	uint32_t inparam_sz;
	uint32_t inoutparam;
	uint32_t inoutparam_sz;
	size_t	 instid;
	uint32_t bufsize;
	char buf[0];
};

struct invoke_request
{
	uint16_t req_action;
	uint32_t svc_id;
	uint128_t method_uuid;
	invoke_reqinfo req_info;
};

struct invoke_reply_info {
	uint16_t result;
	char buf[0];
};

struct invoke_reply
{
	uint16_t req_action;
	uint16_t reply_type;
	size_t	 instid;
	invoke_reply_info reply_info;
};

union observable_invoke_reqinfo_validity_u
{
	uint32_t all;
	struct {
		uint32_t class_name			: 1;
		uint32_t method_name		: 1;
		uint32_t method_id			: 1;
		uint32_t inparam			: 1;
		uint32_t inoutparam			: 1;
		uint32_t instid				: 1;
	} m;
};

struct observable_invoke_reqinfo{
	observable_invoke_reqinfo();
	observable_invoke_reqinfo_validity_u validity;
	uint16_t class_name;
	uint16_t method_name;
	uint16_t method_id;
	uint32_t inparam;
	uint32_t inparam_sz;
	uint32_t inoutparam;
	uint32_t inoutparam_sz;
	size_t	 instid;
	uint32_t bufsize;
	char buf[0];
};

struct observable_invoke_request
{
	uint16_t req_action;
	uint128_t method_uuid;
	observable_invoke_reqinfo req_info;
};

struct observable_invoke_reply_info {
	uint16_t result;
	char buf[0];
};

struct observable_invoke_reply
{
	uint16_t req_action;
	uint16_t reply_type;
	size_t	 instid;
	observable_invoke_reply_info reply_info;
};


enum client_request_service_type
{
	cli_req_svc_stop = 0,
};

struct client_request_service
{
	uint16_t request;
	uint32_t svc_id;
};

struct service_reply_client
{
	int result;
};

union cmd_service_info_validity_u
{
	uint32_t all;
	struct {
		uint32_t pkgname			: 1;
		uint32_t name				: 1;
		uint32_t inst_name			: 1;
	} m;
};

struct cmd_service_info{
	cmd_service_info();
	cmd_service_info_validity_u validity;
	uint16_t pkgname;
	uint16_t name;	
	uint16_t inst_name;	
	uint16_t bufsize;
	char buf[0];
};

struct cmd_request_server
{
	uint16_t request;
	cmd_service_info service_info;
};

struct cmd_server_reply
{
	int result;
	uint16_t reply;
};

//communicate with sysd
EVL_DEFINE_PACKAGE(request_server, REQUEST_SERVER_CTRL);
EVL_DEFINE_PACKAGE(server_reply, SERVER_REPLY_CTRL);
EVL_DEFINE_PACKAGE(request_service, REQUEST_START_SERVICE_CTRL);
EVL_DEFINE_PACKAGE(service_reply, REPLY_START_SERVICE_CTRL);
EVL_DEFINE_PACKAGE(invoke_request, INVOKE_METHOD_REQUEST_CTRL);
EVL_DEFINE_PACKAGE(invoke_reply, INVOKE_METHOD_REPLY_CTRL);
EVL_DEFINE_PACKAGE(observable_invoke_request,	\
	OBSERVABLE_INVOKE_METHOD_REQUEST_CTRL);
EVL_DEFINE_PACKAGE(observable_invoke_reply,	\
	OBSERVABLE_INVOKE_METHOD_REPLY_CTRL);
EVL_DEFINE_PACKAGE(client_request_service, CLIENT_REQUEST_SERVICE_CTRL);
EVL_DEFINE_PACKAGE(service_reply_client, SERVICE_REPLY_CLIENT_CTRL);
EVL_DEFINE_PACKAGE(cmd_request_server, CMD_REQUEST_SERVER_CTRL);
EVL_DEFINE_PACKAGE(cmd_server_reply, CMD_SERVER_REPLY_CTRL);
}}} // end of namespace zas::mware::rpc


#endif /* __CXX_ZAS_ZRPC_PKG_DEF_H__
/* EOF */
