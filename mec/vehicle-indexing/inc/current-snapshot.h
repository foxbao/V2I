#ifndef __CXX_CURRENT_SNAPSHOT_H__
#define __CXX_CURRENT_SNAPSHOT_H__

#include <string.h>
#include "indexing-def.h"
#include "utils/timer.h"
#include "utils/uri.h"

namespace vss {
	class vsnapshot_package;
};
namespace zas::mapcore {
	class sdmap;
};

namespace zas {
namespace vehicle_indexing {

using namespace zas::utils;
class forward_backend;
class indexing_rabbitmq;
class current_snapshot;

struct vehicle_snapshot_item
{
	listnode_t ownerlist;
	avl_node_t avlnode;
	std::string uid;
	vss::vsnapshot_package* curr_snapshot;
	static int veh_ss_avl_compare(avl_node_t*, avl_node_t*);
};

struct request_snapshot_item
{
	listnode_t ownerlist;
	avl_node_t avlnode;
	std::string vid;
	int refcnt;
	static int request_ss_avl_compare(avl_node_t*, avl_node_t*);
};

class current_snapshot
{
public:
	current_snapshot(forward_backend* backend, void* context);
	virtual ~current_snapshot();
	int on_recv(std::string &key, uri &url, void* data, size_t sz);

public:
	int init(void);
	int request_snapshot(std::string vid, bool subscribe);
	int remove_all_request();
	
private:
	class snapshot_update_timer : public timer
	{
	public:
		snapshot_update_timer(timermgr* mgr, uint32_t interval,
			current_snapshot* curr_snapshot)
		: timer(mgr, interval)
		, _curr_snapshot(curr_snapshot) {
		}

		void on_timer(void) {
			assert(nullptr != _curr_snapshot);
			//todo , for kafuka
			_curr_snapshot->kafuka_update_snapshot();
			start();
		}

	private:
		current_snapshot* _curr_snapshot;
		ZAS_DISABLE_EVIL_CONSTRUCTOR(snapshot_update_timer);
	};

private:
	int update_snapshot(std::string &uid, void* data, size_t sz);
	int update_to_center(std::string &uid, void* data, size_t sz);
	int kafuka_update_snapshot(void);
	vehicle_snapshot_item* find_snapshot(std::string &uid);
	int remove_snapshot(std::string &uid);
	int remove_all_snapshot(void);

	request_snapshot_item* add_request_item(std::string &vid);
	int remove_request_item(std::string &vid);
	request_snapshot_item* find_request_item(std::string &vid);

private:
	listnode_t _curr_snapshot_list;
	avl_node_t* _curr_snapshot_tree;

	listnode_t _request_vin_list;
	avl_node_t* _request_vin_tree;
	
	//for rabbitmq
	forward_backend* _backend;
	void* _context;
	indexing_rabbitmq* _rabbitmq;

	// for kafuka
	snapshot_update_timer* _update_timer;
	zas::mapcore::sdmap*		_map;
};

}}	//zas::vehicle_indexing

#endif /* __CXX_CURRENT_SNAPSHOT_H__*/