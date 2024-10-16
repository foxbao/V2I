#include "client-mgr.h"
#include "webcore/logger.h"
#include "webcore/sysconfig.h"
#include "webcore/webapp.h"

namespace zas {
namespace load_balance {

using namespace zas::utils;
using namespace zas::webcore;

client_mgr::client_mgr()
: _client_tree(nullptr)
, _context(nullptr)
, _distribute(nullptr)
{
}

client_mgr::~client_mgr()
{
	remove_all_client();
}

client_mgr* client_mgr::inst()
{
	static client_mgr* _inst = NULL;
	if (_inst) return _inst;
	_inst = new client_mgr();
	assert(NULL != _inst);
	return _inst;
}

int client_mgr::init(void)
{
	auto* backend = forward_backend::inst();
	assert(nullptr != backend);
	_context = backend->create_context();
	assert(_context != nullptr);
	int port = LOAD_BALANCE_DISTRIBUTE_DEFAULT_PORT;
	port = get_sysconfig("load-balance-service.distribute.port", port);
	_port = std::to_string(port);

	_ipaddr.clear();
	char* env = getenv("POD_IP");
	if (!env) {
		_ipaddr = get_sysconfig("load-balance-service.distribute.ipaddr",
			_ipaddr.c_str());
	} else {
		_ipaddr = env;
	}
	printf("distribute ip is %s\n", _ipaddr.c_str());

	std::string url = "tcp://*:";
	url += _port;
	log.d(LOAD_BALANCE_FORWRD_TAG,
		"client mgr recv client uri : %s\n",
		url.c_str());
	_distribute = backend->bind(_context, url.c_str(),
		webapp_socket_router);
	backend->set_distribute_info(url.c_str(), webapp_socket_router);
	return 0;
}

int client_mgr::add_client(std::string& identity)
{
	auto* item = find_client(identity);
	if (item) {
		return 0;
	}
	item = new client_item();
	item->identity = identity;

	if (avl_insert(&_client_tree, &item->avlnode,
		client_item::client_identity_compare)) {
		delete item;
		return -ELOGIC;
	}
	
	return 0;
}

int client_mgr::remove_client(std::string& identity)
{
	auto* item = find_client(identity);
	if (!item) {
		return 0;
	}
	avl_remove(&_client_tree, &item->avlnode);
	delete item;
	return 0;
}

int client_mgr::remove_all_client(void)
{
	while(1) {
		avl_node_t* node = avl_first(_client_tree);
		if (nullptr == node) break;
		auto* item = AVLNODE_ENTRY(client_item, avlnode, node);
		avl_remove(&_client_tree, &item->avlnode);
		delete item;
	}
	_client_tree = nullptr;
	return 0;
}

int client_mgr::check_identity(std::string &identity)
{
	auto* item = find_client(identity);
	if (!item) {
		return -ENOTFOUND;
	}
	return 0;
}

const char* client_mgr::get_distribute_ipaddr(void)
{
	return _ipaddr.c_str();
}

const char* client_mgr::get_distribute_port(void)
{
	return _port.c_str();
}

client_item* 
client_mgr::find_client(std::string &identity)
{
	auto* ret = avl_find(_client_tree, 
		MAKE_FIND_OBJECT(identity, client_item, identity, avlnode),
		client_item::client_identity_compare);
	if (nullptr == ret) {
		return nullptr;
	}
	return AVLNODE_ENTRY(client_item, avlnode, ret);
}

int client_item::client_identity_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(client_item, avlnode, a);
	auto* bb = AVLNODE_ENTRY(client_item, avlnode, b);
	int ret = aa->identity.compare(bb->identity);
	if (ret > 0) { return 1; }
	else if (ret < 0) { return -1; }
	else { return 0; }
	return 0;
}

}}	//zas::load_balance




