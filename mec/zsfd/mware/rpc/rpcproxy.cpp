
#include "inc/rpcproxy.h"

#include "utils/buffer.h"
#include "utils/mutex.h"

#include "rpc/rpcerror.h"
#include "inc/rpcclient.h"
#include "inc/rpcmgr-impl.h"

#include <google/protobuf/message.h>

namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;
using namespace google::protobuf;

static mutex proxymut;

invoke_reqinfo::invoke_reqinfo()
: class_name(0), renew_inst(0), method_name(0)
, method_id(0), inparam(0), inparam_sz(0)
, inoutparam(0), inoutparam_sz(0), instid(0), bufsize(0)
{
	validity.all = 0;
}

int rpcproxy::addref()
{
	auto* impl = reinterpret_cast<rpcproxy_impl*>(this);
	assert(NULL != impl);
	return impl->addref();
}

int rpcproxy::release()
{
	auto* impl = reinterpret_cast<rpcproxy_impl*>(this);
	assert(NULL != impl);
	return impl->release();
}

bool rpcproxy::is_vaild(void)
{
	auto* impl = reinterpret_cast<rpcproxy_impl*>(this);
	assert(NULL != impl);
	return impl->is_vaild();
}

void* rpcproxy::get_instid(void)
{
	auto* impl = reinterpret_cast<rpcproxy_impl*>(this);
	assert(NULL != impl);
	return impl->get_instid();
}

const char* rpcproxy::get_class_name(void)
{
	auto* impl = reinterpret_cast<rpcproxy_impl*>(this);
	assert(NULL != impl);
	return impl->get_class_name();
}

rpcproxy* rpcproxy::invoke(void* instid, uint128_t& uuid,
	google::protobuf::Message* inparam,
	google::protobuf::Message* inout_param,
	rpcproxy_static_data *data,
	uint methodid,  const char *method_name)
{
	rpcproxy_impl* ret = rpcproxy_impl::invoke(instid, uuid, inparam, 
		inout_param, data, methodid, method_name);
	return reinterpret_cast<rpcproxy*>(ret);
}

rpcproxy* rpcproxy::get_singleton_instance(
	rpcproxy_static_data *data)
{
	rpcproxy_impl* ret = rpcproxy_impl::get_singleton_instance( data);
	return reinterpret_cast<rpcproxy*>(ret);
}

rpcproxy* rpcproxy::get_instance(
	google::protobuf::Message* inparam,
	rpcproxy_static_data *data)
{
	rpcproxy_impl* ret = rpcproxy_impl::get_instance(inparam, data);
	return reinterpret_cast<rpcproxy*>(ret);
}

rpcproxy* rpcproxy::from_instid(
	void* instid,
	rpcproxy_static_data *data)
{
	rpcproxy_impl* ret = rpcproxy_impl::from_instid(instid, data);
	return reinterpret_cast<rpcproxy*>(ret);
}


rpcproxy_impl::rpcproxy_impl(void* inst_id,
	rpcproxy_static_data *static_data, bool renew)
: _inst_id(inst_id), _refcnt(1), _renew_inst(renew)
, _static_data(static_data), _flags(0)
{
}

rpcproxy_impl::~rpcproxy_impl()
{
	if (_inst_id && _static_data && _static_data->inst_id_tree) {
		avl_node_t* tree = (avl_node_t*)(_static_data->inst_id_tree);
		avl_remove(&tree, &avlnode);
	}
	if (!_inst_id) return;

	try{
		destory();
	} catch (rpc_error& err) {

	}
}

int rpcproxy_impl::addref(void)
{
	if (!_inst_id) return 0;
	assert(_refcnt >= 0);
	return __sync_add_and_fetch(&_refcnt, 1);
}

int rpcproxy_impl::release(void)
{
	if (!_inst_id) {
		assert(0 == _refcnt);
		delete this;
		return 0;
	}
	
	assert(_refcnt > 0);
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0) delete this;
	return cnt;
}

int rpcproxy_impl::rpcproxy_impl_compared(avl_node_t *a, avl_node_t *b)
{
	rpcproxy_impl *pa = AVLNODE_ENTRY(rpcproxy_impl, avlnode, a);
	rpcproxy_impl *pb = AVLNODE_ENTRY(rpcproxy_impl, avlnode, b);
	if (pa->_inst_id > pb->_inst_id) return 1;
	else if (pa->_inst_id < pb->_inst_id) return -1;
	else return 0;
}

rpcproxy_impl* rpcproxy_impl::find_inst_unlock(void* inst_id,
	rpcproxy_static_data *data)
{
	assert(NULL != data);
	avl_node_t *ndtree = (avl_node_t *)data->inst_id_tree;
	
	if (!ndtree) return NULL;

	avl_node_t* node = avl_find(ndtree,
		MAKE_FIND_OBJECT(inst_id, rpcproxy_impl, _inst_id, avlnode),
		rpcproxy_impl_compared);
	if (!node) return NULL;

	return AVLNODE_ENTRY(rpcproxy_impl, avlnode, node);
}

rpcproxy_impl* rpcproxy_impl::find_inst(void* inst_id,
	rpcproxy_static_data *data)
{
	auto_mutex am(data->list_mut);
	assert(NULL != data);
	avl_node_t *ndtree = (avl_node_t *)data->inst_id_tree;
	
	if (!ndtree) return NULL;

	avl_node_t* node = avl_find(ndtree,
		MAKE_FIND_OBJECT(inst_id, rpcproxy_impl, _inst_id, avlnode),
		rpcproxy_impl_compared);
	if (!node) return NULL;

	return AVLNODE_ENTRY(rpcproxy_impl, avlnode, node);
}

rpcproxy_impl* rpcproxy_impl::from_instid(void* inst_id,
	rpcproxy_static_data *data)
{
	auto_mutex am(data->list_mut);
	auto *impl = find_inst_unlock(inst_id, data);
	if (!impl) {
		impl = new rpcproxy_impl(inst_id, data, false);
		assert(NULL != impl);
		impl->add_to_proxy_tree();
	} 
	return impl;
}

rpcproxy_impl* rpcproxy_impl::find_inst_unlock(void* inst_id)
{
	assert(NULL != _static_data);
	avl_node_t *ndtree = (avl_node_t *)_static_data->inst_id_tree;
	
	if (!ndtree) return NULL;

	avl_node_t* node = avl_find(ndtree,
		MAKE_FIND_OBJECT(inst_id, rpcproxy_impl, _inst_id, avlnode),
		rpcproxy_impl_compared);
	if (!node) return NULL;

	return AVLNODE_ENTRY(rpcproxy_impl, avlnode, node);
}

int rpcproxy_impl::add_to_proxy_tree(void)
{
	assert(NULL != _static_data);
	auto_mutex am(_static_data->list_mut);

	if (!_inst_id) return -EBADPARM;

	auto* proxynd = find_inst_unlock(_inst_id);
	if (proxynd) return -EEXISTS;

	avl_node_t* treeroot = (avl_node_t*)_static_data->inst_id_tree;
	if (avl_insert(&treeroot, &avlnode, rpcproxy_impl_compared))
		return -ELOGIC;

	return 0;
}

rpcproxy_impl* rpcproxy_impl::remote_invoke_unlock(
	void* instid,
	uint128_t& uuid,
	google::protobuf::Message* inparam,
	google::protobuf::Message* inout_param,
	rpcproxy_static_data *data,
	uint methodid, const char *method_name)
{

	// make send data
	size_t sz = strlen(data->class_name) + 1;
	
	size_t inparam_sz = 0;
	size_t inoutparam_sz = 0;

	if (inparam)
		inparam_sz = inparam->ByteSizeLong();
	if (inout_param)
		inoutparam_sz = inout_param->ByteSizeLong();

	if (inparam_sz > 0)
		sz += inparam_sz + 1;
	if (inoutparam_sz > 0)
		sz += inoutparam_sz + 1;

	if (method_name && *method_name)
		sz += strlen(method_name) + 1;

	
	rpcclient_impl* impl = reinterpret_cast<rpcclient_impl*>
		(data->host_client);
	assert(NULL != impl); 
	uint32_t svcid = impl->get_service_id();

	// make send struct
	invoke_request_pkg* invokereq = new(alloca(sizeof(*invokereq) + sz))
		invoke_request_pkg(sz);
	invoke_request& rinfo = invokereq->payload();
	auto* impl_proxy = find_inst(instid, data);
	bool renew = true;
	if (impl_proxy)
		renew = impl_proxy->get_renew_instance();
	set_invoke_request_unlock(req_action_call_method, rinfo, &uuid, instid,
		inparam, inout_param, data, renew, svcid, methodid, method_name);


	rpcinvoke_data invoke_data;
	invoke_data.req_type = req_action_call_method;
	invoke_data.inst = instid;
	invoke_data.data = data;
	invoke_data.retmsg = inout_param;

	evlclient evlcli = impl->get_client();
	int ret = send_message_unlock(evlcli, invokereq, sizeof(*invokereq) + sz,
		&invoke_data, 200000);
	
	if (ret) {
		throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_evloop_error,
			"invoke(): send info error. errcode: " + std::to_string(ret));
	}

	return invoke_data.impl;
}

int rpcproxy_impl::send_message_unlock(evlclient sender,
	invoke_request_pkg* message, size_t sz,
	rpcinvoke_data* data, int timeout)
{

	evpoller poller;
	auto* ev = poller.create_event(evp_evid_package_with_seqid,
		INVOKE_METHOD_REPLY_CTRL, message->header().seqid);
	assert(NULL != ev);
	ev->write_inputbuf(data, sizeof(*data));
	// submit the event
	ev->submit();
	size_t sendsz = sender->write((void*)message, sz);	
	if (sendsz < sz) {
		return -EINVALID;
	}
	if (poller.poll(timeout)) {
		return -ETIMEOUT;
	}
	ev = poller.get_triggered_event();
	assert(!poller.get_triggered_event());

	data->reply_type = reply_type_unknown;
	ev->read_outputbuf(data, sizeof(*data));

	if (reply_type_success != data->reply_type) {
		return -EDATANOTAVAIL;
	}

	return 0;
}

int rpcproxy_impl::set_invoke_request_unlock(invoke_request_action action,
	invoke_request& rinfo, uint128_t* uuid, void* instid, 
	google::protobuf::Message* inparam,
	google::protobuf::Message* inout_param,
	rpcproxy_static_data *data,
	bool renew, uint32_t svcid,
	uint methodid,  const char *method_name)
{
	rinfo.req_action = action;
	if (uuid)
		memcpy(&rinfo.method_uuid, uuid, sizeof(uint128_t));

	rinfo.req_info.class_name = rinfo.req_info.bufsize;
	stpcpy(rinfo.req_info.buf + rinfo.req_info.class_name, data->class_name);
	rinfo.req_info.bufsize += strlen(data->class_name) + 1;
	rinfo.req_info.validity.m.class_name = 1;

	rinfo.req_info.renew_inst = renew ? 1 : 0;
	rinfo.req_info.validity.m.renew_inst = 1;

	if (method_name && *method_name) {	
		rinfo.req_info.method_name = rinfo.req_info.bufsize;
		stpcpy(rinfo.req_info.buf + rinfo.req_info.method_name,
			method_name);
		rinfo.req_info.bufsize += strlen(method_name) + 1;
		rinfo.req_info.validity.m.method_name = 1;
	}

	if (methodid) {
		rinfo.req_info.method_id = methodid;
		rinfo.req_info.validity.m.method_id = 1;
	}
	
	if (inparam) {
		size_t inparam_sz = inparam->ByteSizeLong();
		if (inparam_sz > 0) {
			rinfo.req_info.inparam = rinfo.req_info.bufsize;
			inparam->SerializeToArray(
				(void*)(rinfo.req_info.buf + rinfo.req_info.inparam), inparam_sz);
			rinfo.req_info.bufsize += inparam_sz  + 1;
			rinfo.req_info.inparam_sz = inparam_sz;
			rinfo.req_info.validity.m.inparam = 1;
		}
	}

	if (inout_param) {
		size_t inout_param_sz = inout_param->ByteSizeLong();
		if (inout_param_sz > 0) {
			rinfo.req_info.inoutparam = rinfo.req_info.bufsize;
			inout_param->SerializeToArray(
				(void*)(rinfo.req_info.buf + rinfo.req_info.inoutparam),
				inout_param_sz);
			rinfo.req_info.bufsize += inout_param_sz  + 1;
			rinfo.req_info.inoutparam_sz = inout_param_sz;
			rinfo.req_info.validity.m.inoutparam = 1;
		}
	}

	if (instid) {
		rinfo.req_info.instid = (size_t)instid;
		rinfo.req_info.validity.m.instid = 1;
	}
	rinfo.svc_id = svcid;
	return 0;
}

rpcproxy_impl* rpcproxy_impl::invoke(void* instid, uint128_t& uuid,
	google::protobuf::Message* inparam,
	google::protobuf::Message* inout_param,
	rpcproxy_static_data *data,
	uint methodid, const char *method_name)
{
	if (!data) {
		throw rpc_invalid_argument(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_invalid_param,
			"invoke(): parameter error: ");
	}

	auto_mutex am(data->access_mut);
	std::string errdesc;
	if (!check_service_unlock(errdesc, data)) {
		throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_service_notready,
			"invoke(): service not ready: " + errdesc);
	}

	rpcclient_impl* impl = reinterpret_cast<rpcclient_impl*>
		(data->host_client);
	assert(NULL != impl); 

	if (impl->is_local_service()) {
		std::string classname = data->class_name;
		rpcproxy_impl *impl_proxy = nullptr; 
		bool renew = true;
		void* origin_instid = instid;
		if (instid) {
			impl_proxy = find_inst(instid, data);
			if (impl_proxy)
				renew = impl_proxy->get_renew_instance();
		}
		int ret = impl->handle_local_invoke(&instid, classname,
			uuid, inparam, inout_param, renew);
		if (ret) {
			throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_return_failure,
			"invoke(): local  service request error " + std::to_string(ret));
		}
		if (instid != origin_instid && origin_instid && impl_proxy) {
			impl_proxy->release();
			impl_proxy = nullptr;
		}
		if (impl_proxy) return impl_proxy;
		impl_proxy = new rpcproxy_impl(instid, data);
		assert(NULL != impl_proxy);
		impl_proxy->add_to_proxy_tree();
		impl_proxy->addref();
		return impl_proxy;
	}

	return remote_invoke_unlock(instid, uuid, inparam, inout_param, data,
		methodid, method_name);
}

int rpcproxy_impl::handle_invoke_reply_package(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	invoke_reply* reply = (invoke_reply*)
		alloca(pkghdr.size);
	assert(NULL != reply);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(NULL != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)reply, pkghdr.size);
	assert(sz == pkghdr.size);
	// get the package payload
	auto* ev = queue.dequeue();
	assert(NULL == queue.dequeue());
	rpcinvoke_data invoke_data;
	ev->read_inputbuf(&invoke_data, sizeof(invoke_data));
	invoke_data.impl = NULL;

	if (reply_type_success == reply->reply_type && 0 == reply->instid)
		reply->reply_type = reply_type_invalid_instid;

	if (!((req_action_get_instance == invoke_data.req_type
		|| req_action_get_singleton == invoke_data.req_type) && req_action_get_instance == reply->req_action)) {
		if (invoke_data.req_type != reply->req_action) {
			reply->reply_type = reply_type_evl_pkg_error;
		}
	}

	invoke_data.reply_type = reply->reply_type;
	// service return error
	if (reply_type_success != reply->reply_type) {
		printf("invoke_reply reply type is %d\n", reply->reply_type);
		ev->write_outputbuf(&invoke_data, sizeof(invoke_data));
		return 0;
	}

	rpcproxy_impl *impl = NULL;
	if (invoke_data.inst == (void*)reply->instid) {
		impl = find_inst(invoke_data.inst, 
			invoke_data.data);
	} else {
		if (invoke_data.inst) {
			impl = find_inst(invoke_data.inst,
				invoke_data.data);
			if (impl) impl->release();
		}
		invoke_data.inst = (void*)reply->instid;
		impl = find_inst((void*)reply->instid,
			invoke_data.data);
	}

	if (impl) {
		if (req_action_get_instance == reply->req_action
			|| req_action_get_singleton == reply->req_action) {
			impl->addref();
		}
	}

	if (!impl) {
		impl = new rpcproxy_impl(invoke_data.inst, invoke_data.data);
		assert(NULL != impl);
		impl->add_to_proxy_tree();
		impl->addref();
	}
	invoke_data.impl = impl;
	
	if (req_action_call_method == reply->req_action) {
		std::string result_data;
		if (reply->reply_info.result > 0)
			if (impl) {
				result_data = reply->reply_info.buf;
				impl->set_invoke_result(result_data);
			}
			if (invoke_data.retmsg){
				invoke_data.retmsg->Clear();
				invoke_data.retmsg->ParseFromArray(reply->reply_info.buf, reply->reply_info.result);
			}
	}
	ev->write_outputbuf(&invoke_data, sizeof(invoke_data));
	return 0;
}

rpcproxy_impl*  rpcproxy_impl::get_singleton_instance(
	rpcproxy_static_data *data)
{
	if (!data) {
		throw rpc_invalid_argument(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_invalid_param,
			"get_singleton_instance(): parameter error: ");
	}
	auto_mutex am(data->access_mut);

	std::string errdesc;
	if (!check_service_unlock(errdesc, data)) {
		throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_service_notready,
			"get_singleton_instance(): service not ready: " + errdesc);
	}
	rpcclient_impl* impl = reinterpret_cast<rpcclient_impl*>
		(data->host_client);
	assert(NULL != impl); 

	if (impl->is_local_service()) {
		std::string classname = data->class_name;
		void* instid = NULL;
		int ret = impl->handle_get_instance(&instid, classname, NULL, 0, true);
		if (ret) {
			throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_return_failure,
			"get_singleton_instance(): local get signle instance error "
				 + std::to_string(ret));
		}
		rpcproxy_impl *impl = find_inst_unlock(instid, data);;
		if (impl) {
			impl->addref();
			return impl;
		}
		impl = new rpcproxy_impl(instid, data);
		assert(NULL != impl);
		impl->add_to_proxy_tree();
		impl->addref();
		return impl;
	}

	size_t sz = strlen(data->class_name) + 1;

	invoke_request_pkg* invokereq = new(alloca(sizeof(*invokereq) + sz))
		invoke_request_pkg(sz);
	invoke_request& rinfo = invokereq->payload();

	set_invoke_request_unlock(req_action_get_singleton, rinfo, NULL, NULL, NULL, NULL, data, true, impl->get_service_id());

	evlclient evlcli = impl->get_client();
	rpcinvoke_data invoke_data;
	invoke_data.req_type = req_action_get_singleton;
	invoke_data.inst = NULL;
	invoke_data.data = data;
	int ret = send_message_unlock(evlcli, invokereq, 
		sizeof(*invokereq) + sz, &invoke_data, 10000);
	if (ret) {
		throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_evloop_error,
			"get_singleton_instance(): send info error. errcode: "
			 + std::to_string(ret));
	}

	return invoke_data.impl;
}

rpcproxy_impl*  rpcproxy_impl::get_instance(google::protobuf::Message* inparam,
	rpcproxy_static_data *data)
{
	if (!data) {
		throw rpc_invalid_argument(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_invalid_param,
			"get_instance(): parameter error: ");
	}
	auto_mutex am(data->access_mut);
	std::string errdesc;
	if (!check_service_unlock(errdesc, data)) {
		throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_service_notready,
			"get_instance(): service not ready: " + errdesc);
	}
	rpcclient_impl* impl = reinterpret_cast<rpcclient_impl*>
		(data->host_client);
	assert(NULL != impl); 

	if (impl->is_local_service()) {
		std::string classname = data->class_name;
		void* instid = NULL;
		void* parmdata = NULL;
		size_t sz = 0;
		if (inparam) {
			sz = inparam->ByteSizeLong();
			parmdata = alloca(sz);
			inparam->ParseFromArray(parmdata, sz);
		}
		int ret = impl->handle_get_instance(&instid, 
			classname, parmdata, sz, false);
		if (ret) {
			throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_return_failure,
			"get_instance(): local get signle instance error " + std::to_string(ret));
		}
		rpcproxy_impl *impl = find_inst_unlock(instid, data);
		if (impl) {
			impl->addref();
			return impl;
		}
		impl = new rpcproxy_impl(instid, data);
		assert(NULL != impl);
		impl->add_to_proxy_tree();
		impl->addref();
		return impl;
	}
	
	size_t sz = strlen(data->class_name) + 1;
	if (inparam)
		sz += inparam->ByteSizeLong() + 1;

	invoke_request_pkg* invokereq = new(alloca(sizeof(*invokereq) + sz))
		invoke_request_pkg(sz);
	invoke_request& rinfo = invokereq->payload();

	set_invoke_request_unlock(req_action_get_instance, rinfo, NULL,
		NULL, inparam, NULL, data, true, impl->get_service_id());

	evlclient evlcli = impl->get_client();
	rpcinvoke_data invoke_data;
	invoke_data.req_type = req_action_get_instance;
	invoke_data.inst = NULL;
	invoke_data.data = data;
	int ret = send_message_unlock(evlcli, invokereq, 
		sizeof(*invokereq) + sz, &invoke_data, 10000);
	if (ret) {
		throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_evloop_error,
			"get_instance(): send info error. errcode: " + std::to_string(ret));
	}
	invoke_data.impl->setflags(proxydata_local_singleton_instance);
	return invoke_data.impl;
}

bool rpcproxy_impl::check_service_unlock(std::string& errdesc,
	rpcproxy_static_data *data)
{
	assert(NULL != data);
	if (data->host_client) {
		// check client is vaild
		rpcclient_impl *cli = reinterpret_cast<rpcclient_impl*>
			(data->host_client);
		if (cli->is_destory()) {
			cli->release();
			data->host_client = NULL;
			// TODO: reconnect
			errdesc = "service is destoryed";
			return false;
		}
	}

	rpcclient_mgr *mgr = rpcmgr_impl::instance()->get_client_mgr();
	rpcclient_impl* cli = mgr->get_client(data->package_name,
		data->service_name, data->inst_name);
	
	if (!cli) {
		cli = mgr->add_client(data->package_name,
			data->service_name, data->inst_name);
		if (!cli) {
			errdesc = "client of service create rror";
			return false;	
		}
	}

	if (cli->is_destory()) {
		data->host_client = NULL;
		// TODO: reconnect
		errdesc = "service is destoryed";
		return false;
	}

	if (!cli->is_ready() && !cli->is_local_service()) {
		// request service start
		int ret = cli->get_service_from_server();
		if (ret || (!cli->is_ready() && !cli->is_local_service())) {
			errdesc = "service start failure";
			errdesc += std::to_string(ret);
			return false;
		}
	}

	cli->addref();
	data->host_client = cli;

	return true;
}

void rpcproxy_impl::destory(void)
{

}


}}};
/* EOF */
