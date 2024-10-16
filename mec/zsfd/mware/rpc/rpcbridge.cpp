#include "inc/rpcbridge.h"

#include <unistd.h>
#include <dlfcn.h>

#include "std/list.h"
#include "utils/evloop.h"
#include "utils/buffer.h"
#include "utils/avltree.h"
#include "utils/mutex.h"
#include "mware/pkgconfig.h"
#include "inc/rpc_pkg_def.h"
#include "inc/rpcmgr-impl.h"
#include "inc/rpchost.h"
#include "inc/rpcevent.h"

#include <google/protobuf/message.h>

namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;
#define SYSTME_APPLICATION_ROOTPATH "/zassys/sysapp/"

rpcbridge_impl::rpcbridge_impl()
: _service_id(0), _flags(0)
, _refcnt(1), _bridge_service(nullptr)
{
	listnode_init(_inst_list);
	listnode_init(_client_list);
}

rpcbridge_impl::rpcbridge_impl(const char* pkgname, 
	const char* service_name, const char* inst_name, 
	bridge_service* service)
: _service_id(0), _flags(0)
, _refcnt(1), _bridge_service(service)
, _inst_tree(nullptr)
, _client_tree(nullptr)
{
	listnode_init(_inst_list);
	listnode_init(_client_list);
	_pkg_name = pkgname;
	_service_name = service_name;
	_inst_name.clear();

	if (inst_name && *inst_name) _inst_name = inst_name;
}

rpcbridge_impl::~rpcbridge_impl()
{
	_bridge_service = nullptr;

}


int rpcbridge_impl::addref()
{
	return __sync_add_and_fetch(&_refcnt, 1);
}

int rpcbridge_impl::release()
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0) {
		delete this;
	}
	return cnt;
}

int rpcbridge_impl::senddata(evlclient evl, void* inst_id, uint32_t seqid,
	uint32_t result, uint8_t flag, void* data, size_t sz)
{
	uint32_t action = req_action_call_method;
	if (2 == flag) 
		action = req_action_get_instance;
	else if (3 == flag)
		action = req_action_destory;

	reply_invoke(evl, seqid, action, result, inst_id, data, sz);
	return 0;
}

int rpcbridge_impl::sendevent(std::string &eventid, void* data, size_t sz)
{
	rpcevent_impl::instance()->trigger_event(eventid.c_str(), data, sz);
	return 0;
}

int rpcbridge_impl::run()
{
	request_to_server_unlock(reqtype_service_start);
	return 0;
}

int rpcbridge_impl::stop()
{
	remove_all_client();
	request_to_server_unlock(reqtype_service_stop);
	return 0;
}

int rpcbridge_impl::stop_service(evlclient client)
{
	return remove_client_unlock(client);
}

uint32_t rpcbridge_impl::get_string_size()
{
	uint32_t sz = _pkg_name.length() + 1
		+ _service_name.length() + 1;
	if (_inst_name.length() > 0)
		sz += _inst_name.length() + 1;
	return sz;
}

int rpcbridge_impl::load_info(service_pkginfo* info)
{
	if (_pkg_name.length() < 1 || _service_name.length() < 1)
		return -ENOTAVAIL;

	if (_pkg_name.length() > 0){
		info->pkgname = info->bufsize;
		strcpy(info->buf + info->pkgname,
			_pkg_name.c_str());
		info->validity.m.pkgname = 1;
		info->bufsize +=_pkg_name.length() + 1;
	}
	if (_service_name.length() > 0){
		info->name = info->bufsize;
		strcpy(info->buf + info->name,
			_service_name.c_str());
		info->validity.m.name = 1;
		info->bufsize += _service_name.length() + 1;
	}
	
	if (_inst_name.length() > 0) {
		info->inst_name = info->bufsize;
		strcpy(info->buf + info->inst_name,
			_inst_name.c_str());
		info->validity.m.inst_name = 1;
		info->bufsize += _inst_name.length() + 1;
	}
	return 0;
}

int rpcbridge_impl::update_info(service_pkginfo &info,
	service_pkginfo_validity_u &val)
{
	if (_pkg_name.length() > 0 || _service_name.length() > 0)
		return -EEXISTS;

	if (!info.validity.m.pkgname || !info.validity.m.name)
		return -EBADPARM;

	_pkg_name = info.buf + info.pkgname;
	_service_name = info.buf + info.name;
	val.m.pkgname = 1;
	val.m.name = 1;
	if (info.validity.m.inst_name) {
		_inst_name = info.buf + info.inst_name;
		val.m.inst_name = 1;
	}
	return 0;
}

int rpcbridge_impl::check_info(service_pkginfo& info)
{
	if (_pkg_name.length() < 1 || _service_name.length() < 1)
		return -ENOTAVAIL;

	if (!info.validity.m.pkgname || !info.validity.m.name)
		return -EBADPARM;

	if (strcmp(info.buf + info.pkgname, _pkg_name.c_str())
		|| strcmp(info.buf + info.name, _service_name.c_str()))
		return -ELOGIC;

	if (_inst_name.length() > 0) {
		if (!info.validity.m.inst_name)
			return -EBADPARM;
		if (strcmp(info.buf + info.inst_name, _inst_name.c_str()))
			return -ELOGIC;
	}
	return 0;
}

rpcbridge_mgr::rpcbridge_mgr()
: _bridge_tree(nullptr)
, _bridge_dll_handle(nullptr)
, _bridge_create(nullptr)
, _bridge_destory(nullptr)
{
	listnode_init(_bridge_list);
}

rpcbridge_mgr::~rpcbridge_mgr()
{
	_bridge_create = nullptr;
	_bridge_destory = nullptr;
	if (_bridge_dll_handle)
		dlclose(_bridge_dll_handle);
	_bridge_dll_handle = nullptr;
	release_all_nodes();
}

int rpcbridge_mgr::load_bridge(const char* bridge_exec)
{
	if (!bridge_exec || !*bridge_exec) {
		return -EBADPARM;
	}

	std::string libfile = SYSTME_APPLICATION_ROOTPATH;
	const pkgconfig& p = pkgconfig::getdefault();
	libfile += p.get_package_name();
	libfile += "/";
	libfile += bridge_exec;

	_bridge_dll_handle = dlopen(libfile.c_str(), RTLD_LAZY);
	if (!_bridge_dll_handle) {
		printf("load service library %s\n", libfile.c_str());
		printf("load failure %s\n", dlerror());
		return -ELOGIC;
	}
	return 0;
}

int rpcbridge_mgr::register_bridge(create_bridge_service create,
	destory_bridge_service destory)
{
	if (!create || !destory) {
		return -EBADPARM;
	}
	_bridge_create = create;
	_bridge_destory = destory;
	return 0;
}

rpcbridge_impl* rpcbridge_mgr::add_bridge(const char* pkgname,
	const char* service_name, const char* inst_name)
{
	auto_mutex am(_mut);
	if (!pkgname || !*pkgname || !service_name ||!*service_name)
		return nullptr;

	auto* bridge_impl = find_bridge_unlock(pkgname, service_name, inst_name);
	if (bridge_impl) return nullptr;

	auto* bridges = _bridge_create(pkgname, service_name, inst_name);

	bridge_impl = new rpcbridge_impl(pkgname, service_name, inst_name, bridges);

	if (avl_insert(&_bridge_tree, &bridge_impl->avlnode,
		zrpc_bridge_compared)) {
		delete bridge_impl;
		return nullptr;
	}
	listnode_add(_bridge_list, bridge_impl->ownerlist);
	return bridge_impl;
}

int rpcbridge_mgr::add_bridge(rpcbridge_impl *bridgeimpl)
{
	auto_mutex am(_mut);
	if (!bridgeimpl)
		return -EBADPARM;

	if (avl_insert(&_bridge_tree, &bridgeimpl->avlnode,
		zrpc_bridge_compared)) {
		return -ELOGIC;
	}
	listnode_add(_bridge_list, bridgeimpl->ownerlist);
	return 0;
}

int rpcbridge_mgr::remove_bridge(const char* pkgname,
	const char* service_name, const char* inst_name)
{
	auto_mutex am(_mut);

	auto* bridge_impl = find_bridge_unlock(pkgname, service_name, inst_name);
	if (!bridge_impl) return -ENOTEXISTS;

	avl_remove(&_bridge_tree, &bridge_impl->avlnode);
	listnode_del(bridge_impl->ownerlist);
	delete bridge_impl;
	return 0;
}

rpcbridge_impl* rpcbridge_mgr::get_bridge(const char* pkgname,
	const char* service_name, const char* inst_name)
{
	auto_mutex am(_mut);
	return find_bridge_unlock(pkgname, service_name, inst_name);
}

rpcbridge_impl* rpcbridge_mgr::find_bridge_unlock(const char* pkgname,
	const char* service_name, const char* inst_name)
{
	rpcbridge_impl bridge_impl(pkgname, service_name, inst_name, nullptr);

	avl_node_t *bridge_nd = avl_find(_bridge_tree,
		&bridge_impl.avlnode, zrpc_bridge_compared);
	if (!bridge_nd) return nullptr;

	return AVLNODE_ENTRY(rpcbridge_impl, avlnode, bridge_nd);
}

int rpcbridge_mgr::zrpc_bridge_compared(avl_node_t* aa, avl_node_t* bb)
{
	auto* nda = AVLNODE_ENTRY(rpcbridge_impl, avlnode, aa);
	auto* ndb = AVLNODE_ENTRY(rpcbridge_impl, avlnode, bb);
	int ret = strcmp(nda->_pkg_name.c_str(), ndb->_pkg_name.c_str());
	if (!ret) {
		ret = strcmp(nda->_service_name.c_str(), ndb->_service_name.c_str());
		if (!ret) {
			if (nda->_inst_name.length() < 1 && ndb->_inst_name.length() < 1) {
				ret = 0;
			} else if (nda->_inst_name.length() < 1) {
				ret = -1;
			} else if (ndb->_inst_name.length() < 1) {
				ret = 1;
			} else {
				ret = strcmp(nda->_inst_name.c_str(), ndb->_inst_name.c_str());
			}
		}
	}
	if (ret > 0) return 1;
	else if (ret < 0) return -1;
	else return 0;
}

int rpcbridge_mgr::release_all_nodes(void)
{
	auto_mutex am(_mut);
	while (!listnode_isempty(_bridge_list)) {
		auto *bridge_impl = LIST_ENTRY(rpcbridge_impl,	\
			ownerlist, _bridge_list.next);
		listnode_del(bridge_impl->ownerlist);
		delete bridge_impl;
	}
	_bridge_tree = NULL;
	return 0;
}


int rpcbridge_mgr::handle_server_reply_package(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue,
	server_reply* reply)
{
	int status;
	// get the package payload
	auto* ev = queue.dequeue();
	assert(NULL == queue.dequeue());
	rpcbridge_impl* peer = NULL;
	ev->read_inputbuf(&peer, sizeof(void*));
	assert(NULL != peer);
	status = reply->result;
	if (peer->check_info(reply->service_info))
		status = -ELOGIC;

	if (reply->service_info.validity.m.id) {
		peer->set_service_id(reply->service_info.id);
		add_active_bridge(peer);
	}
	// update info of event
	ev->write_outputbuf(&status, sizeof(status));
	return 0;
}


int rpcbridge_mgr::handle_service_start(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	request_service* req = (request_service*)
		alloca(pkghdr.size);
	assert(NULL != req);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(NULL != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)req, pkghdr.size);
	assert(sz == pkghdr.size);

	rpcbridge_impl tmp;
	service_pkginfo_validity_u val;
	tmp.update_info(req->service_info, val);

	auto* bridge_impl = get_bridge(tmp._pkg_name.c_str(),
		tmp._service_name.c_str(), tmp._inst_name.c_str());
	if (!bridge_impl) {
		//TODO add instance
		if (_bridge_create) {
			auto* service = _bridge_create(tmp._pkg_name.c_str(),
				tmp._service_name.c_str(), tmp._inst_name.c_str());
			bridge_impl = new rpcbridge_impl(tmp._pkg_name.c_str(),
				tmp._service_name.c_str(), tmp._inst_name.c_str(),
				service);
			if (add_bridge(bridge_impl)) {
				printf("add bridge error\n");
				return -ELOGIC;
			}

		}
	}

	//frist reply message then handle command;
	size_t replysz = 0;
	if (bridge_impl)
		replysz = bridge_impl->get_string_size();	
	service_reply_pkg* svrrly = new(alloca(sizeof(*svrrly) + replysz))
		service_reply_pkg(replysz);
	service_reply& rinfo = svrrly->payload();
	svrrly->header().seqid = pkghdr.seqid;	
	if (bridge_impl)
		bridge_impl->load_info(&(rinfo.service_info));
	rinfo.result = bridge_impl ? 0 : -1;

	sender->write((void*)svrrly, sizeof(*svrrly) + replysz);

	// handle command
	if (bridge_impl) {
		if (zrpc_svc_req_start == req->request 
			|| zrpc_svc_start_request == req->request) {
			bridge_impl->run(); 
		} else if (zrpc_svc_stop_request == req->request
			||zrpc_svc_terminate_request == req->request){
			bridge_impl->stop(); 
		}
	}
	return 0;
}


int rpcbridge_mgr::handle_invoke_request(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	invoke_request* reqinfo = (invoke_request*)
		alloca(pkghdr.size);
	assert(NULL != reqinfo);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(NULL != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)reqinfo, pkghdr.size);
	assert(sz == pkghdr.size);
	// get the package payload

	invoke_reqinfo& rinfo = reqinfo->req_info;
	std::string clsname;
	if (rinfo.validity.m.class_name)
		clsname = rinfo.buf + rinfo.class_name;
	
	bool renew_inst = false;
	if (rinfo.validity.m.renew_inst && (1 == rinfo.renew_inst))
		renew_inst = true;

	std::string method_name;
	if (rinfo.validity.m.method_name)
		method_name = rinfo.buf + rinfo.method_name;

	uint16_t method_id = 0;
	if (rinfo.validity.m.method_id)
		method_id = rinfo.method_id;

	void* inst_id = NULL;
	if (rinfo.validity.m.instid)
		inst_id = reinterpret_cast<void*>(rinfo.instid);

	void* data_inbuf = NULL;
	size_t datainsize = 0;
	if (rinfo.validity.m.inparam) {
		data_inbuf = rinfo.buf + rinfo.inparam;
		datainsize = rinfo.inparam_sz;
	}
	void* data_iobuf = NULL;
	size_t dataiosize = 0;
	if (rinfo.validity.m.inoutparam) {
		data_iobuf = rinfo.buf + rinfo.inoutparam;
		dataiosize= rinfo.inoutparam_sz;
	}

	int result = -1;
	auto* active_service = find_active_bridge(reqinfo->svc_id);
	if (!active_service) {
		result = reply_type_no_service;
		return reply_invoke(sender, pkghdr.seqid,reqinfo->req_action,
			result, inst_id);
	}

	switch (reqinfo->req_action)
	{
	case req_action_call_method:
		result = active_service->handle_invoke(sender, inst_id, pkghdr.seqid, 
			clsname, reqinfo->method_uuid, renew_inst, 
			data_inbuf, datainsize,
			data_iobuf, dataiosize);
		break;
	case req_action_get_instance:
		result = active_service->handle_get_instance(sender, inst_id,
			pkghdr.seqid, clsname, false, data_inbuf, datainsize);
		break;
	case req_action_get_singleton:
		result = active_service->handle_get_instance(sender, inst_id,
			pkghdr.seqid, clsname, true, data_inbuf, datainsize);
		break;
	
	default:
		break;
	}
	return result;
}


int rpcbridge_mgr::handle_client_request(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	client_request_service* reqinfo = (client_request_service*)
		alloca(pkghdr.size);
	assert(NULL != reqinfo);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(NULL != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)reqinfo, pkghdr.size);
	assert(sz == pkghdr.size);
	// get the package payload
	int ret = -1;
	if (cli_req_svc_stop == reqinfo->request) {
		auto* active_impl = find_active_bridge(reqinfo->svc_id);
		if (active_impl)
			ret = active_impl->stop_service(sender);
	}

	service_reply_client_pkg* svcreply = new(alloca(sizeof(*svcreply)))
		service_reply_client_pkg(0);
	service_reply_client& rinfo = svcreply->payload();
	svcreply->header().seqid = pkghdr.seqid;
	rinfo.result = ret;
	sender->write((void*)svcreply, sizeof(*svcreply));
	return 0;
}

int rpcbridge_mgr::reply_invoke(evlclient sender, uint32_t seqid,
	uint32_t req_action, uint32_t reply_type,
	void* instid, void *data, size_t sz)
{
	size_t retsz = 0;
	if (reply_type_success == reply_type && data) {
		retsz += sz + 1;
	}
	invoke_reply_pkg* invokereply = new(alloca(sizeof(*invokereply) + sz))
		invoke_reply_pkg(sz);
	invoke_reply& rinfo = invokereply->payload();

	rinfo.req_action = req_action;
	rinfo.reply_type = reply_type;
	if (reply_type_success == reply_type) {
		rinfo.instid = (size_t)instid;
	}

	if (reply_type_success == reply_type && data) {
		rinfo.reply_info.result = sz;
		memcpy(rinfo.reply_info.buf, data, sz);
	}
	invokereply->header().seqid = seqid;
	sender->write((void*)invokereply, sizeof(*invokereply) + retsz);
	return 0;
}

int rpcbridge_mgr::add_active_bridge(rpcbridge_impl* impl)
{
	auto_mutex am(_mut);

	if (!impl || !impl->_service_id) return -EBADPARM;
	
	auto* rhost = find_active_bridge_unlock(impl->_service_id);
	if (rhost) return -EEXISTS;

	if (avl_insert(&_active_bridge_tree, &impl->avlactivenode,
		service_bridge_compared)){
		return -ELOGIC;
	}
	return 0;
}

int rpcbridge_mgr::remove_active_bridge(rpcbridge_impl* impl)
{
	if (!impl) return -EBADPARM;

	auto_mutex am(_mut);
	avl_remove(&_active_bridge_tree, &impl->avlactivenode);
	return 0;
}

int rpcbridge_mgr::remove_active_bridge(uint32_t service_id)
{
	auto_mutex am(_mut);
	auto *impl = find_active_bridge_unlock(service_id);
	if (!impl) return -EBADPARM;
	avl_remove(&_active_bridge_tree, &impl->avlactivenode);
	return 0;
}

rpcbridge_impl* rpcbridge_mgr::find_active_bridge_unlock(uint32_t service_id)
{
	avl_node_t* anode = avl_find(_active_bridge_tree, 
	MAKE_FIND_OBJECT(service_id, rpcbridge_impl, _service_id, avlactivenode),
	service_bridge_compared);
	if (!anode) return NULL;

	return AVLNODE_ENTRY(rpcbridge_impl, avlactivenode, anode);
}

rpcbridge_impl* rpcbridge_mgr::find_active_bridge(uint32_t service_id)
{
	auto_mutex am(_mut);
	return find_active_bridge_unlock(service_id);
}

int rpcbridge_mgr::service_bridge_compared(avl_node_t* aa, avl_node_t* bb)
{
	rpcbridge_impl *ahost = AVLNODE_ENTRY(rpcbridge_impl, avlactivenode, aa);
	rpcbridge_impl *bhost = AVLNODE_ENTRY(rpcbridge_impl, avlactivenode, bb);
	if (ahost->_service_id > bhost->_service_id) return 1;
	else if (ahost->_service_id < bhost->_service_id) return -1;
	else return 0;
}

int rpcbridge_impl::request_to_server_unlock(uint16_t reqtype)
{
	size_t sz = get_string_size();
	request_server_pkg* svrreq = new(alloca(sizeof(*svrreq) + sz))
		request_server_pkg(sz);
	request_server& rinfo = svrreq->payload();
	load_info(&(rinfo.service_info));
	rinfo.request = reqtype;
	if (reqtype_service_stop == reqtype) {
		_flags = 0;
	}

	evpoller poller;
	auto* ev = poller.create_event(evp_evid_package_with_seqid,
		SERVER_REPLY_CTRL, svrreq->header().seqid);
	assert(NULL != ev);
	rpcbridge_impl* self = this;
	ev->write_inputbuf(&self, sizeof(void*));
	// submit the event
	ev->submit();
	
	//get launch cli
	evlclient svrcli = evloop::inst()->getclient(NULL, NULL);
	size_t sendsz = svrcli->write((void*)svrreq, sizeof(*svrreq) + sz);	
	if (sendsz < sizeof(*svrreq) + sz) {
		return -EBADOBJ;
	}

	if (poller.poll(5000)) {
		return -4;
	}
	ev = poller.get_triggered_event();
	assert(!poller.get_triggered_event());
	int status = -1;
	ev->read_outputbuf(&status, sizeof(status));
	return status;
}

rpc_bridge_inst::rpc_bridge_inst(rpcbridge_impl* impl)
: _instid(nullptr)
, _header(impl)
, _refcnt(0)
{
}

rpc_bridge_inst::~rpc_bridge_inst()
{
	if (_instid)
		delete (int*)_instid;
	_instid = nullptr;
}

int rpc_bridge_inst::addref()
{
	return __sync_add_and_fetch(&_refcnt, 1);
}

int rpc_bridge_inst::release()
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0) {
		if (_header)
			_header->remove_inst_unlock(this);
		else 
			delete this;
	}
	return cnt;
}

int rpcbridge_impl::zrpc_bridge_inst_compared(avl_node_t* aa, avl_node_t* bb)
{
	auto* nda = AVLNODE_ENTRY(rpc_bridge_inst, avlnode, aa);
	auto* ndb = AVLNODE_ENTRY(rpc_bridge_inst, avlnode, bb);
	if (nda->_instid > ndb->_instid)
		return 1;
	else if (nda->_instid < ndb->_instid)
		return -1;
	else
		return 0;
}

int rpcbridge_impl::zrpc_bridge_client_compared(avl_node_t* aa, avl_node_t* bb)
{
	auto* nda = AVLNODE_ENTRY(rpc_bridge_client, avlnode, aa);
	auto* ndb = AVLNODE_ENTRY(rpc_bridge_client, avlnode, bb);
	if (nda->_client.get() > ndb->_client.get())
		return 1;
	else if (nda->_client.get() < ndb->_client.get())
		return -1;
	else
		return 0;
}

int rpcbridge_impl::add_client(evlclient cli, rpc_bridge_inst* inst)
{
	if (!inst) return -EBADPARM;
	auto* bridge_cli = find_client_unlock(cli);
	if (!bridge_cli) {
		bridge_cli = new rpc_bridge_client();
		bridge_cli->_client = cli;
		bridge_cli->add_bridge_inst_node(inst);
		return 0;
	}

	auto* node = bridge_cli->find_inst_node(inst);
	if (!node) {
		bridge_cli->add_bridge_inst_node(inst);
	}
	return 0;
}

rpc_bridge_inst* rpcbridge_impl::find_inst_unlock(void* inst_id)
{
	if (!inst_id) return nullptr;
	avl_node_t *bridge_nd = avl_find(_inst_tree,
		MAKE_FIND_OBJECT(inst_id, rpc_bridge_inst, _instid, avlnode), zrpc_bridge_inst_compared);
	if (!bridge_nd) return nullptr;
	return AVLNODE_ENTRY(rpc_bridge_inst, avlnode, bridge_nd);
}

rpc_bridge_client* rpcbridge_impl::find_client_unlock(evlclient cli)
{
	avl_node_t *bridge_nd = avl_find(_client_tree,
		MAKE_FIND_OBJECT(cli, rpc_bridge_client, _client, avlnode), zrpc_bridge_client_compared);
	if (!bridge_nd) return nullptr;
	return AVLNODE_ENTRY(rpc_bridge_client, avlnode, bridge_nd);
}

int rpcbridge_impl::remove_client_unlock(evlclient cli)
{
	auto* client = find_client_unlock(cli);
	if (!client) return -ENOTFOUND;
	avl_remove(&_client_tree, &client->avlnode);
	listnode_del(client->ownerlist);
	delete client;
	return 0;
}
int rpcbridge_impl::remove_inst_unlock(rpc_bridge_inst* inst)
{
	if (!inst) return -ENOTFOUND;
	avl_remove(&_inst_tree, &inst->avlnode);
	listnode_del(inst->ownerlist);
	delete inst;
	return 0;	
}
int rpcbridge_impl::remove_all_client(void)
{
	rpc_bridge_client *client = nullptr;
	while(!listnode_isempty(_client_list)) {
		client = LIST_ENTRY(rpc_bridge_client, ownerlist, _client_list.next);
		avl_remove(&_client_tree, &client->avlnode);
		listnode_del(client->ownerlist);
		delete client;
	}
	return 0;
}

int rpcbridge_impl::handle_invoke(evlclient cli, void* inst_id, uint32_t seqid, 
	std::string &clsname, uint128_t& method_uuid,
	bool renew_inst, void* indata, size_t insize,
	void* inoutdata, size_t inoutsize)
{
	if (!_bridge_service) return -ENOTAVAIL;
	uint8_t flag = 0;
	if (renew_inst) flag = 1;
	_bridge_service->on_recvdata(cli, inst_id, seqid, clsname,
		&method_uuid, flag, indata, insize, inoutdata, inoutsize);
	return 0;
}

int rpcbridge_impl::handle_get_instance(evlclient cli, void* instid,
	uint32_t seqid, std::string& clsname, bool singleton, void* data, size_t sz)
{
	if (!_bridge_service) return -ENOTAVAIL;
	uint8_t flag = 2;
	_bridge_service->on_recvdata(cli, instid, seqid, clsname, 
		nullptr, flag, data, sz, nullptr, 0);
	return 0;
}

int rpcbridge_impl::reply_invoke(evlclient sender, uint32_t seqid,
	uint32_t req_action, uint32_t reply_type,
	void* instid, void *data, size_t sz)
{
	size_t retsz = 0;
	if (reply_type_success == reply_type && data) {
		retsz += sz + 1;
	}
	invoke_reply_pkg* invokereply = new(alloca(sizeof(*invokereply) + sz))
		invoke_reply_pkg(sz);
	invoke_reply& rinfo = invokereply->payload();

	rinfo.req_action = req_action;
	rinfo.reply_type = reply_type;
	if (reply_type_success == reply_type) {
		rinfo.instid = (size_t)instid;
	}

	if (reply_type_success == reply_type && data) {
		rinfo.reply_info.result = sz;
		memcpy(rinfo.reply_info.buf, data, sz);
	}
	invokereply->header().seqid = seqid;
	sender->write((void*)invokereply, sizeof(*invokereply) + retsz);
	return 0;
}

rpc_bridge_client::rpc_bridge_client()
{
	listnode_init(_client_inst_list);
}

rpc_bridge_client::~rpc_bridge_client()
{
	remove_all_nodes();
}

int rpc_bridge_client::add_bridge_inst_node(rpc_bridge_inst* inst)
{
	if (!inst) return -EBADPARM;
	listnode_t * nd = _client_inst_list.next;
	bridge_inst_node *node = nullptr;
	for (; nd != &_client_inst_list; nd = nd->next) {
		node = LIST_ENTRY(bridge_inst_node, ownerlist, nd);
		if (inst == node->inst) return -EEXISTS;
	}
	node = new bridge_inst_node();
	node->inst = inst;
	inst->addref();
	listnode_add(_client_inst_list, node->ownerlist);
	return 0;
}

rpc_bridge_client::bridge_inst_node* 
rpc_bridge_client::find_inst_node(rpc_bridge_inst* inst)
{
	if (!inst) return nullptr;
	listnode_t * nd = _client_inst_list.next;
	bridge_inst_node *node = nullptr;
	for (; nd != &_client_inst_list; nd = nd->next) {
		node = LIST_ENTRY(bridge_inst_node, ownerlist, nd);
		if (inst == node->inst) return node;
	}
	return nullptr;
}

int rpc_bridge_client::remove_bridge_inst_node(rpc_bridge_inst* inst)
{
	if (!inst) return -EBADPARM;
	listnode_t * nd = _client_inst_list.next;
	bridge_inst_node *node = nullptr;
	for (; nd != &_client_inst_list; nd = nd->next) {
		node = LIST_ENTRY(bridge_inst_node, ownerlist, nd);
		if (inst == node->inst) {
			listnode_del(node->ownerlist);
			node->inst->release();
			delete node;
			return 0;
		}
	}
	return -ENOTFOUND;
}

int rpc_bridge_client::remove_all_nodes(void)
{
	bridge_inst_node *node = nullptr;
	while(!listnode_isempty(_client_inst_list)) {
		node = LIST_ENTRY(bridge_inst_node, ownerlist, _client_inst_list.next);
		if (node) {
			listnode_del(node->ownerlist);
			delete node;
		}
	}
	return 0;
}

}}}