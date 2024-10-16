#include "rpc/rpcmgr.h"
#include "utils/mutex.h"
#include "utils/buffer.h"
#include "mware/pkgconfig.h"
#include "inc/rpcmgr-impl.h"
#include "inc/rpchost.h"
#include "inc/rpcobservable.h"
#include "inc/rpcclient.h"
#include "inc/rpcserver.h"
#include "inc/rpc_pkg_def.h"
#include "inc/rpcproxy.h"
#include "inc/rpcobservable.h"
#include "inc/rpcbridge.h"

namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;
using namespace zas::mware;

static mutex zrpcmux;
static rpcmgr_impl* _inst = NULL;

bool zrpc_package_notify(void* owner, evlclient sender,
	const package_header& pkghdr,
	const triggered_pkgevent_queue& queue)
{
	auto *handle = reinterpret_cast<rpcmgr_impl*> (owner);
	return handle->handle_evl_package(sender, pkghdr, queue);
}

bool rpcmgr_impl::handle_evl_package(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{

	int ret = -1;
	switch(pkghdr.pkgid){
		case REQUEST_SERVER_CTRL:
		{
			//zrpc server handle message
			if (_rpc_server)
				ret = _rpc_server->handle_evl_package(sender, pkghdr,queue);
		}
		break;
		case SERVER_REPLY_CTRL:
		{
			// zrpc client or host msg
			return handle_server_reply(sender, pkghdr, queue);
		}
		break;
		case REQUEST_START_SERVICE_CTRL:
		{
			// zrpc client or host msg
			if (_rpc_bridge) {
				return _rpc_bridge->handle_service_start(sender, pkghdr, queue);
			} else if (_rpc_host) {
				return _rpc_host->handle_service_start(sender, pkghdr, queue);
			}
		}
		break;
		case REPLY_START_SERVICE_CTRL:
		{
			//zrpc server handle message
			if (_rpc_server)
				ret = _rpc_server->handle_service_package(sender,
					 pkghdr,queue);
		}
		break;
		case INVOKE_METHOD_REQUEST_CTRL:
		{
			//zrpc server handle message
			if (_rpc_bridge) {
				ret = _rpc_bridge->handle_invoke_request(sender,
					 pkghdr,queue);
			} else if (_rpc_host)
				ret = _rpc_host->handle_invoke_request(sender,
					 pkghdr,queue);
		}
		break;
		case INVOKE_METHOD_REPLY_CTRL:
		{
			//zrpc server handle message
			ret = rpcproxy_impl::handle_invoke_reply_package(sender,
					pkghdr,queue);
		}
		break;
		case CLIENT_REQUEST_SERVICE_CTRL:
		{
			//zrpc server handle message
			if (_rpc_bridge) {
				ret = _rpc_bridge->handle_client_request(sender,pkghdr,queue);
			} else if (_rpc_host)
				ret = _rpc_host->handle_client_request(sender,
					 pkghdr,queue);
		}
		break;
		case SERVICE_REPLY_CLIENT_CTRL:
		{
			//zrpc server handle message
			if (_rpc_client)
				ret = _rpc_client->handle_service_reply_package(sender,
					 pkghdr,queue);
		}
		break;
		case OBSERVABLE_INVOKE_METHOD_REQUEST_CTRL:
		{
			//zrpc server handle message
			ret = rpcobservable_impl::inst()->
				handle_invoke_request_package(sender, pkghdr,queue);
		}
		break;
		case OBSERVABLE_INVOKE_METHOD_REPLY_CTRL:
		{
			//zrpc server handle message
			ret = rpcobservable_impl::inst()->
				handle_invoke_reply_package(sender, pkghdr,queue);
		}
		break;
		case CMD_REQUEST_SERVER_CTRL:
		{
			//zrpc server handle message
			if (_rpc_server)
				ret = _rpc_server->
					handle_cmd_request_package(sender, pkghdr,queue);
		}
		break;
		case CMD_SERVER_REPLY_CTRL:
		{
			//zrpc mgr handle message
			handle_cmd_reply_package(sender, pkghdr,queue);
		}
		break;
		default:
		break;
	}
	if (0 == ret) return true;
	return false;
}

cmd_service_info::cmd_service_info()
: bufsize(0) , pkgname(0), name(0), inst_name(0)
{
	validity.all = 0;
}

int rpcmgr_impl::handle_server_reply(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	server_reply* reply = (server_reply*)
		alloca(pkghdr.size);
	assert(NULL != reply);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(NULL != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)reply, pkghdr.size);
	assert(sz == pkghdr.size);
	if (reply_type_service == reply->reply) {
		if (_rpc_bridge) {
			return _rpc_bridge->handle_server_reply_package(sender,
				pkghdr, queue, reply);
		} else if (_rpc_host) {
			return _rpc_host->handle_server_reply_package(sender,
				pkghdr, queue, reply);
		}
	} else if (reply_type_client == reply->reply
			|| reply_type_other == reply->reply) {
		if (_rpc_client) {
			return _rpc_client->handle_server_reply_package(sender,
				pkghdr, queue, reply);
		}
	}
	return -ENOTFOUND;
}

int rpcmgr_impl::add_package_listener(void)
{
	evloop::inst()->add_package_listener(REQUEST_SERVER_CTRL,
		zrpc_package_notify, this);	
	evloop::inst()->add_package_listener(SERVER_REPLY_CTRL,
		zrpc_package_notify, this);	
	evloop::inst()->add_package_listener(REQUEST_START_SERVICE_CTRL,
		zrpc_package_notify, this);	
	evloop::inst()->add_package_listener(REPLY_START_SERVICE_CTRL,
		zrpc_package_notify, this);	
	evloop::inst()->add_package_listener(INVOKE_METHOD_REQUEST_CTRL,
		zrpc_package_notify, this);	
	evloop::inst()->add_package_listener(INVOKE_METHOD_REPLY_CTRL,
		zrpc_package_notify, this);	
	evloop::inst()->add_package_listener(OBSERVABLE_INVOKE_METHOD_REQUEST_CTRL,
		zrpc_package_notify, this);	
	evloop::inst()->add_package_listener(OBSERVABLE_INVOKE_METHOD_REPLY_CTRL,
		zrpc_package_notify, this);	
	evloop::inst()->add_package_listener(CLIENT_REQUEST_SERVICE_CTRL,
		zrpc_package_notify, this);	
	evloop::inst()->add_package_listener(SERVICE_REPLY_CLIENT_CTRL,
		zrpc_package_notify, this);	
	evloop::inst()->add_package_listener(CMD_REQUEST_SERVER_CTRL,
		zrpc_package_notify, this);	
	evloop::inst()->add_package_listener(CMD_SERVER_REPLY_CTRL,
		zrpc_package_notify, this);	
	return 0;
}

int rpcmgr_impl::remove_package_listener(void)
{
	evloop::inst()->remove_package_listener(REQUEST_SERVER_CTRL, 
		zrpc_package_notify, this);
	evloop::inst()->remove_package_listener(SERVER_REPLY_CTRL, 
		zrpc_package_notify, this);
	evloop::inst()->remove_package_listener(REQUEST_START_SERVICE_CTRL, 
		zrpc_package_notify, this);
	evloop::inst()->remove_package_listener(REPLY_START_SERVICE_CTRL, 
		zrpc_package_notify, this);
	evloop::inst()->remove_package_listener(INVOKE_METHOD_REQUEST_CTRL, 
		zrpc_package_notify, this);
	evloop::inst()->remove_package_listener(INVOKE_METHOD_REPLY_CTRL, 
		zrpc_package_notify, this);
	evloop::inst()->remove_package_listener(
		OBSERVABLE_INVOKE_METHOD_REQUEST_CTRL, zrpc_package_notify, this);
	evloop::inst()->remove_package_listener(
		OBSERVABLE_INVOKE_METHOD_REPLY_CTRL, zrpc_package_notify, this);
	evloop::inst()->remove_package_listener(CLIENT_REQUEST_SERVICE_CTRL, 
		zrpc_package_notify, this);
	evloop::inst()->remove_package_listener(SERVICE_REPLY_CLIENT_CTRL, 
		zrpc_package_notify, this);
	evloop::inst()->remove_package_listener(CMD_REQUEST_SERVER_CTRL, 
		zrpc_package_notify, this);
	evloop::inst()->remove_package_listener(CMD_SERVER_REPLY_CTRL, 
		zrpc_package_notify, this);
	return 0;
}

rpcmgr_impl::rpcmgr_impl()
: _rpc_client(nullptr), _rpc_host(nullptr), _rpc_server(nullptr)
, _rpc_bridge(nullptr)
{
	add_package_listener();
}

rpcmgr_impl::~rpcmgr_impl()
{
	remove_package_listener();
}

rpchost_impl* rpcmgr_impl::get_service(const char* service_name, 
	const char* inst_name)
{
	if (!service_name || !*service_name) return NULL;

	auto_mutex am(_mut);
	// if create rpc server, can't create host again
	if (_rpc_server) return NULL;

	// check host exist
	if (!_rpc_host) return NULL;

	const pkgconfig& p = pkgconfig::getdefault();
	auto* tmp = _rpc_host->get_service(p.get_package_name(),
		service_name);
	if (!tmp) return NULL;	
	if (!inst_name || !*inst_name) return tmp;

	return tmp->find_instance(inst_name);
}

rpchost_impl* rpcmgr_impl::load_service(const char* service_name, 
	const char* inst_name, const char* executive)
{
	if (!service_name || !*service_name
		|| !executive || !*executive) return NULL;

	auto_mutex am(_mut);

	// if create rpc server, can't create host again
	if (_rpc_server) return NULL;

	// check package config and load host
	const pkgconfig& p = pkgconfig::getdefault();
	// not find service
	if (!p.check_service(service_name)) return NULL;

	// get service info
	pkgcfg_service_info service = p.get_service_by_name(service_name);
	// check instance in package
	if (inst_name && *inst_name) {
		if (!service->check_instance(inst_name))
			return nullptr;
	}

	// check host exist
	if (!_rpc_host)
		_rpc_host = new rpchost_mgr();

	// check service info
	auto* impl = _rpc_host->get_service(p.get_package_name(),
		service->get_name());
	if (!impl) {
		// add new service
		impl = _rpc_host->add_service(p.get_package_name(),
			service->get_name(), service->get_executive(),
			service->get_ipaddr(), service->get_port(),
			service->get_version(), service->get_attributes());
		if (impl->is_shared_service()) {
			// if service is shared, add all instance
			int cnt = service->get_instance_count();
			for (int i = 0; i < cnt; i++) {
				impl->add_instance(service->get_instance_by_index(i));
			}
			// if service is shared, add all shared service
			load_all_shared_services_unlock();
		}
	}

	if (inst_name && *inst_name) {
		auto* instimpl = impl->find_inst_unlock(inst_name);
		if (!instimpl) {
			rpchost_impl* instnew = new rpchost_impl(p.get_package_name(),
			service->get_name(), inst_name, service->get_executive(),
			service->get_ipaddr(), service->get_port(),
			service->get_version(), service->get_attributes());
			if (impl->add_instance_unlock(instnew)) {
				delete instnew; return nullptr;
			}
			impl = instnew;
		} else {
			impl = zas_downcast(service_info, rpchost_impl, instimpl);
		}
	}
	return impl;
}

int rpcmgr_impl::load_all_shared_services_unlock(void)
{
	assert(NULL != _rpc_host);
	const pkgconfig& p = pkgconfig::getdefault();
	int svccnt = p.get_service_count();
	for (int i = 0; i < svccnt; i++) {
		pkgcfg_service_info service = p.get_service_by_index(i);
		// shared can be load all shared services
		if (!service->is_shared()) continue;

		// check service exist
		if (_rpc_host->get_service(p.get_package_name(),
			service->get_name()))
			continue;
		// add new service
		auto *impl = _rpc_host->add_service(p.get_package_name(),
			service->get_name(), service->get_executive(),
			service->get_ipaddr(), service->get_port(),
			service->get_version(), service->get_attributes());
		// add service instance
		int cnt = service->get_instance_count();
		for (int j = 0; j < cnt; j++) {
			impl->add_instance(service->get_instance_by_index(j));
		}
	}
	return 0;
}

rpcclient_mgr* rpcmgr_impl::get_client_mgr(void)
{
	if (_rpc_client) return _rpc_client;

	auto_mutex am(_mut);
	if (!_rpc_client)
		_rpc_client = new rpcclient_mgr();
	return _rpc_client;
}

#define RPCSERVER_RUN_PACKAGE "zas.system"
#define RPCSERVER_RUN_HOSTNAME "sysd"

rpcserver_impl* rpcmgr_impl::get_server()
{
	//check curr pkg have this serive
	// const pkgconfig& p = pkgconfig::getdefault();
	// if (strcmp(p.get_pkg_name(), RPCSERVER_RUN_PACKAGE))
	//		return NULL;
	// if (!p.check_pkg_service(RPCSERVER_RUN_HOSTNAME)) return NULL;

	if (_rpc_server) return _rpc_server;

	auto_mutex am(_mut);
	if (NULL == _rpc_server){
		_rpc_server = new rpcserver_impl();
	}
	return _rpc_server;
}

int rpcmgr_impl::load_info(cmd_service_info* info,
	const char* pkgname, const char* service_name, const char* inst_name)
{
	if (!pkgname || !*pkgname
		|| !service_name || !*service_name)
		return -ENOTAVAIL;

	info->pkgname = info->bufsize;
	strcpy(info->buf + info->pkgname, pkgname);
	info->validity.m.pkgname = 1;
	info->bufsize += strlen(pkgname) + 1;

	info->name = info->bufsize;
	strcpy(info->buf + info->name, service_name);
	info->validity.m.name = 1;
	info->bufsize += strlen(service_name) + 1;
	
	if (inst_name && *inst_name) {
		info->inst_name = info->bufsize;
		strcpy(info->buf + info->inst_name,
			inst_name);
		info->validity.m.inst_name = 1;
		info->bufsize += strlen(inst_name) + 1;
	}
	return 0;
}

int rpcmgr_impl::request_service_cmd(uint16_t reqtype,
	const char* pkgname, const char* service_name, const char* inst_name)
{
	if (!pkgname || !*pkgname
		|| !service_name || !*service_name)
		return -EBADPARM;
	size_t sz = strlen(pkgname) + 1 + strlen(service_name) + 1;
	if (inst_name && *inst_name)
		sz += strlen(inst_name) + 1;
	cmd_request_server_pkg* svrreq = new(alloca(sizeof(*svrreq) + sz))
		cmd_request_server_pkg(sz);
	cmd_request_server& rinfo = svrreq->payload();
	load_info(&(rinfo.service_info), pkgname, service_name, inst_name);
	rinfo.request = reqtype;
	//get launch cli
	evlclient svrcli = evloop::inst()->getclient(NULL, NULL);
	evpoller poller;
	auto* ev = poller.create_event(evp_evid_package_with_seqid,
		CMD_SERVER_REPLY_CTRL, svrreq->header().seqid);
	assert(NULL != ev);
	
	// submit the event
	ev->submit();
	size_t sendsz = svrcli->write((void*)svrreq, sizeof(*svrreq) + sz);	
	if (sendsz < sizeof(*svrreq) + sz) {
		return -EBADOBJ;
	}
	if (poller.poll(10000)) {
		return -4;
	}
	ev = poller.get_triggered_event();
	assert(!poller.get_triggered_event());
	int status = -1;
	ev->read_outputbuf(&status, sizeof(status));
	return status;
}

int rpcmgr_impl::handle_cmd_reply_package(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	cmd_server_reply* reply = (cmd_server_reply*)
		alloca(pkghdr.size);
	assert(NULL != reply);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(NULL != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)reply, pkghdr.size);
	assert(sz == pkghdr.size);
	auto* ev = queue.dequeue();
	assert(NULL == queue.dequeue());
	int status = reply->result;
	if (status || reply_type_other != reply->reply) {
		ev->write_outputbuf(&status, sizeof(status));
		return 0;		
	}
	// update info of event
	ev->write_outputbuf(&status, sizeof(status));
	return -ENOTFOUND;
}

int rpcmgr_impl::start_service(const char* pkgname,
	const char* service_name, const char* inst_name)
{
	if (!pkgname || !*pkgname || !service_name || !*service_name)
		return -EBADPARM;
	if (_rpc_server) {
		return _rpc_server->start_service_internal(pkgname, service_name,
			inst_name, cmd_reqtype_start_service);
	}
	return request_service_cmd(cmd_reqtype_start_service, 
		pkgname, service_name, inst_name);
}

int rpcmgr_impl::stop_service(const char* pkgname, 
	const char* service_name, const char* inst_name)
{
	if (!pkgname || !*pkgname || !service_name || !*service_name)
		return -EBADPARM;

	if (!_rpc_client) return -ENOTAVAIL;
	auto* cli = _rpc_client->get_client(pkgname, service_name, inst_name);
	if (!cli) return -ENOTAVAIL;
	return cli->request_service_stop();
}

int rpcmgr_impl::terminate_service(const char* pkgname,
	const char* service_name, const char* inst_name)
{
	if (!pkgname || !*pkgname || !service_name || !*service_name)
		return -EBADPARM;
	return request_service_cmd(cmd_reqtype_terminate_service, 
		pkgname, service_name, inst_name);
}

int rpcmgr_impl::register_service(const char* service_name,
	const char* inst_name,void* create_svc, void* destory_svc)
{
	rpchost_impl* impl = get_service(service_name, inst_name);
	if (!impl) return -ENOTFOUND;
	return impl->set_service_impl_method(create_svc, destory_svc);
}

service* rpcmgr_impl::get_service_impl(const char* service_name,
	const char* inst_name)
{
	rpchost_impl* impl = get_service(service_name, inst_name);
	if (!impl) return nullptr;
	return impl->get_service_impl();
}

rpcbridge_impl* rpcmgr_impl::load_bridge(const char* pkg_name,
	const char* service_name,
	const char* inst_name)
{
	printf("load bridge is start\n");
	if ( !pkg_name || !service_name) return nullptr;
	if (!_rpc_bridge)
		_rpc_bridge = new rpcbridge_mgr();
	return _rpc_bridge->add_bridge(pkg_name, service_name, inst_name);
}

rpcbridge_impl* rpcmgr_impl::get_bridge(const char* pkg_name,
	const char* service_name, const char* inst_name)
{
	if (!pkg_name || !service_name) return nullptr;
	if (!_rpc_bridge)
		_rpc_bridge = new rpcbridge_mgr();
	return _rpc_bridge->get_bridge(pkg_name, service_name, inst_name);
}

int rpcmgr_impl::register_bridge(create_bridge_service create,
	destory_bridge_service destory)
{
	if (!create || !destory) return -EBADPARM;
	if (!_rpc_bridge)
		_rpc_bridge = new rpcbridge_mgr();
	return _rpc_bridge->register_bridge(create, destory);
}

//----------zrpc interface start--------------

rpcmgr* rpcmgr::inst()
{
	if (_inst) return reinterpret_cast<rpcmgr*>(_inst);

	auto_mutex am(zrpcmux);
	if (_inst) return reinterpret_cast<rpcmgr*>(_inst);
	_inst = new rpcmgr_impl();
	assert(NULL != _inst);
	return reinterpret_cast<rpcmgr*>(_inst);
}

rpcmgr_impl* rpcmgr_impl::instance(void)
{
	if (_inst) return _inst;

	auto_mutex am(zrpcmux);
	if (_inst) return _inst;
	_inst = new rpcmgr_impl();
	assert(NULL != _inst);
	return _inst;
}

rpchost rpcmgr::load_service(const char* service_name, const char* inst_name,
	const char* executive, const char* startmode)
{
	rpchost zrpchost;
	rpcmgr_impl* zrpc = reinterpret_cast<rpcmgr_impl*>(this);
	if (!zrpc || !service_name || !*service_name
		|| !executive || !*executive) return zrpchost;

	rpchost_impl* rpcimpl = zrpc->load_service(service_name,
		inst_name, executive);
	if (!rpcimpl) return zrpchost;
	rpcimpl->run();
	if (startmode && *startmode && !strcmp(startmode, "startrequest"))
		rpcimpl->start_service();
	rpcimpl->addref();
	zrpchost.set(reinterpret_cast<rpchost_object*>(rpcimpl));
	return zrpchost;
}

rpchost rpcmgr::get_service(const char* service_name, 
	const char* inst_name)
{
	rpchost zrpchost;
	rpcmgr_impl* zrpc = reinterpret_cast<rpcmgr_impl*>(this);
	if (!zrpc || !service_name || !*service_name) return zrpchost;

	rpchost_impl* rpcimpl = zrpc->get_service(service_name, inst_name);
	if (!rpcimpl) return zrpchost;
	rpcimpl->addref();
	zrpchost.set(reinterpret_cast<rpchost_object*>(rpcimpl));
	return zrpchost;
}

rpcserver rpcmgr::get_server(void)
{
	rpcmgr_impl* zrpc = reinterpret_cast<rpcmgr_impl*>(this);
	rpcserver zrpcsvr;
	if (NULL == zrpc) return zrpcsvr;
	rpcserver_impl* rpcimpl = zrpc->get_server();
	if (NULL == rpcimpl) return zrpcsvr;
	zrpcsvr.set(reinterpret_cast<rpcserver_object*>(rpcimpl));
	return zrpcsvr;
}

int rpcmgr::start_service(const char* pkgname, const char* service_name,
	const char* inst_name)
{
	rpcmgr_impl* zrpc = reinterpret_cast<rpcmgr_impl*>(this);

	if (NULL == zrpc) return -EINVALID;
	if (!pkgname || !pkgname || !service_name || !*service_name)
		return -EBADPARM;

	return zrpc->start_service(pkgname, service_name, inst_name);
}

int rpcmgr::stop_service(const char* pkgname, const char* service_name,
	const char* inst_name)
{
	rpcmgr_impl* zrpc = reinterpret_cast<rpcmgr_impl*>(this);

	if (NULL == zrpc) return -EINVALID;
	if (!pkgname || !pkgname || !service_name || !*service_name)
		return -EBADPARM;

	return zrpc->stop_service(pkgname, service_name, inst_name);
}

int rpcmgr::terminate_service(const char* pkgname, const char* service_name,
	const char* inst_name)
{
	rpcmgr_impl* zrpc = reinterpret_cast<rpcmgr_impl*>(this);

	if (NULL == zrpc) return -EINVALID;
	if (!pkgname || !pkgname || !service_name || !*service_name)
		return -EBADPARM;

	return zrpc->terminate_service(pkgname, service_name, inst_name);
}

int rpcmgr::register_service(const char* service_name, const char* inst_name,
	create_service create_svc, destory_service destory_svc)
{
	rpcmgr_impl* zrpc = reinterpret_cast<rpcmgr_impl*>(this);

	if (NULL == zrpc) return -EINVALID;
	if (!service_name || !*service_name || !create_svc || !destory_svc)
		return -EBADPARM;

	return zrpc->register_service(service_name, inst_name,
		(void*)create_svc, (void*)destory_svc);
}

service* rpcmgr::get_service_impl(const char* service_name,
	const char* inst_name)
{
	rpcmgr_impl* zrpc = reinterpret_cast<rpcmgr_impl*>(this);

	if (NULL == zrpc) return nullptr;
	if (!service_name || !*service_name)
		return nullptr;

	return zrpc->get_service_impl(service_name, inst_name);
}

rpcbridge rpcmgr::load_bridge(const char* pkg_name,
	const char* service_name, const char* inst_name)
{
	rpcmgr_impl* zrpc = reinterpret_cast<rpcmgr_impl*>(this);
	rpcbridge ebridge;
	if (NULL == zrpc) return ebridge;
	auto* bridge_imp = zrpc->load_bridge(pkg_name,
		service_name, inst_name);
	if (!bridge_imp) return ebridge;
	bridge_imp->addref();
	printf("load bridge is rund\n");
	bridge_imp->run();
	printf("load bridge is finished\n");
	ebridge.set(reinterpret_cast<rpcbridge_object*>(bridge_imp));
	return ebridge;
}

rpcbridge rpcmgr::get_bridge(const char* pkg_name,
	const char* service_name, const char* inst_name)
{
	rpcmgr_impl* zrpc = reinterpret_cast<rpcmgr_impl*>(this);
	rpcbridge ebridge;
	if (NULL == zrpc) return ebridge;
	if (!pkg_name || !*pkg_name || !service_name || !*service_name)
		return ebridge;
	auto* bridge_imp = zrpc->get_bridge(pkg_name, service_name, inst_name);
	if (!bridge_imp) return ebridge;
	bridge_imp->addref();
	ebridge.set(reinterpret_cast<rpcbridge_object*>(bridge_imp));
	return ebridge;
}

int rpcmgr::register_bridge(create_bridge_service create,
	destory_bridge_service destory)
{
	rpcmgr_impl* zrpc = reinterpret_cast<rpcmgr_impl*>(this);

	if (NULL == zrpc) return -EINVALID;
	if (!create || !destory)
		return -EBADPARM;

	return zrpc->register_bridge(create, destory);
}

int rpcinterface_object::addref()
{
	rpcclass_node* clsnd = reinterpret_cast<rpcclass_node*>(this);
	if (NULL == clsnd) return 0;
	return clsnd->addref();
}

int rpcinterface_object::release()
{
	rpcclass_node* clsnd = reinterpret_cast<rpcclass_node*>(this);
	if (NULL == clsnd) return 0;
	return clsnd->release();
}

int rpcinterface_object::add_method(const uint128_t& uuid,
	pbf_wrapper inparam, pbf_wrapper inout_param, rpcmethod_handler hdr)
{
	rpcclass_node* clsnd = reinterpret_cast<rpcclass_node*>(this);
	if (NULL == clsnd) return -ENOTAVAIL;
	return clsnd->add_method(uuid, inparam, inout_param, hdr);
}

void rpcinterface_object::set_singleton(bool singleton)
{
	rpcclass_node* clsnd = reinterpret_cast<rpcclass_node*>(this);
	if (NULL == clsnd) return;
	clsnd->set_singleton(singleton);
}

int rpchost_object::addref()
{
	rpchost_impl* zhost = reinterpret_cast<rpchost_impl*>(this);
	if (NULL == zhost) return -ENOTAVAIL;
	return zhost->addref();
}

int rpchost_object::release()
{
	rpchost_impl* zhost = reinterpret_cast<rpchost_impl*>(this);
	if (NULL == zhost) return -ENOTAVAIL;
	return zhost->release();
}

int rpchost_object::add_instid(const char* clsname, void* inst_id)
{
	rpchost_impl* zhost = reinterpret_cast<rpchost_impl*>(this);
	if (NULL == zhost) return -ENOTAVAIL;
	return zhost->add_instid(clsname, inst_id);
}

int rpchost_object::release(void* inst_id)
{
	rpchost_impl* zhost = reinterpret_cast<rpchost_impl*>(this);
	if (NULL == zhost) return -ENOTAVAIL;
	return zhost->release(inst_id);
}

int rpchost_object::keep_instance(void* inst_id)
{
	rpchost_impl* zhost = reinterpret_cast<rpchost_impl*>(this);
	if (NULL == zhost) return -ENOTAVAIL;
	return zhost->keep_instance(inst_id);
}

rpcinterface
rpchost_object::register_interface(const char* clsname,
	void* factory, pbf_wrapper param, uint32_t flags)
{
	rpchost_impl* zhost = reinterpret_cast<rpchost_impl*>(this);
	rpcinterface zif;
	if (!zhost) return zif;
	rpcclass_node* nd = zhost->register_interface(clsname,
		factory, param, flags);
	if (!nd) return zif;
	nd->addref();
	zif.set(reinterpret_cast<rpcinterface_object*>(nd));
	return zif;
}

rpcinterface
rpchost_object::get_interface(const char *clsname)
{
	rpchost_impl* zhost = reinterpret_cast<rpchost_impl*>(this);
	rpcinterface zif;
	if (!zhost) return zif;
	rpcclass_node* nd = zhost->get_interface(clsname);
	if (!nd) return zif;
	nd->addref();
	zif.set(reinterpret_cast<rpcinterface_object*>(nd));
	return zif;
}

int rpcserver_object::addref()
{
	rpcserver_impl* zserver = reinterpret_cast<rpcserver_impl*>(this);
	if (NULL == zserver) return -ENOTAVAIL;
	return zserver->addref();
}

int rpcserver_object::release()
{
	rpcserver_impl* zserver = reinterpret_cast<rpcserver_impl*>(this);
	if (NULL == zserver) return -ENOTAVAIL;
	return zserver->release();
}

int rpcserver_object::load_config_file(uri& file)
{
	rpcserver_impl* zserver = reinterpret_cast<rpcserver_impl*>(this);
	if (NULL == zserver) return -ENOTAVAIL;
	return zserver->load_config_file(file);
}

int rpcserver_object::run()
{
	rpcserver_impl* zserver = reinterpret_cast<rpcserver_impl*>(this);
	if (NULL == zserver) return -ENOTAVAIL;
	return zserver->run();
}

int rpcbridge_object::addref()
{
	rpcbridge_impl* bridge = reinterpret_cast<rpcbridge_impl*>(this);
	if (NULL == bridge) return -ENOTAVAIL;
	return bridge->addref();
}

int rpcbridge_object::release()
{
	rpcbridge_impl* bridge = reinterpret_cast<rpcbridge_impl*>(this);
	if (NULL == bridge) return -ENOTAVAIL;
	return bridge->release();
}

int rpcbridge_object::senddata(evlclient evl, void* inst_id, uint32_t seqid,
	uint32_t result, uint8_t flag, void* data, size_t sz)
{
	rpcbridge_impl* bridge = reinterpret_cast<rpcbridge_impl*>(this);
	if (NULL == bridge) return -ENOTAVAIL;
	return bridge->senddata(evl, inst_id, seqid, result, flag, data, sz);
}

int rpcbridge_object::sendevent(std::string &eventid, void* data, size_t sz)
{
	rpcbridge_impl* bridge = reinterpret_cast<rpcbridge_impl*>(this);
	if (NULL == bridge) return -ENOTAVAIL;
	return bridge->sendevent(eventid, data, sz);
}

//----------zrpc interface end--------------
}}}

