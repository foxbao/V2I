#include "inc/rpcclient.h"

#include <unistd.h>
#include "std/list.h"
#include "utils/evloop.h"
#include "utils/buffer.h"
#include "utils/avltree.h"
#include "utils/mutex.h"
#include "mware/pkgconfig.h"
#include "inc/rpc_pkg_def.h"
#include "inc/rpcmgr-impl.h"
#include "inc/rpchost.h"

namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;


rpcclient_impl::rpcclient_impl(const char* pkgname, 
	const char* service_name, const char* inst_name)
: _host_impl(NULL), _service_id(0), _flags(0), _refcnt(1)
{
	_pkg_name = pkgname;
	_service_name = service_name;
	_inst_name.clear();

	if (inst_name && *inst_name) _inst_name = inst_name;

	_client_name.clear();
	_cliinst_name.clear();
}

rpcclient_impl::~rpcclient_impl()
{
	_host_impl = NULL;
}


int rpcclient_impl::addref()
{
	return __sync_add_and_fetch(&_refcnt, 1);
}

int rpcclient_impl::release()
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0) {
		delete this;
	}
	return cnt;
}

int rpcclient_impl::set_service_client(const char* client_name,
	const char* inst_name, uint32_t service_id)
{
	if(!client_name || !*client_name || !inst_name || !*inst_name)
		return -EBADPARM;
	_client = evloop::inst()->getclient(client_name, inst_name);
	if (NULL == _client.get())
		return -ENOTAVAIL;
	_client_name = client_name;
	_cliinst_name = inst_name;
	_service_id = service_id;
	return 0;
}

int rpcclient_impl::get_service_from_server(void)
{
	if (!check_local_service()) return 0;

	size_t sz = get_string_size();

	request_server_pkg* svrreq = new(alloca(sizeof(*svrreq) + sz))
		request_server_pkg(sz);
	request_server& rinfo = svrreq->payload();
	load_info(&(rinfo.service_info));
	rinfo.request = reqtype_client;
	//get launch cli
	evlclient svrcli = evloop::inst()->getclient(NULL, NULL);

	evpoller poller;
	auto* ev = poller.create_event(evp_evid_package_with_seqid,
		SERVER_REPLY_CTRL, svrreq->header().seqid);
	assert(NULL != ev);
	rpcclient_impl* self = this;
	ev->write_inputbuf(&self, sizeof(void*));
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
	printf("rpc client get service frome server result: %d, service %d\n",
		status, _service_id);
	return status;
}

int rpcclient_impl::request_service_stop()
{
	client_request_service_pkg* svrreq = new(alloca(sizeof(*svrreq)))
		client_request_service_pkg(0);
	client_request_service& rinfo = svrreq->payload();
	rinfo.request = cli_req_svc_stop;

	if (!_client.get()) return -ENOTAVAIL;

	evpoller poller;
	auto* ev = poller.create_event(evp_evid_package_with_seqid,
		SERVICE_REPLY_CLIENT_CTRL, svrreq->header().seqid);
	assert(NULL != ev);
	rpcclient_impl* self = this;
	ev->write_inputbuf(&self, sizeof(void*));
	// submit the event
	ev->submit();
	
	size_t sendsz = _client->write((void*)svrreq, sizeof(*svrreq));	
	if (sendsz < sizeof(*svrreq)) {
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

int rpcclient_impl::check_local_service(void)
{
	const pkgconfig& p = pkgconfig::getdefault();
	const char* pkgname =  p.get_package_name();
	if (!pkgname) return -ENOTAVAIL;
	if (strcmp(_pkg_name.c_str(), pkgname))
		return -ENOTEXISTS;
	
	rpchost_impl* hostimpl = rpcmgr_impl::instance()->
		get_service(_service_name.c_str(), _inst_name.c_str());
	
	if (!hostimpl) return -ENOTFOUND;
	_host_impl = hostimpl;
	_f.local_service = 1;
	return 0;
}

int rpcclient_impl::handle_local_invoke(void** instid, std::string& clsname,
	uint128_t& method_uuid, google::protobuf::Message *inparam,
	google::protobuf::Message *inout_param, bool renew_inst)
{
	// TODO temp logic
	if (!_host_impl) return -ENOTAVAIL;
	return _host_impl->handle_local_invoke(instid, clsname,
		method_uuid, inparam, inout_param, renew_inst);
}

int rpcclient_impl::handle_get_instance(void** instid,
	std::string& clsname, void* data, size_t sz, bool singleton)
{
	if (!_host_impl) return -ENOTAVAIL;
	evlclient tmp;
	return _host_impl->handle_get_instance(tmp, instid, clsname,
		data, sz, singleton);
}

uint32_t rpcclient_impl::get_string_size()
{
	uint32_t sz = _pkg_name.length() + 1
		+ _service_name.length() + 1;
	if (_inst_name.length() > 0)
		sz += _inst_name.length() + 1;
	return sz;
}

int rpcclient_impl::load_info(service_pkginfo* info)
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

int rpcclient_impl::check_info(service_pkginfo& info)
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

rpcclient_mgr::rpcclient_mgr()
: _client_tree(NULL)
{
	listnode_init(_client_list);
}

rpcclient_impl* rpcclient_mgr::add_client(const char* pkgname,
	const char* service_name, const char* inst_name)
{
	auto_mutex am(_mut);

	auto cli_impl = find_client_unlock(pkgname, service_name, inst_name);
	if (cli_impl) return NULL;

	cli_impl = new rpcclient_impl(pkgname, service_name, inst_name);

	if (avl_insert(&_client_tree, &cli_impl->avlnode,
		zrpc_client_compared)) {
		delete cli_impl;
		return NULL;
	}
	listnode_add(_client_list, cli_impl->ownerlist);
	return cli_impl;
}

int rpcclient_mgr::remove_client(const char* pkgname,
	const char* service_name, const char* inst_name)
{
	auto_mutex am(_mut);

	auto cli_impl = find_client_unlock(pkgname, service_name, inst_name);
	if (!cli_impl) return -ENOTEXISTS;

	avl_remove(&_client_tree, &cli_impl->avlnode);
	listnode_del(cli_impl->ownerlist);
	delete cli_impl;
	return 0;
}

rpcclient_impl* rpcclient_mgr::get_client(const char* pkgname,
	const char* service_name, const char* inst_name)
{
	auto_mutex am(_mut);
	return find_client_unlock(pkgname, service_name, inst_name);
}

rpcclient_impl* rpcclient_mgr::find_client_unlock(const char* pkgname,
	const char* service_name, const char* inst_name)
{
	rpcclient_impl cli_tmp(pkgname, service_name, inst_name);

	avl_node_t *cli_nd = avl_find(_client_tree,
		&cli_tmp.avlnode, zrpc_client_compared);
	if (!cli_nd) return NULL;

	return AVLNODE_ENTRY(rpcclient_impl, avlnode, cli_nd);
}

int rpcclient_mgr::zrpc_client_compared(avl_node_t* aa, avl_node_t* bb)
{
	auto* nda = AVLNODE_ENTRY(rpcclient_impl, avlnode, aa);
	auto* ndb = AVLNODE_ENTRY(rpcclient_impl, avlnode, bb);
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

int rpcclient_mgr::release_all_nodes(void)
{
	auto_mutex am(_mut);
	while (!listnode_isempty(_client_list)) {
		auto *cli_impl = LIST_ENTRY(rpcclient_impl,	\
			ownerlist, _client_list.next);
		listnode_del(cli_impl->ownerlist);
		delete cli_impl;
	}
	_client_tree = NULL;
	return 0;
}

int rpcclient_mgr::handle_server_reply_package(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue,
	server_reply* reply)
{
	int status;
	// get the package payload
	auto* ev = queue.dequeue();
	assert(NULL == queue.dequeue());
	rpcclient_impl* peer = NULL;
	ev->read_inputbuf(&peer, sizeof(void*));
	assert(NULL != peer);
	service_pkginfo_validity_u val;
	status = reply->result;
	if (status || reply_type_other == reply->reply) {
		ev->write_outputbuf(&status, sizeof(status));
		return 0;		
	}

	if (peer->check_info(reply->service_info))
		status = -ELOGIC;

	if (reply->service_info.validity.m.cli_name
		&& reply->service_info.validity.m.cli_inst_name
		&& reply->service_info.validity.m.id) {
		peer->set_service_client(
			reply->service_info.buf + reply->service_info.client_name,
			reply->service_info.buf + reply->service_info.cli_inst_name,
			reply->service_info.id);
		if (reply->service_info.id) {
			peer->set_ready();
		}
	}

	// update info of event
	ev->write_outputbuf(&status, sizeof(status));
	return 0;
}

int rpcclient_mgr::handle_service_reply_package(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	service_reply_client* reply = (service_reply_client*)
		alloca(pkghdr.size);
	assert(NULL != reply);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(NULL != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)reply, pkghdr.size);
	assert(sz == pkghdr.size);
	int status;
	// get the package payload
	auto* ev = queue.dequeue();
	assert(NULL == queue.dequeue());
	rpcclient_impl* peer = NULL;
	ev->read_inputbuf(&peer, sizeof(void*));
	assert(NULL != peer);
	status = reply->result;
	ev->write_outputbuf(&status, sizeof(status));
	return 0;
}


}}}