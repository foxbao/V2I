#include "inc/rpcobservable.h"

#include <unistd.h>

#include "std/list.h"
#include "utils/avltree.h"
#include "utils/mutex.h"
#include "utils/buffer.h"

#include "rpc/rpcmgr.h"
#include "rpc/rpcerror.h"
#include "inc/rpcclient.h"

#include <google/protobuf/message.h>

namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;

static mutex obsmut;
static rpcobservable_impl* obs_impl_inst = NULL;

struct rpcobservable_reply_data
{
	rpcobservable_reply_data();
	uint16_t	replytype;
	rpcobservable_client_instance *instnode;
	google::protobuf::Message	*inoutparam;
	std::string*		inoutdata;
};
rpcobservable_reply_data::rpcobservable_reply_data()
{
	replytype = 0;
	instnode = nullptr;
	inoutparam = nullptr;
	inoutdata = nullptr;
}

observable_invoke_reqinfo::observable_invoke_reqinfo()
: class_name(0), method_name(0), method_id(0),
inparam(0), inparam_sz(0), inoutparam(0), inoutparam_sz(0),
instid(0),  bufsize(0)
{
	validity.all = 0;
}

rpcobservable_client_instance::rpcobservable_client_instance()
: _inst_id(NULL), _refcnt(1)
{
}

rpcobservable_client_instance::~rpcobservable_client_instance()
{
}

int rpcobservable_client_instance::addref()
{
	return __sync_add_and_fetch(&_refcnt, 1);
}

int rpcobservable_client_instance::release()
{
	int ret = __sync_sub_and_fetch(&_refcnt, 1);
	if (ret <= 0) {
		rpcobservable_impl::inst()->remove_client_inst(this);
		delete this;
	}
	return ret;
}

rpcobservable_impl::rpcobservable_impl()
: _class_tree(NULL), _instance_tree(NULL), _client_instance_tree(NULL)
{
	_obs_data.pkg_name = evloop::inst()->get_name();
	_obs_data.inst_name = evloop::inst()->get_instance_name();
	listnode_init(_class_list);
	listnode_init(_instance_list);
	listnode_init(_client_instance_list);
}

rpcobservable_impl::~rpcobservable_impl()
{
	release_all_class_node();
}

int rpcobservable_impl::add_observable_instance(const char *clsname, void *inst)
{
	auto_mutex am(_mut);

	if (!clsname || !inst) return -EBADPARM;

	std::string obvname = "?observable.";
	obvname += clsname;

	auto* clsnode = find_class_node_unlock(obvname.c_str());
	if (!clsnode) return -ENOTFOUND;

	auto* inst_node = find_instance_unlocked(inst);
	if (inst_node) return -EEXISTS;

	inst_node = new rpcclass_instance_node();
	inst_node->_class_node = clsnode;
	inst_node->_inst_id = inst;
	if (avl_insert(&_instance_tree, &inst_node->avlnode,
	observable_instnode_compared)){
		delete inst_node;
		return -ELOGIC;
	}
	
	listnode_add(_instance_list, inst_node->ownerlist);
	return 0;
}

int rpcobservable_impl::add_client_instance(const char *cliname,
	const char* inst_name, void *inst)
{
	auto_mutex am(_mut);

	if (!cliname || !*cliname || !inst_name || !*inst_name || !inst)
		return -EBADPARM;

	auto* clinode = find_client_instance_unlocked(inst);
	printf("add client instance %p\n", clinode);
	if (clinode) {
		clinode->addref();
		return 0;
	}
	clinode = new rpcobservable_client_instance();
	clinode->_pkg_name = cliname;
	clinode->_inst_name = inst_name;
	clinode->_inst_id = inst;
	clinode->_client = evloop::inst()->getclient(cliname, inst_name);
	if (avl_insert(&_client_instance_tree, &clinode->avlnode,
	client_instnode_compared)){
		delete clinode;
		return -ELOGIC;
	}
	
	listnode_add(_client_instance_list, clinode->ownerlist);
	return 0;
}

void rpcobservable_impl::release_observable_instance(void *inst)
{
	auto_mutex am(_mut);
	auto* inst_node = find_instance_unlocked(inst);
	if (!inst_node) {
		throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_release,
			errid_not_found,
			"observablerelease: not found releas oject");
	}
	avl_remove(&_instance_tree, &inst_node->avlnode);
	listnode_del(inst_node->ownerlist);
	delete inst_node;
}

void rpcobservable_impl::release_client_instance(void *inst)
{
	auto_mutex am(_mut);
	auto* inst_node = find_client_instance_unlocked(inst);
	if (!inst_node) {
		//todo , operator = 
		return;
		// throw rpc_logic_error(RPC_ERR_FILEINFO,
		// 	err_scenario_release,
		// 	errid_not_found,
		// 	"observablerelease: not found releas oject");
	}
	inst_node->release();
}

int rpcobservable_impl::remove_client_inst(
		rpcobservable_client_instance* clinode)
{
	auto_mutex am(_mut);
	if (!clinode) return -EBADPARM;
	avl_remove(&_client_instance_tree, &clinode->avlnode);
	listnode_del(clinode->ownerlist);
	return 0;
}

rpcobservable_data* rpcobservable_impl::get_observable_data()
{
	return &_obs_data;
}

void rpcobservable_impl::invoke_observable_method(void* inst_id,
	const char* clsname, const uint128_t& uuid,
	google::protobuf::Message* inparam,
	google::protobuf::Message* inout_param,
	uint methodid,
	const char *method_name)
{
	if (!inst_id) {
		throw rpc_invalid_argument(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_invalid_param,
			"observableinvoke(): parameter error");
	}

	// if (impl->is_local_service()) {
	// 	std::string classname = data->class_name;
	// 	void* instid = NULL;
	// 	std::string param;
	// 	int ret = impl->handle_get_instance(&instid, classname, param, true);
	// 	if (ret) {
	// 		throw rpc_logic_error(RPC_ERR_FILEINFO,
	// 		err_scenario_invoke,
	// 		errid_return_failure,
	// 		"invoke(): local get signle instance error " + std::to_string(ret));
	// 	}
	// 	rpcproxy_impl *impl = find_inst(instid, data);;
	// 	if (impl) {
	// 		impl->addref();
	// 		return impl;
	// 	}
	// 	impl = new rpcproxy_impl(instid, data);
	// 	assert(NULL != impl);
	// 	impl->add_to_proxy_tree();
	// 	impl->addref();
	// 	return impl;
	// }
	auto *cli = find_client_instance((void*)inst_id);
	if (!cli || !cli->_client.get()) {
		throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_evloop_error,
			"invoke(): evlcient write failure");
	}

	size_t sz = strlen(clsname) + 1;
	if (inparam)
		sz += inparam->ByteSizeLong() + 1;
	if (inout_param)
		sz += inout_param->ByteSizeLong() + 1;
	if (method_name && *method_name)
		sz += strlen(method_name) + 1;

	observable_invoke_request_pkg* invokereq = new(alloca(sizeof(*invokereq)
		+ sz))observable_invoke_request_pkg(sz);
	observable_invoke_request& rinfo = invokereq->payload();

	rinfo.req_action = req_action_call_method;
	memcpy(&rinfo.method_uuid, &uuid, sizeof(uint128_t));

	rinfo.req_info.class_name = rinfo.req_info.bufsize;
	stpcpy(rinfo.req_info.buf + rinfo.req_info.class_name, clsname);
	rinfo.req_info.bufsize += strlen(clsname) + 1;
	rinfo.req_info.validity.m.class_name = 1;
	
	if (method_name && *method_name) {
		rinfo.req_info.method_name = rinfo.req_info.bufsize;
		stpcpy(rinfo.req_info.buf + rinfo.req_info.method_name, method_name);
		rinfo.req_info.bufsize += strlen(method_name) + 1;
		rinfo.req_info.validity.m.method_name = 1;
	}
	
	size_t inparam_sz = inparam->ByteSizeLong();
	if (inparam_sz > 0 && inparam) {
		rinfo.req_info.inparam = rinfo.req_info.bufsize;
		inparam->SerializeToArray(
			(void*)(rinfo.req_info.buf + rinfo.req_info.inparam), inparam_sz);
		rinfo.req_info.bufsize += inparam_sz  + 1;
		rinfo.req_info.inparam_sz = inparam_sz;
		rinfo.req_info.validity.m.inparam = 1;
	}

	size_t inout_param_sz = inout_param->ByteSizeLong();
	if (inout_param_sz > 0 && inout_param) {
		rinfo.req_info.inoutparam = rinfo.req_info.bufsize;
		inout_param->SerializeToArray(
			(void*)(rinfo.req_info.buf + rinfo.req_info.inoutparam),
			inout_param_sz);
		rinfo.req_info.bufsize += inout_param_sz  + 1;
		rinfo.req_info.inoutparam_sz = inout_param_sz;
		rinfo.req_info.validity.m.inoutparam = 1;
	}
	
	rinfo.req_info.method_id = methodid;
	rinfo.req_info.validity.m.method_id = 1;
	
	rinfo.req_info.instid = (size_t)inst_id;
	rinfo.req_info.validity.m.instid = 1;

	evpoller poller;
	auto* ev = poller.create_event(evp_evid_package_with_seqid,
		OBSERVABLE_INVOKE_METHOD_REPLY_CTRL, invokereq->header().seqid);
	assert(NULL != ev);
	rpcobservable_reply_data data;
	data.instnode = cli;
	data.inoutparam = inout_param;
	ev->write_inputbuf(&data, sizeof(rpcobservable_reply_data));
	// submit the event
	ev->submit();

	size_t sendsz = cli->_client->write((void*)invokereq,
		sizeof(*invokereq) + sz);	
	if (sendsz < sizeof(*invokereq) + sz) {
		throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_evloop_error,
			"invoke(): evlcient write failure");
	}
	
	if (poller.poll(10000)) {
		throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_service_timeout,
			"invoke(): wait service timeout");
	}
	ev = poller.get_triggered_event();
	assert(!poller.get_triggered_event());

	ev->read_outputbuf(&data, sizeof(rpcobservable_reply_data));
	if (reply_type_success != data.replytype) {
		throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_return_failure,
			"invoke(): service service request error "
				+ std::to_string(data.replytype));
	}
}

void rpcobservable_impl::invoke_observable_method(void* inst_id,
	const char* clsname, const uint128_t& uuid, void* input, size_t insize,
	std::string &inout, size_t &inoutsize, uint32_t methodid,
	const char* method_name)
{
	if (!inst_id) {
		throw rpc_invalid_argument(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_invalid_param,
			"observableinvoke(): parameter error");
	}

	// if (impl->is_local_service()) {
	// 	std::string classname = data->class_name;
	// 	void* instid = NULL;
	// 	std::string param;
	// 	int ret = impl->handle_get_instance(&instid, classname, param, true);
	// 	if (ret) {
	// 		throw rpc_logic_error(RPC_ERR_FILEINFO,
	// 		err_scenario_invoke,
	// 		errid_return_failure,
	// 		"invoke(): local get signle instance error " + std::to_string(ret));
	// 	}
	// 	rpcproxy_impl *impl = find_inst(instid, data);;
	// 	if (impl) {
	// 		impl->addref();
	// 		return impl;
	// 	}
	// 	impl = new rpcproxy_impl(instid, data);
	// 	assert(NULL != impl);
	// 	impl->add_to_proxy_tree();
	// 	impl->addref();
	// 	return impl;
	// }
	auto *cli = find_client_instance((void*)inst_id);
	if (!cli || !cli->_client.get()) {
		throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_evloop_error,
			"invoke(): evlcient write failure");
	}

	size_t sz = strlen(clsname) + 1 + insize + inoutsize;
	if (method_name && *method_name)
		sz += strlen(method_name) + 1;

	observable_invoke_request_pkg* invokereq = new(alloca(sizeof(*invokereq)
		+ sz))observable_invoke_request_pkg(sz);
	observable_invoke_request& rinfo = invokereq->payload();

	rinfo.req_action = req_action_call_method;
	memcpy(&rinfo.method_uuid, &uuid, sizeof(uint128_t));

	rinfo.req_info.class_name = rinfo.req_info.bufsize;
	stpcpy(rinfo.req_info.buf + rinfo.req_info.class_name, clsname);
	rinfo.req_info.bufsize += strlen(clsname) + 1;
	rinfo.req_info.validity.m.class_name = 1;
	
	if (method_name && *method_name) {
		rinfo.req_info.method_name = rinfo.req_info.bufsize;
		stpcpy(rinfo.req_info.buf + rinfo.req_info.method_name, method_name);
		rinfo.req_info.bufsize += strlen(method_name) + 1;
		rinfo.req_info.validity.m.method_name = 1;
	}
	

	if (insize > 0 && input) {
		rinfo.req_info.inparam = rinfo.req_info.bufsize;
		memcpy((void*)(rinfo.req_info.buf + rinfo.req_info.inparam),
			input, insize);
		rinfo.req_info.bufsize += insize;
		rinfo.req_info.inparam_sz = insize;
		rinfo.req_info.validity.m.inparam = 1;
	}

	if (inoutsize > 0) {
		rinfo.req_info.inoutparam = rinfo.req_info.bufsize;
		memcpy((void*)(rinfo.req_info.buf + rinfo.req_info.inoutparam),
			inout.c_str(), inoutsize);
		rinfo.req_info.bufsize += inoutsize;
		rinfo.req_info.inoutparam_sz = inoutsize;
		rinfo.req_info.validity.m.inoutparam = 1;
	}
	
	rinfo.req_info.method_id = methodid;
	rinfo.req_info.validity.m.method_id = 1;
	
	rinfo.req_info.instid = (size_t)inst_id;
	rinfo.req_info.validity.m.instid = 1;

	evpoller poller;
	auto* ev = poller.create_event(evp_evid_package_with_seqid,
		OBSERVABLE_INVOKE_METHOD_REPLY_CTRL, invokereq->header().seqid);
	assert(NULL != ev);
	rpcobservable_reply_data data;
	data.instnode = cli;
	data.inoutparam = nullptr;
	data.inoutdata = &inout;
	ev->write_inputbuf(&data, sizeof(rpcobservable_reply_data));
	// submit the event
	ev->submit();

	size_t sendsz = cli->_client->write((void*)invokereq,
		sizeof(*invokereq) + sz);	
	if (sendsz < sizeof(*invokereq) + sz) {
		throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_evloop_error,
			"invoke(): evlcient write failure");
	}
	
	if (poller.poll(10000)) {
		throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_service_timeout,
			"invoke(): wait service timeout");
	}
	ev = poller.get_triggered_event();
	assert(!poller.get_triggered_event());

	ev->read_outputbuf(&data, sizeof(rpcobservable_reply_data));
	if (reply_type_success != data.replytype) {
		throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_return_failure,
			"invoke(): service service request error "
				+ std::to_string(data.replytype));
	}
	inoutsize = inout.length();
}

void rpcobservable_impl::invoke_observable_method(void* inst_id,
	const char* cli_name, const char* inst_name,
	const char* clsname, const uint128_t& uuid, void* input, size_t insize,
	std::string &inout, size_t &inoutsize, uint32_t methodid,
	const char* method_name)
{
	if (!inst_id) {
		throw rpc_invalid_argument(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_invalid_param,
			"observableinvoke(): parameter error");
	}

	// if (impl->is_local_service()) {
	// 	std::string classname = data->class_name;
	// 	void* instid = NULL;
	// 	std::string param;
	// 	int ret = impl->handle_get_instance(&instid, classname, param, true);
	// 	if (ret) {
	// 		throw rpc_logic_error(RPC_ERR_FILEINFO,
	// 		err_scenario_invoke,
	// 		errid_return_failure,
	// 		"invoke(): local get signle instance error " + std::to_string(ret));
	// 	}
	// 	rpcproxy_impl *impl = find_inst(instid, data);;
	// 	if (impl) {
	// 		impl->addref();
	// 		return impl;
	// 	}
	// 	impl = new rpcproxy_impl(instid, data);
	// 	assert(NULL != impl);
	// 	impl->add_to_proxy_tree();
	// 	impl->addref();
	// 	return impl;
	// }
	evlclient cli = evloop::inst()->getclient(cli_name, inst_name);
	if (!cli.get()) {
		throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_evloop_error,
			"invoke(): evlcient write failure");
	}

	size_t sz = strlen(clsname) + 1 + insize + inoutsize;
	if (method_name && *method_name)
		sz += strlen(method_name) + 1;

	observable_invoke_request_pkg* invokereq = new(alloca(sizeof(*invokereq)
		+ sz))observable_invoke_request_pkg(sz);
	observable_invoke_request& rinfo = invokereq->payload();

	rinfo.req_action = req_action_call_method;
	memcpy(&rinfo.method_uuid, &uuid, sizeof(uint128_t));

	rinfo.req_info.class_name = rinfo.req_info.bufsize;
	stpcpy(rinfo.req_info.buf + rinfo.req_info.class_name, clsname);
	rinfo.req_info.bufsize += strlen(clsname) + 1;
	rinfo.req_info.validity.m.class_name = 1;
	
	if (method_name && *method_name) {
		rinfo.req_info.method_name = rinfo.req_info.bufsize;
		stpcpy(rinfo.req_info.buf + rinfo.req_info.method_name, method_name);
		rinfo.req_info.bufsize += strlen(method_name) + 1;
		rinfo.req_info.validity.m.method_name = 1;
	}
	

	if (insize > 0 && input) {
		rinfo.req_info.inparam = rinfo.req_info.bufsize;
		memcpy((void*)(rinfo.req_info.buf + rinfo.req_info.inparam),
			input, insize);
		rinfo.req_info.bufsize += insize;
		rinfo.req_info.inparam_sz = insize;
		rinfo.req_info.validity.m.inparam = 1;
	}

	if (inoutsize > 0) {
		rinfo.req_info.inoutparam = rinfo.req_info.bufsize;
		memcpy((void*)(rinfo.req_info.buf + rinfo.req_info.inoutparam),
			inout.c_str(), inoutsize);
		rinfo.req_info.bufsize += inoutsize;
		rinfo.req_info.inoutparam_sz = inoutsize;
		rinfo.req_info.validity.m.inoutparam = 1;
	}
	
	rinfo.req_info.method_id = methodid;
	rinfo.req_info.validity.m.method_id = 1;
	
	rinfo.req_info.instid = (size_t)inst_id;
	rinfo.req_info.validity.m.instid = 1;
	
	evpoller poller;
	auto* ev = poller.create_event(evp_evid_package_with_seqid,
		OBSERVABLE_INVOKE_METHOD_REPLY_CTRL, invokereq->header().seqid);
	assert(NULL != ev);
	rpcobservable_reply_data data;
	data.inoutparam = nullptr;
	data.inoutdata = &inout;
	ev->write_inputbuf(&data, sizeof(rpcobservable_reply_data));
	// submit the event
	ev->submit();

	size_t sendsz = cli->write((void*)invokereq,
		sizeof(*invokereq) + sz);	
	if (sendsz < sizeof(*invokereq) + sz) {
		throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_evloop_error,
			"invoke(): evlcient write failure");
	}
	
	if (poller.poll(10000)) {
		throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_service_timeout,
			"invoke(): wait service timeout");
	}
	ev = poller.get_triggered_event();
	assert(!poller.get_triggered_event());

	ev->read_outputbuf(&data, sizeof(rpcobservable_reply_data));
	if (reply_type_success != data.replytype) {
		throw rpc_logic_error(RPC_ERR_FILEINFO,
			err_scenario_invoke,
			errid_return_failure,
			"invoke(): service service request error "
				+ std::to_string(data.replytype));
	}
	inoutsize = inout.length();
}

int rpcobservable_impl::handle_invoke_reply_package(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	observable_invoke_reply* reply = (observable_invoke_reply*)
		alloca(pkghdr.size);
	assert(NULL != reply);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(NULL != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)reply, pkghdr.size);
	assert(sz == pkghdr.size);
	// get the package payload
	auto* ev = queue.dequeue();
	assert(NULL == queue.dequeue());
	rpcobservable_reply_data data;
	ev->read_inputbuf(&data, sizeof(rpcobservable_reply_data));

	data.replytype = reply->reply_type;
	if (reply_type_success != reply->reply_type) {
		printf("return type is %d\n", reply->reply_type);
		ev->write_outputbuf(&data, sizeof(rpcobservable_reply_data));
		return 0;
	}
	if (data.inoutparam) {
		data.inoutparam->Clear();
		if (reply->reply_info.result > 0)
			data.inoutparam->ParseFromArray(reply->reply_info.buf, reply->reply_info.result);
	} else if (data.inoutdata) {
		data.inoutdata->clear();
		if (reply->reply_info.result > 0)
			data.inoutdata->append(reply->reply_info.buf, reply->reply_info.result);	
	}
	ev->write_outputbuf(&data, sizeof(rpcobservable_reply_data));
	return 0;
}

rpcclass_instance_node* 
rpcobservable_impl::find_instance(void* inst_id)
{
	auto_mutex am(_mut);
	return find_instance_unlocked(inst_id);
}

rpcobservable_client_instance* 
rpcobservable_impl::find_client_instance(void* inst_id)
{
	auto_mutex am(_mut);
	return find_client_instance_unlocked(inst_id);
}

int rpcobservable_impl::handle_invoke_request_package(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	observable_invoke_request* reqinfo = (observable_invoke_request*)
		alloca(pkghdr.size);
	assert(NULL != reqinfo);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(NULL != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)reqinfo, pkghdr.size);
	assert(sz == pkghdr.size);
	// get the package payload

	observable_invoke_reqinfo& rinfo = reqinfo->req_info;
	std::string clsname;
	if (rinfo.validity.m.class_name)
		clsname = rinfo.buf + rinfo.class_name;

	std::string method_name;
	if (rinfo.validity.m.method_name)
		method_name = rinfo.buf + rinfo.method_name;

	uint16_t method_id = 0;
	if (rinfo.validity.m.method_id)
		method_id = rinfo.method_id;

	void* inst_id = NULL;
	if (rinfo.validity.m.instid)
		inst_id = reinterpret_cast<void*>(rinfo.instid);

	rpcclass_node* clsnode = get_observable_interface(clsname.c_str());
	if (!clsnode) {
		return reply_invoke(sender, pkghdr,reqinfo->req_action,
				reply_type_no_class, inst_id, NULL);
	}

	auto* inst_node = find_instance_unlocked(inst_id);
	if (!inst_node) {
		return reply_invoke(sender, pkghdr,reqinfo->req_action,
				reply_type_no_instance, inst_id, NULL);
	}

	rpcmethod_node* methodnd = 
		clsnode->get_method_node(reqinfo->method_uuid);
	if (!methodnd) {
		return reply_invoke(sender, pkghdr,reqinfo->req_action,
				reply_type_no_method, inst_id, NULL);
	}

	google::protobuf::Message* inproto = nullptr;
	if (methodnd->_inparam.get()) {
		inproto = methodnd->_inparam->get_pbfmessage();
		inproto->Clear();
	}
	if (rinfo.validity.m.inparam && inproto){
		inproto->ParseFromArray((void*)(rinfo.buf + rinfo.inparam),
			rinfo.inparam_sz);
	}
	google::protobuf::Message* outproto = nullptr;
	if (methodnd->_inout_param.get()) {
		outproto = methodnd->_inout_param->get_pbfmessage();
		outproto->Clear();
	}
	if (rinfo.validity.m.inoutparam && outproto){
		outproto->ParseFromArray((void*)(rinfo.buf + rinfo.inoutparam),
			rinfo.inoutparam_sz);
	}
	uint16_t ret = ((uint16_t (*)(void*, pbf_wrapper, pbf_wrapper))
		methodnd->_handler)(
			inst_id, 
			methodnd->_inparam,
			methodnd->_inout_param);

	return reply_invoke(sender, pkghdr,reqinfo->req_action,
		reply_type_success, inst_id, outproto);
}

int rpcobservable_impl::reply_invoke(evlclient sender,
	const package_header &pkghdr,
	uint32_t req_action, uint32_t reply_type,
	void* instid, google::protobuf::Message *retmsg)
{
	size_t sz = 0;
	if (reply_type_success == reply_type && retmsg) {
		sz += retmsg->ByteSizeLong() + 1;
	}
	observable_invoke_reply_pkg* invokereply = 
		new(alloca(sizeof(*invokereply) + sz))observable_invoke_reply_pkg(sz);
	observable_invoke_reply& rinfo = invokereply->payload();

	rinfo.req_action = req_action;
	rinfo.reply_type = reply_type;
	rinfo.reply_info.result = 0;
	if (reply_type_success == reply_type && retmsg) {
		rinfo.reply_info.result = retmsg->ByteSizeLong();
		retmsg->SerializeToArray(rinfo.reply_info.buf, rinfo.reply_info.result);
	}
	rinfo.instid = (size_t)instid;
	invokereply->header().seqid = pkghdr.seqid;
	sender->write((void*)invokereply, sizeof(*invokereply) + sz);
	return 0;
}

rpcclass_node*
rpcobservable_impl::regist_observable_interface(const char* clsname)
{
	auto_mutex am(_mut);
	if (!clsname || !*clsname) return nullptr;

	std::string obvname = "?observable.";
	obvname += clsname;

	if (find_class_node_unlock(obvname.c_str()))
		return nullptr;	
	pbf_wrapper param;
	auto* node = new rpcclass_node(obvname.c_str(), nullptr, param);
	node->setflags(rpccls_node_observable);

	if (avl_insert(&_class_tree, &node->avlnode, 
		rpcclass_node_compared)) {
		delete node;
		return nullptr;
	}
	listnode_add(_class_list, node->ownerlist);
	return node;

}

rpcclass_node*
rpcobservable_impl::get_observable_interface(const char *clsname)
{
	auto_mutex am(_mut);
	if (!clsname || !*clsname) return NULL;

	std::string obvname = "?observable.";
	obvname += clsname;
	return find_class_node_unlock(obvname.c_str());
}

rpcclass_node*
rpcobservable_impl::find_class_node_unlock(const char* clsname)
{
	if (!clsname || !*clsname) return NULL;
	avl_node_t* anode = avl_find(_class_tree, 
		MAKE_FIND_OBJECT(clsname, rpcclass_node, _class_name, avlnode),
		rpcclass_node_compared);

	if (NULL == anode) return NULL;
	return AVLNODE_ENTRY(rpcclass_node, avlnode, anode);
}

int rpcobservable_impl::rpcclass_node_compared(avl_node_t* aa, avl_node_t* bb)
{
	auto* nda =  AVLNODE_ENTRY(rpcclass_node, avlnode, aa);
	auto* ndb =  AVLNODE_ENTRY(rpcclass_node, avlnode, bb);
	int ret = strcmp(nda->_class_name.c_str(), ndb->_class_name.c_str());
	if (ret > 0) return 1;
	else if (ret < 0) return -1;
	else return 0;
}

int rpcobservable_impl::release_all_class_node(void)
{
	auto_mutex am(_mut);
	rpcclass_node* nd = NULL;
	while (!listnode_isempty(_class_list))
	{
		nd = LIST_ENTRY(rpcclass_node, ownerlist, _class_list.next);
		if (!nd) return -ELOGIC;
		listnode_del(nd->ownerlist);
		delete nd;
	}
	_class_tree = NULL;	
	return 0;
}

int rpcobservable_impl::release_all_instance_node(void)
{
	auto_mutex am(_mut);
	rpcclass_instance_node* nd = NULL;
	while (!listnode_isempty(_instance_list))
	{
		nd = LIST_ENTRY(rpcclass_instance_node, ownerlist, _instance_list.next);
		if (!nd) return -ELOGIC;
		listnode_del(nd->ownerlist);
		delete nd;
	}
	_instance_tree = NULL;
	return 0;
}

int rpcobservable_impl::release_all_client_node(void)
{
	auto_mutex am(_mut);
	rpcobservable_client_instance* nd = NULL;
	while (!listnode_isempty(_client_instance_list))
	{
		nd = LIST_ENTRY(rpcobservable_client_instance, ownerlist,	\
			_client_instance_list.next);
		if (!nd) return -ELOGIC;
		listnode_del(nd->ownerlist);
		delete nd;
	}
	_client_instance_tree = NULL;
	return 0;
}

rpcclass_instance_node* rpcobservable_impl::find_instance_unlocked(void* inst)
{
	if (!inst) return NULL;
	avl_node_t* anode = avl_find(_instance_tree, 
		MAKE_FIND_OBJECT(inst, rpcclass_instance_node, _inst_id, avlnode),
		observable_instnode_compared);

	if (NULL == anode) return NULL;
	return AVLNODE_ENTRY(rpcclass_instance_node, avlnode, anode);
}

int rpcobservable_impl::observable_instnode_compared(avl_node_t* aa,
	avl_node_t* bb) {
	auto* anode = AVLNODE_ENTRY(rpcclass_instance_node, avlnode, aa);
	auto* bnode = AVLNODE_ENTRY(rpcclass_instance_node, avlnode, bb);
	if (anode->_inst_id > bnode->_inst_id) return 1;
	else if (anode->_inst_id < bnode->_inst_id) return -1;
	else return 0;
}

rpcobservable_client_instance* 
rpcobservable_impl::find_client_instance_unlocked(void* inst)
{
	if (!inst) return NULL;
	avl_node_t* anode = avl_find(_client_instance_tree, 
		MAKE_FIND_OBJECT(inst, rpcobservable_client_instance,	\
			_inst_id, avlnode),
		client_instnode_compared);

	if (NULL == anode) return NULL;
	return AVLNODE_ENTRY(rpcobservable_client_instance, avlnode, anode);
}

int rpcobservable_impl::client_instnode_compared(avl_node_t* aa, 
	avl_node_t* bb) {
	auto* anode = AVLNODE_ENTRY(rpcobservable_client_instance, avlnode, aa);
	auto* bnode = AVLNODE_ENTRY(rpcobservable_client_instance, avlnode, bb);
	if (anode->_inst_id > bnode->_inst_id) return 1;
	else if (anode->_inst_id < bnode->_inst_id) return -1;
	else return 0;
}

rpcobservable_impl* rpcobservable_impl::inst()
{
	if (obs_impl_inst) return obs_impl_inst;
	auto_mutex am(obsmut);
	if (!obs_impl_inst)
		obs_impl_inst = new rpcobservable_impl();
	assert(NULL != obs_impl_inst);
	return obs_impl_inst;
}

rpcobservable* rpcobservable::inst()
{
	return reinterpret_cast<rpcobservable*>(rpcobservable_impl::inst());
}

rpcinterface rpcobservable::register_observable_interface(const char* clsname)
{
	rpcobservable_impl* impl = reinterpret_cast<rpcobservable_impl*>(this);
	auto* cls = impl->regist_observable_interface(clsname);
	rpcinterface rif;
	if (!cls) return rif;
	cls->addref();
	rif.set(reinterpret_cast<rpcinterface_object*>(cls));
	return rif;
}
rpcinterface rpcobservable::get_observable_interface(const char *clsname)
{
	rpcobservable_impl* impl = reinterpret_cast<rpcobservable_impl*>(this);
	auto* cls = impl->get_observable_interface(clsname);
	rpcinterface rif;
	if (!cls) return rif;
	cls->addref();
	rif.set(reinterpret_cast<rpcinterface_object*>(cls));
	return rif;
}

rpcobservable_data* rpcobservable::get_observable_data(void)
{
	rpcobservable_impl* impl = reinterpret_cast<rpcobservable_impl*>(this);
	return impl->get_observable_data();
}

int rpcobservable::add_observable_instance(const char *clsname, void *inst)
{
	rpcobservable_impl* impl = reinterpret_cast<rpcobservable_impl*>(this);
	return impl->add_observable_instance(clsname, inst);
}
void rpcobservable::release_observable_instance(void *inst)
{
	rpcobservable_impl* impl = reinterpret_cast<rpcobservable_impl*>(this);
	impl->release_observable_instance(inst);
}

int rpcobservable::add_client_instance(const char *cliname, 
	const char *inst_name, void *inst)
{
	rpcobservable_impl* impl = reinterpret_cast<rpcobservable_impl*>(this);
	return impl->add_client_instance(cliname, inst_name, inst);
}
void rpcobservable::release_client_instance(void *inst)
{
	rpcobservable_impl* impl = reinterpret_cast<rpcobservable_impl*>(this);
	impl->release_client_instance(inst);
}

void rpcobservable::invoke_observable_method(void* inst_id, const char* clsname,
	const uint128_t& uuid,
	google::protobuf::Message* inparam,
	google::protobuf::Message* inout_param,
	uint32_t methodid, const char* method_name)
{
	rpcobservable_impl* impl = reinterpret_cast<rpcobservable_impl*>(this);
	impl->invoke_observable_method(inst_id, clsname, uuid, inparam,
		inout_param, methodid, method_name);
}

void rpcobservable::invoke_observable_method(void* inst_id, const char* clsname,
	const uint128_t& uuid, void* input, size_t insize, std::string &inout,
	size_t &inoutsize, uint32_t methodid, const char* method_name)
{
	rpcobservable_impl* impl = reinterpret_cast<rpcobservable_impl*>(this);
	impl->invoke_observable_method(inst_id, clsname, uuid, input, insize,
		inout, inoutsize, methodid, method_name);
}

void rpcobservable::invoke_observable_method(void* inst_id,
	const char* cli_name, const char* inst_name, const char* clsname,
	const uint128_t& uuid, void* input, size_t insize,
	std::string &inout, size_t &inoutsize, uint32_t methodid,
	const char* method_name)
{
	rpcobservable_impl* impl = reinterpret_cast<rpcobservable_impl*>(this);
	impl->invoke_observable_method(inst_id, cli_name, inst_name, clsname,
		uuid, input, insize, inout, inoutsize, methodid, method_name);
}

}}} // end of namespace zas::mware::rpc
/* EOF */