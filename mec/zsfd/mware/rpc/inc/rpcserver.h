#ifndef __CXX_ZAS_RPC_SERVER_IMPL_H__
#define __CXX_ZAS_RPC_SERVER_IMPL_H__

#include <string>

#include "std/list.h"
#include "utils/avltree.h"
#include "utils/evloop.h"
#include "utils/mutex.h"
#include "utils/wait.h"
#include "utils/uri.h"

#include "serviceinfo.h"


namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;

class server_service;
// host is container
class host_info
{
friend class rpcserver_impl;
friend class server_service;
public:
	host_info(const char* pkgname, const char* containername, bool bshared);
	~host_info();

	int addref();
	int release();

	// add host to list
	int addto_host_list();
	int attach_service(server_service *service);
	int detach_service(server_service *service);
	int add_wait_container_start(server_service *service, bool startcb);
	int set_evlclient(evlclient hostcli);
	static host_info* get_host_info(const char* containername);
	static int disconnect_host(const char* cliname, const char* instname);
	static int release_all_host(void);

	evlclient get_evlclient(void) {
		return _evlclient;
	}

	int set_starting(bool bstart) {
		return _f.starting = bstart ? 1 : 0;
	}

	int start_wait_service(void);
	int set_running(bool brun) {
		if (!_f.running && brun) {
			_f.running = brun;
			start_wait_service();
		}
		return _f.running = brun ? 1 : 0;
	}

	bool is_launched() {
		return (1 == _f.starting);
	}

	bool is_running() {
		return (1 == _f.running);
	}

	static int host_info_compared(avl_node_t *aa, avl_node_t *bb);

private:
	struct host_service_info {
		listnode_t	ownerlist;
		server_service *service;
	};

	struct wait_host_service {
		listnode_t	ownerlist;
		server_service *service;
		bool	startcb;
	};

	host_service_info* find_host_service(server_service *svc);
	wait_host_service* find_wait_host_service(server_service *svc);


private:
	listnode_t ownerlist;
	avl_node_t avlnode;
	std::string _pkg_name;
	std::string _container_inst;
	union {
		uint32_t _flags;
		struct {
			uint32_t starting : 1;
			uint32_t running : 1;
			uint32_t shared : 1;
		} _f;
	};
	evlclient  _evlclient;
	listnode_t	_service_list;
	listnode_t	_wait_host_list;
	int _refcnt;
};

class zrpc_server_evloop_listener : public evloop_listener
{
public:
	zrpc_server_evloop_listener(){}
	~zrpc_server_evloop_listener(){}
	virtual void accepted(evlclient client);
	virtual void connected(evlclient client);
	virtual void disconnected(const char* cliname, const char* instname);
};

class zrpc_server_mgr_impl;

class server_service : public service_info
{
friend class zrpc_server_mgr_impl;
public:
	server_service(const char* pkgname,
		const char* servicename,
		const char* instname,
		const char* executive,
		const char* ipaddr, uint32_t port,
		uint32_t version, uint32_t attributes);
	virtual ~server_service();
	
	int attach_host(void);
	int detach_host(void);

	int add_instance(const char* instname);
	int remove_instance(const char* instname);
	server_service* find_instance(const char* instname);
	
	int set_running(bool running);

	host_info* get_host(void) {
		return _host_info;
	}

	int launch_service(bool startcb = false);
	int request_container(service_request_type reqtype);
	int add_service_start_listener(evlclient sender, uint32_t seqid);
	int send_service_start_reply(evlclient sender, uint32_t seqid);

private:
	// current service is not start & client is runing
	// start service

	uint32_t alloc_service_id(void);

	struct service_start_lnr {
		listnode_t	ownerlist;
		uint32_t	seqid;
		evlclient	lnr_client;
	};

private:
	host_info* _host_info;
	listnode_t	_service_lnr_list;
	mutex _mut;
};


class rpcserver_impl : public service_mgr_base
{
public:
	rpcserver_impl();
	virtual ~rpcserver_impl();
	int addref();
	int release();

	int run(void);
	int load_config_file(uri& file);
	server_service* add_service(const char* pkgname, const char* servicename,
		const char* inst_name, const char* executive,
		const char* ipaddr, uint32_t port,
		uint32_t version, uint32_t attributes);
	int remove_service(const char* pkgname, const char* servicename);
	server_service* get_service(const char* pkgname,
		const char* servicename, const char* inst_name);
	int release_all_nodes(void);

	int handle_evl_package(evlclient sender,
		const package_header &pkghdr,
		const triggered_pkgevent_queue& queue);

	int handle_cmd_request_package(evlclient sender,
		const package_header &pkghdr,
		const triggered_pkgevent_queue& queue);

	int handle_service_package(evlclient sender,
		const package_header &pkghdr,
		const triggered_pkgevent_queue& queue);
	int start_service_internal(const char* pkgname,
	const char* svc_name, const char* inst_name, uint32_t action);

private:
	int handle_host_package(evlclient sender,
		const package_header &pkghdr,
		request_server* request);
	int handle_client_package(evlclient sender,
		const package_header &pkghdr,
		request_server* request);
	int reply_cmd_request(evlclient sender, int result, uint32_t seqid);

private:
	zrpc_server_evloop_listener _evl_lnr;
	mutex	_mut;
};

}}}// end of namespace zas::mware::rpc

#endif /*__CXX_ZAS_rpcserver_impl_H__*/