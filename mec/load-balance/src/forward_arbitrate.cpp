#include "load-balance-def.h"
#include "forward_arbitrate.h"
#include "forward.h"
#include "server-endpoint-mgr.h"
#include "webcore/webapp.h"
#include "client-mgr.h"

#include "webcore/sysconfig.h"
#include "webcore/logger.h"

namespace zas {
namespace load_balance {
using namespace zas::webcore;

forward_arbitrate::forward_arbitrate()
: _context(nullptr)
, _client(nullptr)
, _server(nullptr)
, _rule_tree(nullptr)
, _server_mgr(nullptr)
{
	listnode_init(_rule_list);
	_server_mgr = new server_endpoint_mgr();
	assert(nullptr != _server_mgr);
}

forward_arbitrate::~forward_arbitrate()
{
	release_all_service();
	if (_client) {
		_client->release();
		_client = nullptr;
	}
	if (_server) {
		_server->release();
		_server = nullptr;
	}
	if (_server_mgr) {
		delete _server_mgr;
		_server_mgr = nullptr;
	}
}

int forward_arbitrate::release_all_service(void)
{
	while (!listnode_isempty(_rule_list)) {
		auto* svc_item = 
			LIST_ENTRY(route_rule_item, ownerlist, _rule_list.next);
		listnode_del(svc_item->ownerlist);
		delete svc_item;
	}
	_rule_tree = nullptr;
	return 0;
}

int forward_arbitrate::init(void)
{
	// load all service config
	init_rule_list();

	std::string client_uri;
	std::string server_uri;
	init_client_endpoint(client_uri);
	init_server_endpoint(server_uri);
	auto* backend = forward_backend::inst();
	backend->set_forward_info(client_uri.c_str(), webapp_socket_router,
		server_uri.c_str(), webapp_socket_router);
	backend->set_forward_callback(this);
	return 0;
}

int forward_arbitrate::load_rule_url_name(std::string &name, int order)
{
	if (order < 0) { return -EBADPARM; }

	std::string name_path = "load-balance-service.route-rule.";
	int ret = 0;
	name_path += std::to_string(order);
	name_path += ".url-name";

	name = get_sysconfig(name_path.c_str(), name.c_str(), &ret);
	if (ret) {
		log.e(LOAD_BALANCE_FORWRD_TAG,
			"forward arbitrate load rule url error\n");
		return -ENOTFOUND;
	}
	return 0;
}

int forward_arbitrate::load_service_name(std::string &name, int order)
{
	if (order < 0) { return -EBADPARM; }

	std::string name_path = "load-balance-service.route-rule.";
	int ret = 0;
	name_path += std::to_string(order);
	name_path += ".service.name";

	name = get_sysconfig(name_path.c_str(), name.c_str(), &ret);
	if (ret) {
		log.e(LOAD_BALANCE_FORWRD_TAG,
			"forward arbitrate load route service error\n");
		return -ENOTFOUND;
	}
	return 0;
}

int forward_arbitrate::load_service_type(route_rule_item* item, int order)
{
	if (!item) { return -EBADPARM; }
	std::string type_path = "load-balance-service.route-rule.";
	std::string type;
	int ret = 0;
	type_path += std::to_string(order);
	type_path += ".service.type";
	type = get_sysconfig(type_path.c_str(), type.c_str(), &ret);
	return 0;
}

int forward_arbitrate::init_rule_list(void)
{
	size_t cnt = 0;
	int ret = 0;
	cnt = get_sysconfig("load-balance-service.rule-cnt", cnt, &ret);
	assert(cnt > 0 && !ret);

	std::string name;
	std::string svc_name;
	for (int i = 0; i < cnt; i++) {
		if (load_rule_url_name(name, i)) {
			continue;
		}
		auto* item = find_rule(name);
		if (!item) {
			item = add_rule(name);
		}
		assert(nullptr != item);

		if (load_service_name(svc_name, i)) {
			continue;
		}
		item->name = svc_name;
		load_service_type(item, i);
		auto svc = _server_mgr->get_service_item(svc_name);
		if (!svc.get()) {
			log.e(LOAD_BALANCE_FORWRD_TAG,
				"forward arbitrate load note exist route service\n");
			continue;
		}
		item->service = svc;
	}
	return 0;
}

int forward_arbitrate::get_urlname_from_uri(uri &url, std::string &url_name)
{
	std::string fullpath = url.get_fullpath();
	const char* name = fullpath.c_str();
	if (nullptr == name) {
		return -ENOTAVAIL;
	}
	const char* first_slash = strchr(name, '/');
	if (nullptr == first_slash) {
		return -ENOTFOUND;
	}
	size_t sz = strlen(name) - (first_slash - name);
	url_name.clear();
	url_name.append(first_slash, sz);
	return 0;
}

route_rule_item* forward_arbitrate::add_rule(std::string &name)
{
	auto* item = find_rule(name);
	if (item) {
		return item;
	}
	item = new route_rule_item();
	item->url_name = name;
	if (avl_insert(&_rule_tree, &item->avlnode,
		route_rule_item::route_rule_compare)) {
		delete item;
		return nullptr;
	}
	listnode_add(_rule_list, item->ownerlist);
	return item;
}

route_rule_item* forward_arbitrate::find_rule(std::string &name)
{
	avl_node_t* anode = avl_find(_rule_tree, 
		MAKE_FIND_OBJECT(name, route_rule_item, url_name, avlnode),
		route_rule_item::route_rule_compare);
	if (!anode)
		return nullptr;
	return AVLNODE_ENTRY(route_rule_item, avlnode, anode);
}

int route_rule_item::route_rule_compare(avl_node_t* a,
	avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(route_rule_item, avlnode, a);
	auto* bb = AVLNODE_ENTRY(route_rule_item, avlnode, b);
	int ret = aa->url_name.compare(bb->url_name);
	if (ret > 0) {
		return 1;
	} else if (ret < 0) {
		return -1;
	} else return 0;
}

int forward_arbitrate::init_client_endpoint(std::string &url)
{
	auto* backend = forward_backend::inst();
	assert(nullptr != backend);
	_context = backend->create_context();
	assert(_context != nullptr);
	size_t port = LOAD_BALANCE_CLIENT_DEFAULT_PORT;
	port = get_sysconfig("load-balance-service.client-endpoint", port);
	url = "tcp://*:";
	url += std::to_string(port);
	log.d(LOAD_BALANCE_FORWRD_TAG,
		"forward arbitrate client uri : %s\n",
		url.c_str());
	_client = backend->bind(_context, url.c_str(),
		webapp_socket_router);
	assert(nullptr != _client);
	return 0;
}

int forward_arbitrate::init_server_endpoint(std::string &url)
{
	auto* backend = forward_backend::inst();
	assert(nullptr != backend);
	_context = backend->create_context();
	assert(_context != nullptr);
	size_t port = LOAD_BALANCE_SERVER_ENDPOINT_DEFAULT_PORT;
	port = get_sysconfig("load-balance-service.server-endpoint.port", port);
	url = "tcp://*:";
	url += std::to_string(port);
	log.d(LOAD_BALANCE_FORWRD_TAG, 
		"forward arbitrate server uri : %s\n",
		url.c_str());
	_server = backend->bind(_context, url.c_str(),
		webapp_socket_router);
	assert(nullptr != _server);
	return 0;
}

int forward_arbitrate::service_request(zmq_backend_socket* wa_sock,
	webapp_zeromq_msg* data)
{
	std::string identify = data->get_part(0);
	std::string hdr_info = data->get_part(2);
	auto* hdr = reinterpret_cast<server_header*>((char*)hdr_info.c_str());
	if (hdr->svc_type == service_msg_request) {
		std::string addr = data->body(1);
		if (addr.length() < 1) {
			log.e(LOAD_BALANCE_FORWRD_TAG,
				"vehicle indexing recv error message\n");
			return -EBADPARM;
		}
		auto* frame_uri = (basicinfo_frame_uri*)(addr.c_str());
		std::string add_uri(frame_uri->uri, frame_uri->uri_length);
		uri addr_info(add_uri);
		data->erase_part(2);
		return forward_to_service(data, addr_info);
	} else if (hdr->svc_type == service_msg_register) {
		std::string svr_info = data->get_part(3);
		auto* s_info = reinterpret_cast<server_request_info*>((char*)svr_info.c_str());
		assert(nullptr != s_info);
		assert(nullptr != _server_mgr);
		s_info->buf[s_info->name_len] = '\0';
		_server_mgr->add_server_endpoint(s_info->buf, identify.c_str());
		server_header rep_hdr;
		rep_hdr.svc_type = service_msg_reply;
		data->set_part(2, (char*)&rep_hdr, sizeof(server_header));
		server_reply_info rep_info;
		rep_info.result = 0;
		data->set_part(3, (char*)&rep_info, sizeof(server_reply_info));
		return forward_backend::inst()->send_to_service(wa_sock, data);
	}
	return 0;
}

int forward_arbitrate::forward_to_service(webapp_zeromq_msg* data, uri &url)
{
	if (!data) {
		return -EBADPARM;
	}
	std::string url_name;
	if (get_urlname_from_uri(url, url_name)) {
		log.e(LOAD_BALANCE_FORWRD_TAG, 
			"get url error\n");
		return -EBADPARM;
	}
	auto* rule = find_rule(url_name);
	if (!rule) {
		log.e(LOAD_BALANCE_FORWRD_TAG, 
			"no forward rule %s\n", url_name.c_str());
		return -ENOTFOUND;
	}
	assert(nullptr != _server_mgr);
	if (!rule->service.get()) {
		auto svc = _server_mgr->get_service_item(rule->name);
		if (!svc.get()) {
			log.e(LOAD_BALANCE_FORWRD_TAG,
				"forward arbitrate not find service %s, url %s\n", rule->name.c_str(), rule->url_name.c_str());
			return -ENOTFOUND;
		}
		rule->service = svc;
	}
	forward_backend::inst()->forward_to_service(data,
		_server_mgr->get_service_endpoint(rule->service.get()));
	return 0;
}


}}	// zas::load_balance
