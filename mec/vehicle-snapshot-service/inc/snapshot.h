#ifndef __CXX_SNAPSHOT_SERVICE_SNAPSHOT_H__
#define __CXX_SNAPSHOT_SERVICE_SNAPSHOT_H__

#include <string.h>
#include "snapshot-service-def.h"
#include "std/list.h"
#include "utils/avltree.h"
#include "utils/timer.h"
#include "utils/uri.h"
#include "proto/vehicle_snapshot.pb.h"
#include "proto/distribute_package.pb.h"

namespace zas::mapcore {
	class sdmap;
};
namespace zas {
namespace vehicle_snapshot_service {

using namespace zas::utils;

#define VEHICLE_SNAPSHOT_HISTORY_MAX (20)

struct vehicle_snapshot_item
{
	listnode_t ownerlist;
	listnode_t linkownerlist;
	listnode_t junctionownerlist;
	listnode_t junprevownerlist;
	avl_node_t avlnode;
	std::string vid;
	std::string identity;
	std::string ipaddr;
	std::string port;
	uint32_t index;
	uint64_t recv_time;
	bool update;
	uint32_t leftime;
	vss::vehicle_snapshot snapshot[VEHICLE_SNAPSHOT_HISTORY_MAX];
	static int veh_ss_avl_compare(avl_node_t*, avl_node_t*);
};

// rabbitmq subscriber
struct request_snapshot_item
{
	listnode_t ownerlist;
	avl_node_t avlnode;
	std::string vid;
	int refcnt;
	static int request_ss_avl_compare(avl_node_t*, avl_node_t*);
};

class vss_snapshot
{
public:
	vss_snapshot();
	virtual ~vss_snapshot();

	static vss_snapshot* inst();

	int on_recv(std::string &vincode, uri &info, void* data, size_t sz);
	int request_snapshot(std::string vid, bool subscribe);
	int remove_all_request(void);

public:
	int init();
	int kafuka_send_snapshot(void);
	int send_to_vehicle(vehicle_snapshot_item* item,
		const char* data, size_t sz);
		
private:
	int update_to_junction(vehicle_snapshot_item* item);
	int update_traffic_to_vehilce(vehicle_snapshot_item* item);
	int update_snapshot(std::string &vincode, uri &info, void* data, size_t sz);
	vehicle_snapshot_item* find_snapshot(std::string &vid);

	int remove_snapshot(std::string &vid);
	int remove_all_snapshot(void);

	request_snapshot_item* add_request_item(std::string &vid);
	int remove_request_item(std::string &vid);
	request_snapshot_item* find_request_item(std::string &vid);
	
	int set_way_and_arround_vehilce(vehicle_snapshot_item* item,
		jos::distribute_package &dspkg);
	int set_around_vehicle(jos::distribute_package &dspkg, listnode_t *hd,
		vss::positioning_system_info& gps);

	int send_to_subscribe(vehicle_snapshot_item* item,
		const char* data, size_t sz);
	int send_vehicle_to_kafka(vehicle_snapshot_item* item);

private:
	listnode_t _vss_snapshot_list;
	avl_node_t* _vss_snapshot_tree;

	listnode_t _request_vin_list;
	avl_node_t* _request_vin_tree;

	zas::mapcore::sdmap*		_map;
};

}}	//zas::vehicle_snapshot_service

#endif /* __CXX_SNAPSHOT_SERVICE_SNAPSHOT_H__*/