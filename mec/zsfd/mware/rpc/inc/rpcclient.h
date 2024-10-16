
#ifndef __CXX_ZAS_RPC_CLIENT_H__
#define __CXX_ZAS_RPC_CLIENT_H__

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

class rpcclient_mgr;
class rpchost_impl;
class rpcclient_impl
{
friend class rpcclient_mgr;
public:
	rpcclient_impl(const char* pkgname, 
		const char* service_name, const char* inst_name);
	~rpcclient_impl();
	int addref();
	int release();
	int get_service_from_server(void);
	int request_service_stop(void);

	bool is_ready(void) {
		return (_f.ready == 1) ? true : false;
	}

	bool is_destory(void) {
		return (_f.desotry == 1) ? true : false;
	}

	bool is_local_service(void) {
		return (_f.local_service == 1 && !_host_impl) ? true : false;
	}

	void set_ready() {
		_f.ready = 1;
	}

	void set_destory() {
		_f.desotry = 1;
	}

	void set_local_service() {
		_f.local_service = 1;
	}

	evlclient get_client(void) {
		return _client;
	}

	uint32_t get_service_id(void) {
		return _service_id;
	}
	
	int handle_local_invoke(void** instid, std::string& clsname,
		uint128_t& method_uuid, google::protobuf::Message *inparam,
		google::protobuf::Message *inout_param, bool renew_inst);

	int handle_get_instance(void** instid, std::string& clsname,
		void* data, size_t sz, bool singleton);

private:
	uint32_t get_string_size(void);
	int set_service_client(const char* client_name,
		const char* inst_name,uint32_t service_id);
	int load_info(service_pkginfo* info);
	int check_info(service_pkginfo& info);

	int check_local_service(void);

	union {
		uint32_t _flags;
		struct {
			uint32_t	ready	 : 1;
			uint32_t	desotry	 : 1;
			uint32_t	local_service	 : 1;
		} _f;
	};

private:
	listnode_t	ownerlist;
	avl_node_t	avlnode;
	std::string 	_pkg_name;
	std::string 	_service_name;
	std::string		_inst_name;
	std::string		_client_name;
	std::string		_cliinst_name;
	rpchost_impl*	_host_impl;
	uint32_t		_service_id;
	evlclient		_client;
	int 			_refcnt;

};

class rpcclient_mgr
{
public:
	rpcclient_mgr();
	~rpcclient_mgr();

	rpcclient_impl* add_client(const char* pkgname, const char* service_name,
		const char* inst_name);
	int remove_client(const char* pkgname, const char* service_name,
		const char* inst_name);
	rpcclient_impl* get_client(const char* pkgname, const char* service_name,
		const char* inst_name);
	int handle_server_reply_package(evlclient sender,
		const package_header &pkghdr,
		const triggered_pkgevent_queue& queue,
		server_reply* reply);
	int handle_service_reply_package(evlclient sender,
		const package_header &pkghdr,
		const triggered_pkgevent_queue& queue);
	
private:
	rpcclient_impl* find_client_unlock(const char* pkgname,
		const char* service_name,
		const char* inst_name);
	static int zrpc_client_compared(avl_node_t* aa, avl_node_t* bb);
	int release_all_nodes(void);

private:
	listnode_t _client_list;
	avl_node_t* _client_tree;
	mutex	_mut;
};

}}} // end of namespace zas::mware::rpc

#endif /* __CXX_ZAS_ZRPC_CLIENT_H__
/* EOF */
