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

namespace vehicle_indexing {

#define service_type_forward	(1)
#define service_type_internal	(2)

using namespace zas::utils;
using namespace zas::webcore;

struct service_endpoint
{
	server_endpoint_node endpoint;
	std::string identity;
};

struct forward_service_node
{
	listnode_t ownerlist;
	avl_node_t avlnode;
	std::string name;	// service name
	std::string node_id;	// service node unique id(vin id)
	service_endpoint master;
	service_endpoint slaver;
	static int forward_service_node_compare(avl_node_t* a, avl_node_t* b);
};

struct forwork_service_item
{
	listnode_t ownerlist;
	avl_node_t avlnode;
	std::string name;	// service name
	std::string keyword;	//service keyword(access-token or uid)
	int type;
	int inter_change;
	avl_node_t* forward_node_tree;
	listnode_t forward_node_list;
	static int forward_service_item_compare(avl_node_t* a, avl_node_t* b);
};

struct route_rule_item
{
	listnode_t ownerlist;
	avl_node_t avlnode;
	std::string url_name;	// rule
	listnode_t forward_service_list;
	avl_node_t* forward_service_tree;
	static int route_rule_compare(avl_node_t* a, avl_node_t* b);
};

class forward_arbitrate : public forward_mgr_callback
{
public: 
	forward_arbitrate();
	~forward_arbitrate();

	int init(void);
	forwork_service_item* find_service(uri &url, std::string &svc_name);

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
	forwork_service_item* add_service(route_rule_item* item,
		std::string &svc_name);
	forwork_service_item* find_service(route_rule_item* item,
		std::string &svc_name);
	int load_service_type(forwork_service_item *item, int order);
	int load_service_keyword(forwork_service_item *item, int order);
	int load_service_change(forwork_service_item *item, int order);

	int init_client_endpoint(std::string &url);
	int init_server_endpoint(std::string &url);
	int release_all_service(void);

	forward_service_node* add_forward_service_node(forwork_service_item* item,
		std::string &node_id);
	forward_service_node* find_forward_service_node(forwork_service_item* item,
		std::string &node_id);
	int check_set_service_node(forward_service_node* node);
	int remove_forward_service_node(forwork_service_item* item,
		std::string &node_id);

	int handle_internal_change(forwork_service_item* item,
		webapp_zeromq_msg* data, forward_service_node* node);

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