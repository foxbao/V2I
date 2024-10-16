#include "current-snapshot.h"
#include "proto/snapshot_package.pb.h"
#include "proto/vehicle_snapshot.pb.h"
#include "webcore/logger.h"
#include "webcore/webapp.h"
#include "indexing-rabbitmq.h"
#include "forward.h"
#include "utils/uri.h"
#include "vehicle-mgr.h"
#include "mapcore/sdmap.h"
#include "webcore/sysconfig.h"

namespace zas {
namespace vehicle_indexing {

using namespace zas::utils;
using namespace zas::webcore;
using namespace zas::mapcore;
using namespace vss;

current_snapshot::current_snapshot(forward_backend* backend, void* context)
: _curr_snapshot_tree(nullptr)
, _backend(backend)
, _context(context)
, _rabbitmq(nullptr)
, _update_timer(nullptr)
, _request_vin_tree(nullptr)
, _map(nullptr)
{
	listnode_init(_curr_snapshot_list);
	listnode_init(_request_vin_list);
}

current_snapshot::~current_snapshot()
{
	remove_all_request();
	remove_all_snapshot();
	if (_rabbitmq) {
		delete _rabbitmq;
		_rabbitmq = nullptr;
	}
	if (_map) {
		delete _map;
		_map = nullptr;
	}
}

int current_snapshot::on_recv(std::string &key, uri &url, void* data, size_t sz)
{
	assert(nullptr != data);
	std::string uid = url.query_value(key.c_str());
	std::string vid;
	if (vehicle_mgr::inst()->get_vehicle_id(uid, vid)) {
		log.e(INDEXING_SNAPSHOT_TAG, "update snapshot uid error\n");
		return -ELOGIC;
	}
	if (update_snapshot(vid, data, sz)) {
		return -ELOGIC;
	}

	return 0;
}

int current_snapshot::init(void)
{
	if (!_rabbitmq) {
		assert(nullptr != _backend);
		_rabbitmq = new indexing_rabbitmq(rabbitmq_role_producing, "snapshot",
			_backend, _context, _backend->get_timermgr(_context));
		_rabbitmq->init();
	}
	if (!_update_timer) {
		assert(nullptr != _backend);
		auto* tmrmgr = _backend->get_timermgr(_context);
		assert(nullptr != tmrmgr);
		_update_timer = new snapshot_update_timer(tmrmgr, 1000, this);
		assert(nullptr != _update_timer);
		_update_timer->start();
	}
	
	if (!_map) {
		_map = new sdmap();
		assert(nullptr != _map);
		std::string mapfilename = "/zassys/sysapp/others/fullmap";
		mapfilename = get_sysconfig("indexing-service.mapdata",
			mapfilename.c_str());
		_map->load_fromfile(mapfilename.c_str());
	}

	return 0;
}

int current_snapshot::request_snapshot(std::string vid, bool subscribe)
{
	if (subscribe) {
		if (add_request_item(vid)){
			return 0;
		}
		return -ELOGIC;
	} else {
		return remove_request_item(vid);
	}

	return 0;
}

int current_snapshot::update_snapshot(std::string &uid, void* data, size_t sz)
{
	if (uid.length() == 0) {
		log.e(INDEXING_SNAPSHOT_TAG, "snapshot no uid\n");
		return -EBADPARM;
	}
	if (!data || 0 == sz) {
		return -EBADPARM;
	}
	auto* item = find_snapshot(uid);
	if (!item) {
		item = new vehicle_snapshot_item();
		assert(nullptr != item);
		item->uid = uid;
		if(avl_insert(&_curr_snapshot_tree, &item->avlnode,
			vehicle_snapshot_item::veh_ss_avl_compare)) {
			delete item;
			return -ELOGIC;
		}
		item->curr_snapshot = new vsnapshot_package();
		listnode_add(_curr_snapshot_list, item->ownerlist);
	}
	assert(nullptr != item->curr_snapshot);
	item->curr_snapshot->Clear();
	item->curr_snapshot->ParseFromArray(data, sz);
	timeval tv;
	tv.tv_sec = item->curr_snapshot->timestamp_sec();
	tv.tv_usec = item->curr_snapshot->timestamp_usec();
	tm t;
	localtime_r(&tv.tv_sec, &t);
	printf("data time %d/%02d/%02d %02d:%02d:%02d:%06ld\n",
		t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
		t.tm_hour, t.tm_min, t.tm_sec,
		tv.tv_usec);
	printf("snapshot distance %f m, speed %f km/h\n",
		item->curr_snapshot->distance(), item->curr_snapshot->vehicle_speed());
	printf("snapshot gps latitude %.8f, longtitude %.8f, heading %f\n",
		item->curr_snapshot->gpsinfo().latitude(),
		item->curr_snapshot->gpsinfo().longtitude(), 
		item->curr_snapshot->gpsinfo().heading());
	printf("snapshot acc_pedal_pos  %f, brake_pedal %f, steering_wheel %f\n",
		item->curr_snapshot->acc_pedal_pos_percentage(),
		item->curr_snapshot->brake_pedal_pos_percentage(),
		item->curr_snapshot->steering_wheel_angle()
		);
	printf("snapshot vehicle_state %d, flags %d, shift_lever_position %d\n",
		item->curr_snapshot->vehicle_state(),
		item->curr_snapshot->flags(),
		item->curr_snapshot->shift_lever_position()
		);
	// printf("uid is %s\n", uid.c_str());
	if (find_request_item(uid)) {
		sdmap_nearest_link link;
		assert(nullptr != _map);
		auto& gps = item->curr_snapshot->gpsinfo();
		int ret = _map->search_nearest_links(gps.latitude(),
			gps.longtitude(), &link);
		if (ret) {
			printf("error %d\n", ret);
			return 1;
		}
		assert(nullptr != link.link);
		auto* way = link.link->get_way();
		assert(nullptr != way);
		vehicle_snapshot vsnapshot;
		if (way->get_name()) {
			vsnapshot.set_road_name(way->get_name());
		}
		vsnapshot.set_deviation(link.distance);
		auto* pkg = vsnapshot.mutable_package();
		*pkg = *(item->curr_snapshot);
		std::string cdata;
		vsnapshot.SerializeToString(&cdata);
		update_to_center(uid, (void*)cdata.c_str(), cdata.length());
	}
	return 0;
}

int current_snapshot::update_to_center(std::string &uid, void* data, size_t sz)
{
	if (uid.length() == 0) {
		log.e(INDEXING_SNAPSHOT_TAG, "snapshot no uid\n");
		return -EBADPARM;
	}
	if (!data || 0 == sz) {
		return -EBADPARM;
	}
	std::string routekey = "vehicle_detail_";
	routekey += uid;
	_rabbitmq->publishing("vehicle_information", routekey, data, sz);

	return 0;
}

int current_snapshot::kafuka_update_snapshot(void)
{

	return 0;
}

vehicle_snapshot_item* 
current_snapshot::find_snapshot(std::string &uid)
{
	auto* ret = avl_find(_curr_snapshot_tree, 
		MAKE_FIND_OBJECT(uid, vehicle_snapshot_item, uid, avlnode),
		vehicle_snapshot_item::veh_ss_avl_compare);
	if (nullptr == ret) {
		return nullptr;
	}
	return AVLNODE_ENTRY(vehicle_snapshot_item, avlnode, ret);
}

int current_snapshot::remove_snapshot(std::string &uid)
{
	auto* item = find_snapshot(uid);
	if (!item) {
		return -ENOTFOUND;
	}

	avl_remove(&_curr_snapshot_tree, &item->avlnode);
	listnode_del(item->ownerlist);
	if (item->curr_snapshot) {
		delete item->curr_snapshot;
		item->curr_snapshot = nullptr;
	}
	delete item;
	return 0;
}

int current_snapshot::remove_all_snapshot(void)
{
	while (!listnode_isempty(_curr_snapshot_list)) {
		auto* item = LIST_ENTRY(vehicle_snapshot_item,	\
			ownerlist,_curr_snapshot_list.next);
		if (!item) {
			return -ELOGIC;
		}
		listnode_del(item->ownerlist);
		if (item->curr_snapshot) {
			delete item->curr_snapshot;
			item->curr_snapshot = nullptr;
		}
		delete item;
	}
	_curr_snapshot_tree = nullptr;
	return 0;
}

int vehicle_snapshot_item::veh_ss_avl_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(vehicle_snapshot_item, avlnode, a);
	auto* bb = AVLNODE_ENTRY(vehicle_snapshot_item, avlnode, b);
	int ret = aa->uid.compare(bb->uid);
	if (ret > 0) { return 1; }
	else if (ret < 0) { return -1; }
	else { return 0; }
	return 0;
}

request_snapshot_item* current_snapshot::add_request_item(std::string &vid)
{
	auto* item = find_request_item(vid);
	if (item) {
		item->refcnt++;
		return item;
	}
	item  = new request_snapshot_item();
	item->refcnt = 1;
	item->vid = vid;
	if (avl_insert(&_request_vin_tree, &item->avlnode,
		request_snapshot_item::request_ss_avl_compare)) {
		delete item;
		log.e(INDEXING_SNAPSHOT_TAG, "update requeset snapshot insert error\n");
		return nullptr;
	}
	listnode_add(_request_vin_list, item->ownerlist);
	return item;
}

request_snapshot_item* current_snapshot::find_request_item(std::string &vid)
{
	auto* ret = avl_find(_request_vin_tree, 
		MAKE_FIND_OBJECT(vid, request_snapshot_item, vid, avlnode),
		request_snapshot_item::request_ss_avl_compare);
	if (nullptr == ret) {
		return nullptr;
	}
	return AVLNODE_ENTRY(request_snapshot_item, avlnode, ret);
}

int current_snapshot::remove_request_item(std::string &vid)
{
	auto* item = find_request_item(vid);
	if (!item) {
		return 0;
	}
	item->refcnt--;
	if (item->refcnt > 0) {
		return 0;
	}
	avl_remove(&_request_vin_tree, &item->avlnode);
	listnode_del(item->ownerlist);
	delete item;
	log.w(INDEXING_SNAPSHOT_TAG, "delete vhicle item\n");
	return 0;
}

int current_snapshot::remove_all_request()
{
	while (!listnode_isempty(_request_vin_list)) {
		auto* item = LIST_ENTRY(request_snapshot_item,	\
			ownerlist, _request_vin_list.next);
		listnode_del(item->ownerlist);
		delete item;
	}
	_request_vin_tree = nullptr;
	return 0;
}

int request_snapshot_item::request_ss_avl_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(request_snapshot_item, avlnode, a);
	auto* bb = AVLNODE_ENTRY(request_snapshot_item, avlnode, b);
	int ret = aa->vid.compare(bb->vid);
	if (ret > 0) { return 1; }
	else if (ret < 0) { return -1; }
	else { return 0; }
	return 0;
}

}}	//zas::vehicle_indexing




