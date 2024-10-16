
#ifndef __CXX_ZAS_RPC_HOST_H__
#define __CXX_ZAS_RPC_HOST_H__

#include <string>
#include "std/list.h"
#include "utils/avltree.h"
#include "utils/evloop.h"
#include "utils/mutex.h"
#include "utils/wait.h"
#include "mware/rpc/rpcmgr.h"
#include "serviceinfo.h"
#include "rpc_pkg_def.h"

namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;

class rpchost_mgr;
class rpcobservable_impl;

enum rpcclass_node_flag
{
	rpccls_node_singleton = 1,
	rpccls_node_observable = 2,
};

class rpcmethod_node
{
public:
	rpcmethod_node(const uint128_t& method_uuid,
	pbf_wrapper inparam, pbf_wrapper inout_param, void* hdr);
	~rpcmethod_node() {_handler = NULL;}

public:
	avl_node_t	avlnode;
	listnode_t	ownerlist;
	uint128_t	_uuid;
	pbf_wrapper _inparam;
	pbf_wrapper _inout_param;
	void* _handler;
};

// rpcclass_node is zrpc_interface_impl
class rpcclass_node
{
	friend class rpchost_impl;
	friend class rpcobservable_impl;
public:
	rpcclass_node(const char* clsname, void* factory, pbf_wrapper param);
	~rpcclass_node();
	int addref() {
		return __sync_add_and_fetch(&_refcnt, 1);
	}
	int release();
public:
	void set_singleton(bool singleton);
	int add_method(const uint128_t& uuid,
		pbf_wrapper inparam,
		pbf_wrapper inout_param,
		rpcmethod_handler hdr);
	rpcmethod_node* get_method_node(const uint128_t& uuid);

public:
	void setflags(uint32_t flags) {
		_flags |= flags;
	}

	void clearflags(uint32_t flags) {
		_flags &= ~flags;
	}

	bool testflags(uint32_t flags) {
		return ((_flags & flags) == flags) ? true : false;
	}

	void* getfatory(void) {
		return _factory_hdr;
	}

	pbf_wrapper getparam(void) {
		return _factory_param;
	}

private:
	rpcmethod_node* find_method_node_unlock(const uint128_t& uuid);
	static int rpcmethod_node_compared(avl_node_t* aa, avl_node_t* bb);
	void release_all_method_node(void);

private:
	avl_node_t	avlnode;
	listnode_t	ownerlist;

	std::string _class_name;
	uint32_t 	_flags;
	void*		_factory_hdr;
	pbf_wrapper	_factory_param;
	int			_refcnt;

	avl_node_t* _method_tree;
	listnode_t	_method_list;
	mutex	_mut;
};

class rpcclass_instance_node
{
public:
	rpcclass_instance_node()
	:_refcnt(1), _flags(0) {}
	~rpcclass_instance_node() {}
	int addref();
	int release();
	int set_internal_create();
	int set_keep_instance();
	int clear_keep_instance();
	avl_node_t avlnode;
	avl_node_t avlclsnode;
	listnode_t ownerlist;

	rpcclass_node*	_class_node;
	void*			_inst_id;

private:
	int _refcnt;
	union {
		uint32_t _flags;
		struct {
			uint32_t keep_instacne : 1;
		} _f;
	};


};

class rpchost_impl;

class rpcinst_node_mgr
{
	friend class rpchost_impl;
public:
	rpcinst_node_mgr();
	~rpcinst_node_mgr();
	rpcclass_instance_node* find_create_instance_node(void** instid,
		rpcclass_node* clsnode, bool renew_inst,
		void* data = nullptr, size_t sz = 0);
	int add_instid(void* instid, rpcclass_node* clsnode);
	int remove_instance_node(void* inst_id);
	int keep_instance_node(void* inst_id);
	int handle_invoke(rpcclass_instance_node* inst_node, rpcclass_node* clsnode,
		uint128_t& method_uuid, invoke_reqinfo& rinfo,
		google::protobuf::Message **retmsg, bool renew_inst);

	int handle_local_invoke(rpcclass_instance_node* inst_node,
		rpcclass_node* clsnode, uint128_t& method_uuid,
		google::protobuf::Message *inparam,
		google::protobuf::Message *outparam, bool renew_inst);

	rpcclass_instance_node* find_instance(void* inst_id);
	rpcclass_instance_node* find_singleton_instance(void* inst_id);
	rpcclass_instance_node* find_singleton_instance_class(
		rpcclass_node* clsnode);
	static int instance_node_compared(avl_node_t* aa, avl_node_t* bb);
	static int instance_class_compared(avl_node_t* aa, avl_node_t* bb);
		
private:
	rpcclass_instance_node* find_instance_unlocked(void* inst_id);
	rpcclass_instance_node* find_singleton_instance_unlocked(void* inst_id);
	rpcclass_instance_node* 
		find_singleton_instance_class_unlocked(rpcclass_node* clsnode);
	int release_all_instance(void);

private:
	listnode_t	_instance_list;
	avl_node_t*	_instance_tree;
	avl_node_t*	_singleton_tree;
	avl_node_t* _sinst_class_tree;

	mutex _mut;
};


class service_visitor
{
	friend class rpchost_impl;
public:
	service_visitor(evlclient cli);
	~service_visitor();

	bool check_inst_node_exist(rpcclass_instance_node *inst_node);
	int add_inst_node(rpcclass_instance_node *inst_node);

	struct  visitor_inst_node
	{
		avl_node_t 	avlnode;
		listnode_t ownerlist;
		rpcclass_instance_node *inst_node;
	};
	static int compare_visitor_inst_node(avl_node_t* aa, avl_node_t* bb);
	int releaseallnode(void);

private:
	evlclient _evlclient;
	avl_node_t avlnode;
	listnode_t ownerlist;
	listnode_t _inst_node_list;
	avl_node_t*	_inst_node_tree;
	mutex _mut;
};

class rpchost_impl : public service_info
{
friend class rpchost_mgr;
public:
	rpchost_impl(const char* pkgname,
		const char* servicename,
		const char* instname,
		const char* executive,
		const char* ipaddr, uint32_t port,
		uint32_t version, uint32_t attributes);
	virtual ~rpchost_impl();
	int run();
	int start_service(void);
	int stop_service(evlclient sender);
	int terminate(void);

public:
	int add_instance(const char* instname);
	int remove_instance(const char* instname);
	rpchost_impl* find_instance(const char* instname);

	int set_service_id(uint32_t service_id);
	int set_service_impl_method(void* create, void*destory);
	service* get_service_impl(void);
	int load_service_impl(void);
	int load_service_dll(void);

	rpcclass_node* register_interface(const char* clsname,
		void* factory, pbf_wrapper param, uint32_t flags = 0);
	rpcclass_node* get_interface(const char *clsname);
	int add_instid(const char* clsname, void* inst_id);
	int release(void* inst_id);
	int keep_instance(void* inst_id);
	int release();
	
	int handle_invoke(evlclient sender, void** instid, std::string& clsname,
		uint128_t& method_uuid, invoke_reqinfo& rinfo,
		google::protobuf::Message **retmsg, bool renew_inst);
	int handle_get_instance(evlclient sender, void** instid,
		std::string& clsname, void* data, size_t sz, bool singleton);

	int handle_local_invoke(void** instid,
		std::string& clsname, uint128_t& method_uuid,
		google::protobuf::Message *inparam,
		google::protobuf::Message *outparam, bool renew_inst);

	bool check_service_right_unlock(void);

private:
	int regist_to_server(void);
	int request_to_server(uint16_t reqtype);
	int request_to_server_unlock(uint16_t reqtype);
	rpcclass_node* find_class_node_unlock(const char* clsname);
	static int rpcclass_node_compared(avl_node_t* aa, avl_node_t* bb);
	int release_all_class_node(void);

	service_visitor* find_and_create_visitor(evlclient evlclient, rpcclass_instance_node* node);
	bool check_visitor_right_unlock(evlclient evlclient);
	int remove_service_visitor(evlclient client);
	int release_service_visitor(evlclient client);
	service_visitor* find_service_visitor_unlocked(evlclient client);
	int release_all_vistor(void);
	static int service_visitor_compared(avl_node_t* aa, avl_node_t* bb);

	int service_status_change_stop(void);


private:
	avl_node_t	avlactivenode;
	listnode_t	_class_list;
	avl_node_t*	_class_tree;
	listnode_t	_service_visitor_list;
	avl_node_t*	_service_visitor_tree;
	void*		_servcie_dll_handle;
	void*		_service_create_instance;
	void*		_service_destory_instance;
	service*	_service_handle;
	rpcinst_node_mgr	_inst_node_mgr;
	mutex _mut;
};

class rpchost_mgr : public service_mgr_base
{
public:
	rpchost_mgr();
	~rpchost_mgr();
	int addref(void);
	int release(void);

	//zrpc host initialize
	rpchost_impl* add_service(const char* pkgname, const char* servicename,
		const char* executive,
		const char* ipaddr, uint32_t port,
		uint32_t version, uint32_t attributes);
	int remove_service(const char* pkgname, const char* servicename);
	rpchost_impl* get_service(const char* pkgname, const char* servicename);
	int release_all_nodes(void);
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

	int reply_invoke(evlclient sender, const package_header &pkghdr,
		uint32_t req_action, uint32_t reply_type,
		void* instid, google::protobuf::Message *retmsg);

	rpchost_impl* find_active_service(uint32_t service_id);
	rpchost_impl* find_service(const char* pkgname, const char* servicename);

private:
	int add_active_service(rpchost_impl* impl);
	int remove_active_service(uint32_t service_id);
	int remove_active_service(rpchost_impl* impl);
	rpchost_impl* find_active_service_unlock(uint32_t service_id);
	static int service_active_compared(avl_node_t* aa, avl_node_t* bb);
	
private:
	avl_node_t*	_active_service_tree;
	mutex		_mut;
	int			_refcnt;
};
}}} // end of namespace zas::mware::rpc

#endif /* __CXX_ZAS_ZRPC_HOST_H__
/* EOF */
