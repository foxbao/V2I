#include "indexing-def.h"
#include "forward_arbitrate.h"
#include "forward.h"
#include "server-endpoint-mgr.h"
#include "webcore/webapp.h"
#include "vehicle-mgr.h"

#include "webcore/sysconfig.h"
#include "webcore/logger.h"

namespace zas {
namespace vehicle_indexing {
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
		auto* route_item = 
			LIST_ENTRY(route_rule_item, ownerlist, _rule_list.next);
		while (!listnode_isempty(route_item->forward_service_list)) {
			auto* svc_item = 
				LIST_ENTRY(forwork_service_item, ownerlist,	\
					route_item->forward_service_list.next);
			while (!listnode_isempty(svc_item->forward_node_list))
			{
				auto* node = LIST_ENTRY(forward_service_node,	\
					ownerlist, svc_item->forward_node_list.next);
				node->master.endpoint = nullptr;
				node->slaver.endpoint = nullptr;
				listnode_del(node->ownerlist);
				delete node;
			}
			listnode_del(svc_item->ownerlist);
			delete svc_item;
		}
		listnode_del(route_item->ownerlist);
		delete route_item;
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
	backend->set_forward_info(client_uri.c_str(), webapp_socket_worker,
		server_uri.c_str(), webapp_socket_router);
	backend->set_forward_callback(this);
	return 0;
}

forwork_service_item* forward_arbitrate::find_service(uri &url,
	std::string &svc_name)
{
	std::string url_name;
	if (get_urlname_from_uri(url, url_name)) {
		log.e(INDEXING_FORWRD_TAG, 
			"get url error\n");
		return nullptr;
	}
	auto* rule = find_rule(url_name);
	if (!rule) {
		log.e(INDEXING_FORWRD_TAG, 
			"no forward rule %s\n", url_name.c_str());
		return nullptr;
	}
	auto* item = find_service(rule, svc_name);
	return item;
}

int forward_arbitrate::load_rule_url_name(std::string &name, int order)
{
	if (order < 0) { return -EBADPARM; }

	std::string name_path = "indexing-service.route-rule.rule-";
	int ret = 0;
	name_path += std::to_string(order);
	name_path += ".url-name";

	name = get_sysconfig(name_path.c_str(), name.c_str(), &ret);
	if (ret) {
		log.e(INDEXING_FORWRD_TAG,
			"forward arbitrate load rule url error\n");
		return -ENOTFOUND;
	}
	return 0;
}

int forward_arbitrate::load_service_name(std::string &name, int order)
{
	if (order < 0) { return -EBADPARM; }

	std::string name_path = "indexing-service.route-rule.rule-";
	int ret = 0;
	name_path += std::to_string(order);
	name_path += ".service.name";

	name = get_sysconfig(name_path.c_str(), name.c_str(), &ret);
	if (ret) {
		log.e(INDEXING_FORWRD_TAG,
			"forward arbitrate load route service error\n");
		return -ENOTFOUND;
	}
	return 0;
}

forwork_service_item*
forward_arbitrate::add_service(route_rule_item* item,
	std::string &svc_name)
{
	if (!item) { return nullptr; }
	auto* svc_item = find_service(item, svc_name);
	if (svc_item) {
		return svc_item;
	}
	svc_item = new forwork_service_item();
	svc_item->name = svc_name;
	svc_item->type = 0;
	svc_item->inter_change = 0;
	svc_item->keyword.clear();
	svc_item->forward_node_tree = nullptr;
	listnode_init(svc_item->forward_node_list);
	if (avl_insert(&item->forward_service_tree, &svc_item->avlnode,
		forwork_service_item::forward_service_item_compare)) {
		delete svc_item;
		return nullptr;
	}
	listnode_add(item->forward_service_list, svc_item->ownerlist);
	return svc_item;
}

forwork_service_item*
forward_arbitrate::find_service(route_rule_item* item,
	std::string &svc_name)
{
	if (!item) { return nullptr; }
	avl_node_t* anode = avl_find(item->forward_service_tree, 
		MAKE_FIND_OBJECT(svc_name, forwork_service_item, name, avlnode),
		forwork_service_item::forward_service_item_compare);
	if (!anode)
		return nullptr;
	return AVLNODE_ENTRY(forwork_service_item, avlnode, anode);
}

int forwork_service_item::forward_service_item_compare(avl_node_t* a,
	avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(forwork_service_item, avlnode, a);
	auto* bb = AVLNODE_ENTRY(forwork_service_item, avlnode, b);
	int ret = aa->name.compare(bb->name);
	if (ret > 0) {
		return 1;
	} else if (ret < 0) {
		return -1;
	} else return 0;
}

int forward_arbitrate::load_service_type(forwork_service_item *item, int order)
{
	if (!item) { return -EBADPARM; }
	std::string type_path = "indexing-service.route-rule.rule-";
	std::string type;
	int ret = 0;
	type_path += std::to_string(order);
	type_path += ".service.type";
	type = get_sysconfig(type_path.c_str(), type.c_str(), &ret);
	if (ret) {
		item->type = 0;
	} else if(!type.compare("all")) {
		item->type = service_type_forward | service_type_internal;
	} else if (!type.compare("internal")) {
		item->type = service_type_internal;
	} else if (!type.compare("forward")) {
		item->type = service_type_forward;
	} else {
		log.w(INDEXING_FORWRD_TAG,
			"forward arbitrate load service type unknown\n");
		item->type = 0;
	}
	return 0;
}

int forward_arbitrate::load_service_keyword(forwork_service_item *item,
	int order)
{
	if (!item) { return -EBADPARM; }
	std::string keyword_path = "indexing-service.route-rule.rule-";
	std::string type;
	int ret = 0;
	keyword_path += std::to_string(order);
	keyword_path += ".service.keyword";
	item->keyword = get_sysconfig(keyword_path.c_str(),
		item->keyword.c_str(), &ret);
	if ((item->type & service_type_forward)
		&& (ret || item->keyword.length() == 0)) {
		log.e(INDEXING_FORWRD_TAG,
			"forward arbitrate load forward and no keyward\n");
		//todo, return ?
	}
	return 0;
}

int forward_arbitrate::load_service_change(forwork_service_item *item,
	int order)
{
	if (!item) { return -EBADPARM; }
	std::string inter_change_path = "indexing-service.route-rule.rule-";
	std::string type;
	int ret = 0;
	inter_change_path += std::to_string(order);
	inter_change_path += ".service.inter_change";
	item->inter_change = get_sysconfig(inter_change_path.c_str(),
		item->inter_change, &ret);

	return 0;
}

int forward_arbitrate::init_rule_list(void)
{
	size_t cnt = 0;
	int ret = 0;
	cnt = get_sysconfig("indexing-service.route-rule.rule-cnt", cnt, &ret);
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

		if (find_service(item, svc_name)) {
			log.e(INDEXING_FORWRD_TAG,
				"forward arbitrate load exist route service\n");
			continue;
		}
		auto* svc_item = add_service(item, svc_name);
		if (load_service_type(svc_item, i)) {
			continue;
		}
		if (load_service_keyword(svc_item, i)) {
			continue;
		}
		if (load_service_change(svc_item, i)) {
			continue;
		}
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
	item->forward_service_tree = nullptr;
	listnode_init(item->forward_service_list);
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

forward_service_node* 
forward_arbitrate::find_forward_service_node(forwork_service_item* item,
		std::string &node_id)
{
	if (!item) { return nullptr; }
	avl_node_t* anode = avl_find(item->forward_node_tree, 
		MAKE_FIND_OBJECT(node_id, forward_service_node, node_id, avlnode),
		forward_service_node::forward_service_node_compare);
	if (!anode) { return nullptr; }
	return AVLNODE_ENTRY(forward_service_node, avlnode, anode);
}

forward_service_node* 
forward_arbitrate::add_forward_service_node(forwork_service_item* item,
	std::string &node_id)
{
	if (!item || node_id.length() == 0) {
		log.e(INDEXING_FORWRD_TAG, "create forward node param error\n");
		return nullptr;
	}
	auto* svc_node = find_forward_service_node(item, node_id);
	if (svc_node) { return svc_node; }

	svc_node = new forward_service_node();
	svc_node->node_id = node_id;
	svc_node->name = item->name;
	svc_node->master.endpoint = nullptr;
	svc_node->master.identity.clear();
	svc_node->slaver.endpoint = nullptr;
	svc_node->slaver.identity.clear();

	if (avl_insert(&item->forward_node_tree, &svc_node->avlnode,
		forward_service_node::forward_service_node_compare)) {
		delete item;
		return nullptr;
	}
	listnode_add(item->forward_node_list, svc_node->ownerlist);
	return svc_node;
}

int forward_arbitrate::check_set_service_node(forward_service_node* node)
{
	if (!node) { return -EBADPARM; }
	assert(nullptr != _server_mgr);

	bool maste_invail = false;
	if (nullptr == node->master.endpoint.get()
		|| node->master.endpoint->_status != 1) {
		maste_invail = true;
	}
	bool slave_invail = false;
	if (nullptr == node->slaver.endpoint.get()
		|| node->slaver.endpoint->_status != 1) {
		slave_invail = true;
	}

	if (maste_invail && slave_invail){
		node->master.endpoint = 
			_server_mgr->alloc_server_endpoint_to_vehicle(node->name, nullptr);
		if (!node->master.endpoint.get()) {
			return -EINVALID;
		}
		node->master.identity = node->master.endpoint->_identity;

		node->slaver.endpoint = 
			_server_mgr->alloc_server_endpoint_to_vehicle(node->name,
				node->master.endpoint);
		if (node->slaver.endpoint.get()) {
			node->slaver.identity = node->slaver.endpoint->_identity;
		}
	} else if (!maste_invail && slave_invail) {
		node->slaver.endpoint = 
			_server_mgr->alloc_server_endpoint_to_vehicle(node->name,
				node->master.endpoint);
		if (node->slaver.endpoint.get()) {
			node->slaver.identity = node->slaver.endpoint->_identity;
		}
	} else if (maste_invail && !slave_invail) {
		node->master.endpoint = 
			_server_mgr->alloc_server_endpoint_to_vehicle(node->name,
				node->slaver.endpoint);
		if (!node->master.endpoint.get()) {
			node->master.endpoint = node->slaver.endpoint;
			node->master.identity = node->master.endpoint->_identity;
			node->slaver.endpoint = nullptr;
			node->slaver.identity.clear();
		} else {
			node->master.identity = node->master.endpoint->_identity;
		}
	} else {

	}
	return 0;
}

int forward_arbitrate::init_client_endpoint(std::string &url)
{
	auto* backend = forward_backend::inst();
	assert(nullptr != backend);
	_context = backend->create_context();
	assert(_context != nullptr);
	size_t port = INDEXING_CLIENT_DEFAULT_PORT;
	port = get_sysconfig("indexing-service.client.port", port);
	std::string ipaddr;
	ipaddr = get_sysconfig("indexing-service.client.ipaddr", ipaddr.c_str());
	url = "tcp://";
	url += ipaddr + ":";
	url += std::to_string(port);
	log.d(INDEXING_FORWRD_TAG,
		"forward arbitrate client uri : %s\n",
		url.c_str());
	_client = backend->connect(_context, url.c_str(),
		webapp_socket_worker);
	assert(nullptr != _client);
	return 0;
}

int forward_arbitrate::init_server_endpoint(std::string &url)
{
	auto* backend = forward_backend::inst();
	assert(nullptr != backend);
	_context = backend->create_context();
	assert(_context != nullptr);
	size_t port = INDEXING_SERVER_ENDPOINT_DEFAULT_PORT;
	port = get_sysconfig("indexing-service.server.port", port);
	url = "tcp://*:";
	url += std::to_string(port);
	log.d(INDEXING_FORWRD_TAG, 
		"forward arbitrate server uri : %s\n",
		url.c_str());
	_server = backend->bind(_context, url.c_str(),
		webapp_socket_router);
	assert(nullptr != _server);
	return 0;
}

int forward_service_node::forward_service_node_compare(avl_node_t* a,
	avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(forward_service_node, avlnode, a);
	auto* bb = AVLNODE_ENTRY(forward_service_node, avlnode, b);
	int ret = aa->node_id.compare(bb->node_id);
	if (ret > 0) {
		return 1;
	} else if (ret < 0) {
		return -1;
	} else return 0;
}

int forward_arbitrate::remove_forward_service_node(
	forwork_service_item* item,
	std::string &node_id)
{
	assert(nullptr != item);
	auto* node = find_forward_service_node(item, node_id);
	if (!node) {
		return 0;
	}
	avl_remove(&item->forward_node_tree, &node->avlnode);
	listnode_del(node->ownerlist);
	node->master.endpoint = nullptr;
	node->slaver.endpoint = nullptr;
	delete node;
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
			log.e(INDEXING_FORWRD_TAG,
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
		_server_mgr->add_server_endpoint(s_info->buf, identify.c_str(), s_info->veh_cnt);
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
		log.e(INDEXING_FORWRD_TAG, 
			"get url error\n");
		return -EBADPARM;
	}
	auto* rule = find_rule(url_name);
	if (!rule) {
		log.e(INDEXING_FORWRD_TAG, 
			"no forward rule %s\n", url_name.c_str());
		return -ENOTFOUND;
	}

	int interval_svc = 0;
	auto* nd = rule->forward_service_list.next;
	for (; nd != &(rule->forward_service_list); nd = nd->next) {
		auto* item = LIST_ENTRY(forwork_service_item, ownerlist, nd);
		assert(nullptr != item);
		if (item->type == 0 || (item->type | service_type_internal)) {
			interval_svc++;
		}
		if (!(item->type & service_type_forward)) {
			continue;
		}

		std::string uid = url.query_value(item->keyword);
		if (uid.length() < 1) {
			log.e(INDEXING_FORWRD_TAG, "no uid in message\n");
			continue;
		}
		auto* node = find_forward_service_node(item, uid);
		if (!node) {
			node = add_forward_service_node(item, uid);
			if (!node) {
				log.e(INDEXING_FORWRD_TAG, 
					"transfer node creat error\n");
				continue;
			}
		}
		if (check_set_service_node(node)) {
			// log.e(INDEXING_FORWRD_TAG, 
			// 	"no transfer sever node\n");
			continue;
		}
		if (item->inter_change != 0) {
			handle_internal_change(item, data, node);
		} else {
			forward_backend::inst()->forward_to_service(data, node);
		}
	}
	return interval_svc;
}

int forward_arbitrate::handle_internal_change(forwork_service_item* item,
	webapp_zeromq_msg* data, forward_service_node* node)
{
	assert(nullptr != item);
	if (!item->name.compare("snapshot")) {
		auto* cpdata = create_webapp_zeromq_msg(data);
		assert(nullptr != cpdata);
		std::string addr = cpdata->body(0);
		assert(0 < addr.length());
		auto* frame_uri = (basicinfo_frame_uri*)(addr.c_str());
		auto* frame_content = (basicinfo_frame_content*)(addr.c_str() +
		sizeof(basicinfo_frame_uri) + frame_uri->uri_length);
		std::string addr_uri(frame_uri->uri, frame_uri->uri_length);
		uri addr_info(addr_uri);
		std::string uid = addr_info.query_value(item->keyword);

		std::string vincode;
		vehicle_mgr::inst()->get_vehicle_id(uid, vincode);
		addr_info.change_query(item->keyword.c_str(), vincode.c_str());
		addr_uri = addr_info.tostring();

		size_t hdr_sz = sizeof(basicinfo_frame_uri)
			+ sizeof(basicinfo_frame_content) + addr_uri.length();
		uint8_t* basic_info = (uint8_t*)(alloca(hdr_sz));
		auto* basic_uri = reinterpret_cast<basicinfo_frame_uri*>(basic_info);
		basic_uri->uri_length = addr_uri.length();
		if (nullptr == basic_uri->uri || addr_uri.length() == 0) {
			return -EBADPARM;
		}
		strncpy(basic_uri->uri, addr_uri.c_str(), addr_uri.length());
		auto* basic_content = reinterpret_cast<basicinfo_frame_content*>(
			basic_info + sizeof(basicinfo_frame_uri) + addr_uri.length());
		memcpy(basic_content, frame_content, sizeof(basicinfo_frame_content));

		cpdata->set_part(cpdata->parts() - cpdata->body_parts(),
			(char*)basic_info, hdr_sz);

		forward_backend::inst()->forward_to_service(cpdata, node);
		release_webapp_zeromq_msg(cpdata);
	} else {
		forward_backend::inst()->forward_to_service(data, node);
	}
	return 0;
}

}}	// zas::vehicle_indexing
