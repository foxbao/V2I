#ifndef __CXX_SERVER_ENDPOINT_MGR_H__
#define __CXX_SERVER_ENDPOINT_MGR_H__

#include <string>
#include "std/smtptr.h"
#include "std/list.h"
#include "utils/avltree.h"

namespace zas {
namespace load_balance {

using namespace zas::utils;

struct server_item
{
	int addref();
	int release();
	listnode_t ownerlist;
	avl_node_t avlnode;
	std::string _name;		//server name
	size_t _max_cnt;
	int64_t _expiry;
	int _refcnt;
	listnode_t _endpoints_list;
	avl_node_t* _endpoints_tree;
	static int server_item_compare(avl_node_t* a, avl_node_t* b);
};

// server communicate endpoint
struct server_endpoint
{
	int addref();
	int release();
	listnode_t ownerlist;
	avl_node_t avlnode;
	std::string _identity;	//server endpoint unique id
	server_item* _server;		//server point
	size_t _count;		//veh count in this server
	int _refcnt;
	int _status;	// 0: server disable, 1: server enable
	static int server_endpoint_compare(avl_node_t* a, avl_node_t* b);
};

typedef zas::smtptr<server_endpoint> server_endpoint_node;
typedef zas::smtptr<server_item> server_item_node;

class server_endpoint_mgr
{
public:
	server_endpoint_mgr();
	virtual ~server_endpoint_mgr();

	server_endpoint* add_server_endpoint(const char* svr_name,
		const char* identity);
	server_endpoint* find_server_endpoint(const char* svr_name,
		const char* identity);
	server_endpoint_node get_service_endpoint(server_item *item);
	server_item_node get_service_item(std::string &svr_name);

private:
	//server item
	server_item* add_server_item(std::string &name);
	server_item* find_server_item(std::string &name);
	int init_server_info(server_item* item);

	//server endpoint
	server_endpoint* add_endpoint(server_item* item,
		std::string &identity);
	server_endpoint* find_endpoint(server_item* item,
		std::string &identity);
	int remove_endpoint(server_item* item, std::string &identity);

private:
	int remove_server_item(server_item* item);
	int release_all_nodes(void);

private:
	// service node info list
	listnode_t			_server_list;
	avl_node_t*			_server_tree;
};

}}	//	zas::vehicle_indexing

#endif /* __CXX_SERVER_ENDPOINT_MGR_H__*/