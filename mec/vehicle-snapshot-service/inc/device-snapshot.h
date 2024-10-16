#ifndef __CXX_SNAPSHOT_SERVICE_DEVICE_SNAPSHOT_H__
#define __CXX_SNAPSHOT_SERVICE_DEVICE_SNAPSHOT_H__

#include <string.h>
#include "snapshot-service-def.h"
#include "std/list.h"
#include "utils/avltree.h"
#include "utils/timer.h"
#include "utils/uri.h"
#include "utils/mutex.h"

#include "mapcore/mapcore.h"
#include "mapcore/hdmap.h"
#include "fusion-improver.h"
#include "kafka-consumer.h"

#include "proto/junction_package.pb.h"
#include "proto/fusion_service_pkg.pb.h"
#include "proto/distribute_package.pb.h"
#include "proto/junction_fusion_package.pb.h"

namespace zas {
namespace vehicle_snapshot_service {

#define DEVICE_SNAPSHOT_CACHER (4096)
#define JUNCTION_COUNT_CYCLE_TIME (1000)

using namespace zas::utils;
using namespace jos;

class device_wa_request;
class fusion_data_update;
class vehicle_snapshot_item;
class snapshot_rabbitmq;

struct request_jd_item
{
	listnode_t ownerlist;
	avl_node_t avlnode;
	std::string id;
	int refcnt;
	static int request_jd_avl_compare(avl_node_t*, avl_node_t*);
};

struct device_info
{
	listnode_t ownerlist;
	avl_node_t avlnode;
	std::string dev_id;
	uint32_t index;
	bool inited;
	jos::radar_vision_info info[DEVICE_SNAPSHOT_CACHER];
	static int device_avl_compare(avl_node_t*, avl_node_t*);
};

struct junction_info {
	std::string id;
	double lat;
	double lon;
	double radius;
};

struct device_junction_item
{
	listnode_t ownerlist;
	listnode_t veh_list;
	listnode_t veh_prev_list;
	avl_node_t avlnode;
	listnode_t device_list;
	avl_node_t* device_tree;
	uint32_t index;
	junction_info jun_info;
	jos::fusion_package info[DEVICE_SNAPSHOT_CACHER];
	spfusion_improver improver;
	spfusion_juncion_info fjunc;
	static int device_junction_avl_compare(avl_node_t*, avl_node_t*);
};

class device_snapshot
{
public:
	device_snapshot();
	virtual ~device_snapshot();

	static device_snapshot* inst();

	int on_recv(std::string &vincode, uri &info, void* data, size_t sz);
	snapshot_rabbitmq* get_rabbitmq(void);

public:
	int init();
	// subscribe junciton/device by rabbitmq
	int request_junction(std::string &id, bool subscribe);
	int request_device(std::string &id, bool subscribe);
	int remove_all_request(void);

	// send to center kafka
	int send_junction_to_kafka(device_junction_item* item);

	// accept fusion result
	int on_request_reply(device_wa_request* wa_req,
		void* context, void* data, size_t sz);

	int update_fusion_data(device_junction_item *item, device_info* radar_info);

	// for vehicle snapshot
	int get_traffice_info(vehicle_snapshot_item* item,
		distribute_package &dist_pkg);
	int add_vehicle_to_junction(vehicle_snapshot_item* item);

private:
	int load_junction_info(spfusion_juncion_info info);
	int load_junction_count_info(spfusion_juncion_info info);
	int load_count_incomming_road(std::string &junc_path, spcount_junction info);
	int load_count_outgoing_road(std::string &junc_path, spcount_junction info);

	// update device snapshot
	int update_snapshot(std::string &vincode, uri &info, void* data, size_t sz);
	device_junction_item* check_create_junction_item(std::string &uid);
	device_info* check_create_device_item(device_junction_item *item,
		std::string &uid);
	int add_device_info(device_info* device,
		const jos::radar_vision_info &info);

	device_junction_item* find_junction(std::string &uid);
	device_info* find_device(device_junction_item *item, std::string &uid);
	int remove_device(device_junction_item *item, std::string &uid);
	int remove_all_snapshot(void);

	request_jd_item* add_request_item(avl_node_t** root, std::string &id);
	request_jd_item* find_request_item(avl_node_t** root, std::string &id);
	int remove_request_item(avl_node_t** root, std::string &id);

	// send info to center by rabbitmq
	int send_subscribe_device_info(const jos::radar_vision_info &info);
	int send_subscribe_junction_info(device_junction_item* jitem,
		jos::fusion_package& pkg);
	int update_device_to_center(std::string &id, void* data, size_t sz);
	int update_junction_to_center(std::string &id, void* data, size_t sz);

	int generate_veh_frame(device_junction_item *item, 
		junction_fusion_package &pkg, uint64_t endtime);
	// send device&vehicle to fusion_servcie
	int send_to_fusion_service(std::string &junc_id, 
		const uint8_t* data, size_t sz);

	// send fusion junciton info to vehicle
	int send_junciton_to_vehicle(device_junction_item* item);

private:
	union {
		struct {
			uint32_t init_device_ss : 1;
		} _f;
		uint32_t _flags;
	};

private:
	listnode_t _device_junction_list;
	avl_node_t* _device_junction_tree;
	device_wa_request*	_device_sender;
	avl_node_t* _request_jucntion_tree;
	avl_node_t* _request_device_tree;
	snapshot_rabbitmq* _rabbitmq;
	vss_kafka_consumer* _kafka_consumer;

	zas::mapcore::rendermap*		_hdmap;
	zas::mapcore::proj*		_proj;
	zas::mapcore::point3d 	_projoffset;
	mutex	_mut;
};

}}	//zas::vehicle_snapshot_service

#endif /* __CXX_SNAPSHOT_SERVICE_DEVICE_SNAPSHOT_H__*/