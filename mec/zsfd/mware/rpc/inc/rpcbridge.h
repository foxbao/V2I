
#ifndef __CXX_ZAS_RPC_BRIDGE_H__
#define __CXX_ZAS_RPC_BRIDGE_H__

#include <string>
#include "utils/evloop.h"
#include "utils/wait.h"
#include "utils/mutex.h"
#include "serviceinfo.h"
#include "rpc_pkg_def.h"
#include "rpc/rpcmgr.h"

namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;

class rpcbridge_impl;
class rpc_bridge_inst
{
public:
	rpc_bridge_inst(rpcbridge_impl* impl);
	~rpc_bridge_inst();
	int addref();
	int release();
	void*		_instid;
	listnode_t ownerlist;
	avl_node_t avlnode;
private:
	rpcbridge_impl* _header;
	int		_refcnt;
};

class rpc_bridge_client
{
public:
	rpc_bridge_client();
	~rpc_bridge_client();
	listnode_t ownerlist;
	avl_node_t avlnode;
	evlclient _client;
	struct bridge_inst_node {
		listnode_t ownerlist;
		rpc_bridge_inst* inst;
	};
	int add_bridge_inst_node(rpc_bridge_inst* inst);
	bridge_inst_node* find_inst_node(rpc_bridge_inst* inst);
	int remove_bridge_inst_node(rpc_bridge_inst* inst);
	int remove_all_nodes(void);
private:
	listnode_t _client_inst_list;
};


class rpcbridge_mgr;
class rpcbridge_impl
{
friend class rpcbridge_mgr;
public:
	rpcbridge_impl();
	rpcbridge_impl(const char* pkgname, const char* service_name,
		const char* inst_name, bridge_service* service);
	~rpcbridge_impl();
	int addref();
	int release();
	int senddata(evlclient evl, void* inst_id, uint32_t seqid,
		uint32_t result, uint8_t flag, void* data, size_t sz);
	int sendevent(std::string &eventid, void* data, size_t sz);
	int run();
	int stop();
	int stop_service(evlclient client);

	bool is_ready(void) {
		return (_f.ready == 1) ? true : false;
	}

	bool is_destory(void) {
		return (_f.desotry == 1) ? true : false;
	}

	void set_ready() {
		_f.ready = 1;
	}

	void set_destory() {
		_f.desotry = 1;
	}

	uint32_t get_service_id(void) {
		return _service_id;
	}
	int set_service_id(uint32_t service_id)
	{
		_service_id = service_id;
		return 0;
	}

	int update_info(service_pkginfo &info,
		service_pkginfo_validity_u &val);
	int remove_inst_unlock(rpc_bridge_inst* inst);
	int handle_invoke(evlclient cli, void* inst_id, uint32_t seqid, 
		std::string &clsname, uint128_t& method_uuid,
		bool renew_inst, void* indata, size_t insize,
		void* inoutdata, size_t inoutsize);
	int handle_get_instance(evlclient cli,void* instid, uint32_t seqid,
		std::string& clsname, bool singleton, void* data, size_t sz);

	int reply_invoke(evlclient sender,  uint32_t seqid,
		uint32_t req_action, uint32_t reply_type, void* instid,
		void *data = nullptr, size_t sz = 0);
private:
	uint32_t get_string_size(void);
	int load_info(service_pkginfo* info);
	int check_info(service_pkginfo& info);

	int request_to_server_unlock(uint16_t reqtype);

	union {
		uint32_t _flags;
		struct {
			uint32_t	ready	 : 1;
			uint32_t	desotry	 : 1;
		} _f;
	};

private:
	int add_client(evlclient cli, rpc_bridge_inst* inst);
	rpc_bridge_inst* find_inst_unlock(void* inst_id);
	rpc_bridge_client* find_client_unlock(evlclient cli);
	int remove_client_unlock(evlclient cli);
	int remove_all_client(void);


	static int zrpc_bridge_inst_compared(avl_node_t* aa, avl_node_t* bb);
	static int zrpc_bridge_client_compared(avl_node_t* aa, avl_node_t* bb);

private:
	listnode_t	ownerlist;
	avl_node_t	avlnode;
	avl_node_t	avlactivenode;
	std::string 	_pkg_name;
	std::string 	_service_name;
	std::string		_inst_name;
	uint32_t		_service_id;
	bridge_service	*_bridge_service;

	listnode_t		_inst_list;
	avl_node_t*		_inst_tree;
	listnode_t		_client_list;
	avl_node_t*		_client_tree;
	int 			_refcnt;

};

class rpcbridge_mgr
{
public:
	rpcbridge_mgr();
	~rpcbridge_mgr();

	int load_bridge(const char* bridge_exec);
	int register_bridge(create_bridge_service create,
		destory_bridge_service destory);

	rpcbridge_impl* add_bridge(const char* pkgname, const char* service_name,
		const char* inst_name);
	int add_bridge(rpcbridge_impl *bridgeimpl);
	int remove_bridge(const char* pkgname, const char* service_name,
		const char* inst_name);
	int remove_bridge_by_client(evlclient client);
	rpcbridge_impl* get_bridge(const char* pkgname, const char* service_name,
		const char* inst_name);
		
	int handle_server_reply_package(evlclient sender,
		const package_header &pkghdr,
		const triggered_pkgevent_queue& queue,
		server_reply* reply);

	int handle_service_start(evlclient sender,
		const package_header &pkghdr,
		const triggered_pkgevent_queue& queue);

	int handle_invoke_request(evlclient sender,
		const package_header &pkghdr,
		const triggered_pkgevent_queue& queue);

	int handle_client_request(evlclient sender,
		const package_header &pkghdr,
		const triggered_pkgevent_queue& queue);

	int reply_invoke(evlclient sender,  uint32_t seqid,
		uint32_t req_action, uint32_t reply_type, void* instid,
		void *data = nullptr, size_t sz = 0);

	int add_active_bridge(rpcbridge_impl* impl);
	int remove_active_bridge(rpcbridge_impl* impl);
	int remove_active_bridge(uint32_t service_id);
	rpcbridge_impl* find_active_bridge(uint32_t service_id);

private:
	rpcbridge_impl* find_bridge_unlock(const char* pkgname,
		const char* service_name,
		const char* inst_name);
	static int zrpc_bridge_compared(avl_node_t* aa, avl_node_t* bb);
	int release_all_nodes(void);
	rpcbridge_impl* find_active_bridge_unlock(uint32_t service_id);
	static int service_bridge_compared(avl_node_t* aa, avl_node_t* bb);

private:
	listnode_t 	_bridge_list;
	avl_node_t* _bridge_tree;
	avl_node_t* _active_bridge_tree;
	void*		_bridge_dll_handle;
	create_bridge_service	_bridge_create;
	destory_bridge_service	_bridge_destory;
	mutex	_mut;
};

}}} // end of namespace zas::mware::rpc

#endif /* __CXX_ZAS_ZRPC_BRIDGE_H__
/* EOF */
