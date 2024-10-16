#ifndef ___CXX_ZAS_RPC_BRIDGE_SERVICE_IMPL_H__
#define ___CXX_ZAS_RPC_BRIDGE_SERVICE_IMPL_H__

#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <assert.h>
#include "std/list.h"
#include "utils/avltree.h"
#include "mware/rpc/rpcmgr.h"
#include "bridge_msg_def.h"

using namespace std;
using namespace zas::utils;
using namespace zas::mware::rpc;

namespace zas {
namespace servicebridge {

class service_node {

};

class rpcservice_listner;
class rpcservice_mgr_listner;
class rpcbridge_status_listener;

class rpcbridge_service: public zas::mware::rpc::bridge_service
{
friend class rpcbridge_service_mgr;
public:
	rpcbridge_service(const char* pkg_name, const char* service_name,
		const char* inst_name);
	~rpcbridge_service();
public:
	static zas::mware::rpc::bridge_service* create_instance(
		const char* pkg_name, const char* service_name, const char* inst_name);
	static void destory_instance(bridge_service* s);
	int on_recv_msg(void* data, size_t sz);

	int on_recvdata(utils::evlclient cli, void* inst_id, uint32_t seqid,
		std::string& clsname, uint128_t* method_uuid, uint8_t flag,
		void* indata, size_t insize, void* inoutdata, size_t inoutsize);
	int on_eventdata(std::string &eventid, uint8_t action);
	int settopic(std::string &topic);

	int settoken(std::string &token) {
		_service_token = token;
		return 0;
	}

private:
	int handle_method_back(reply_method_invoke *ret);
	int handle_event_trigger(recv_event_invoke *ret);
	int handle_observable_call(observable_called *ret);

	struct msg_session_hdr
	{
		listnode_t ownerlist;
		evlclient client;
		uint32_t seqid;
	};

	int add_session(evlclient evl, uint32_t seqid);
	evlclient get_add_session(uint64_t evl, uint32_t seqid);
	int clear_session(void);

private:
	avl_node_t	avlnode;
	listnode_t	ownerlist;
	listnode_t	_session_list;
	rpcbridge	_rpc_bridge_obj;
	std::string _pkg_name;
	std::string _service_name;
	std::string _inst_name;
	std::string _service_topic;
	std::string _service_token;
	rpcservice_listner*	_service_lnr;
};

class rpcbridge_service_mgr
{
public:
	rpcbridge_service_mgr();
	~rpcbridge_service_mgr();

	static rpcbridge_service_mgr* inst();

	int add_service(rpcbridge_service* svc);
	int remove_service(rpcbridge_service* svc);
	int remove_all_service(void);
	rpcbridge_service* find_service(const char* pkg_name,
		const char* service_name, const char* inst_name);
	int get_service_topic(rpcbridge_service* svc);

	int on_recv_msg(void* data, size_t sz);
	void on_mqtt_connect(void);
private:
	rpcbridge_service* find_service_unlock(const char* pkg_name,
		const char* service_name, const char* inst_name);
	static int rpc_bridge_service_compared(avl_node_t* aa, avl_node_t* bb);

private: 
	int init_mqtt();
private:
	listnode_t	_service_list;
	avl_node_t*	_service_tree;
	rpcservice_mgr_listner* _mqtt_lnr;
	rpcbridge_status_listener* _mqtt_sta_lnr;
	mutex	_mut;
};

}} // end of namespace zas::utils
/* EOF */

#endif