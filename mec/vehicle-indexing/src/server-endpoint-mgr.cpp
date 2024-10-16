
#include "indexing-def.h"
#include "server-endpoint-mgr.h"

#include "webcore/sysconfig.h"
#include "webcore/logger.h"

namespace zas {
namespace vehicle_indexing {

using namespace zas::webcore;

server_endpoint_mgr::server_endpoint_mgr()
: _server_tree(nullptr)
{
	listnode_init(_server_list);
}

server_endpoint_mgr::~server_endpoint_mgr()
{
	release_all_nodes();
}

server_item* server_endpoint_mgr::add_server_item(std::string &name)
{
	auto* item = find_server_item(name);
	if (item) {
		return item;
	}
	item = new server_item();
	item->_name = name;
	if (init_server_info(item)) {
		delete item;
		return nullptr;
	}
	if (avl_insert(&_server_tree, &item->avlnode,
		server_item::server_item_compare)) {
		log.e(INDEXING_FORWRD_TAG, "insert server error\n");
		delete item;
		return nullptr;
	}
	listnode_add(_server_list, item->ownerlist);
	return item;
}

server_item* server_endpoint_mgr::find_server_item(std::string &name)
{
	avl_node_t* anode = avl_find(_server_tree, 
		MAKE_FIND_OBJECT(name, server_item, _name, avlnode),
		server_item::server_item_compare);
	if (!anode)
		return nullptr;
	return AVLNODE_ENTRY(server_item, avlnode, anode);
}

int server_item::server_item_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(server_item, avlnode, a);
	auto* bb = AVLNODE_ENTRY(server_item, avlnode, b);
	int ret = aa->_name.compare(bb->_name);
	if (ret > 0) {
		return 1;
	} else if (ret < 0) {
		return -1;
	} else return 0;
}

int server_endpoint_mgr::init_server_info(server_item* item)
{
	std::string rootpath = "indexing-service.service.";
	rootpath += item->_name;
	rootpath += ".";
	std::string path = rootpath + "name";
	std::string url_name;
	int ret = 0;
	url_name = get_sysconfig(path.c_str(), url_name.c_str(), &ret);
	if (ret) {
		log.e(INDEXING_FORWRD_TAG, "server not in config\n");
		return -ENOTEXISTS;
	}

	item->_endpoints_tree = nullptr;
	listnode_init(item->_endpoints_list);

	path = rootpath + "max-vehicle-count";
	int max_cnt = INDEXING_SERVER_ENDPOINT_VEH_MAX_CNT;
	item->_veh_max_cnt = get_sysconfig(path.c_str(), max_cnt, &ret);

	path = rootpath + "keepalive.internal";
	size_t interval = HEARTBEAT_INTERVAL_DEFAULT;
	interval = get_sysconfig(path.c_str(), interval, &ret);

	path = rootpath + "keepalive.liveness";
	size_t liveness = HEARTBEAT_INTERVAL_DEFAULT;
	interval = get_sysconfig(path.c_str(), interval, &ret);

	log.d(INDEXING_FORWRD_TAG, 
		"liveness is %ld, interval is %ld\n", liveness, interval);
	item->_expiry = liveness * interval;
	return 0;
}
server_endpoint* server_endpoint_mgr::add_endpoint(server_item* item,
	std::string &identity, size_t veh_cnt)
{
	if (!item) {
		return nullptr;
	}
	auto* node = find_endpoint(item, identity);
	if (node) {
		return node;
	}

	node = new server_endpoint();
	node->_server = item;
	node->_identity = identity;
	node->_veh_cnt = veh_cnt;
	node->_refcnt = 1;
	node->_status = 1;
	if (avl_insert(&item->_endpoints_tree, &node->avlnode,
		server_endpoint::server_endpoint_compare)) {
		log.e(INDEXING_FORWRD_TAG, "insert server error\n");
		delete node;
		return nullptr;
	}
	listnode_add(item->_endpoints_list, node->ownerlist);
	return node;
}

server_endpoint* server_endpoint_mgr::find_endpoint(server_item* item,
	std::string &identity)
{
	avl_node_t* anode = avl_find(item->_endpoints_tree, 
		MAKE_FIND_OBJECT(identity, server_endpoint, _identity, avlnode),
		server_endpoint::server_endpoint_compare);
	if (!anode)
		return nullptr;
	return AVLNODE_ENTRY(server_endpoint, avlnode, anode);
}

int server_endpoint::addref(void)
{
	return _refcnt++;
}

int server_endpoint::release(void)
{
	int cnt = _refcnt--;
	if (0 == cnt) {
		delete this;
	}
	return cnt;
}

int server_endpoint_mgr::remove_endpoint(server_item* item,
	std::string &identity)
{
	auto* node = find_endpoint(item, identity);
	if (!node) {
		return -ENOTFOUND;
	}
	avl_remove(&item->_endpoints_tree, &node->avlnode);
	listnode_del(node->ownerlist);
	node->_server = nullptr;
	node->release();
	return 0;
}

int server_endpoint::server_endpoint_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(server_endpoint, avlnode, a);
	auto* bb = AVLNODE_ENTRY(server_endpoint, avlnode, b);
	int ret = aa->_identity.compare(bb->_identity);
	if (ret > 0) {
		return 1;
	} else if (ret < 0) {
		return -1;
	} else return 0;
}

int server_endpoint_mgr::remove_server_item(server_item* item)
{
	if (!item) {
		return -EBADPARM;
	}

	while (!listnode_isempty(item->_endpoints_list)) {
		auto* node = LIST_ENTRY(server_endpoint,	\
			ownerlist, item->_endpoints_list.next);
		assert(nullptr != node);
		avl_remove(&item->_endpoints_tree, &node->avlnode);
		listnode_del(node->ownerlist);
		node->_server = nullptr;
		node->release();
	}
	item->_endpoints_tree = nullptr;

	avl_remove(&_server_tree, &item->avlnode);
	listnode_del(item->ownerlist);
	delete item;
	return 0;
}

int server_endpoint_mgr::release_all_nodes(void)
{
	while (listnode_isempty(_server_list)) {
		auto* item = LIST_ENTRY(server_item,	\
			ownerlist, _server_list.next);
		assert(nullptr != item);
		remove_server_item(item);
	}
	_server_tree = nullptr;
	return 0;
}


server_endpoint* server_endpoint_mgr::add_server_endpoint(const char* svr_name,
	const char* identity, size_t veh_cnt)
{
	if (!svr_name || !*svr_name || !identity || !*identity) {
		return nullptr;
	}

	std::string name = svr_name;
	auto* item = find_server_item(name);
	if (!item) {
		item = add_server_item(name);
	}
	assert(nullptr != item);
	
	std::string idt = identity;
	auto* node = find_endpoint(item, idt);
	if (node) {
		return node;
	}
	return add_endpoint(item, idt, veh_cnt);
}

server_endpoint* server_endpoint_mgr::find_server_endpoint(const char* svr_name,
	const char* identity)
{
	if (!svr_name || !*svr_name || !identity || !*identity) {
		return nullptr;
	}

	std::string name = svr_name;
	auto* item = find_server_item(name);
	if (!item) {
		return nullptr;
	}
	
	std::string idt = identity;
	auto* node = find_endpoint(item, idt);
	return node;
}

server_endpoint_node server_endpoint_mgr::alloc_server_endpoint_to_vehicle(
	std::string &svr_name,
	server_endpoint_node master)
{
	auto *item = find_server_item(svr_name);
	if (!item) {
		return nullptr;
	}

	if (listnode_isempty(item->_endpoints_list)) {
		return nullptr;
	}
	
	listnode_t* nd = item->_endpoints_list.next;
	server_endpoint* alloc_point = nullptr;
	uint32_t min_veh_cnt = item->_veh_max_cnt;
	auto* node = master.get();
	for (; nd != &item->_endpoints_list; nd = nd->next) {
		auto* point = list_entry(server_endpoint, ownerlist, nd);
		if (point->_status != 1) {
			continue;
		}
		if (point->_veh_cnt < min_veh_cnt) {
			if (node && node == point) {
				continue;
			}
			min_veh_cnt = point->_veh_cnt;
			alloc_point = point;
		}
	}
	if (alloc_point) {
		alloc_point->_veh_cnt++;
	}
	server_endpoint_node alloc_node(alloc_point);
	return alloc_node;
}
	

}}	//zas::vehicle_indexing
