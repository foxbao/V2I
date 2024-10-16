
#ifndef __CXX_ZAS_RPC_PROXY_BASE_H__
#define __CXX_ZAS_RPC_PROXY_BASE_H__

#include <string>

#include "rpc/rpcmgr.h"

#include "std/list.h"
#include "utils/avltree.h"
#include "utils/mutex.h"

#include "rpc_pkg_def.h"

namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;

class rpcproxy_impl;
struct rpcinvoke_data
{
	uint16_t		req_type;
	uint16_t		reply_type;
	void* 			inst;
	rpcproxy_static_data*	data;
	google::protobuf::Message*	retmsg;
	rpcproxy_impl*	impl;
};

class rpcproxy_impl
{
public:
	rpcproxy_impl(void* inst, rpcproxy_static_data *data, bool renew = true);
	virtual ~rpcproxy_impl();
	int addref();
	int release();

public:
	bool is_vaild(void) {
		return (!_inst_id) ? false : true;
	}

	void* get_instid(void) const {
		return _inst_id;
	}

	const char* get_class_name(void) {
		assert(NULL != _static_data);
		return _static_data->class_name;
	}

	void set_invoke_result(std::string &data) {
		_invoke_result.clear();
		_invoke_result = data;
	}

	void get_invoke_result(std::string &data) {
		data = _invoke_result;
	}

	bool get_renew_instance(void) {
		return _renew_inst;
	}

public:
	static rpcproxy_impl* invoke(void* instid, uint128_t& uuid,
		google::protobuf::Message* inparam,
		google::protobuf::Message* inout_param,
		rpcproxy_static_data *data,
		uint methodid = 0,  const char *method_name = NULL);
	static rpcproxy_impl* get_singleton_instance(rpcproxy_static_data *data);
	static rpcproxy_impl* get_instance(google::protobuf::Message* inparam,
		rpcproxy_static_data *data);

public:
	static int set_invoke_request_unlock(invoke_request_action action,
		invoke_request& rinfo, uint128_t* uuid, void* instid, 
		google::protobuf::Message* inparam,
		google::protobuf::Message* inout_param,
		rpcproxy_static_data *data,
		bool renew, uint32_t svcid,
		uint methodid = 0,  const char *method_name = NULL);

	static rpcproxy_impl* find_inst_unlock(void* inst_id,
		rpcproxy_static_data *data);
	static rpcproxy_impl* find_inst(void* inst_id,
		rpcproxy_static_data *data);
	static rpcproxy_impl* from_instid(void* inst_id,
		rpcproxy_static_data *data);
	static bool check_service_unlock(std::string& errdesc,
		rpcproxy_static_data *data);
	static int handle_invoke_reply_package(evlclient sender,
		const package_header &pkghdr,
		const triggered_pkgevent_queue& queue);

	int add_to_proxy_tree(void);

private:
	void destory(void);

	static int rpcproxy_impl_compared(avl_node_t *a, avl_node_t *b);
	rpcproxy_impl* find_inst_unlock(void* inst_id);
	static rpcproxy_impl* remote_invoke_unlock(void* instid, uint128_t& uuid,
		google::protobuf::Message* inparam,
		google::protobuf::Message* inout_param,
		rpcproxy_static_data *data,
		uint methodid,  const char *method_name);
	static int send_message_unlock(evlclient sender,
		invoke_request_pkg* message, size_t sz,
		rpcinvoke_data* data, int timeout);

public:
	enum proxy_static_flag
	{
		proxy_static_host_wait_handle_regist = 1,
	};

	enum proxy_data_flag
	{
		// this flag will keep the instance remain even execute "destroy"
		proxydata_local_keep_instance = 2,

		// this flag mean the object instance is a singleton object
		proxydata_local_singleton_instance = 4,
	};

public:	

	rpcproxy_static_data* get_static_data(void) {
		return _static_data;
	}

	void setflags(uint32_t f) {
		_flags |= f;
	}

	void clearflags(uint32_t f) {
		_flags &= ~f;
	}

	bool testflags(uint32_t f) {
		return ((_flags & f) == f) ? true : false;
	}

private:
	avl_node_t 		avlnode;
	void*			_inst_id;
	int				_refcnt;
	bool			_renew_inst;
	uint32_t 		_flags;
	std::string 	_invoke_result;
	rpcproxy_static_data* _static_data;
	mutex	_mut;
};

}}};

#endif
/* EOF */
