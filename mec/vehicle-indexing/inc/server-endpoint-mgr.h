#ifndef __CXX_SERVER_ENDPOINT_MGR_H__
#define __CXX_SERVER_ENDPOINT_MGR_H__

#include <string>
#include "std/smtptr.h"
#include "std/list.h"
#include "utils/avltree.h"

namespace zas {
namespace vehicle_indexing {

using namespace zas::utils;

struct server_item
{
	listnode_t ownerlist;
	avl_node_t avlnode;
	std::string _name;		//server name
	size_t _veh_max_cnt;
	int64_t _expiry;	
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
	size_t _veh_cnt;		//veh count in this server
	int _refcnt;
	int _status;	// 0: server disable, 1: server enable
	static int server_endpoint_compare(avl_node_t* a, avl_node_t* b);
};

typedef zas::smtptr<server_endpoint> server_endpoint_node;

class server_endpoint_mgr
{
public:
	server_endpoint_mgr();
	virtual ~server_endpoint_mgr();

	server_endpoint* add_server_endpoint(const char* svr_name,
		const char* identity, size_t veh_cnt);
	server_endpoint* find_server_endpoint(const char* svr_name,
		const char* identity);
	server_endpoint_node alloc_server_endpoint_to_vehicle(std::string &svr_name,
		server_endpoint_node master);

private:
	//server item
	server_item* add_server_item(std::string &name);
	server_item* find_server_item(std::string &name);
	int init_server_info(server_item* item);

	//server endpoint
	server_endpoint* add_endpoint(server_item* item,
		std::string &identity, size_t veh_cnt);
	server_endpoint* find_endpoint(server_item* item,
		std::string &identity);
	int remove_endpoint(server_item* item, std::string &identity);

private:
	int remove_server_item(server_item* item);
	int release_all_nodes(void);

private:
	// snapshot node info list
	listnode_t			_server_list;
	avl_node_t*			_server_tree;
};

}}	//	zas::vehicle_indexing

#endif /* __CXX_SERVER_ENDPOINT_MGR_H__*/