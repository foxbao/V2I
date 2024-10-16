
#ifndef __CXX_ZAS_RPC_OBSERVABLE_IMPL_H__
#define __CXX_ZAS_RPC_OBSERVABLE_IMPL_H__

#include <string>

#include "std/list.h"
#include "utils/avltree.h"
#include "rpchost.h"

namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;

class rpcobservable_client_instance
{
public:
	rpcobservable_client_instance();
	~rpcobservable_client_instance();
	int addref();
	int release();
public:
	avl_node_t avlnode;
	listnode_t ownerlist;
	std::string _pkg_name;
	std::string _inst_name;
	std::string _data;
	evlclient _client;
	void* _inst_id;
	int _refcnt;
};

class rpcobservable_impl
{
public:
	rpcobservable_impl();
	virtual ~rpcobservable_impl();

	static rpcobservable_impl* inst();

	rpcclass_node* regist_observable_interface(const char* clsname);
	rpcclass_node* get_observable_interface(const char *clsname);

	int add_observable_instance(const char *clsname, void *inst);
	void release_observable_instance(void *inst);

	int add_client_instance(const char *cliname, const char *inst_name,
		void *inst);
	void release_client_instance(void *inst);
	int remove_client_inst(rpcobservable_client_instance* clinode);

	rpcobservable_data* get_observable_data(void);

	void invoke_observable_method(void* inst_id,const char *clsname,
		const uint128_t& uuid, google::protobuf::Message* inparam,
		google::protobuf::Message* inout_param,
		uint methodid = 0, const char *method_name = NULL);

	void invoke_observable_method(void* inst_id, const char* clsname,
		const uint128_t& uuid, void* input, size_t insize,
		std::string &inout, size_t &inoutsize, uint32_t methodid = 0,
		const char* method_name = nullptr);
	
	void invoke_observable_method(void* inst_id,
		const char* cli_name, const char* inst_name,
		const char* clsname, const uint128_t& uuid, void* input, size_t insize,
		std::string &inout, size_t &inoutsize, uint32_t methodid,
		const char* method_name = nullptr);

	int handle_invoke_reply_package(evlclient sender,
		const package_header &pkghdr,
		const triggered_pkgevent_queue& queue);

	int handle_invoke_request_package(evlclient sender,
		const package_header &pkghdr,
		const triggered_pkgevent_queue& queue);

	rpcclass_instance_node* find_instance(void* inst_id);
	rpcobservable_client_instance* find_client_instance(void* inst_id);

	int reply_invoke(evlclient sender, const package_header &pkghdr,
		uint32_t req_action, uint32_t reply_type,
		void* instid, google::protobuf::Message *retmsg);

private:
	rpcclass_node* find_class_node_unlock(const char* clsname);
	static int rpcclass_node_compared(avl_node_t* aa, avl_node_t* bb);
	int release_all_class_node(void);

	rpcclass_instance_node* find_instance_unlocked(void* inst_id);
	static int observable_instnode_compared(avl_node_t* aa, avl_node_t* bb);
	int release_all_instance_node(void);

	rpcobservable_client_instance* find_client_instance_unlocked(void* inst);
	static int client_instnode_compared(avl_node_t* aa, avl_node_t* bb);
	int release_all_client_node(void);

private:
	listnode_t	_class_list;
	avl_node_t*	_class_tree;
	listnode_t	_instance_list;
	avl_node_t*	_instance_tree;
	listnode_t	_client_instance_list;
	avl_node_t*	_client_instance_tree;
	rpcobservable_data _obs_data;
	mutex _mut;
};

}}} // end of namespace zas::mware::zrpc

#endif /* __CXX_ZAS_ZRPC_HOST_H__
/* EOF */
