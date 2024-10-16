#ifndef __CXX_VEHICLE_MGR_H__
#define __CXX_VEHICLE_MGR_H__

#include <string.h>
#include "indexing-def.h"
#include "utils/timer.h"
#include "utils/uri.h"

namespace vss {
	class snapshot_request;
};

namespace zas {
namespace vehicle_indexing {

using namespace zas::utils;

struct vehicle_item
{
	listnode_t ownerlist;
	avl_node_t avlnode;
	avl_node_t avlvinnode;
	std::string uid;
	std::string vid;
	std::string acctoken;
	static int vehicle_uid_compare(avl_node_t*, avl_node_t*);
	static int vehicle_vin_compare(avl_node_t*, avl_node_t*);
};

class vehicle_mgr
{
public:
	vehicle_mgr();
	virtual ~vehicle_mgr();
	static vehicle_mgr* inst();
	int vehicle_register(std::string &key, uri &url, void* data, size_t sz);
	int vehicle_remove(std::string &key, uri &url, void* data, size_t sz);
	int get_vehicle_id(std::string& uid, std::string& vid);

private:
	vehicle_item* add_vehicle(std::string& vid, std::string& acc_token);
	vehicle_item* find_vehicle_uid(std::string& uid);
	vehicle_item* find_vehicle_vid(std::string &vid);
	int remove_vehicle_by_uid(std::string& uid);
	int remove_vehicle_by_vid(std::string &vid);
	int remove_all_vehicle(void);
	std::string alloc_uid(void);

private:
	listnode_t _vehicle_list;
	avl_node_t* _vehicle_vin_tree;
	avl_node_t* _vehicle_uid_tree;
};

}}	//zas::vehicle_indexing

#endif /* __CXX_CURRENT_SNAPSHOT_H__*/