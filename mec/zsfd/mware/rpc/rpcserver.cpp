
#include "inc/rpcserver.h"

#include "std/list.h"
#include "utils/absfile.h"
#include "utils/avltree.h"
#include "utils/buffer.h"
#include "utils/dir.h"
#include "utils/datapool.h"
#include "utils/evloop.h"
#include "inc/servicemgr.h"

namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;

#define SHARED_CONTAINER_NAME_PREFIX "__shared_contiander_"
#define ZRPC_SERVER_EVLOOP_LNR_NAME "zrp_server_listener"
#define ZRPC_SERVER_LAUNCH_CMD_PREFIX "exec://"
#define ZAS_SYSSVR_LAUNCH_ELE "zas.system.worker"
#define ZAS_SYSTEM_PROXY_PKG "zas.system"
#define ZAS_SYSTEM_PROXY_SERVICE "rpcbridge"
#define ZAS_SYSTEM_PROXY_EXE "services/rpcbridge.so"

static mutex host_info_mut;
static listnode_t host_info_list = LISTNODE_INITIALIZER(host_info_list);
static avl_node_t* host_info_tree = NULL;
static uint32_t zrpc_server_service_cnt = 0;

struct launch_pkg_info{
	uint16_t pkg_name;
	uint16_t inst_name;
	uint16_t pkg_info;
	uint16_t bufsz;
	char buf[0];
};

host_info::host_info(const char* pkgname,
	const char* containername, bool bshared)
: _pkg_name(pkgname), _container_inst(containername)
, _flags(0), _refcnt(0)
{
	_f.shared = bshared ? 1 : 0;
	listnode_init(_service_list);
	listnode_init(_wait_host_list);
}

host_info::~host_info()
{
	auto_mutex am(host_info_mut);
	listnode_del(ownerlist);
	avl_remove(&host_info_tree, &avlnode);

	host_service_info* info = NULL;
	while (!listnode_isempty(_service_list)) {
		info = LIST_ENTRY(host_service_info,	\
			ownerlist, _service_list.next);
		listnode_del(info->ownerlist);
		delete info;		
	}
}

int host_info::addto_host_list()
{
	auto_mutex am(host_info_mut);
	host_info *info = get_host_info(_container_inst.c_str());
	if (info) return -EEXISTS;
	if (avl_insert(&host_info_tree, &avlnode, host_info_compared)){
		return -ELOGIC;
	}
	listnode_add(host_info_list, ownerlist);
	return 0;
}

int host_info::attach_service(server_service* info)
{
	auto_mutex am(host_info_mut);
	if (!info) return -EBADPARM;

	if (find_host_service(info))
		return -EEXISTS;

	host_service_info* sinfo = new host_service_info();
	sinfo->service = info;
	listnode_add(_service_list, sinfo->ownerlist);
	addref();
	return 0;
}

int host_info::detach_service(server_service *info)
{
	auto_mutex am(host_info_mut);
	if (!info) return -EBADPARM;

	host_service_info* sinfo = find_host_service(info);
	if (!sinfo) return -ENOTEXISTS;

	delete sinfo; 
	release();

	return 0;
}

int host_info::add_wait_container_start(server_service *info, bool startcb)
{
	auto_mutex am(host_info_mut);

	if (find_wait_host_service(info))
		return -EEXISTS;
	if (is_running()) {
		if (startcb) {
			info->request_container(zrpc_svc_start_request);
		} else {
			info->request_container(zrpc_svc_req_start);
		}		
		return 0;
	}
	wait_host_service* svc = new wait_host_service();
	svc->service = info;
	svc->startcb = startcb;
	listnode_add(_wait_host_list, svc->ownerlist);
	return 0;
}

int host_info::start_wait_service(void)
{
	auto_mutex am(host_info_mut);
	if (listnode_isempty(_wait_host_list))
		return 0;
	wait_host_service *sinfo = NULL;
	while(!listnode_isempty(_wait_host_list)) {
		sinfo = LIST_ENTRY(wait_host_service, ownerlist, _wait_host_list.next);
		if (sinfo->startcb) {
			sinfo->service->request_container(zrpc_svc_start_request);
		} else {
			sinfo->service->request_container(zrpc_svc_req_start);
		}
		listnode_del(sinfo->ownerlist);
		delete sinfo;
	}
	return 0;
}

host_info::wait_host_service*
host_info::find_wait_host_service(server_service *svc)
{
	if (listnode_isempty(_wait_host_list))
		return NULL;

	listnode_t* nd = _wait_host_list.next;
	wait_host_service *sinfo = NULL;
	for (; nd != &_wait_host_list; nd = nd->next) {
		sinfo = LIST_ENTRY(wait_host_service, ownerlist, nd);
		if (sinfo->service == svc) return sinfo;
	}
	return NULL;
}

host_info::host_service_info*
host_info::find_host_service(server_service *svc)
{
	if (listnode_isempty(_service_list))
		return NULL;

	listnode_t* nd = _service_list.next;
	host_service_info *sinfo = NULL;
	for (; nd != &_service_list; nd = nd->next) {
		sinfo = LIST_ENTRY(host_service_info, ownerlist, nd);
		if (sinfo->service == svc) return sinfo;
	}
	return NULL;
}

int host_info::host_info_compared(avl_node_t *aa, avl_node_t *bb)
{
	host_info* infoa = AVLNODE_ENTRY(host_info, avlnode, aa);
	host_info* infob = AVLNODE_ENTRY(host_info, avlnode, bb);
	int ret = strcmp(infoa->_container_inst.c_str(),
		infob->_container_inst.c_str());
	if (ret > 0) return 1;
	else if (ret < 0) return -1;
	else return 0;
}

host_info* host_info::get_host_info(const char* containername)
{
	auto_mutex am(host_info_mut);

	avl_node_t* info = avl_find(host_info_tree, 
		MAKE_FIND_OBJECT(containername, host_info, _container_inst, avlnode),
		host_info_compared);
	if (!info) return NULL;

	return AVLNODE_ENTRY(host_info, avlnode, info);;
}

int host_info::disconnect_host(const char* cliname, const char* instname)
{
	auto_mutex am(host_info_mut);

	//find host_containner.
	// container name is instname, and container is unique
	avl_node_t* info = avl_find(host_info_tree, 
		MAKE_FIND_OBJECT(instname, host_info, _container_inst, avlnode),
		host_info_compared);
	if (!info) return -ENOTEXISTS;

	host_info* hinfo = AVLNODE_ENTRY(host_info, avlnode, info);
	if (!hinfo) return -ELOGIC;

	// recheck client name 
	if (strcmp(cliname, hinfo->_pkg_name.c_str()))
		return -ELOGIC;
	
	// set container is disconenct
	hinfo->set_running(false);
	evlclient tmp;
	hinfo->set_evlclient(tmp);
	
	// set service stop
	listnode_t *nd = hinfo->_service_list.next;
	for(; nd != &hinfo->_service_list; nd = nd->next) {
		auto* svc = LIST_ENTRY(host_service_info, ownerlist, nd);
		assert(NULL != svc);
		assert(NULL != svc->service);
		svc->service->set_running(false);
	}

	return 0;
}

int host_info::release_all_host(void)
{
	auto_mutex am(host_info_mut);
	host_info *nd;
	while (!listnode_isempty(host_info_list)) {
		nd = LIST_ENTRY(host_info, ownerlist, host_info_list.next);
		listnode_del(nd->ownerlist);
		delete nd;
	}
	host_info_tree = NULL;
	return 0;
}

int host_info::addref()
{
	return __sync_add_and_fetch(&_refcnt, 1);
}

int host_info::release()
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt < 1) delete this;
	return cnt;
}

int host_info::set_evlclient(evlclient hostcli)
{
	if (!hostcli.get()) return -EBADPARM;
	if (strcmp(_container_inst.c_str(), hostcli->get_instname()))
		return -EBADPARM;
	_evlclient = hostcli;
	return 0;
}

void zrpc_server_evloop_listener::accepted(evlclient client)
{
}

void zrpc_server_evloop_listener::connected(evlclient client)
{
}

void zrpc_server_evloop_listener::disconnected(const char* cliname,
	const char* instname)
{
	host_info::disconnect_host(cliname, instname);
}

server_service::server_service(const char* pkgname,
	const char* servicename,
	const char* instname,
	const char* executive,
	const char* ipaddr, uint32_t port,
	uint32_t version, uint32_t attributes)
: service_info(pkgname, servicename, instname, executive,
	ipaddr, port, version, attributes)
, _host_info(NULL)
{
	listnode_init(_service_lnr_list);
}

server_service::~server_service()
{
	detach_host();
}

int server_service::add_instance(const char* instname)
{
	auto_mutex am(_mut);
	if (!instname || !*instname) return -EBADPARM;

	if (!is_default_service())
		return -ENOTAVAIL;

	if (!check_multi_instance())
		return -ENOTALLOWED;

	if (find_inst_unlock(instname)) return -EEXISTS;
	server_service* inst = new server_service(_pkg_name.c_str(),
		_service_name.c_str(), instname, _executive.c_str(),
		_ipaddr.c_str(), _port,
		_version, _flags);

	if (add_instance_unlock(inst)) {
		delete inst;
		return -ELOGIC;
	}
	return 0;	
}

int server_service::remove_instance(const char* instname)
{
	auto_mutex am(_mut);
	if (!instname || !*instname) return -EBADPARM;
	return remove_instance_unlock(instname);
}

server_service* server_service::find_instance(const char* instname)
{
	auto_mutex am(_mut);
	if (!instname || !*instname) return NULL;
	service_info* svcbase = find_inst_unlock(instname);
	return zas_downcast(service_info, server_service, svcbase);
}

int server_service::set_running(bool brunning)
{
	if (brunning && _service_id) return -EEXISTS;
	if (brunning) {
		_f.starting = 1;
		_f.running = 1;
		_service_id = alloc_service_id();
		service_start_lnr* lnr = NULL;
		while (!listnode_isempty(_service_lnr_list))
		{
			lnr = LIST_ENTRY(service_start_lnr,	\
				ownerlist, _service_lnr_list.next);
			send_service_start_reply(lnr->lnr_client, lnr->seqid);
			listnode_del(lnr->ownerlist);
			delete lnr;
		}
	}
	else {
		_f.starting = 0;
		_f.running = 0;
		_service_id = 0;
	}
	return 0;
}

uint32_t server_service::alloc_service_id()
{
	zrpc_server_service_cnt++;
	return zrpc_server_service_cnt;
}

int server_service::launch_service(bool startcb)
{
	if (_host_info->is_running()) {
		if (startcb) {
			return request_container(zrpc_svc_start_request);
		} else {
			return request_container(zrpc_svc_req_start);
		}
	} else if (_host_info->is_launched()) {
		_host_info->add_wait_container_start(this, startcb);
		return 0;
	}

	std::string cmd = ZRPC_SERVER_LAUNCH_CMD_PREFIX;
	// 32 is default
	if ((0 == (_f.is_default?_flags - 32:_flags) 
		&& 0 == _version && _executive.empty())) {
		return -EBADPARM;
	} else {
		cmd += _pkg_name;
		cmd += "?hostid=" + _host_info->_container_inst;
		cmd += "&service=" + _service_name;
		if (!is_default_service() && _inst_name.length() > 0)
			cmd += "&instance=" + _inst_name;
		cmd += "&executive=" + _executive;
		if (_ipaddr.length() > 0) {
			cmd += "&ipaddr=" + _ipaddr;
		}
		if (_port > 0) {
			cmd += "&listenport="+ std::to_string(_port);
		}
		if (startcb)
			cmd += "&mode=startrequest";
	}

	size_t sz = _pkg_name.length() + 1
		+ _host_info->_container_inst.length() + 1
		+ cmd.length() + 1;

	launch_pkg_info* pkginfo = new(alloca(sizeof(*pkginfo) + sz))
		launch_pkg_info;
	pkginfo->bufsz = 0;
	pkginfo->pkg_name = pkginfo->bufsz;
	strcpy(pkginfo->buf + pkginfo->pkg_name, _pkg_name.c_str());
	pkginfo->bufsz += _pkg_name.length() + 1;

	pkginfo->inst_name = pkginfo->bufsz;
	strcpy(pkginfo->buf + pkginfo->inst_name,
		_host_info->_container_inst.c_str());
	pkginfo->bufsz += _host_info->_container_inst.length() + 1;

	pkginfo->pkg_info = pkginfo->bufsz;
	strcpy(pkginfo->buf + pkginfo->pkg_info, cmd.c_str());
	pkginfo->bufsz += cmd.length() + 1;

	datapool_element dpele = datapool::inst()->
		get_element(ZAS_SYSSVR_LAUNCH_ELE);

	if (!dpele.get()) return -ENOTAVAIL;
	_host_info->set_starting(true);
	return dpele->notify(pkginfo, sizeof(*pkginfo) + sz);
}

int server_service::add_service_start_listener(evlclient sender, uint32_t seqid)
{
	auto_mutex am(_mut);

	if (!sender->get_clientname()) return -EBADPARM;

	listnode_t* nd = _service_lnr_list.next;
	service_start_lnr* lnr = NULL;
	for (; nd != &_service_lnr_list; ) {
		lnr = LIST_ENTRY(service_start_lnr, ownerlist, nd);
		if (!lnr) return -ELOGIC;
		if (!lnr->lnr_client->get_clientname()) {
			nd = nd->next;
			listnode_del(lnr->ownerlist);
			delete lnr;
			continue;
		}

		if (!strcmp(lnr->lnr_client->get_clientname(), 
			sender->get_clientname())
			&& !strcmp(lnr->lnr_client->get_instname(), 
				sender->get_instname())) {
			lnr->seqid = seqid;
			lnr->lnr_client = sender;
			return 0;
		}
		nd = nd->next;
	}

	lnr = new service_start_lnr();
	lnr->seqid = seqid;
	lnr->lnr_client = sender;
	listnode_add(_service_lnr_list, lnr->ownerlist);
	return 0;
}

int server_service::send_service_start_reply(evlclient sender, uint32_t seqid)
{
	int ret = 0;
	size_t sz = 0;
	if (!is_running()) return -ENOTAVAIL;
	if (!_host_info) return -ENOTAVAIL;

	sz = get_string_size();
	sz += _host_info->_pkg_name.length() + 1;
	sz += _host_info->_container_inst.length() + 1;


	server_reply_pkg* svrrly = new(alloca(sizeof(*svrrly) + sz))
		server_reply_pkg(sz);
	server_reply& rinfo = svrrly->payload();
	svrrly->header().seqid = seqid;
	load_info(&(rinfo.service_info));

	if (_host_info->_pkg_name.length() > 0){
		rinfo.service_info.client_name = rinfo.service_info.bufsize;
		strcpy(rinfo.service_info.buf + rinfo.service_info.client_name,
			_host_info->_pkg_name.c_str());
		rinfo.service_info.validity.m.cli_name = 1;
		rinfo.service_info.bufsize += _host_info->_pkg_name.length() + 1;
	}
	if (_host_info->_container_inst.length() > 0){
		rinfo.service_info.cli_inst_name = rinfo.service_info.bufsize;
		strcpy(rinfo.service_info.buf + rinfo.service_info.cli_inst_name,
			_host_info->_container_inst.c_str());
		rinfo.service_info.validity.m.cli_inst_name = 1;
		rinfo.service_info.bufsize += 
			_host_info->_container_inst.length() + 1;
	}

	rinfo.result = ret;
	rinfo.reply = reply_type_client;

	sender->write((void*)svrrly, sizeof(*svrrly) + sz);
	return 0;
}

// start service to host(container)
// start service from container directly
int server_service::request_container(service_request_type reqtype)
{
	size_t sz = get_string_size();

	request_service_pkg* svrreq = new(alloca(sizeof(*svrreq) + sz))
		request_service_pkg(sz);
	request_service& rinfo = svrreq->payload();
	load_info(&(rinfo.service_info));
	rinfo.request = reqtype;

	//get launch cli
	evlclient svccli = _host_info->get_evlclient();

	evpoller poller;
	auto* ev = poller.create_event(evp_evid_package_with_seqid,
		REPLY_START_SERVICE_CTRL, svrreq->header().seqid);
	assert(NULL != ev);
	server_service* self = this;
	ev->write_inputbuf(&self, sizeof(void*));
	// submit the event
	ev->submit();
	
	size_t sendsz = svccli->write((void*)svrreq, sizeof(*svrreq) + sz);	
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

int server_service::attach_host(void)
{
	auto_mutex am(_mut);
	// generate host name
	std::string container;
	if (0 == (_f.is_default?_flags - 32:_flags) 
		 && 0 == _version && _executive.empty()) {
		container = "_";
		container += ZAS_SYSTEM_PROXY_PKG;
		container += "_";
		container += ZAS_SYSTEM_PROXY_SERVICE;
	} else if (_f.shared)
		container = SHARED_CONTAINER_NAME_PREFIX + _pkg_name;
	else {
		container = "_" + _pkg_name + "_" + _service_name;
		if (!is_default_service())
			container += "_" + _inst_name;
	}

	host_info* info = host_info::get_host_info(container.c_str());
	if (!info) {
		if (0 == (_f.is_default?_flags - 32:_flags) 
			 && 0 == _version && _executive.empty()) {
			info = new host_info(ZAS_SYSTEM_PROXY_PKG, 
				container.c_str(), 0);
		} else {
			info = new host_info(_pkg_name.c_str(), 
				container.c_str(), _f.shared);
		}
		assert (NULL != info);
		if (info->addto_host_list()) {
			delete info;
			return -ELOGIC;
		}
	}
	info->attach_service(this);
	_host_info = info;
	return 0;
}

int server_service::detach_host(void)
{
	auto_mutex am(_mut);
	if (!_host_info) return -ENOTEXISTS;
	_host_info->detach_service(this);
	_host_info = NULL;
	return 0;
}

rpcserver_impl::rpcserver_impl()
{
	evloop::inst()->add_listener(ZRPC_SERVER_EVLOOP_LNR_NAME, &_evl_lnr);
}

rpcserver_impl::~rpcserver_impl()
{
	evloop::inst()->remove_listener(ZRPC_SERVER_EVLOOP_LNR_NAME);
	release_all_nodes();
}

int rpcserver_impl::addref()
{
	return 1;
}

int rpcserver_impl::release()
{
	return 1;
}

int rpcserver_impl::run(void)
{
	return 0;
}

int rpcserver_impl::load_config_file(uri& file)
{
	if (!fileexists(file.get_fullpath().c_str()))
		return -ENOTEXISTS;
	
	if (!listnode_isempty(_service_info_list))
		return -EEXISTS;

	return load_rpc_service_config(file, this);
}

server_service* rpcserver_impl::add_service(const char* pkgname,
	const char* servicename,
	const char* inst_name,
	const char* executive,
	const char* ipaddr, uint32_t port,
	uint32_t version, 
	uint32_t attributes)
{
	auto_mutex am(_mut);
	// check service exist
	server_service* impl = NULL;
	auto *svcimp = find_service_unlock(pkgname, servicename);
	if (svcimp && (!inst_name || !*inst_name))
		return NULL;
	
	if (!svcimp) {
		impl = new server_service(pkgname, servicename, NULL, executive,
			ipaddr, port, version, attributes);
		if (impl->attach_host()) {
			delete impl; return NULL;
		}
		if (add_service_unlock(impl)) {
			impl->detach_host();
			delete impl; return NULL;
		}
	} else {
		impl = zas_downcast(service_info, server_service, svcimp);
	}

	if (inst_name && *inst_name) {
		svcimp = impl->find_inst_unlock(inst_name);
		if (svcimp) return NULL;
		server_service* inst = new server_service(pkgname, servicename,
			inst_name, executive, ipaddr, port, version, attributes);

		if (impl->add_instance_unlock(inst)) {
			delete inst; return NULL;
		}
		impl = inst;
	}
	return impl;	
}

int rpcserver_impl::remove_service(const char* pkgname,
	const char* servicename)
{
	auto_mutex am(_mut);

	remove_service_unlock(pkgname, servicename);
	return 0;
}
server_service* rpcserver_impl::get_service(const char* pkgname,
	const char* servicename, const char* inst_name)
{
	auto_mutex am(_mut);
	auto* info = find_service_unlock(pkgname, servicename);
	if (!info) return NULL;
	if (inst_name && *inst_name) {
		info = info->find_inst_unlock(inst_name);
	}
	return zas_downcast(service_info, server_service, info);
}

int rpcserver_impl::release_all_nodes()
{
	auto_mutex am(_mut);
	release_all_service_unlock();
	return 0;
}

int rpcserver_impl::handle_evl_package(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	request_server* request = (request_server*)
		alloca(pkghdr.size);
	assert(NULL != request);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(NULL != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)request, pkghdr.size);
	assert(sz == pkghdr.size);

	if (reqtype_service_start == request->request
		|| reqtype_service_stop == request->request) {
		return handle_host_package(sender, pkghdr, request);
	} else if (reqtype_client == request->request) {
		return handle_client_package(sender, pkghdr, request);
	} 
	return -ENOTFOUND;
}

int rpcserver_impl::start_service_internal(const char* pkgname,
	const char* svc_name, const char* inst_name, uint32_t action)
{
	server_service *svrsvc = get_service(pkgname,
	svc_name, inst_name);

	if (!svrsvc)
		return -ENOTEXISTS;

	int ret = 0;
	bool bproxy = false;
	if (!strcmp(pkgname, ZAS_SYSTEM_PROXY_PKG) 
	 	&& !strcmp(svc_name, ZAS_SYSTEM_PROXY_SERVICE))
		 bproxy = true;

	if (cmd_reqtype_start_service == action) {
		if (!svrsvc->is_running())
			ret = svrsvc->launch_service(true);
		else
			ret = svrsvc->request_container(zrpc_svc_start_request);
	}
	else if (cmd_reqtype_terminate_service == action) {
		if (svrsvc->is_running()) {
			ret = svrsvc->request_container(zrpc_svc_terminate_request);
			return ret;
		}
	}
	else
		ret = -EBADPARM;
	return ret;
}

int rpcserver_impl::handle_cmd_request_package(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	cmd_request_server* request = (cmd_request_server*)
		alloca(pkghdr.size);
	assert(NULL != request);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(NULL != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)request, pkghdr.size);
	assert(sz == pkghdr.size);
	if (cmd_reqtype_start_service != request->request
		&& cmd_reqtype_terminate_service != request->request) {
		return -ENOTFOUND;
	}

	assert(NULL != request);
	service_info tmp;
	cmd_service_info_validity_u val;
	tmp.update_info(request->service_info, val);
	
	// find service
	const char* instname = nullptr;
	if (tmp._inst_name.length() > 0)
		instname = tmp._inst_name.c_str();
	server_service *svrsvc = get_service(tmp._pkg_name.c_str(),
		tmp._service_name.c_str(), instname);

	if (!svrsvc)
		return reply_cmd_request(sender, -ENOTEXISTS, pkghdr.seqid);

	int ret = 0;
	if (cmd_reqtype_start_service == request->request) {
		if (!svrsvc->is_running())
			ret = svrsvc->launch_service(true);
		else
			ret = svrsvc->request_container(zrpc_svc_start_request);
	}
	else if (cmd_reqtype_terminate_service == request->request) {
		if (svrsvc->is_running()) {
			reply_cmd_request(sender, 0, pkghdr.seqid);
			ret = svrsvc->request_container(zrpc_svc_terminate_request);
			return ret;
		}
	}
	else
		ret = -EBADPARM;
	return reply_cmd_request(sender, ret, pkghdr.seqid);
}

// receive reply from host(containder) about direct start service result
int rpcserver_impl::handle_service_package(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	int status;

	auto* ev = queue.dequeue();
	assert(NULL == queue.dequeue());
	server_service* peer = NULL;
	ev->read_inputbuf(&peer, sizeof(void*));
	assert(NULL != peer);
	service_reply* reply = (service_reply*)
		alloca(pkghdr.size);
	assert(NULL != reply);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(NULL != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)reply, pkghdr.size);
	assert(sz == pkghdr.size);

	status = reply->result;
	service_pkginfo_validity_u val;
	if (peer->check_info(reply->service_info, val))
		status = -ELOGIC;
	// update info of event
	ev->write_outputbuf(&status, sizeof(status));
	return 0;
}

// service starting from service.
// if service not in config file, create new service.
int rpcserver_impl::handle_host_package(evlclient sender,
	const package_header &pkghdr,
	request_server* request)
{
	assert(NULL != request);
	service_info tmp;
	service_pkginfo_validity_u val;
	tmp.update_info(request->service_info, val);
	
	// find service
	// first find service root node. root node is deafult service
	// then find service by instance
	const char* instname = NULL;
	if (tmp._inst_name.length() > 0)
		instname = tmp._inst_name.c_str();

	server_service *svrsvc = get_service(tmp._pkg_name.c_str(),
		tmp._service_name.c_str(), instname);

	// if don't find service, create new service.
	// service is create by service config file.
	// if service not in configfile, add new service (TBD.)
	if (!svrsvc && reqtype_service_start == request->request) {
		svrsvc = add_service(tmp._pkg_name.c_str(),
			tmp._service_name.c_str(), instname, tmp._executive.c_str(),
			tmp._ipaddr.c_str(), tmp._port,
			tmp._version, tmp._flags);
	}

	// update service status
	// set service status is runing.
	if (svrsvc) {
		if (reqtype_service_start == request->request) {
			host_info* hinfo = svrsvc->get_host();
			if (!hinfo->is_running()) {
				// set service host(container) is running
				hinfo->set_evlclient(sender);
				hinfo->set_running(true);
			}
			// set service is running
			svrsvc->set_running(true);
		} else if (reqtype_service_stop == request->request) {
			svrsvc->set_running(false);
		}
	}

	// reply service id to serivce.
	size_t sz = 0;
	if (svrsvc)
		sz = svrsvc->get_string_size();

	server_reply_pkg* svrrly = new(alloca(sizeof(*svrrly) + sz))
		server_reply_pkg(sz);
	server_reply& rinfo = svrrly->payload();
	svrrly->header().seqid = pkghdr.seqid;
	if (svrsvc)
		svrsvc->load_info(&(rinfo.service_info));
	rinfo.result = svrsvc ? 0 : -1;
	rinfo.reply = reply_type_service;
	sender->write((void*)svrrly, sizeof(*svrrly) + sz);
	return 0;
}

// request service start from client.
// if service not start, register listener. 
// when service is runing, send to client listener
int rpcserver_impl::handle_client_package(evlclient sender,
	const package_header &pkghdr,
	request_server* request)
{
	assert(NULL != request);
	service_info tmp;
	service_pkginfo_validity_u val;
	tmp.update_info(request->service_info, val);
	
	// find service
	const char* instname = nullptr;
	if (tmp._inst_name.length() > 0) {
		instname = tmp._inst_name.c_str();
	}
	server_service *svrsvc = get_service(tmp._pkg_name.c_str(),
		tmp._service_name.c_str(), instname);

	if(!svrsvc && instname) {
		svrsvc = get_service(tmp._pkg_name.c_str(),
		tmp._service_name.c_str(), nullptr);
	}

	if (!svrsvc) {
		svrsvc = add_service(tmp._pkg_name.c_str(),
			tmp._service_name.c_str(), instname, nullptr,
			tmp._ipaddr.c_str(), tmp._port, 0, 0);
	}

	int ret = 0;
	size_t sz = 0;
	if (svrsvc) {
		// if service isnot running, add listener and start service
		if (!svrsvc->is_running()) {
			svrsvc->add_service_start_listener(sender, pkghdr.seqid);
			svrsvc->launch_service();
		} else {
			//if service is ruing ,send servcie id to client
			return svrsvc->send_service_start_reply(sender, pkghdr.seqid);
		}
	} else {
		// start service not exist, reply client error
		server_reply_pkg* svrrly = new(alloca(sizeof(*svrrly)))
			server_reply_pkg(sz);
		server_reply& rinfo = svrrly->payload();
		svrrly->header().seqid = pkghdr.seqid;
		rinfo.result = -ENOTEXISTS;;
		rinfo.reply = reply_type_client;
		sender->write((void*)svrrly, sizeof(*svrrly));
	}

	return 0;
}

int rpcserver_impl::reply_cmd_request(evlclient sender, 
	int result, uint32_t seqid)
{
	cmd_server_reply_pkg* svrrly = new(alloca(sizeof(*svrrly)))
		cmd_server_reply_pkg(0);
	cmd_server_reply& rinfo = svrrly->payload();
	svrrly->header().seqid = seqid;
	rinfo.result = result;
	rinfo.reply = reply_type_other;
	sender->write((void*)svrrly, sizeof(*svrrly));
	return 0;
}


}}} // end of namespace zas::mware::rpc

/* EOF */
