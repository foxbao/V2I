#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <assert.h>

#include "utils/mutex.h"
#include "utils/uuid.h"
#include "utils/timer.h"
#include "mware/rpc/rpcmgr.h"

#include "bridge_msg_def.h"
#include "bridge_servicemgr.h"
#include "mqtt_mgr.h"

namespace zas {
namespace servicebridge {

using namespace std;
using namespace zas::utils;
using namespace zas::mware::rpc;

#define ZAS_RPCSERVER_TOPIC "/rpc/server"

static zas::utils::mutex servciemgr;

zas::mware::rpc::bridge_service* rpcbridge_service::create_instance(
	const char* pkg_name, const char* service_name, const char* inst_name)
{
	auto_mutex am(servciemgr);
	auto* impl = new rpcbridge_service(pkg_name, service_name, inst_name);
	rpcbridge_service_mgr::inst()->add_service(impl);
	return impl;
}

void rpcbridge_service::destory_instance(bridge_service* s)
{
	auto_mutex am(servciemgr);
	delete s;
}


class rpcservice_listner: public mqtt_message_listener
{
public:
	rpcservice_listner(rpcbridge_service *svc)
	{
		_svc = svc;
	}
	~rpcservice_listner(){}
	int on_recv_msg(void* data, size_t sz)
	{
		return _svc->on_recv_msg(data, sz);
	}
private:
	rpcbridge_service* _svc;
};

class rpcservice_mgr_listner: public mqtt_message_listener
{
public:
	rpcservice_mgr_listner(rpcbridge_service_mgr* svc_mgr)
	{
		_svc_mgr = svc_mgr;
	}
	~rpcservice_mgr_listner(){}
	int on_recv_msg(void* data, size_t sz)
	{
		return _svc_mgr->on_recv_msg(data, sz);
	}
private:
	rpcbridge_service_mgr* _svc_mgr;
};

class rpcbridge_status_listener: public mqtt_client_listener
{
public:
	rpcbridge_status_listener(rpcbridge_service_mgr* svc_mgr)
	{
		_svc_mgr = svc_mgr;
	}
	~rpcbridge_status_listener(){}
	void on_connect_success(void) {
		_svc_mgr->on_mqtt_connect();
	}

private:
	rpcbridge_service_mgr* _svc_mgr;
};

rpcbridge_service::rpcbridge_service(const char* pkg_name,
	const char* service_name, const char* inst_name)
: _service_lnr(nullptr)
{
	listnode_init(_session_list);
	if (!pkg_name || !service_name) return ;
	_pkg_name = pkg_name;
	_service_name = service_name;
	if (inst_name) _inst_name = inst_name;
}

rpcbridge_service::~rpcbridge_service()
{

}

rpcbridge_service_mgr::rpcbridge_service_mgr()
{
	listnode_init(_service_list);
	_service_tree = nullptr;
	_mqtt_lnr = nullptr;
	_mqtt_sta_lnr = nullptr;
}

rpcbridge_service_mgr::~rpcbridge_service_mgr()
{
	if (_mqtt_sta_lnr)
		delete _mqtt_sta_lnr;
	_mqtt_sta_lnr = nullptr;
	if (_mqtt_lnr)
		delete _mqtt_lnr;
	_mqtt_lnr = nullptr;
}

rpcbridge_service_mgr* rpcbridge_service_mgr::inst()
{
	static rpcbridge_service_mgr* _inst = nullptr;
	if (_inst) return _inst;

	auto_mutex am(servciemgr);
	if (_inst) return _inst;
	_inst = new rpcbridge_service_mgr();
	assert(nullptr != _inst);
	_inst->init_mqtt();
	return _inst;
}

int rpcbridge_service_mgr::init_mqtt()
{
	if (!_mqtt_lnr)
		_mqtt_lnr = new rpcservice_mgr_listner(this);
	if (!_mqtt_sta_lnr)
		_mqtt_sta_lnr = new rpcbridge_status_listener(this);

	mqtt_client::inst()->register_client_listener(_mqtt_sta_lnr);
	std::string subtopicrc = ZAS_RPCSERVER_TOPIC;
	subtopicrc += SUBSCRIB_TOPIC_PRIFEX;
	subtopicrc += mqtt_client::inst()->get_client_id();
	mqtt_client::inst()->register_message_listener(subtopicrc.c_str(),
		_mqtt_lnr);
	return 0;
}

int rpcbridge_service_mgr::add_service(rpcbridge_service *svc)
{
	auto_mutex am(_mut);
	if (!svc)
		return -EBADPARM;

	auto* svc_imp = find_service_unlock(svc->_pkg_name.c_str(),
		svc->_service_name.c_str(), svc->_inst_name.c_str());
	if (svc_imp) return -EEXISTS;

	if (avl_insert(&_service_tree, &svc->avlnode,
		rpc_bridge_service_compared)) {
		return -ELOGIC;
	}
	listnode_add(_service_list, svc->ownerlist);
	// rpcbridge no need to cloud
	if (!strcmp(svc->_pkg_name.c_str(), BRIDGE_SERVICE_PKG)
		&& !strcmp(svc->_service_name.c_str(), BRIDGE_SERVICE_NAME))
		return -ENAVAIL;

	get_service_topic(svc);
	return 0;
}

int rpcbridge_service_mgr::get_service_topic(rpcbridge_service* svc)
{
	if (!svc) return -EBADPARM;
	if (!mqtt_client::inst()->is_runing())
		return -ENOTAVAIL;
	size_t sz = sizeof(request_topic)
				+ strlen(mqtt_client::inst()->get_client_id())
				+ svc->_pkg_name.length()
				+ svc->_service_name.length();
	if (svc->_inst_name.length() > 0)
		sz += svc->_inst_name.length();
	if (svc->_service_token.length() > 0)
		sz += svc->_service_token.length();
	auto* req_msg = new(alloca(sz))request_topic;
	req_msg->type = 0;

	size_t datalen = 0;
	req_msg->clientid = strlen(mqtt_client::inst()->get_client_id());
	strcpy(req_msg->buf + datalen, mqtt_client::inst()->get_client_id());
	datalen += req_msg->clientid;

	req_msg->pkg = svc->_pkg_name.length();
	strcpy(req_msg->buf + datalen, svc->_pkg_name.c_str());
	datalen += req_msg->pkg;

	req_msg->name = svc->_service_name.length();
	strcpy(req_msg->buf + datalen, svc->_service_name.c_str());
	datalen += req_msg->name;

	req_msg->naming = 0;
	if (svc->_inst_name.length() > 0) {
		req_msg->naming = svc->_inst_name.length();
		strcpy(req_msg->buf + datalen, svc->_inst_name.c_str());
		datalen += req_msg->naming;
	}

	req_msg->securecode = 0;
	req_msg->token = 0;
	if (svc->_service_token.length() > 0) {
		req_msg->token = svc->_service_token.length();
		strcpy(req_msg->buf + datalen, svc->_service_token.c_str());
		datalen += req_msg->token;
	}
	printf("get_service_topic pkg: %s, service %s\n", svc->_pkg_name.c_str(), svc->_service_name.c_str());
	mqtt_client::inst()->send_message(ZAS_RPCSERVER_TOPIC,
		(void*)req_msg, sz);
	return 0;
}

int rpcbridge_service_mgr::remove_service(rpcbridge_service *svc)
{
	auto_mutex am(_mut);

	auto* svc_imp = find_service_unlock(svc->_pkg_name.c_str(),
		svc->_service_name.c_str(), svc->_inst_name.c_str());
	if (!svc_imp) return -ENOTEXISTS;

	avl_remove(&_service_tree, &svc->avlnode);
	listnode_del(svc->ownerlist);
	return 0;
}

rpcbridge_service* rpcbridge_service_mgr::find_service(
	const char* pkgname,const char* service_name, const char* inst_name)
{
	auto_mutex am(_mut);
	return find_service_unlock(pkgname, service_name, inst_name);
}

int rpcbridge_service_mgr::remove_all_service(void)
{
	rpcbridge_service *client = nullptr;
	while(!listnode_isempty(_service_list)) {
		client = LIST_ENTRY(rpcbridge_service, ownerlist, _service_list.next);
		avl_remove(&_service_tree, &client->avlnode);
		listnode_del(client->ownerlist);
	}
	return 0;
}

rpcbridge_service* rpcbridge_service_mgr::find_service_unlock(
	const char* pkgname,const char* service_name, const char* inst_name)
{
	rpcbridge_service svc_impl(pkgname, service_name, inst_name);

	avl_node_t *svc_nd = avl_find(_service_tree,
		&svc_impl.avlnode, rpc_bridge_service_compared);
	if (!svc_nd) return nullptr;

	return AVLNODE_ENTRY(rpcbridge_service, avlnode, svc_nd);
}


int rpcbridge_service_mgr::rpc_bridge_service_compared(
	avl_node_t* aa, avl_node_t* bb)
{
	auto* nda = AVLNODE_ENTRY(rpcbridge_service, avlnode, aa);
	auto* ndb = AVLNODE_ENTRY(rpcbridge_service, avlnode, bb);
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

void rpcbridge_service_mgr::on_mqtt_connect(void)
{
	init_mqtt();
	rpcbridge_service *svc = nullptr;
	listnode_t* nd = _service_list.next; 
	for(; nd != &_service_list; nd = nd->next) {
		svc = LIST_ENTRY(rpcbridge_service, ownerlist, nd);
		if (!strcmp(svc->_pkg_name.c_str(), BRIDGE_SERVICE_PKG)
			&& !strcmp(svc->_service_name.c_str(), BRIDGE_SERVICE_NAME))
			continue;
		get_service_topic(svc);
	}
}

int rpcbridge_service_mgr::on_recv_msg(void* data, size_t sz)
{
	printf("rpcbridge_service_mgr rec %ld\n", sz);
	auto* reply = reinterpret_cast<topic_reply*>(data);
	size_t datasize = sizeof(topic_reply) + reply->pkg
		+ reply->name + reply->naming + reply->topic
		+ reply->subtopic + reply->token;

	printf("need data size %lu\n", datasize);
	if (!reply->pkg || !reply->name || !reply->token
		|| !reply->topic || 1 !=reply->code) {
		printf("recv from remote serve, param error %d\n", reply->code);
		return -EBADPARM;
	}

	size_t pos = 0;

	std::string pkgname;
	pkgname.append(reply->buf + pos, reply->pkg);
	pos += reply->pkg;

	std::string service_name;
	service_name.append(reply->buf + pos, reply->name);
	pos += reply->name;

	std::string inst_name;
	if (reply->naming > 1) {
		service_name.append(reply->buf + pos, reply->naming);
		pos += reply->naming;
	} else {
		inst_name.clear();
	}

	std::string topic = "/";
	topic.append(reply->buf + pos, reply->topic);
	pos += reply->topic;
	if (reply->subtopic > 1) {
		topic += "/";
		topic.append(reply->buf + pos, reply->subtopic);
		pos += reply->subtopic;
	}

	std::string token;
	token.append(reply->buf + pos, reply->token);

	auto* svc = find_service_unlock(pkgname.c_str(),
		service_name.c_str(), inst_name.c_str());

	if (!svc) {
		printf("recv from remote serve ,servcie [%s] [%s] not find\n",
			pkgname.c_str(), service_name.c_str());
		return -ENOTFOUND;
	}

	svc->settoken(token);
	svc->settopic(topic);
	return 0;
}

int rpcbridge_service::settopic(std::string &topic) {
	printf("settopic is %s\n", topic.c_str());
	_service_topic = topic;
	if (!_service_lnr)
		_service_lnr = new rpcservice_listner(this);
	std::string subtopic = topic;
	subtopic += SUBSCRIB_TOPIC_PRIFEX;
	subtopic += mqtt_client::inst()->get_client_id();
	mqtt_client::inst()->register_message_listener(subtopic.c_str(),
		_service_lnr);

	std::string event_topic = topic + "/event";
	mqtt_client::inst()->register_message_listener(event_topic.c_str(),
		_service_lnr);
	if (!_rpc_bridge_obj.get())
		_rpc_bridge_obj = rpcmgr::inst()->get_bridge(_pkg_name.c_str(),
			_service_name.c_str(), _inst_name.c_str());
	return 0;
}

int rpcbridge_service::on_recv_msg(void* data, size_t sz)
{
	uint8_t *msgid = (uint8_t*) data;

	int ret = 0;
	switch (*msgid)
	{
	case 0: {
		auto* method = reinterpret_cast<reply_method_invoke*>(data);
		try {
			ret = handle_method_back(method);
		}catch(rpc_error e) {
			printf("method error %s\n", e.get_description().c_str());
		}
		break;
	}
	case 1:{
		auto* event = reinterpret_cast<recv_event_invoke*>(data);
		ret = handle_event_trigger(event);
		break;
	}
	case 2:{
		auto* obscall = reinterpret_cast<observable_called*>(data);
		try {
			ret = handle_observable_call(obscall);
		}catch(rpc_error e) {
			printf("observable error %s\n", e.get_description().c_str());
		}
		break;
	}
	default:
		break;
	}

	return ret;
}

int rpcbridge_service::handle_method_back(reply_method_invoke *ret)
{
	evlclient evl = get_add_session(ret->seqid.evlid, ret->seqid.seqid);
	printf("msg handle flag %d, coder %d, time %ld, timestamp %lu\n",
		ret->flag,
		ret->code,
		gettick_millisecond() - ret->seqid.retained,
		gettick_millisecond());
	if (1 != ret->code && ret->errlen > 0) {
		std::string errcode(ret->buf, ret->errlen);
		printf("return error is %s", errcode.c_str());
	}
	return _rpc_bridge_obj->senddata(evl, (void*)ret->instanceid,
			ret->seqid.seqid,ret->code, ret->flag,
			(ret->buf + ret->errlen + ret->clslen + ret->methodid),
			ret->outputsz);
}

int rpcbridge_service::handle_event_trigger(recv_event_invoke *ret)
{
	std::string name;
	name.append(ret->buf, ret->namelen);
	void* data = nullptr;
	if (ret->inputlen)
		data = (void*)(ret->buf + ret->namelen);
	return _rpc_bridge_obj->sendevent(name, data, ret->inputlen);
}

int rpcbridge_service::handle_observable_call(observable_called *ret)
{
	size_t data_pos = 0;
	std::string clsname;
	clsname.append(ret->buf, ret->classlen);
	data_pos += ret->classlen;
	std::string uuidstr;
	uuidstr.append(ret->buf + data_pos, ret->uuid);
	data_pos += ret->uuid;

	uuid tmpuid;
	tmpuid.set(uuidstr.c_str());

	std::string client_name;
	client_name.append(ret->buf + data_pos, ret->client_name);
	data_pos += ret->client_name;

	std::string inst_name;
	inst_name.append(ret->buf + data_pos, ret->inst_name);
	data_pos += ret->inst_name;

	std::string inparam;
	inparam.append(ret->buf + data_pos, ret->inputsz);
	data_pos += ret->inputsz;

	std::string inout;
	inparam.append(ret->buf + data_pos, ret->outputsz);
	data_pos += ret->outputsz;

	size_t outsz = ret->outputsz;

	printf("handle_observable_call %ld, timestamp %lu\n",
		ret->instanceid,
		gettick_millisecond());
	rpcobservable::inst()->invoke_observable_method((void*)ret->instanceid, 
		client_name.c_str(), inst_name.c_str(),
		clsname.c_str(), tmpuid.getuuid(),
		(void*)inparam.c_str(), ret->inputsz, inout, outsz);
	size_t sendsz = sizeof(observable_return) + _service_token.length()
		+ clsname.length() + uuidstr.length() + inout.length();
	auto* req_msg = new(alloca(sendsz))observable_return;
	req_msg->msgid = 2;
	req_msg->code = 1;
	req_msg->errmsg = 0;

	size_t datalen = 0;
	
	memcpy(req_msg->buf + datalen, _service_token.c_str(),  _service_token.length());
	req_msg->token = (uint8_t)_service_token.length();
	datalen += _service_token.length();
	
	memcpy(req_msg->buf + datalen, clsname.c_str(), clsname.length());
	req_msg->clslen = (uint8_t)clsname.length();
	datalen += clsname.length();

	req_msg->uuid = (uint8_t)uuidstr.length();
	memcpy(req_msg->buf + datalen, uuidstr.c_str(), uuidstr.length());
	datalen += uuidstr.length();
	req_msg->instanceid = (int64_t)ret->instanceid;
	req_msg->outputsz = (uint16_t)inout.length();
	memcpy(req_msg->buf + datalen, inout.c_str(), outsz);
	datalen += outsz;
	mqtt_client::inst()->send_message(_service_topic.c_str(), req_msg, sendsz);
	return 0;
}

int rpcbridge_service::on_recvdata(utils::evlclient cli, void* inst_id,
		uint32_t seqid, std::string& classname, uint128_t* method_uuid,
		uint8_t flag, void* indata, size_t insize,
		void* inoutdata, size_t inoutsize)
{
	if (_service_token.empty()) {
		msleep(1000);
		if (_service_token.empty())
			return -ENOTAVAIL;
	}

	size_t sz = insize + inoutsize  + _service_token.length()
		+ classname.length() + sizeof(request_method_invoke);
	std::string methodstrid;
	if (method_uuid) {
		uuid tmp;
		tmp = *method_uuid;
		tmp.to_hex(methodstrid);
		sz += methodstrid.length();
	}

	auto* req_msg = new(alloca(sz))request_method_invoke;
	req_msg->msgid = 0;
	req_msg->flag = flag;
	req_msg->instancid = 0;
	if (inst_id) req_msg->instancid = (uint64_t)inst_id;
	// printf("instance id is %08lx\n", req_msg->instancid);
	req_msg->seqid.evlid = (uint64_t)cli.get();
	req_msg->seqid.seqid = seqid;
	req_msg->seqid.retained = gettick_millisecond();
	req_msg->inputsz = insize;
	req_msg->outputsz = inoutsize;
	size_t datalen = 0;
	
	strcpy(req_msg->buf + datalen, _service_token.c_str());
	req_msg->token = _service_token.length();
	datalen += _service_token.length();

	strcpy(req_msg->buf + datalen, classname.c_str());
	req_msg->clslen = classname.length();
	datalen += classname.length();

	req_msg->methodid = 0;
	if (method_uuid) {
		req_msg->methodid = methodstrid.length();
		strcpy(req_msg->buf + datalen, methodstrid.c_str());
		datalen += methodstrid.length();
	}

	if (indata && insize > 0) {
		memcpy(req_msg->buf + datalen, indata, insize);
		datalen += insize;
	}

	if (inoutdata && inoutsize > 0) {
		memcpy(req_msg->buf + datalen, inoutdata, inoutsize);
		datalen += inoutsize;
	}

	add_session(cli, seqid);

	mqtt_client::inst()->send_message(_service_topic.c_str(), req_msg, sz);
	return 0;
}

int rpcbridge_service::on_eventdata(std::string &eventid, uint8_t action)
{
	if (_service_token.empty()) return -ENOTAVAIL;

	size_t sz = eventid.length() + 1
		+ _service_token.length() + 1;

	auto* req_msg = new(alloca(sz))send_event_invoke;
	req_msg->msgid = 0;
	req_msg->action = action;
	size_t datalen = 0;
	
	strcpy(req_msg->buf + datalen, _service_token.c_str());
	req_msg->token =  _service_token.length() + 1;
	datalen += _service_token.length() + 1;

	strcpy(req_msg->buf + datalen, eventid.c_str());
	req_msg->token =  eventid.length() + 1;
	datalen += eventid.length() + 1;

	mqtt_client::inst()->send_message(_service_topic.c_str(), req_msg,
		sizeof(send_event_invoke) + sz);
	return 0;
}

int rpcbridge_service::add_session(evlclient evl, uint32_t seqid)
{
	listnode_t *nd = _session_list.next;
	for (; nd != &_session_list; nd = nd->next) {
		auto* session = LIST_ENTRY(msg_session_hdr, ownerlist, nd);
		if (evl == session->client && seqid == session->seqid)
			return -EEXISTS;
	}
	auto* session = new msg_session_hdr;
	session->client = evl;
	session->seqid = seqid;
	listnode_add(_session_list, session->ownerlist);
	return 0;
}

evlclient rpcbridge_service::get_add_session(uint64_t evl, uint32_t seqid)
{
	evlclient cli;
	listnode_t *nd = _session_list.next;
	for (; nd != &_session_list; nd = nd->next) {
		auto* session = LIST_ENTRY(msg_session_hdr, ownerlist, nd);
		if (evl == (uint64_t)(session->client.get())
			&& seqid == session->seqid) {
				cli = session->client;
				listnode_del(session->ownerlist);
				delete session;
				return cli;
			}
	}
	return cli;
}

int rpcbridge_service::clear_session(void)
{
	while(listnode_isempty(_session_list)) {
		auto* session = 
			LIST_ENTRY(msg_session_hdr, ownerlist, _session_list.next);
		listnode_del(session->ownerlist);
		delete session;
	}
	return 0;
}

}} // end of namespace zas::rpcbridge
/* EOF */