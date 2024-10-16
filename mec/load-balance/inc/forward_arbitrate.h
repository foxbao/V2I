#ifndef __CXX_FORWARD_ARBITRATE_H__
#define __CXX_FORWARD_ARBITRATE_H__

#include <string>
#include "std/list.h"
#include "utils/avltree.h"
#include "utils/uri.h"
#include "forward.h"
#include "server-endpoint-mgr.h"

namespace zas {

namespace webcore {
class webapp_socket;
};

namespace load_balance {

#define service_type_forward	(1)
#define service_type_internal	(2)

using namespace zas::utils;
using namespace zas::webcore;

struct route_rule_item
{
	listnode_t ownerlist;
	avl_node_t avlnode;
	std::string url_name;	// rule
	std::string name;
	server_item_node service;
	static int route_rule_compare(avl_node_t* a, avl_node_t* b);
};

class forward_arbitrate : public forward_mgr_callback
{
public: 
	forward_arbitrate();
	~forward_arbitrate();

	int init(void);

public:
	int forward_to_service(webapp_zeromq_msg* data, uri &url);
	int service_request(zmq_backend_socket* wa_sock, webapp_zeromq_msg* data);

private:
	int init_rule_list(void);
	int get_urlname_from_uri(uri &url, std::string &url_name);
	route_rule_item* find_rule(std::string &url_name);
	route_rule_item* add_rule(std::string &url_name);
	int load_rule_url_name(std::string &name, int order);
	int load_service_name(std::string &name, int order);
	int load_service_type(route_rule_item*, int order);

	int init_client_endpoint(std::string &url);
	int init_server_endpoint(std::string &url);
	int release_all_service(void);

private:
	void* _context;
	webapp_socket* _client;
	webapp_socket* _server;

	avl_node_t* _rule_tree;
	listnode_t _rule_list;
	server_endpoint_mgr* _server_mgr;
};

}}

#endif /* __CXX_FORWARD_ARBITRATE_H__*/