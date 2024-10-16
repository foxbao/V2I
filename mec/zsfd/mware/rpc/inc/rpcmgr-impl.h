
#ifndef __CXX_ZAS_RPC_MGR_IMPL_H__
#define __CXX_ZAS_RPC_MGR_IMPL_H__

#include <string>
#include "utils/evloop.h"
#include "utils/wait.h"
#include "utils/avltree.h"
#include "std/list.h"
#include "rpc_pkg_def.h"

namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;

class rpchost_impl;
class rpchost_mgr;
class rpcclient_impl;
class rpcclient_mgr;
class rpcserver_impl;
class rpcbridge_mgr;
class rpcbridge_impl;

class rpcmgr_impl {
public:
	rpcmgr_impl();
	~rpcmgr_impl();
	rpchost_impl* load_service(const char* service_name, 
		const char* inst_name, const char* executive);
	rpchost_impl* get_service(const char* service_name, 
		const char* inst_name);
	rpcserver_impl* get_server(void);

	int start_service(const char* pkgname, const char* service_name,
		const char* inst_name);
	int stop_service(const char* pkgname, const char* service_name,
		const char* inst_name);
	int terminate_service(const char* pkgname, const char* service_name,
		const char* inst_name);
	int register_service(const char* service_name, const char* inst_name,
		void* create_svc, void* destory_svc);
	service* get_service_impl(const char* service_name, const char* inst_name);
	rpcclient_mgr* get_client_mgr(void);
	static rpcmgr_impl* instance();

	bool handle_evl_package(evlclient sender, const package_header &pkghdr,
		const triggered_pkgevent_queue& queue);

	rpcbridge_impl* load_bridge(const char* pkg_name,
		const char* service_name, const char* inst_name);
	rpcbridge_impl* get_bridge(const char* pkg_name, const char* service_name,
		const char* inst_name);

	int register_bridge(create_bridge_service create,
		destory_bridge_service destory);

private:
	int load_all_shared_services_unlock(void);

	int add_package_listener(void);
	int remove_package_listener(void);
	int handle_server_reply(evlclient sender,
		const package_header &pkghdr,
		const triggered_pkgevent_queue& queue);
	int load_info(cmd_service_info* info, const char* pkgname,
		const char* service_name, const char* inst_name);
	int request_service_cmd(uint16_t reqtype, const char* pkgname,
		const char* service_name, const char* inst_name);
	int handle_cmd_reply_package(evlclient sender,
		const package_header &pkghdr,
		const triggered_pkgevent_queue& queue);

private:
	rpchost_mgr*			_rpc_host;
	rpcclient_mgr*		_rpc_client;
	rpcserver_impl*			_rpc_server;
	rpcbridge_mgr*			_rpc_bridge;
	mutex	_mut;
};

}}} // end of namespace zas::mware::rpc

#endif /* __CXX_ZAS_rpcmgr_impl_H__
/* EOF */
