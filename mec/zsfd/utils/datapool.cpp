/** @file datapool.cpp
 * implementation of the global and local datapool
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_DATAPOOL

#include "inc/datapool-impl.h"
#include "inc/dpmsg.h"
#include "utils/datapool.h"
#include "utils/evloop.h"
#include "utils/buffer.h"
#include "utils/timer.h"

namespace zas {
namespace utils {

using namespace std;

//locker used by create datapool instance
static mutex dpmut; 

///---------------------------
//datapool_element_impl start


datapool_element_impl::datapool_element_impl(
	datapool_impl* datapool, 
	const char* name, 
	bool is_global, 
	bool need_persistent)
: _flags(0)
, _datapool(nullptr)
, _refcnt(1)
, _element_name(name)
{
	_datapool = datapool;
	_f.is_global = is_global ? 1 : 0;
	_f.need_persistent = need_persistent ? 1 : 0;
}

datapool_element_impl::~datapool_element_impl()
{
}

int datapool_element_impl::addref(void)
{
	return __sync_add_and_fetch(&_refcnt, 1);
}
int datapool_element_impl::release(void)
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0) {
		delete this;
	}
	return cnt;
}

int datapool_element_impl::setdata(const char* str)
{
	if (!_datapool) return -EINVALID;
	if (!str | !(*str)) return -EBADPARM;
	return _datapool->setdata(_element_name.c_str(),
		is_global(), need_persistent(), (void*)str, strlen(str));
}

int datapool_element_impl::setdata(const string &str)
{
	if (!_datapool) return -EINVALID;
	if (str.empty()) return -EBADPARM;
	return _datapool->setdata(_element_name.c_str(),
		is_global(), need_persistent(),
		(void*)str.c_str(), str.length());
}

int datapool_element_impl::setdata(void* buffer, size_t sz)
{
	if (!_datapool) return -EINVALID;
	if (nullptr == buffer || 0 == sz) return -EBADPARM;
	return _datapool->setdata(_element_name.c_str(),
		is_global(), need_persistent(), buffer, sz);
}

int datapool_element_impl::getdata(std::string &data)
{
	if (!_datapool) return -EINVALID;
	return _datapool->getdata(_element_name.c_str(),
		is_global(), need_persistent(), data);
}

int datapool_element_impl::notify(const char* str)
{
	if (!_datapool) return -EINVALID;
	size_t len = 0;
	if (str) len = strlen(str);
	return _datapool->notify(_element_name.c_str(),
		is_global(), need_persistent(), (void*)str, len);
}

int datapool_element_impl::notify(const string &str)
{
	if (!_datapool) return -EINVALID;
	if (str.empty()) return -EBADPARM;
	return _datapool->notify(_element_name.c_str(),
		is_global(), need_persistent(),
		(void*)str.c_str(), str.length());
}

int datapool_element_impl::notify(void* buffer, size_t sz)
{
	if (!_datapool) return -EINVALID;
	return _datapool->notify(_element_name.c_str(),
		is_global(), need_persistent(), buffer, sz);
}

int datapool_element_impl::add_listener(notifier notify, void* owner)
{
	if (!_datapool) return -EINVALID;
	if (nullptr == notify) return -EBADPARM;
	return _datapool->add_listener(_element_name.c_str(),
		is_global(), need_persistent(), notify, owner);
}

static void datapool_notify_listener(void* owner, void* data, size_t sz)
{
	if (nullptr == owner) return;
    auto* ptr = reinterpret_cast<datapool_listener*>(owner);
    ptr->on_notify(data, sz);
}

int datapool_element_impl::add_listener(datapool_listener* lnr)
{
	if (!_datapool) return -EINVALID;
	//datapool_listener change to noifier by datapool_notify_listener
	//notifier is datapool_notify_listener, owner is lnr 
	if (nullptr == lnr) return -EBADPARM;
	return _datapool->add_listener(_element_name.c_str(),
		is_global(), need_persistent(), datapool_notify_listener, lnr);
}

int datapool_element_impl::remove_listener(notifier notify, void* owner)
{
	if (!_datapool) return -EINVALID;
	if (nullptr == notify) return -EBADPARM;
	return _datapool->remove_listener(_element_name.c_str(),
		is_global(), need_persistent(), notify, owner);
}

int datapool_element_impl::remove_listener(datapool_listener* lnr)
{
	if (!_datapool) return -EINVALID;
	return _datapool->remove_listener(_element_name.c_str(),
		is_global(), need_persistent(), datapool_notify_listener, lnr);
}

//datapool_element_impl end
///---------------------------

element_info::element_info()
: is_global(false), need_persistent(false)
, notify(nullptr), bufsz(0)
, name(0), databuf(0)
{
	validity.all = 0;
}

///---------------------------
//datapool_impl start
datapool_impl::datapool_impl()
: _flags(0)
, _globalpond(nullptr)
, _pond_manager(nullptr)
{
	init_datapool();
}

datapool_impl::~datapool_impl()
{
	if (issvr()) {
		evloop::inst()->remove_package_listener(DP_CLIENT_CTRL_REQUEST, &_pkg_lnr);
	} else {
		evloop::inst()->remove_package_listener(DP_SERVER_CTRL_REPLY, &_pkg_lnr);
		evloop::inst()->remove_package_listener(DP_SERVER_CTRL_NOTIFY, &_pkg_lnr);
	}
	if (_globalpond)
		delete _globalpond;
	_globalpond = nullptr;
	if (_pond_manager)
		delete _pond_manager;
	_pond_manager = nullptr;
	_flags = 0;
}

datapool_element 
datapool_impl::create_element(const char* name,
	bool is_global,
	bool need_persistent)
{
	datapool_element element;
	// check datapool initilized
	if (!is_init()) return element;

	if (!name || !*name) return element;

	element_base_info elebase(name, is_global, need_persistent);
	datapool_element_impl *elementnode = nullptr;

	if (issvr()) {
		//element exist, return nullptr element;
		pond_element *ele = find_srv_element(name, is_global);
		if (ele) return element;

		// check server.
		// getclient(nullptr, nullptr) return server client;
		evlclient client = evloop::inst()->getclient(nullptr, nullptr);
		if (!client.get()) return element;

		// get element pond
		pond* pnode = nullptr;
		if (is_global)
			pnode = _globalpond;
		else if (need_persistent) {
			//get lcoal&persistent pond
			pnode = find_create_pond(client->get_clientname());
			if (nullptr == pnode) return element;
		} else
			pnode = &_localpond;
		assert(nullptr != pnode);

		elementnode = create_pond_element(pnode, &elebase, client->get_clientname());
	} else {
		// is_global is true, first check local has same element
		if (!is_global)
			if (_localpond.find_element_node(name)) return element;

		if (!is_global && !need_persistent) {
			// create local element must check local&persistent on server
			if (check_server_element_exist(&elebase) <= 0) return element;
			elementnode = create_pond_element(&_localpond, &elebase);
		} else {
			elementnode = create_remote_element_unlock(&elebase);
		}
	}
	
	if (elementnode) {
		// change to datapool_element_object
		element.set(reinterpret_cast<datapool_element_object*>(elementnode));
	}
	return element;
}

datapool_element
datapool_impl::get_element(const char* name, bool is_global)
{
	datapool_element element;
	// check datapool initilized
	if (!is_init()) return element;
	if (!name || !*name) return element;

	element_base_info elebase(name, is_global, false);
	datapool_element_impl *node = nullptr;

	if(issvr()) {
		pond_element *ele = find_srv_element(name, is_global);
		if (nullptr == ele) return element;
		node = new datapool_element_impl(this, name, is_global, false);
	} else {
		//is_global false, frist check local pond
		if (!is_global) 
			node = get_pond_element(&_localpond, &elebase);
		// element don't find in local
		// it must be find in global
		if (nullptr == node) {
			elebase.need_persistent = is_global ? true : false;
			node = get_remote_element_unlocked(&elebase);
		}
	}

	if (node) {
		element.set(reinterpret_cast<datapool_element_object*>(node));
	}
	return element;
}

bool datapool_impl::init_datapool(void)
{
	evloop_role evlrole = evloop::inst()->getrole();
	//unknown: evloop error
	if (evloop_role_unknown == evlrole) return false;

	// set initialed
	_f.is_init = 1;

	//check role
	if (evloop_role_server == evlrole)
		_f.is_srv = 1;

	if (issvr()) {
		// create global pond & pond manager
		if (nullptr == _globalpond)
			_globalpond = new pond();
		if (nullptr == _pond_manager)
			_pond_manager = new pond_manager();
		assert(nullptr != _globalpond);
		assert(nullptr != _pond_manager);
	}
	// set package listener handle point
	_pkg_lnr.set_datapool_impl(this);
	if (issvr()) {
		evloop::inst()->add_package_listener(DP_CLIENT_CTRL_REQUEST, &_pkg_lnr);
	} else {
		evloop::inst()->add_package_listener(DP_SERVER_CTRL_REPLY, &_pkg_lnr);
		evloop::inst()->add_package_listener(DP_SERVER_CTRL_NOTIFY, &_pkg_lnr);
	}
	return true;
}

datapool_element_impl* 
datapool_impl::create_remote_element_unlock(element_base_info *info)
{
	if (!info) return nullptr;

	dp_evl_msg_retinfo retinfo;
	retinfo.result = -1;
	retinfo.name = info->name;
	int ret = remote_evl_request(dp_pkg_owner_datapool,
		dp_dpowr_act_create_element, true, info, &retinfo);

	// evl send failure, return nullptr;
	if (ret) return nullptr;

	// remote failure
	if (retinfo.result) return nullptr;

	datapool_element_impl *elementnode = new datapool_element_impl(this, 
		info->name.c_str(),
		info->is_global,
		info->need_persistent);
	return elementnode;
}

datapool_impl::package_listener::package_listener()
: _dp_impl(nullptr)
{
}

int datapool_impl::package_listener::set_datapool_impl(datapool_impl *impl)
{
	_dp_impl = impl;
	return 0;
}

bool datapool_impl::package_listener::on_package(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	if (nullptr == _dp_impl) return false;
	return _dp_impl->on_evl_package(sender, pkghdr, queue);
}

datapool_impl::element_base_info::element_base_info(const char* nm, 
	bool isglobal, bool needpersistent)
{
	name = nm;
	is_global = isglobal;
	need_persistent = needpersistent;
}

bool datapool_impl::on_evl_package(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	//datapool not init
	if (!is_init()) return false;

	//check package id of message
	if (issvr()) {
		//server only handle request from client
		if (DP_CLIENT_CTRL_REQUEST != pkghdr.pkgid)
			return false;
	} else {
		//cllient handle reply pkg or noitfy pkg
		if (DP_SERVER_CTRL_NOTIFY != pkghdr.pkgid
			&& DP_SERVER_CTRL_REPLY != pkghdr.pkgid)
			return false;
	}

	bool ret = false;
	if (DP_CLIENT_CTRL_REQUEST == pkghdr.pkgid) {
		ret =handle_evl_client_request_pkg(sender, pkghdr, queue);
	} else if (DP_SERVER_CTRL_REPLY == pkghdr.pkgid) {
		ret = handle_evl_server_reply_pkg(pkghdr, queue);
	} else if (DP_SERVER_CTRL_NOTIFY == pkghdr.pkgid) {
		ret = handle_evl_server_noitfy_pkg(pkghdr, queue);
	}
	return ret;
}

bool datapool_impl::handle_evl_client_request_pkg(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	datapool_client_request* reqinfo = (datapool_client_request*)
			alloca(pkghdr.size);
	assert(nullptr != reqinfo);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(nullptr != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)reqinfo, pkghdr.size);
	assert(sz == pkghdr.size);

	// name & is_global is mustbe valid
	if (!reqinfo->element.validity.m.name
		|| !reqinfo->element.validity.m.is_global)
		return false;
	
	dp_pkg_owner pkgownr = reqinfo->pkgowr;

	if (dp_pkg_owner_datapool == pkgownr) {
		// handle datapool request
		return handle_evl_client_dp_request(sender, pkghdr.seqid, reqinfo);
	} else {
		// handle element request
		return handle_evl_client_element_request(sender, pkghdr.seqid, reqinfo);
	}
	return false;
}

bool datapool_impl::handle_evl_server_reply_pkg(const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	datapool_server_reply* replyinfo = (datapool_server_reply*)
			alloca(pkghdr.size);
	assert(nullptr != replyinfo);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(nullptr != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)replyinfo, pkghdr.size);
	assert(sz == pkghdr.size);

	auto* ev = queue.dequeue();
	assert(nullptr == queue.dequeue());
	dp_evl_msg_retinfo *invoke_data = NULL;
	ev->read_inputbuf(&invoke_data, sizeof(void*));

	// elementname of reply message must be vallid
	if (!(replyinfo->element.validity.m.name)) {
		invoke_data->result = -EINVALID;
		return false;
	}

	std::string name(replyinfo->element.buf);
	// check elementname of reply message with send elementname
	if (strcmp(name.c_str(), invoke_data->name.c_str())) {
		invoke_data->result = -EINVALID;
		return false;
	}

	if (replyinfo->element.validity.m.databuf) {
		invoke_data->data.clear();
		invoke_data->data.append((replyinfo->element.buf 
			+ replyinfo->element.databuf), replyinfo->element.datasz);
	}
	invoke_data->result = replyinfo->result;
	return true;
}

bool datapool_impl::handle_evl_server_noitfy_pkg(const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	datapool_server_noitfy* notifyinfo = (datapool_server_noitfy*)
			alloca(pkghdr.size);
	assert(nullptr != notifyinfo);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(nullptr != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)notifyinfo, pkghdr.size);
	assert(sz == pkghdr.size);

	element_info* eleinfo = &(notifyinfo->element);

	if (!(eleinfo->validity.m.name)
		|| !(eleinfo->validity.m.notifier))
		return false;

	if (nullptr == eleinfo->notify) return false;
	notifier snotify = (notifier)(eleinfo->notify);
	if (eleinfo->validity.m.databuf) {
		snotify(eleinfo->owner, 
			(eleinfo->buf + eleinfo->databuf), eleinfo->datasz);
	} else {
		snotify(eleinfo->owner, nullptr, 0);
	}
	return true;
}

int datapool_impl::create_evl_pond_element(const char* sendername,
	element_base_info *info)
{
	assert(nullptr != info);
	datapool_element_impl* eleimpl = nullptr;
	if (info->is_global) {
		eleimpl = create_pond_element(_globalpond, info, sendername);
	} else {
		pond* pondnode = find_create_pond(sendername);
		eleimpl = create_pond_element(pondnode, info);
	}
	if (eleimpl) return 0;
	return -1;
}

int datapool_impl::get_evl_pond_element(const char* sendername,
        element_base_info *info)
{
	datapool_element_impl* eleimpl = nullptr;
	if (info->is_global) {
		eleimpl = get_pond_element(_globalpond, info);
	} else {
		pond* pondnode = find_create_pond(sendername);
		if (nullptr == pondnode) return -ENOTFOUND;
		eleimpl = get_pond_element(pondnode, info);
	}
	if (eleimpl) return 0;
	return -ENOTEXISTS;
}

int datapool_impl::remove_evl_pond_element(const char* sendername,
        element_base_info *info)
{
	assert(nullptr != info);
	int ret = 0;
	if (info->is_global) {
		assert(nullptr != _globalpond);
		ret = _globalpond->remove_pond_element(info->name.c_str(), sendername);
	} else {
		pond* pondnode = find_create_pond(sendername);
		if (nullptr == pondnode) return -ENOTFOUND;
		ret = pondnode->remove_pond_element(info->name.c_str(), sendername);
	}
	return ret;
}

bool datapool_impl::handle_evl_client_dp_request(evlclient sender,
	uint32_t seqid, datapool_client_request* reqinfo)
{
	int ret = -1;
	bool need_persistent = false;
	// when need persistenet invalid
	// not global, need persistent must ture
	if (reqinfo->element.validity.m.need_persistent)
		need_persistent = reqinfo->element.need_persistent;
	else
		need_persistent = reqinfo->element.is_global ? false : true;
	element_base_info elebase(reqinfo->element.buf + reqinfo->element.name,
		reqinfo->element.is_global, need_persistent);
	
	std::string sendername = sender->get_clientname();

	datapool_dpowner_action dpowract = reqinfo->dpowract;
	if (dp_dpowr_act_create_element == dpowract) {
		ret = create_evl_pond_element(sendername.c_str(), &elebase);
	} else if (dp_dpowr_act_get_element == dpowract) {
		ret = get_evl_pond_element(sendername.c_str(), &elebase);
	} else if (dp_dpowr_act_remove_element == dpowract) {
		ret = remove_evl_pond_element(sendername.c_str(), &elebase);
	} else if (dp_dpowr_act_check_element == dpowract) {
		ret = ENOTFOUND;
		pond* pondnode = find_create_pond(sendername.c_str());
		if (pondnode){
			if (pondnode->find_element_node(elebase.name.c_str()))
				ret = 0;
		}
	}

	if (!reqinfo->needreply)
		return true;

	size_t pkgsz = elebase.name.length() + 1;
	datapool_server_reply_pkg* replyinfo =
		new(alloca(sizeof(*replyinfo) + pkgsz))
		datapool_server_reply_pkg(pkgsz);
	replyinfo->header().seqid = seqid;
	datapool_server_reply& info = replyinfo->payload();
	info.result = ret;

	// fill element name
	info.element.name = info.element.bufsz;
	strcpy(&(info.element.buf[info.element.name]), elebase.name.c_str());

	info.element.bufsz += elebase.name.length() + 1;
	info.element.validity.m.name = 1;

	size_t writesz = sender->write((void*)replyinfo,
		(sizeof(*replyinfo) + pkgsz));
	return true;
}
bool datapool_impl::handle_evl_client_element_request(evlclient sender,
	uint32_t seqid, datapool_client_request* reqinfo)
{
	pond *pnode= nullptr;
	int ret = -1;
	size_t datasize = 0;
	pond_element* elenode = nullptr;

	std::string name = (reqinfo->element.buf + reqinfo->element.name);
	std::string sendername = sender->get_clientname();
	datapool_eleowner_action action = reqinfo->eleowract;
	bool is_global = reqinfo->element.is_global;

	if (is_global) 
		pnode = _globalpond;
	else
		pnode = find_create_pond(sendername.c_str());
	
	if (nullptr == pnode) goto error;
	elenode = pnode->find_element_node(name.c_str());
	if (nullptr == elenode) goto error;

	if (dp_eleowr_act_getdata == action) {
		datasize = elenode->getdatasize();
		ret = 0;
	} else if (dp_eleowr_act_setdata == action) {
		if (!reqinfo->element.validity.m.databuf)
			goto error;
		ret = elenode->setdata((reqinfo->element.buf + reqinfo->element.databuf),
			reqinfo->element.datasz);
	} else if (dp_eleowr_act_notify == action) {
		if (!reqinfo->element.validity.m.databuf) {
			ret = elenode->notify(nullptr, 0);
		} else {
			ret = elenode->notify((reqinfo->element.buf + 
				reqinfo->element.databuf), reqinfo->element.datasz);
		}
	} else if (dp_eleowr_act_addlistener == action) {
		if (!reqinfo->element.validity.m.notifier)
			goto error;
		ret = elenode->add_listener((notifier)reqinfo->element.notify,
			reqinfo->element.owner, sender);
	} else if (dp_eleowr_act_rmlistener == action) {
		if (!reqinfo->element.validity.m.notifier)
			goto error;
		ret = elenode->remove_listener((notifier)reqinfo->element.notify,
			reqinfo->element.owner);
	}

error: 

	if (!reqinfo->needreply)
		return true;

	size_t pkgsz = name.length() + 1 + datasize;
	// fill request data
	datapool_server_reply_pkg* replyinfo = 
		new(alloca(sizeof(*replyinfo) + pkgsz))
		datapool_server_reply_pkg(pkgsz);
	datapool_server_reply& info = replyinfo->payload();
	replyinfo->header().seqid = seqid;
	info.result = ret;
	info.element.name = info.element.bufsz;
	strcpy(&(info.element.buf[info.element.name]), name.c_str());
	info.element.bufsz += name.length() + 1;
	info.element.validity.m.name = 1;

	// datasize > 0, need reply element data
	if (datasize) {
		info.element.databuf = info.element.bufsz;
		memcpy((info.element.buf + info.element.databuf),
			elenode->getdatabuf(), datasize);
		info.element.bufsz += datasize;
		info.element.datasz = datasize;
		info.element.validity.m.databuf = 1;
	}

	size_t writesz = sender->write((void*)replyinfo,
		(sizeof(*replyinfo) + pkgsz));
	return true;
}

int datapool_impl::remote_evl_request(int operation, int action,
	bool reply, element_base_info *baseinfo, dp_evl_msg_retinfo* retinfo,
	notifier ele_notify, void* owner,
	void* buf, size_t bufsz)
{
	size_t sz =  baseinfo->name.length() + 1 + bufsz;
	// init request data struct
	datapool_client_request_pkg* dpinfo = new(alloca(sizeof(*dpinfo) + sz))
		datapool_client_request_pkg(sz);
	datapool_client_request& info = dpinfo->payload();
	// fill package owner
	info.pkgowr = (dp_pkg_owner)operation;
	info.needreply = reply;
	if (dp_pkg_owner_datapool == operation)
		info.dpowract = (datapool_dpowner_action)action;
	else
		info.eleowract = (datapool_eleowner_action)action;
	// fill element info
	info.element.is_global = baseinfo->is_global;
	info.element.need_persistent = baseinfo->need_persistent;
	info.element.name = info.element.bufsz;
	strcpy(&(info.element.buf[info.element.name]), baseinfo->name.c_str());
	info.element.bufsz += baseinfo->name.length() + 1;
	info.element.validity.m.is_global = 1;
	info.element.validity.m.need_persistent = 1;
	info.element.validity.m.name = 1;

	// buf != nullptr, need send buf data
	if (buf) {
		char* infobuf = info.element.buf + info.element.bufsz;
		memcpy(infobuf, buf, bufsz);
		info.element.databuf = info.element.bufsz;
		info.element.validity.m.databuf = 1;
		info.element.bufsz += bufsz;
		info.element.datasz = bufsz;
	}
	// notify != nullptr, fill notify&owner
	if (nullptr != ele_notify)
	{
		info.element.owner = owner;
		info.element.notify = (void*)ele_notify;
		info.element.validity.m.notifier = 1;
	}

	// get server client object
	evlclient client = evloop::inst()->getclient(nullptr, nullptr);
	if (nullptr == client.get()) return -ENOTHANDLED;
	if (!reply) {
		client->write((void*)dpinfo, sizeof(*dpinfo) + sz);	
		return 0;
	}

	evpoller poller;
	auto* ev = poller.create_event(evp_evid_package_with_seqid,
		DP_SERVER_CTRL_REPLY, dpinfo->header().seqid);
	assert(nullptr != ev);
	ev->write_inputbuf(&retinfo, sizeof(void*));

	//write message
	size_t sendsz = client->write((void*)dpinfo, sizeof(*dpinfo) + sz);	
	if (sendsz < sz) return -EINVALID;
	// submit the event
	ev->submit();

	if (poller.poll(1000)) {
		return -ETIMEOUT;
	}
	ev = poller.get_triggered_event();
	assert(!poller.get_triggered_event());

	return 0;
}

datapool_element_impl* 
datapool_impl::create_pond_element(pond *pondnode,
	element_base_info *info, const char* creator)
{
	assert(nullptr != pondnode);
	assert(nullptr != info);

	if (info->is_global && nullptr == creator) return nullptr;

	// create node 
	pond_element* node = pondnode->create_element_node(info->name.c_str(),
		info->need_persistent, 
		info->is_global ? creator : nullptr);
	// create failure
	if (nullptr == node) return nullptr;

	// init element info
	datapool_element_impl *elementnode = new datapool_element_impl(this, 
		info->name.c_str(),
		info->is_global, 
		info->need_persistent);
	return elementnode;
}

pond* 
datapool_impl::find_create_pond(const char* pondname)
{
	auto_mutex am(_mut);
	assert(nullptr != _pond_manager);
	if (nullptr == pondname) return nullptr;

	pond* pnode = _pond_manager->find_pond(pondname);
	if (pnode) return pnode;
	//not find pond, then create pond
	pnode = _pond_manager->createpond(pondname);
	if (!pnode) return nullptr;
	return pnode;
}

pond_element* 
datapool_impl::find_srv_element(const char* name, bool is_global)
{
	if (is_global){
		if (nullptr == _globalpond) return nullptr;
		return _globalpond->find_element_node(name);
	} else {
		// first find local pond
		pond_element *ele = _localpond.find_element_node(name);
		if (ele) return ele;

		// client == nullptr, evl client error
		evlclient client = evloop::inst()->getclient(nullptr, nullptr);
		if (nullptr == client.get()) return nullptr;

		// find local&persistent pond
		pond* pnode = find_create_pond(client->get_clientname());
		if (nullptr == pnode) return nullptr;
		ele = pnode->find_element_node(name);
		return ele;
	}
}

datapool_element_impl* 
datapool_impl::get_pond_element(pond* pondnode, element_base_info *info)
{
	assert(nullptr != pondnode);
	assert(nullptr != info);
	pond_element *elementnode = 
		pondnode->find_element_node(info->name.c_str());
	if (nullptr == elementnode) return nullptr;
	//datapool_element_impl is temp node
	//it record node info.
	//it is not pond_element
	datapool_element_impl *node = new datapool_element_impl(this,
		info->name.c_str(),
		info->is_global,
		info->need_persistent);
	return node;
}

datapool_element_impl* 
datapool_impl::get_remote_element_unlocked(element_base_info *info)
{
	if (!info) return nullptr;

	dp_evl_msg_retinfo retinfo;
	retinfo.result = -1;
	retinfo.name = info->name;
	int ret = remote_evl_request(dp_pkg_owner_datapool,
		dp_dpowr_act_get_element, true, info, &retinfo);

	// evl send eror
	if (ret) return nullptr;
	
	// retinfo.result == 0 , success
	if (retinfo.result) return nullptr;

	datapool_element_impl *elementnode = new datapool_element_impl(this, 
		info->name.c_str(),
		info->is_global,
		info->need_persistent);
	return elementnode;
}

int datapool_impl::remove_element(const char *name,
	bool is_global)
{

	if (issvr()) {
		evlclient client = evloop::inst()->getclient(nullptr, nullptr);
		if (nullptr == client.get()) return -ENOTAVAIL;

		if (is_global) {
			assert(nullptr != _globalpond);
			return _globalpond->remove_pond_element(name,
				client->get_clientname());
		} else {
			int ret = _localpond.remove_pond_element(name);
			if (!ret) return ret;
			// remove local pond failure
			// try remove local&persistent pond
			pond* pondnode = find_create_pond(client->get_clientname());
			if (!pondnode) return -ENOTFOUND;
			return pondnode->remove_pond_element(name);
		}
	} else {
		int ret = -1;
		if (!is_global) {
			// is_global is false, try remove local pond first
			// if failure, try remove local&persistent on server
			ret = _localpond.remove_pond_element(name);
		} 
		if (ret) {
			//need_persistent only used on create element
			element_base_info elebase(name, is_global, false);
			ret = remove_remote_element_unlocked(&elebase);
		}
		return ret;
	}
}

int datapool_impl::remove_remote_element_unlocked(element_base_info* info)
{
	if (!info) return -ENOTAVAIL;

	dp_evl_msg_retinfo retinfo;
	retinfo.result = -1;
	retinfo.name = info->name;
	int ret = remote_evl_request(dp_pkg_owner_datapool,
		dp_dpowr_act_remove_element, true, info, &retinfo);

	if (ret) return -ETIMEOUT;
	return retinfo.result;
}

// check server 
// > 0 not find on server
// < 0 error
// = 0 find element
int datapool_impl::check_server_element_exist(element_base_info* info)
{
	if (!info) return -ENOTAVAIL;

	dp_evl_msg_retinfo retinfo;
	retinfo.result = -1;
	retinfo.name = info->name;
	int ret = remote_evl_request(dp_pkg_owner_datapool,
		dp_dpowr_act_check_element, true, info, &retinfo);

	if (ret) return -ETIMEOUT;
	return retinfo.result;
}

int datapool_impl::setdata(const char* name, bool is_global,
	bool need_persistent, void* buffer, size_t sz)
{
	if (!name || !*name 
		|| nullptr == buffer || 0 == sz) return -EBADPARM;

	// for server
	if (issvr()) {
		pond_element *ele = find_srv_element(name, is_global);
		if (!ele) return -ENOTFOUND;
		return ele->setdata(buffer, sz);
	} 

	//for client
	if (!is_global) {
		// is_global is false, try find element in _localpond first
		// if not find, try setdata in local&persistent
		pond_element* element = _localpond.find_element_node(name);
		if (element)
			return element->setdata(buffer, sz);
	}

	element_base_info elebase(name, is_global, need_persistent);
	dp_evl_msg_retinfo retinfo;
	retinfo.result = -1;
	retinfo.name = name;
	int ret = remote_evl_request(dp_pkg_owner_element,
		dp_eleowr_act_setdata, true, &elebase, &retinfo,
		nullptr, nullptr, buffer, sz);

	// evl send error
	if (ret) return -ETIMEOUT;
	return retinfo.result;
}
int datapool_impl::getdata(const char* name, bool is_global,
	bool need_persistent, std::string &data)
{
	if (!name || !*name) return -EBADPARM;
	// for server
	if (issvr()) {
		pond_element *ele = find_srv_element(name, is_global);
		if (!ele) return -ENOTFOUND;
		return ele->getdata(data);
	}
	//for client
	if (!is_global) {
		pond_element* element = _localpond.find_element_node(name);
		if (element)
			return element->getdata(data);
	}

	element_base_info elebase(name, is_global, need_persistent);
	dp_evl_msg_retinfo retinfo;
	retinfo.result = -1;
	retinfo.name = name;
	int ret = remote_evl_request(dp_pkg_owner_element,
		dp_eleowr_act_getdata, true, &elebase, &retinfo);

	// evl send error
	if (ret) return -ETIMEOUT;
	
	data = retinfo.data;
	return data.length();
}
int datapool_impl::notify(const char* name, bool is_global,
	bool need_persistent, void* buffer, size_t sz)
{
	if (!name || !*name) return -EBADPARM;

	// for server
	if (issvr()) {
		pond_element *ele = find_srv_element(name, is_global);
		if (!ele) return -ENOTFOUND;
		return ele->notify(buffer, sz);
	}
	//for client
	if (!is_global) {
		pond_element* element = _localpond.find_element_node(name);
		if (element)
			return element->notify(buffer, sz);
	}

	element_base_info elebase(name, is_global, need_persistent);
	dp_evl_msg_retinfo retinfo;
	retinfo.result = -1;
	retinfo.name = name;
	int ret = remote_evl_request(dp_pkg_owner_element,
		dp_eleowr_act_notify, true, &elebase, &retinfo,
		nullptr, nullptr, buffer, sz);

	// evl send error
	if (ret) return -ETIMEOUT;
	return retinfo.result;
}

int datapool_impl::add_listener(const char* name, bool is_global,
	bool need_persistent, notifier notify, void* owner)
{
	if (!name || !*name 
		|| nullptr == notify) return -EBADPARM;

	// for server
	if (issvr()) {
		pond_element *ele = find_srv_element(name, is_global);
		if (!ele) return -ENOTFOUND;
		return ele->add_listener(notify, owner);
	}
	//for client
	if (!is_global) {
		pond_element* element = _localpond.find_element_node(name);
		if (element)
			return element->add_listener(notify, owner);
	}

	element_base_info elebase(name, is_global, need_persistent);
	dp_evl_msg_retinfo retinfo;
	retinfo.result = -1;
	retinfo.name = name;
	int ret = remote_evl_request(dp_pkg_owner_element,
		dp_eleowr_act_addlistener, true, &elebase, &retinfo,
		notify, owner);

	// evl send error
	if (ret) return -ETIMEOUT;

	return retinfo.result;
}

int datapool_impl::remove_listener(const char* name, bool is_global,
	bool need_persistent, notifier notify, void* owner)
{
	if (!name || !*name 
		|| nullptr == notify) return -EBADPARM;

	// for server
	if (issvr()) {
		pond_element *ele = find_srv_element(name, is_global);
		if (!ele) return -ENOTFOUND;
		return ele->remove_listener(notify, owner);
	}
	//for client
	if (!is_global) {
		pond_element* element = _localpond.find_element_node(name);
		if (element)
			return element->remove_listener(notify, owner);
	}
	
	element_base_info elebase(name, is_global, need_persistent);
	dp_evl_msg_retinfo retinfo;
	retinfo.result = -1;
	retinfo.name = name;
	int ret = remote_evl_request(dp_pkg_owner_element,
		dp_eleowr_act_rmlistener, true, &elebase, &retinfo,
		notify, owner);

	// evl send error
	if (ret) return -ETIMEOUT;

	return retinfo.result;
}

//datapool_impl end
///---------------------------

///---------------------------
//pond_element start

pond_element::pond_element(const char* name, bool needpersistent,
const char* creator)
: _element_name(name)
, _need_persistent(needpersistent)
, _element_data(nullptr)
{
	listnode_init(_notify_list);
	if (creator) _creator_name = creator;
	else _creator_name.clear();
}

pond_element::~pond_element()
{
	if (_element_data) {
		delete _element_data;
		_element_data = nullptr;
	}		
	release_all_notifier();
}

int pond_element::setdata(void* buffer, size_t sz)
{
	if (!_element_data)
		_element_data = new std::string();
	_element_data->clear();
	//storage buffer with string
	_element_data->assign((const char*)buffer, sz);
	return 0;
}

int pond_element::getdata(std::string &data)
{
	data.clear();
	if (!_element_data) {
		return 0;
	}
	data.append(_element_data->c_str(), _element_data->length());
	return _element_data->length();
}

size_t pond_element::getdatasize(void)
{
	if (!_element_data) {
		return 0;
	}
	return _element_data->length();
}

const char* pond_element::getdatabuf(void)
{
	if (!_element_data) {
		return nullptr;
	}
	return _element_data->c_str();
}

int pond_element::notify(void* buffer, size_t sz)
{
	listnode_t *nextnode = _notify_list.next;
	element_listener* elnode = nullptr;
	//storage the data to element
	if (buffer && (sz > 0)) {
		if (!_element_data)
			_element_data = new std::string();
		_element_data->clear();
		_element_data->assign((const char*)buffer, sz);
	} else {
		if (_element_data) {
			buffer = (void*)_element_data->c_str();
			sz = _element_data->length();
		}
	}

	//notify all listener
	//need send in another thread?
	for (; nextnode != &_notify_list; nextnode = nextnode->next)
	{
		elnode = LIST_ENTRY(element_listener, ownerlist, nextnode);
		if (nullptr == elnode->client.get()) {
			//local listener
			(elnode->notify)(elnode->owner, buffer, sz);
		} else {
			size_t pkgsz = _element_name.length() + 1 + sz;
			// fill request data
			datapool_server_noitfy_pkg* notifyinfo = 
				new(alloca(sizeof(*notifyinfo) + pkgsz))
				datapool_server_noitfy_pkg(pkgsz);
			datapool_server_noitfy& info = notifyinfo->payload();
			// fill element info
			info.element.name = info.element.bufsz;
			strcpy(&(info.element.buf[info.element.name]),
				_element_name.c_str());
			info.element.bufsz += _element_name.length() + 1;
			info.element.validity.m.name = 1;

			// fill noitfy
			info.element.notify = (void*)(elnode->notify);
			info.element.owner = elnode->owner;
			info.element.validity.m.notifier = 1;
			// fill data buffer
			if (buffer && sz > 0) {
				info.element.databuf = info.element.bufsz;
				memcpy((info.element.buf + info.element.bufsz),
						buffer, sz);
				info.element.datasz = sz;
				info.element.bufsz += sz;
				info.element.validity.m.databuf = 1;
			}
			elnode->client->write((void*)notifyinfo,(sizeof(*notifyinfo) + pkgsz));			
		}
	}	
	return 0;
}

int pond_element::add_listener(notifier notify, void* owner,
	evlclient client)
{
	// create node
	element_listener *enode = new element_listener;
	enode->notify = notify;
	enode->owner = owner;
	enode->client = client;
	//add to list
	listnode_add(_notify_list, enode->ownerlist);
	return 0; 
}

int pond_element::add_listener(notifier notify, void* owner)
{
	// create node
	element_listener *enode = new element_listener;
	enode->notify = notify;
	enode->owner = owner;
	//add to list
	listnode_add(_notify_list, enode->ownerlist);
	return 0; 
}

int pond_element::remove_listener(notifier notify, void* owner)
{
	listnode_t *nextnode = _notify_list.next;
	element_listener* elnode = nullptr;
	//find noitfy node
	for (; nextnode != &_notify_list; nextnode = nextnode->next)
	{
		elnode = LIST_ENTRY(element_listener, ownerlist, nextnode);
		if ((notify == elnode->notify)
			&& (owner == elnode->owner)) {
			//remove from list
			listnode_del(elnode->ownerlist);
			//release notify node
			delete elnode;
			return 0;
		}
	}
	return -ENOTEXISTS;
}

bool pond_element::get_need_persistent(void)
{
	return _need_persistent;
}

void pond_element::release_all_notifier(void)
{
	element_listener *nd;
	listnode_t *node;
	while (!listnode_isempty(_notify_list))
	{
		//get listener list node
		node = _notify_list.next;
		nd = LIST_ENTRY(element_listener, ownerlist, node);
		//remove from list
		listnode_del(nd->ownerlist);
		//release node
		delete nd;
	}
}

//pond_element end
///---------------------------

///---------------------------
//pond start

pond::pond()
: _element_tree(nullptr)
{
	listnode_init(_element_list);
}

pond::~pond()
{
	release_all_node();
}

pond_element* pond::create_element_node(const char* name,
	bool need_persistent, const char* creator)
{
	auto_mutex am(_mut);
	if (!name || !*name) return nullptr;
	// check node is exist
	pond_element* enode = find_element_node_unlocked(name);
	if (enode) return nullptr;
	// create node
	enode = new pond_element(name, need_persistent, creator);

	// add to avltree
	if (avl_insert(&_element_tree, &enode->avlnode,
		pond_element_compare)) {
		delete enode; return nullptr;
	}

	// add to the list
	listnode_add(_element_list, enode->_listowner);
	return enode;
}

void pond::release_all_node()
{
	pond_element *nd;
	listnode_t *node;
	while (!listnode_isempty(_element_list))
	{
		node = _element_list.next;
		nd = LIST_ENTRY(pond_element, _listowner, node);
		listnode_del(nd->_listowner);
		delete nd;
	}
	_element_tree = nullptr;	
}

int pond::pond_element_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(pond_element, avlnode, a);
	auto* bb = AVLNODE_ENTRY(pond_element, avlnode, b);
	int ret = strcmp(aa->_element_name.c_str(), bb->_element_name.c_str());
	if (ret < 0) return -1;
	else if (ret > 0) return 1;
	else return 0;
}

pond_element* pond::find_element_node(const char* name)
{
	auto_mutex am(_mut);
	return find_element_node_unlocked(name);
}

pond_element* pond::find_element_node_unlocked(const char* name)
{
	if (nullptr == name) return nullptr;

	string elementname = name;
	// find avlnode
	// MAKE_FIND_OBJECT same as create tmp pond_element
	avl_node_t* anode = avl_find(_element_tree,
		MAKE_FIND_OBJECT(elementname, pond_element, _element_name, avlnode),
		pond_element_compare);
	if (nullptr == anode) return nullptr;
	return AVLNODE_ENTRY(pond_element, avlnode, anode);	
}

int pond::remove_pond_element(const char* name,
	const char* remover)
{
	auto_mutex am(_mut);
	return remove_pond_element_unlocked(name, remover);
}

int pond::remove_pond_element_unlocked(const char* name,
	const char* remover)
{
	if (!name ||!*name) return -EBADPARM;

	//find remove node
	pond_element* enode = find_element_node_unlocked(name);
	if (nullptr == enode) return -ENOTFOUND;
	// if has creator_name, remove must check creator 
	if (enode->_creator_name.length() > 0) {
		if (nullptr == remover) return -EBADPARM;
		if (strcmp(remover, enode->_creator_name.c_str()))
			return -ENOTAVAIL;
	}
	// remove node
	avl_remove(&_element_tree, (&enode->avlnode));
	listnode_del(enode->_listowner);
	delete enode;

	return 0;
}

//pond end
///---------------------------

///---------------------------
//pond_manager start

pond_manager::pond_manager()
: _pond_tree(nullptr)
{
	listnode_init(_pond_list);
}

pond_manager::~pond_manager() {
	release_all_nodes();
}

int pond_manager::pondmanager_pond_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(pond, avlnode, a);
	auto* bb = AVLNODE_ENTRY(pond, avlnode, b);
	int ret = strcmp(aa->_pond_name.c_str(), bb->_pond_name.c_str());
	if (ret < 0) return -1;
	else if (ret > 0) return 1;
	else return 0;
}

pond* pond_manager::createpond(const char* name)
{
	auto_mutex am(_mut);

	if (!name || !*name) return nullptr;
	
	//check pond exist
	pond* node = find_pond_unlocked(name);
	if (node) return nullptr;

	//create pond
	node = new pond();
	node->_pond_name = name;

	// add to avltree
	if (avl_insert(&_pond_tree, &node->avlnode,
		pondmanager_pond_compare)) {
		delete node; return nullptr;
	}

	// add to the list
	listnode_add(_pond_list, node->_listowner);
	return node;
}

pond* pond_manager::find_pond(const char* name)
{
	auto_mutex am(_mut);
	return find_pond_unlocked(name);
}

pond* pond_manager::find_pond_unlocked(const char* name)
{
	string pondname = name;
	avl_node_t* anode = avl_find(_pond_tree,
		MAKE_FIND_OBJECT(pondname, pond, _pond_name, avlnode),
		pondmanager_pond_compare);
	if (nullptr == anode) return nullptr;
	return AVLNODE_ENTRY(pond, avlnode, anode);
}

int pond_manager::remove_pond(const char* name)
{
	auto_mutex am(_mut);

	if (!name || !*name) return -EINVALID;
	
	pond* node = find_pond_unlocked(name);
	if (nullptr == node) return -ENOTFOUND;
	//remove avl node
	avl_remove(&_pond_tree, &(node->avlnode));
	//remove list node
	listnode_del(node->_listowner);
	//release node
	delete node;
	return 0;
}

void pond_manager::release_all_nodes(void)
{
	auto_mutex am(_mut);
	pond *nd;
	listnode_t *node;
	while (!listnode_isempty(_pond_list))
	{
		node = _pond_list.next;
		nd = LIST_ENTRY(pond, _listowner, node);
		listnode_del(nd->_listowner);
		delete nd;
	}
	_pond_tree = nullptr;
}

//pond_manager end
///---------------------------

///---------------------------
//datapool_element_object start

int datapool_element_object::addref(void)
{
	datapool_element_impl* cl = reinterpret_cast<datapool_element_impl*>(this);
	return cl->addref();
}

int datapool_element_object::release(void)
{
	datapool_element_impl* cl = reinterpret_cast<datapool_element_impl*>(this);
	return cl->release();
}

int datapool_element_object::setdata(const char* str)
{
	if (!str || !*str) return -1;
	datapool_element_impl* cl = reinterpret_cast<datapool_element_impl*>(this);
	return cl->setdata(str);
}

int datapool_element_object::setdata(const std::string &str)
{
	datapool_element_impl* cl = reinterpret_cast<datapool_element_impl*>(this);
	return cl->setdata(str);
}

int datapool_element_object::setdata(void* buffer, size_t sz)
{
	if (nullptr == buffer || 0 == sz) return -1;
	datapool_element_impl* cl = reinterpret_cast<datapool_element_impl*>(this);
	return cl->setdata(buffer, sz);
}

int datapool_element_object::getdata(std::string &data)
{
	datapool_element_impl* cl = reinterpret_cast<datapool_element_impl*>(this);
	return cl->getdata(data);
}

int datapool_element_object::notify(const char* str)
{
	datapool_element_impl* cl = reinterpret_cast<datapool_element_impl*>(this);
	return cl->notify(str);
}
int datapool_element_object::notify(const std::string &str)
{
	datapool_element_impl* cl = reinterpret_cast<datapool_element_impl*>(this);
	return cl->notify(str);
}
int datapool_element_object::notify(void* buffer, size_t sz)
{
	if (nullptr == buffer || 0 == sz) return -EINVALID;
	datapool_element_impl* cl = reinterpret_cast<datapool_element_impl*>(this);
	return cl->notify(buffer, sz);
}

int datapool_element_object::add_listener(notifier notify, void* owner)
{
	if (nullptr == notify) return -EINVALID;
	datapool_element_impl* cl = reinterpret_cast<datapool_element_impl*>(this);
	return cl->add_listener(notify, owner);
}

int datapool_element_object::add_listener(datapool_listener* lnr)
{
	if (nullptr == lnr) return -EINVALID;
	datapool_element_impl* cl = reinterpret_cast<datapool_element_impl*>(this);
	return cl->add_listener(lnr);
}

int datapool_element_object::remove_listener(notifier notify, void* owner)
{
	if (nullptr == notify) return -EINVALID;
	datapool_element_impl* cl = reinterpret_cast<datapool_element_impl*>(this);
	return cl->remove_listener(notify, owner);
}

int datapool_element_object::remove_listener(datapool_listener* lnr)
{
	if (nullptr == lnr) return -EINVALID;
	datapool_element_impl* cl = reinterpret_cast<datapool_element_impl*>(this);
	return cl->remove_listener(lnr);
}

//datapool_element_object end
///---------------------------

///---------------------------
//datapool start

datapool* datapool::inst(void)
{
	static datapool_impl* _inst = nullptr;
	if (_inst) return reinterpret_cast<datapool*>(_inst);

	auto_mutex am(dpmut);
	if (_inst) return reinterpret_cast<datapool*>(_inst);
	_inst = new datapool_impl();
	assert(nullptr != _inst);
	return reinterpret_cast<datapool*>(_inst);
}

datapool_element datapool::create_element(const char* name,
	bool is_global,
	bool need_persistent)
{
	datapool_element element;
	if (!name || !*name) return element;
	datapool_impl* dp = reinterpret_cast<datapool_impl*>(this);
	if (nullptr == dp) return element;
	return dp->create_element(name, is_global, need_persistent);
}

datapool_element datapool::get_element(const char* name,
	bool is_global)
{
	datapool_element element;
	if (!name || !*name) return element;
	datapool_impl* dp = reinterpret_cast<datapool_impl*>(this);
	if (nullptr == dp) return element;
	return dp->get_element(name, is_global);
}

int datapool::remove_element(const char* name,
	bool is_global)
{
	datapool_element element;
	if (!name || !*name) return element;
	datapool_impl* dp = reinterpret_cast<datapool_impl*>(this);
	if (nullptr == dp) return element;
	return dp->remove_element(name, is_global);
}
//datapool end
///---------------------------

}} // end of namespace zas::utils
#endif // UTILS_ENABLE_FBLOCK_DATAPOOL
/* EOF */
