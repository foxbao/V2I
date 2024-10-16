#include "snapshot.h"
#include "snapshot-service-def.h"
#include "device-snapshot.h"
#include "snapshot-rabbitmq.h"
#include "proto/snapshot_package.pb.h"
#include "proto/vehicle_snapshot.pb.h"
#include "proto/distribute_package.pb.h"
#include "proto/center_kafka_data.pb.h"

#include <sys/time.h>

#include "mapcore/sdmap.h"
#include "utils/uri.h"
#include "webcore/logger.h"
#include "webcore/webapp.h"
#include "service-worker.h"
#include "webcore/sysconfig.h"

namespace zas {
namespace vehicle_snapshot_service {

using namespace zas::utils;
using namespace zas::webcore;
using namespace zas::mapcore;
using namespace vss;
using namespace jos;

class vss_request : public wa_request
{
public:
	vss_request(uri& url)
	: wa_request(url){}
	~vss_request(){}
	int on_reply(void* context, void* data, size_t sz)
	{
		printf("device_wa_request size [%ld]: %s\n", sz, (char*)data);
		return 0;
	}

};

vss_snapshot::vss_snapshot()
: _vss_snapshot_tree(nullptr)
, _map(nullptr)
, _request_vin_tree(nullptr)
{
	listnode_init(_vss_snapshot_list);
	listnode_init(_request_vin_list);
}

vss_snapshot::~vss_snapshot()
{
	remove_all_snapshot();
	remove_all_request();
	if (_map) {
		delete _map;
		_map = nullptr;
	}
}

vss_snapshot* vss_snapshot::inst()
{
	static vss_snapshot* _inst = NULL;
	if (_inst) return _inst;
	_inst = new vss_snapshot();
	assert(NULL != _inst);
	return _inst;
}

int vss_snapshot::on_recv(std::string &vincode, uri &info, 
	void* data, size_t sz)
{
	assert(nullptr != data);

	if (update_snapshot(vincode, info, data, sz)) {
		return -ELOGIC;
	}
	return 0;
}

int vss_snapshot::init(void)
{
	service_backend::inst()->create_kafka_sender("kafka_center");
	if (!_map) {
		_map = new sdmap();
		assert(nullptr != _map);
		std::string mapfilename = "/zassys/sysapp/others/fullmap";
		mapfilename = get_sysconfig("snapshot-service.mapdata",
			mapfilename.c_str());
		_map->load_fromfile(mapfilename.c_str());
	}
	return 0;
}

int vss_snapshot::update_snapshot(std::string &vincode, uri &info, 
	void* data, size_t sz)
{
	if (vincode.length() == 0) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, "snapshot no uid\n");
		return -EBADPARM;
	}
	if (!data || 0 == sz) {
		return -EBADPARM;
	}
	auto* item = find_snapshot(vincode);
	if (!item) {
		item = new vehicle_snapshot_item();
		assert(nullptr != item);
		item->vid = vincode;
		item->identity = info.query_value("cli_identity");
		item->ipaddr = info.query_value("issued_ipaddr");
		item->port = info.query_value("issued_port");
		item->index = 0;
		item->update = false;
		item->recv_time = 0;
		item->leftime = VEHICLE_KAFKA_SEND_MIN_INTERVAL;
		if(avl_insert(&_vss_snapshot_tree, &item->avlnode,
			vehicle_snapshot_item::veh_ss_avl_compare)) {
			delete item;
			return -ELOGIC;
		}
		listnode_init(item->linkownerlist);
		listnode_init(item->junctionownerlist);
		listnode_init(item->junprevownerlist);
		listnode_add(_vss_snapshot_list, item->ownerlist);
	} else {
		item->index = (item->index + 1) % VEHICLE_SNAPSHOT_HISTORY_MAX;
	}
	item->update = true;
	item->identity = info.query_value("cli_identity");
	std::string rec_time = info.query_value("start_time");
	if (rec_time.length() > 0) {
		item->recv_time = stoull(rec_time.c_str(), nullptr, 10);
	} else {
		item->recv_time = 0;
	}
	item->snapshot[item->index].Clear();
	item->snapshot[item->index].set_vincode(vincode);
	auto* ss_pkg = item->snapshot[item->index].mutable_package();
	ss_pkg->ParseFromArray(data, sz);

	update_traffic_to_vehilce(item);
	update_to_junction(item);
	send_vehicle_to_kafka(item);
	return 0;
}

int vss_snapshot::update_to_junction(vehicle_snapshot_item* item)
{
	listnode_del(item->junctionownerlist);
	listnode_del(item->junprevownerlist);
	device_snapshot::inst()->add_vehicle_to_junction(item);
	return 0;
}

int vss_snapshot::update_traffic_to_vehilce(vehicle_snapshot_item* item)
{
	assert(nullptr != item);

	// send vehicle data
	distribute_package dspkg;
	timeval tv;
	gettimeofday(&tv, nullptr);
	uint64_t times = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	dspkg.set_timestamp(times);
	set_way_and_arround_vehilce(item, dspkg);
	
	// // set jucntion info
	// device_snapshot::inst()->get_traffice_info(item, dspkg);

	std::string senddata;
	dspkg.SerializeToString(&senddata);
	send_to_vehicle(item, senddata.c_str(), senddata.length());
	send_to_subscribe(item, senddata.c_str(), senddata.length());
	return 0;
}

int vss_snapshot::set_way_and_arround_vehilce(vehicle_snapshot_item* item,
	distribute_package &dspkg)
{
	assert(nullptr != item);
	assert(nullptr != _map);
	sdmap_nearest_link nearest_link;
	auto vsspkg = item->snapshot[item->index].package();
	auto gps = vsspkg.gpsinfo();
	int ret = _map->search_nearest_links(gps.latitude(),
		gps.longtitude(), &nearest_link);
		// timeval tv;
		// tv.tv_sec = vsspkg.timestamp_sec();
		// tv.tv_usec = vsspkg.timestamp_usec();
		// tm t;
		// localtime_r(&tv.tv_sec, &t);
		// printf("data time %d/%02d/%02d %02d:%02d:%02d:%06ld\n",
		// 	t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
		// 	t.tm_hour, t.tm_min, t.tm_sec,
		// 	tv.tv_usec);
		// printf("snapshot distance %f m, speed %f km/h\n",
		// 	vsspkg.distance(), vsspkg.vehicle_speed());
		// printf("snapshot gps latitude %.8f, longtitude %.8f, heading %f\n",
		// 	vsspkg.gpsinfo().latitude(),
		// 	vsspkg.gpsinfo().longtitude(), 
		// 	vsspkg.gpsinfo().heading());
		// printf("snapshot acc_pedal_pos  %f, brake_pedal %f, steering_wheel %f\n",
		// 	vsspkg.acc_pedal_pos_percentage(),
		// 	vsspkg.brake_pedal_pos_percentage(),
		// 	vsspkg.steering_wheel_angle()
		// 	);
		// printf("snapshot vehicle_state %d, flags %d, shift_lever_position %d\n",
		// 	vsspkg.vehicle_state(),
		// 	vsspkg.flags(),
		// 	vsspkg.shift_lever_position()
		// 	);
	if (!ret && nearest_link.link) {
		auto* curr_link = nearest_link.link;
		auto* way = curr_link->get_way();
		assert(nullptr != way);
		if (way->get_name()) {
			item->snapshot[item->index].set_road_name(way->get_name());
			// printf("snapshot vehicle road %s.\n", way->get_name());
		}
		// printf("snapshot vehicle  distance %.8lf\n", nearest_link.distance);
		item->snapshot[item->index].set_deviation(nearest_link.distance);
		auto* vsssnap = dspkg.mutable_vsspkg();
		*vsssnap = item->snapshot[item->index];
		auto* data = curr_link->get_userdata();
		if (!data) {
			// create userdata
			auto* list_root = new listnode_t;
			listnode_init(*list_root);
			data = (void*)list_root;
			curr_link->set_userdata(data);
		} 
		auto* listhd = reinterpret_cast<listnode_t*>(data);
		listnode_del(item->linkownerlist);
		set_around_vehicle(dspkg, listhd, gps);
		listnode_add(*listhd, item->linkownerlist);	

		auto* prevlink = curr_link->prev();
		if (prevlink) { 
			auto* nddata = prevlink->get_userdata();
			if (nddata) {
				auto* hdr = reinterpret_cast<listnode_t*>(nddata);
				set_around_vehicle(dspkg, hdr, gps);
			}
		}
		auto* nextlink = curr_link->next();
		if (nextlink) { 
			auto* nddata = nextlink->get_userdata();
			if (nddata) {
				auto* hdr = reinterpret_cast<listnode_t*>(nddata);
				set_around_vehicle(dspkg, hdr, gps);
			}
		}

	} else {
		printf("map error\n");
	}

	return 0;
}

int vss_snapshot::set_around_vehicle(distribute_package &dspkg, listnode_t *hd,
	vss::positioning_system_info& curr_gps)
{
	if (listnode_isempty(*hd)) {
		return 0;
	}
	auto* nd = hd->next;
	for (; nd != hd; nd = nd->next) {
		auto* item = LIST_ENTRY(vehicle_snapshot_item,	\
			linkownerlist, nd);
		assert(nullptr != item);
		auto vsspkg = item->snapshot[item->index].package();
		auto gps = vsspkg.gpsinfo();

		auto dist = zas::mapcore::distance(
			curr_gps.longtitude(), 
			curr_gps.latitude(), 
			gps.longtitude(), 
			gps.latitude());
		if (dist > VEHICLE_MAXDISTANCE) {
			continue;
		}

		auto* vehi = dspkg.add_vehicles();
		vehi->set_id(item->vid);
		vehi->set_lat(gps.latitude());
		vehi->set_lon(gps.longtitude());
		vehi->set_speed(vsspkg.vehicle_speed());
		vehi->set_heading(gps.heading());
		uint64_t vehtime = vsspkg.timestamp_sec() * 1000;
		vehtime += vsspkg.timestamp_usec();
		vehi->set_timestamp(vehtime);
	}
	return 0;
}

int vss_snapshot::send_to_vehicle(vehicle_snapshot_item* item,
	const char* data, size_t sz)
{
	assert(nullptr != item);
	if (item->port.length() > 0 && item->ipaddr.length() > 0) {
		std::string iussed_uri = "ztcp://";
		iussed_uri += item->ipaddr + ":";
		iussed_uri += item->port + "/digdup/jos/v1/distribute?";
		iussed_uri += "identity=" + item->identity;
		iussed_uri += "&start_time=" + std::to_string(item->recv_time);
		uri iussed_url(iussed_uri.c_str());
		// printf("data_collector creat ztcp req %s\n", iussed_uri.c_str());
		auto distribute = vss_request(iussed_url);
		distribute.send((const uint8_t*)data, sz);
	}

	return 0;
}

int vss_snapshot::send_to_subscribe(vehicle_snapshot_item* item,
	const char* data, size_t sz)
{
	assert(nullptr != item);
	if (find_request_item(item->vid)) {
		auto* rbq = device_snapshot::inst()->get_rabbitmq();
		if (rbq) {
			std::string routekey = "vehicle_edge_detail_";
			routekey += item->vid;
			rbq->publishing("vehicle_edge_information",
				routekey, (void*)data, sz);
		}
	}
	return 0;
}

int vss_snapshot::kafuka_send_snapshot(void)
{
	if (!listnode_isempty(_vss_snapshot_list)) {
		listnode_t *nd = _vss_snapshot_list.next;
		for(; nd != &_vss_snapshot_list; nd = nd->next) {
			auto* item = LIST_ENTRY(vehicle_snapshot_item,	\
				ownerlist, _vss_snapshot_list.next);
			assert(nullptr != item);
			if (item->leftime > SNAPSHOT_SERVICE_TIMER_MIN_INTERVAL) {
				item->leftime -= SNAPSHOT_SERVICE_TIMER_MIN_INTERVAL;
			} else {
				item->leftime = 0;
			}
			if (!item->update) {continue;}
			send_vehicle_to_kafka(item);
		}
	}
	return 0;
}

int vss_snapshot::send_vehicle_to_kafka(vehicle_snapshot_item* item)
{
	assert(nullptr != item);
	item->update = false;
	item->leftime = VEHICLE_KAFKA_SEND_MIN_INTERVAL;
	center_kafka_data kafka_data;
	auto* ss_data = kafka_data.add_snapshot();
	*ss_data = item->snapshot[item->index];
	std::string cdata;
	kafka_data.SerializeToString(&cdata);
	service_backend::inst()->send_kafka_data(cdata.c_str(),
		cdata.length());
	return 0;
}

vehicle_snapshot_item* 
vss_snapshot::find_snapshot(std::string &vid)
{
	auto* ret = avl_find(_vss_snapshot_tree, 
		MAKE_FIND_OBJECT(vid, vehicle_snapshot_item, vid, avlnode),
		vehicle_snapshot_item::veh_ss_avl_compare);
	if (nullptr == ret) {
		return nullptr;
	}
	return AVLNODE_ENTRY(vehicle_snapshot_item, avlnode, ret);
}

int vss_snapshot::remove_snapshot(std::string &vid)
{
	auto* item = find_snapshot(vid);
	if (!item) {
		return -ENOTFOUND;
	}

	avl_remove(&_vss_snapshot_tree, &item->avlnode);
	listnode_del(item->ownerlist);
	listnode_del(item->linkownerlist);
	delete item;
	return 0;
}

int vss_snapshot::remove_all_snapshot(void)
{
	while (!listnode_isempty(_vss_snapshot_list)) {
		auto* item = LIST_ENTRY(vehicle_snapshot_item,	\
			ownerlist, _vss_snapshot_list.next);
		if (!item) {
			return -ELOGIC;
		}
		listnode_del(item->ownerlist);
		listnode_del(item->linkownerlist);
		listnode_del(item->junctionownerlist);
		listnode_del(item->junprevownerlist);
		delete item;
	}
	_vss_snapshot_tree = nullptr;
	return 0;
}

int vehicle_snapshot_item::veh_ss_avl_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(vehicle_snapshot_item, avlnode, a);
	auto* bb = AVLNODE_ENTRY(vehicle_snapshot_item, avlnode, b);
	int ret = aa->vid.compare(bb->vid);
	if (ret > 0) { return 1; }
	else if (ret < 0) { return -1; }
	else { return 0; }
	return 0;
}

int vss_snapshot::request_snapshot(std::string vid, bool subscribe)
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

request_snapshot_item* vss_snapshot::add_request_item(std::string &vid)
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
		log.e(SNAPSHOT_SNAPSHOT_TAG, "update requeset snapshot insert error\n");
		return nullptr;
	}
	listnode_add(_request_vin_list, item->ownerlist);
	return item;
}

request_snapshot_item* vss_snapshot::find_request_item(std::string &vid)
{
	auto* ret = avl_find(_request_vin_tree, 
		MAKE_FIND_OBJECT(vid, request_snapshot_item, vid, avlnode),
		request_snapshot_item::request_ss_avl_compare);
	if (nullptr == ret) {
		return nullptr;
	}
	return AVLNODE_ENTRY(request_snapshot_item, avlnode, ret);
}

int vss_snapshot::remove_request_item(std::string &vid)
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
	log.w(SNAPSHOT_SNAPSHOT_TAG, "delete vhicle item\n");
	return 0;
}

int vss_snapshot::remove_all_request()
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

}}	//zas::vehicle_snapshot_service




