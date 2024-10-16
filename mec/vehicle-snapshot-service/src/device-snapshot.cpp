#include "device-snapshot.h"
#include "service-worker.h"
#include "proto/junction_package.pb.h"
#include "proto/center_junction_detail.pb.h"
#include "proto/center_device_detail.pb.h"
#include "proto/center_kafka_data.pb.h"
#include "snapshot-rabbitmq.h"
#include "snapshot.h"

#include <sys/time.h>
#include <Eigen/Dense>
#include "utils/uri.h"
#include "webcore/logger.h"
#include "webcore/webapp.h"
#include "webcore/sysconfig.h"

namespace zas {
namespace vehicle_snapshot_service {

using namespace zas::utils;
using namespace zas::webcore;
using namespace zas::mapcore;
using namespace jos;

class device_wa_request : public wa_request
{
public:
	device_wa_request(uri& url, device_snapshot *ds)
	: wa_request(url)
	, _dt(ds){}
	~device_wa_request(){}
	int on_reply(void* context, void* data, size_t sz)
	{
		assert(nullptr != _dt);
		// printf("device_wa_request size [%ld]: %s\n", sz, (char*)data);
		return _dt->on_request_reply(this, context, data, sz);
	}

private:
	device_snapshot* _dt;
};

device_snapshot::device_snapshot()
: _device_junction_tree(nullptr)
, _device_sender(nullptr)
, _flags(0)
, _request_jucntion_tree(nullptr)
, _request_device_tree(nullptr)
, _rabbitmq(nullptr)
, _hdmap(nullptr)
, _proj(nullptr)
, _kafka_consumer(nullptr)
{
	listnode_init(_device_junction_list);
}

device_snapshot* device_snapshot::inst()
{
	static device_snapshot* _inst = NULL;
	if (_inst) return _inst;
	_inst = new device_snapshot();
	assert(NULL != _inst);
	return _inst;
}

device_snapshot::~device_snapshot()
{
	remove_all_snapshot();
	remove_all_request();
	if (_rabbitmq) {
		delete _rabbitmq;
		_rabbitmq = nullptr;
	}
	if (_hdmap) {
		delete _hdmap;
		_hdmap = nullptr;
	}
	if (_proj) {
		delete _proj;
		_proj = nullptr;
	}
	if (_kafka_consumer) {
		_kafka_consumer->release();
	}
}

int device_snapshot::on_recv(std::string &vid, uri &info, void* data, size_t sz)
{
	assert(nullptr != data);
	update_snapshot(vid, info, data, sz);
	return 0;
}

snapshot_rabbitmq* device_snapshot::get_rabbitmq(void)
{
	return _rabbitmq;
}

int device_snapshot::init(void)
{
	if (_f.init_device_ss) {
		return -EEXISTS;
	}
	_f.init_device_ss = 1;

	if (!_hdmap) {
		_hdmap = new rendermap();
		assert(nullptr != _hdmap);
		std::string hdmapfilename = "/zassys/sysapp/others/hdmap/rendermap";
		hdmapfilename = get_sysconfig("snapshot-service.rendermap",
			hdmapfilename.c_str());
		int ret = _hdmap->load_fromfile(hdmapfilename.c_str());
		if (!ret) {
			_proj = new proj(GREF_WGS84, _hdmap->get_georef());
			double x, y;
			_hdmap->get_offset(x, y);
			_projoffset.set(x,y);
		} else {
			printf("load hdmap file error:[%d], file: %s\n",
				ret, hdmapfilename.c_str());
		}
	}
	if (!_rabbitmq) {
		void* ws_sock = webapp::inst()->get_context()->get_sock_context();
		_rabbitmq = new snapshot_rabbitmq(rabbitmq_role_producing, "junciton",
			service_backend::inst(), ws_sock, service_backend::inst()->get_timermgr());
		_rabbitmq->init();
	}
	_kafka_consumer = new vss_kafka_consumer();
	_kafka_consumer->start();
	return 0;
}

int device_snapshot::on_request_reply(device_wa_request* wa_req,
	void* context, void* data, size_t sz)
{
	if (!data || !sz) {
		return -EBADPARM;
	}
	fusion_service_pkg fpkg;
	fpkg.ParseFromArray((const char*)data, sz);
	auto fustgt = fpkg.fus_pkg();
	std::string jid = fustgt.junc_id();
	auto* jitem = find_junction(jid);
	if (!jitem) {
		return -ENOTFOUND;
	}
	int index = jitem->index;
	jitem->info[index].Clear();
	jitem->info[index] = fustgt;
	//add fusion service data to junciton
	timeval tv;
	gettimeofday(&tv, nullptr);
	uint64_t start_tm = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	jitem->improver->improve_fusion_target_to_junction(jitem, fpkg);
	gettimeofday(&tv, nullptr);
	uint64_t end_tm = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	if (end_tm - start_tm > 5) {
		printf("++++++++[%d]use[%d] time %lu\n", jitem->info[jitem->index].targets_size() , fpkg.fus_pkg().targets_size(), end_tm - start_tm);
	}
	if (end_tm - fustgt.timestamp() > 3000) {
		printf("++++++++recv fusion %lu \n", end_tm - fustgt.timestamp());
	}
	jitem->index = (jitem->index + 1) % DEVICE_SNAPSHOT_CACHER;
	send_junciton_to_vehicle(jitem);
	send_subscribe_junction_info(jitem, jitem->info[index]);
	send_junction_to_kafka(jitem);
	return 0;
}

// send junction fusion info to vehicle
int device_snapshot::send_junciton_to_vehicle(device_junction_item* jitem)
{
	assert(nullptr != jitem);
	int currindex = (jitem->index + DEVICE_SNAPSHOT_CACHER -1) % DEVICE_SNAPSHOT_CACHER;
	auto* item = &(jitem->info[currindex]);
	distribute_package dspkg;
	dspkg.set_timestamp(item->timestamp());
	auto* jun_info = dspkg.add_junctions();
	jun_info->set_junction_id(jitem->jun_info.id);
	jun_info->set_lat(jitem->jun_info.lat);
	jun_info->set_lon(jitem->jun_info.lon);
	for (int i = 0; i < item->targets_size(); i++) {
		auto* jtgt = jun_info->add_targets();
		*jtgt = item->targets(i); 
	}
	std::string senddata;
	dspkg.SerializeToString(&senddata);

	listnode_t* nd = jitem->veh_list.next;
	for (; nd != &jitem->veh_list; nd = nd->next){
		auto* vitem = LIST_ENTRY(vehicle_snapshot_item, junctionownerlist, nd);
		assert(nullptr != vitem);
		vss_snapshot::inst()->send_to_vehicle(vitem, 
			senddata.c_str(), senddata.length());
	}

	nd = jitem->veh_prev_list.next;
	for (; nd != &jitem->veh_prev_list; nd = nd->next){
		auto* vitem = LIST_ENTRY(vehicle_snapshot_item, junctionownerlist, nd);
		assert(nullptr != vitem);
		vss_snapshot::inst()->send_to_vehicle(vitem, 
			senddata.c_str(), senddata.length());
	}

	return 0;
}

// send junction info to center by rabbitmq
int device_snapshot::send_subscribe_junction_info(device_junction_item* jitem,
	jos::fusion_package& fpkg)
{
	if (find_request_item(&_request_jucntion_tree, jitem->jun_info.id)) {
		junction junpkg;
		junpkg.set_junction_id(jitem->jun_info.id);
		junpkg.set_timestamp(fpkg.timestamp());
		auto* timeinfo = junpkg.mutable_timeinfo();
		*timeinfo = fpkg.timeinfo();
		listnode_t* dnd = jitem->device_list.next;
		for (; dnd != &jitem->device_list; dnd = dnd->next) {
			auto* info = LIST_ENTRY(device_info, ownerlist, dnd);
			assert(nullptr != info);
			if (info->inited) {
				continue;
			}
			auto* dev_info = junpkg.add_devices();
			dev_info->set_device_id(info->dev_id);
			int currid = (info->index + DEVICE_SNAPSHOT_CACHER - 1) % DEVICE_SNAPSHOT_CACHER;
			dev_info->set_lat(info->info[currid].lat());
			dev_info->set_lon(info->info[currid].lon());
			dev_info->set_hdg(info->info[currid].rdir());
		}
		size_t ftg_sz = fpkg.targets_size();
		for (int i = 0; i < ftg_sz; i++) {
			auto dev_tg = fpkg.targets(i);
			auto* cdev_tg = junpkg.add_targets();
			*cdev_tg = dev_tg;
		}
		std::string data;
		junpkg.SerializeToString(&data);
		update_junction_to_center(jitem->jun_info.id,
			(void*)data.c_str(), data.length());
	}
	return 0;
}

device_junction_item* 
device_snapshot::check_create_junction_item(std::string &uid)
{
	auto* item = find_junction(uid);
	if (!item) {
		item = new device_junction_item();
		assert(nullptr != item);
		item->jun_info.id = uid;
		item->index = 0;
		item->device_tree = nullptr;
		item->jun_info.lat = 0.0;
		item->jun_info.lon = 0.0;
		item->jun_info.radius = 0.0;
		if (_hdmap) {
			uint32_t junuid = stoul(uid.c_str(), nullptr, 10);
			const auto* hdjuction = _hdmap->get_junction(junuid);
			if (hdjuction && _proj) {
				double centerx,centery;
				hdjuction->center_point(centerx, centery, item->jun_info.radius);
				point3d utm3d = {centerx, centery};
				utm3d += _projoffset;
				point3d w843d = _proj->transform(utm3d, true);
				item->jun_info.lat = w843d.llh.lat;
				item->jun_info.lon = w843d.llh.lon;
			} else {
				printf("hdmap juction[%u] is null %p\n", junuid, hdjuction);
			}
		} else {
			printf("hdmap not load failure\n");
		}
		item->improver = std::make_shared<fusion_improver>();
		item->fjunc = std::make_shared<fusion_juncion_info>();
		item->fjunc->junction_id = std::stoi(uid);
		int junret = load_junction_info(item->fjunc);
		if (junret < 0) {
			printf("hdmap load junction info[%d] info error\n", item->fjunc->junction_id);
			item->fjunc = nullptr;
		} else {
			junret = load_junction_count_info(item->fjunc);
			if (junret < 0) {
				printf("hdmap load junction count[%d] count error\n", item->fjunc->junction_id);
			}
		}
		junret = item->improver->init_junction_info(item->fjunc);
		if (junret < 0) {
			item->fjunc = nullptr;
		}
		listnode_init(item->device_list);
		if(avl_insert(&_device_junction_tree, &item->avlnode,
			device_junction_item::device_junction_avl_compare)) {
			delete item;
			return nullptr;
		}
		listnode_add(_device_junction_list, item->ownerlist);
	}
	return item;
}

device_info* 
device_snapshot::check_create_device_item(device_junction_item *item,
	std::string &uid)
{
	auto* info = find_device(item, uid);
	if (!info) {
		info = new device_info();
		info->dev_id = uid;
		info->index = 0;
		info->inited = true;
		if(avl_insert(&item->device_tree, &info->avlnode,
			device_info::device_avl_compare)) {
			delete info;
			return nullptr;
		}
		listnode_init(item->veh_list);
		listnode_init(item->veh_prev_list);
		listnode_add(item->device_list, info->ownerlist);
	}
	return info;
}

int device_snapshot::add_device_info(device_info* device,
	const radar_vision_info &info)
{
	assert(nullptr != device);
	if (device->inited) {
		device->inited = false;
	} else {
		uint32_t currid = (device->index
			+ DEVICE_SNAPSHOT_CACHER -1) % DEVICE_SNAPSHOT_CACHER;
		if (info.timestamp() <= device->info[currid].timestamp()) {
			return -EEXISTS;
		}
	}
	device->info[device->index].Clear();
	device->info[device->index] = info;
	device->index = (device->index + 1) % DEVICE_SNAPSHOT_CACHER;
	return 0;
}

int device_snapshot::load_junction_info(spfusion_juncion_info info)
{
	printf("device_snapshot load junction\n");
	ssize_t jun_cnt = 0;
	jun_cnt = get_sysconfig("junctions.jun_cnt", jun_cnt);
	if (jun_cnt == 0) {
		printf("device_snapshot junction number is 0\n");
		return -ENOTFOUND;
	}
	std::string junc_path;
	size_t index = 0;
	for (; index < jun_cnt; index++) {
		junc_path = "junctions.junction_info.";
		junc_path += std::to_string(index);
		std::string jun_id_path = junc_path + ".juntion_id";
		ssize_t junid = 0;
		int ret  = 0;
		junid = get_sysconfig(jun_id_path.c_str(), junid, &ret);
		if (ret < 0) {
			printf("device_snapshot junction get error\n");
			return -ENOTFOUND;
		}
		if(junid == info->junction_id) {
			break;
		}
	}
	if (index >= jun_cnt) {
		printf("device_snapshot not find junction in config file\n");
		return -ENOTFOUND;
	}
	ssize_t incom_rd_cnt = 0;
	std::string in_rd_cnt_path = junc_path + ".incom_road_cnt";
	incom_rd_cnt = get_sysconfig(in_rd_cnt_path.c_str(), incom_rd_cnt);
	if (incom_rd_cnt == 0) {
		printf("device_snapshot incomming road cout  is 0\n");
		return -ENOTFOUND;
	}
	std::string incom_road_path;
	for (size_t i = 0; i < incom_rd_cnt; i++) {
		incom_road_path = junc_path + ".incom_road_info.";
		incom_road_path += std::to_string(i);
		std::string rd_id_path = incom_road_path + ".road_id";
		int ret  = 0;
		ssize_t road_id = 0;
		road_id = get_sysconfig(rd_id_path.c_str(), road_id, &ret);
		if (ret < 0) {
			printf("device_snapshot read road error inconfig\n");
			return -ENOTFOUND;
		}
		auto road = std::make_shared<incomming_road>();
		road->road_id = road_id;
		std::string lane_cnt_path = incom_road_path + ".lane_cnt";
		ssize_t lane_cnt = get_sysconfig(lane_cnt_path.c_str(), lane_cnt);
		if (lane_cnt == 0) {
			printf("device_snapshot incomming road lane count  is 0\n");
			return -ENOTFOUND;
		}
		std::string lanes_path = incom_road_path + ".lane_info.";
		for (size_t lane_index = 0; lane_index < lane_cnt; lane_index++) {
			std::string lane_path = lanes_path + std::to_string(lane_index);
			std::string lane_id_path = lane_path + ".id";
			int lane_id = 0;
			int ret = 0;
			lane_id = get_sysconfig(lane_id_path.c_str(), lane_id, &ret);
			if (ret) {
				printf("device_snapshot lanes get lane id is 0\n");
				return -ENOTFOUND;
			}
			std::string next_lane_cnt_path = lane_path + ".next_road_cnt";
			ssize_t next_lane_cnt = get_sysconfig(next_lane_cnt_path.c_str(), next_lane_cnt, &ret);
			if (ret) {
				printf("device_snapshot next road count  is 0\n");
				return -ENOTFOUND;
			}
			auto lane = std::make_shared<fusion_junc_lane_info>();
			lane->lane_id = lane_id;
			std::string next_lanes_path = lane_path + ".next_roads.";
			for (int nl_index = 0; nl_index < next_lane_cnt; nl_index++) {
				std::string next_lane_path = next_lanes_path + std::to_string(nl_index);
				std::string lane_light_path = next_lane_path + ".light_id";
				int light_id = 0;
				light_id = get_sysconfig(lane_light_path.c_str(), light_id);
				std::string next_road_path = next_lane_path + ".next_road_id";
				ssize_t next_road_id = 0 ;
				next_road_id = get_sysconfig(next_road_path.c_str(), next_road_id);
				std::string next_lane_id_path = next_lane_path + ".next_lane_id";
				int next_lane = 0;
				next_lane = get_sysconfig(next_lane_id_path.c_str(), next_lane, &ret);
				if (ret) {
					printf("device_snapshot lanes get next_lane id is 0\n");
					return -ENOTFOUND;
				}
				auto next_lane_info = std::make_shared<fusion_junc_next_lane>();
				next_lane_info->light_id = light_id;
				next_lane_info->next_road_id = next_road_id;
				next_lane_info->next_lane_id = next_lane;
				lane->next_lanes[nl_index] = next_lane_info;
				if (0 == nl_index) {
					printf("junction %d,  incomm road id %lu, lane id %d, next road id %lu, next lane id %d, light_id %d\n", info->junction_id, road_id, lane_id, next_road_id, next_lane, light_id);
				}
			}
			road->lane_info[lane_id] = lane;
			lane->root.status = -2;
			listnode_init(lane->root.ownerlist);
		}
		info->road_info[road_id] = road;
	}
	return 0;
}

int device_snapshot::load_junction_count_info(spfusion_juncion_info info)
{
	printf("device_snapshot load junction count info\n");
	ssize_t jun_cnt = 0;
	jun_cnt = get_sysconfig("count_info.count_jun_cnt", jun_cnt);
	if (jun_cnt == 0) {
		printf("device_snapshot junction count number is 0\n");
		return -ENOTFOUND;
	}
	std::string junc_path;
	size_t index = 0;
	for (; index < jun_cnt; index++) {
		junc_path = "count_info.junction_info.";
		junc_path += std::to_string(index);
		std::string jun_id_path = junc_path + ".juntion_id";
		ssize_t junid = 0;
		int ret  = 0;
		junid = get_sysconfig(jun_id_path.c_str(), junid, &ret);
		if (ret < 0) {
			printf("device_snapshot junction count get id error\n");
			return -ENOTFOUND;
		}
		if(junid == info->junction_id) {
			break;
		}
	}
	if (index >= jun_cnt) {
		printf("device_snapshot not find junction count info in configfile\n");
		return -ENOTFOUND;
	}

	int ret = 0;
	auto cnt_jun = std::make_shared<count_junction>();
	std::string cnt_jun_name_path = junc_path + ".name";
	std::string cnt_jun_name = get_sysconfig(cnt_jun_name_path.c_str(), cnt_jun_name.c_str(), &ret);
	if (!ret) {
		cnt_jun->name = cnt_jun_name;
	}
	std::string cnt_jun_id_path = junc_path + ".id";
	ssize_t cnt_jun_id = 0;
	cnt_jun_id = get_sysconfig(cnt_jun_id_path.c_str(), cnt_jun_id, &ret);
	if (!ret) {
		cnt_jun->id = cnt_jun_id;
	}
	std::string cnt_jun_area_path = junc_path + ".area";
	ssize_t cnt_jun_area = 0;
	cnt_jun_area = get_sysconfig(cnt_jun_area_path.c_str(), cnt_jun_area, &ret);
	if (!ret) {
		cnt_jun->area = cnt_jun_area;
	}
	// count_junction
	info->count_info = cnt_jun;
	ret = load_count_incomming_road(junc_path, info->count_info);
	if (ret < 0) {
		return ret;
	}

	info->junc_count.Clear();
	info->junc_count.set_area(info->count_info->area);
	info->junc_count.set_junction_id(info->junction_id);
	info->junc_count.set_id(info->count_info->id);
	info->junc_count.set_name(info->count_info->name);
	
	return load_count_outgoing_road(junc_path, info->count_info);
}

int device_snapshot::load_count_incomming_road(std::string &junc_path, spcount_junction cnt_jun)
{
	printf("device_snapshot load_count_incomming_road\n");
	ssize_t incom_rd_cnt = 0;
	std::string in_rd_cnt_path = junc_path + ".incom_road_cnt";
	incom_rd_cnt = get_sysconfig(in_rd_cnt_path.c_str(), incom_rd_cnt);
	if (incom_rd_cnt == 0) {
		printf("device_snapshot incomming road cout  is 0\n");
		return -ENOTFOUND;
	}
	int ret = 0;
	std::string incom_road_path;
	for (size_t i = 0; i < incom_rd_cnt; i++) {
		incom_road_path = junc_path + ".incom_road.";
		incom_road_path += std::to_string(i);
		std::string rd_id_path = incom_road_path + ".id";
		std::string cnt_road_id = get_sysconfig(rd_id_path.c_str(), cnt_road_id.c_str(), &ret);
		if (ret < 0) {
			printf("device_snapshot read road error inconfig\n");
			return -ENOTFOUND;
		}
		std::string rd_name_path = incom_road_path + ".name";
		std::string cnt_road_name = get_sysconfig(rd_name_path.c_str(), cnt_road_name.c_str(), &ret);
		if (ret < 0) {
			printf("device_snapshot read road error inconfig\n");
			return -ENOTFOUND;
		}
		std::string orien_path = incom_road_path + ".orien";
		ssize_t orien = get_sysconfig(orien_path.c_str(), orien);
		
		auto in_road = std::make_shared<count_incomming_road>();
		in_road->id = cnt_road_id;
		in_road->name = cnt_road_name;
		in_road->orien = orien;
		cnt_jun->incom_info[i] = in_road;

		std::string movement_cnt_path = incom_road_path + ".movement_cnt";
		ssize_t movemt = get_sysconfig(movement_cnt_path.c_str(), movemt);
		if (movemt == 0) {
			printf("device_snapshot count incomming road  movement  is 0\n");
			return -ENOTFOUND;
		}
		std::string movements_path = incom_road_path + ".movements.";
		for (size_t mvmt_index = 0; mvmt_index < movemt; mvmt_index++) {
			std::string movemt_path = movements_path + std::to_string(mvmt_index);
			std::string movemt_type_path = movemt_path + ".type";
			int mm_type = 0;
			mm_type = get_sysconfig(movemt_type_path.c_str(), mm_type);
			auto cnt_movemt = std::make_shared<count_movement>();
			cnt_movemt->type = mm_type;
			std::string lane_cnt_path = movemt_path + ".lane_cnt";
			ssize_t lane_cnt = get_sysconfig(lane_cnt_path.c_str(), lane_cnt);
			if (lane_cnt == 0) {
				printf("device_snapshot incomming road  lane of movement count  is 0\n");
				return -ENOTFOUND;
			}
			std::string lanes_path = movemt_path + ".lanes.";
			for (size_t lane_index = 0; lane_index < lane_cnt; lane_index++) {
				std::string lane_path = lanes_path + std::to_string(lane_index);
				int ret = 0;
				std::string incom_road_id_path = lane_path + ".incom_road";
				int incom_road_id = 0;
				incom_road_id = get_sysconfig(incom_road_id_path.c_str(), incom_road_id, &ret);
				if (ret) {
					printf("device_snapshot movement not have incomming road\n");
					return -ENOTFOUND;
				}
				std::string incom_lane_path = lane_path + ".incom_lane";
				int incom_lane_id = 0;
				incom_lane_id = get_sysconfig(incom_lane_path.c_str(), incom_lane_id, &ret);
				if (ret) {
					printf("device_snapshot movement not have incomming lane\n");
					return -ENOTFOUND;
				}
				std::string lane_road_path = lane_path + ".lane_road";
				int lane_road_id = 0;
				lane_road_id = get_sysconfig(lane_road_path.c_str(), lane_road_id, &ret);
				if (ret) {
					printf("device_snapshot movement not have road\n");
					return -ENOTFOUND;
				}
				std::string lane_id_path = lane_path + ".lane_id";
				int lane_id = 0;
				lane_id = get_sysconfig(lane_id_path.c_str(), lane_id, &ret);
				if (ret) {
					printf("device_snapshot movement not have lane id\n");
					return -ENOTFOUND;
				}
				auto cnt_lane = std::make_shared<count_lane>();
				cnt_lane->name = cnt_road_name;
				cnt_lane->id = cnt_road_id;
				cnt_lane->type = mm_type;
				cnt_lane->in_laneid = incom_lane_id;
				cnt_lane->in_rdid = incom_road_id;
				cnt_lane->laneid = lane_id;
				cnt_lane->rdid = lane_road_id;
				cnt_lane->hdlane = nullptr;
				cnt_jun->incomm_time[incom_road_id][incom_lane_id] = 0;
				cnt_movemt->lanes[lane_index] = cnt_lane;
				if (0 == lane_index) {
					printf("road %s, id %s, type %d, incomm road id %d, lane id %d, road id %d, lane id %d\n", cnt_road_name.c_str(), cnt_road_id.c_str(), mm_type, incom_road_id, incom_lane_id, lane_road_id, lane_id);
				}
			}
			cnt_jun->incom_info[i]->movemts[mvmt_index] = cnt_movemt;
		}
	}
	return 0;
}

int device_snapshot::load_count_outgoing_road(std::string &junc_path, spcount_junction cnt_jun)
{
	printf("device_snapshot load_count_outgoing_road\n");
	ssize_t outgoing_rd_cnt = 0;
	std::string out_rd_cnt_path = junc_path + ".outgoing_road_cnt";
	outgoing_rd_cnt = get_sysconfig(out_rd_cnt_path.c_str(), outgoing_rd_cnt);
	if (outgoing_rd_cnt == 0) {
		printf("device_snapshot incomming road cout  is 0\n");
		return -ENOTFOUND;
	}
	int ret = 0;
	std::string outgoing_road_path;
	for (size_t i = 0; i < outgoing_rd_cnt; i++) {
		outgoing_road_path = junc_path + ".outgoing_road.";
		outgoing_road_path += std::to_string(i);
		std::string rd_id_path = outgoing_road_path + ".id";
		std::string cnt_road_id = get_sysconfig(rd_id_path.c_str(), cnt_road_id.c_str(), &ret);
		if (ret < 0) {
			printf("device_snapshot read outgoing road error inconfig\n");
			return -ENOTFOUND;
		}
		std::string rd_name_path = outgoing_road_path + ".name";
		std::string cnt_road_name = get_sysconfig(rd_name_path.c_str(), cnt_road_name.c_str(), &ret);
		if (ret < 0) {
			printf("device_snapshot read outgoing road name error inconfig\n");
			return -ENOTFOUND;
		}
		std::string orien_path = outgoing_road_path + ".orien";
		ssize_t orien = get_sysconfig(orien_path.c_str(), orien);
		std::string road_id_path = outgoing_road_path + ".road_id";
		ssize_t road_id = get_sysconfig(road_id_path.c_str(), road_id);
		if (ret < 0) {
			printf("device_snapshot read outgoing road id error inconfig\n");
			return -ENOTFOUND;
		}
		
		auto out_road = std::make_shared<count_outgoing_road>();
		out_road->id = cnt_road_id;
		out_road->name = cnt_road_name;
		out_road->orien = orien;
		out_road->rdid = road_id;
		cnt_jun->outgoing_info[i] = out_road;
	}
	return 0;
}

int device_snapshot::update_snapshot(std::string &vincode, uri &info, void* data, size_t sz)
{
	if (vincode.length() == 0) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, "snapshot no uid\n");
		return -EBADPARM;
	}
	if (!data || 0 == sz) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, "snapshot no data\n");
		return -EBADPARM;
	}
	auto* item = check_create_junction_item(vincode);
	if (!item) {
		log.e(SNAPSHOT_SNAPSHOT_TAG,
			"snapshot junction create error\n");
		return -ELOGIC;
	}
	timeval tv;
	gettimeofday(&tv, nullptr);
	uint64_t times = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	uint64_t starttime = 0;
	std::string rec_time = info.query_value("start_time");
	if (rec_time.length() > 0) {
		starttime = stoull(rec_time.c_str(), nullptr, 10);
	}
	if (times - starttime > 2000) {
		printf("snapshot recv date time is %lu\n", times - starttime);
	}
	junction_package junpkg;
	junpkg.ParseFromArray(data, sz);
	int count = junpkg.radar_info_size();
	for (int i = 0; i < count; i ++) {
		auto* rinfo = junpkg.mutable_radar_info(i);
		auto* timeinfo = rinfo->mutable_timeinfo();
		timeinfo->set_erecv_timestamp_sec(starttime / 1000);
		timeinfo->set_erecv_timestamp_usec((starttime % 1000 * 1000));
		// printf("target time [%lu]\n", times - rinfo->timestamp());
		std::string radaruid = std::to_string(rinfo->id());
		auto* info = check_create_device_item(item, radaruid);
		if (!info) {
			log.e(SNAPSHOT_SNAPSHOT_TAG, "snapshot no uid\n");
			return -ELOGIC;
		}
		if (add_device_info(info, *rinfo)) {
			return -ELOGIC;
		}
		send_subscribe_device_info(*rinfo);

		update_fusion_data(item, info);
	}
	return 0;
}

int device_snapshot::send_subscribe_device_info(
	const jos::radar_vision_info &rinfo)
{
	std::string radaruid = std::to_string(rinfo.id());
	if (find_request_item(&_request_device_tree, radaruid)) {
		device_detail devpkg;
		devpkg.set_type(rinfo.type());
		devpkg.set_id(radaruid);
		devpkg.set_lon(rinfo.lon());
		devpkg.set_lat(rinfo.lat());
		devpkg.set_rdir(rinfo.rdir());
		devpkg.set_timestamp(rinfo.timestamp());
		auto* timeinfo = devpkg.mutable_timeinfo();
		*timeinfo = rinfo.timeinfo();
		timeval tv;
		gettimeofday(&tv, nullptr);
		timeinfo->set_esend_timestamp_sec(tv.tv_sec);
		timeinfo->set_esend_timestamp_usec(tv.tv_usec);
		size_t dev_sz = rinfo.targets_size();
		for (int i = 0; i < dev_sz; i++) {
			auto* cdev_tg = devpkg.add_targets();
			*cdev_tg = rinfo.targets(i);
		}
		std::string data;
		devpkg.SerializeToString(&data);
		printf("device target number %d\n", devpkg.targets_size());
		update_device_to_center(radaruid,
			(void*)data.c_str(), data.length());
	}
	return 0;
}

int device_snapshot::update_device_to_center(std::string &id,
	void* data, size_t sz)
{
	if (id.length() == 0) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, "update_device_to_center no id\n");
		return -EBADPARM;
	}
	if (!data || 0 == sz) {
		return -EBADPARM;
	}
	std::string routekey = "device_detail_";
	routekey += id;
	_rabbitmq->publishing("device_information", routekey, data, sz);

	return 0;
}

int device_snapshot::update_junction_to_center(std::string &id,
	void* data, size_t sz)
{
	if (id.length() == 0) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, "update_junction_to_center no id\n");
		return -EBADPARM;
	}
	if (!data || 0 == sz) {
		return -EBADPARM;
	}
	std::string routekey = "junction_detail_";
	routekey += id;
	_rabbitmq->publishing("junction_information", routekey, data, sz);

	return 0;
}

int device_snapshot::send_to_fusion_service(std::string &junc_id,
	const uint8_t* data, size_t sz)
{
	if (!_device_sender) {
		printf("datacollect creat ztcp req\n");
		std::string ip = "localhost";
		ip = get_sysconfig("snapshot-service.fusion-service.ipaddr",
			ip.c_str());
		int port = 5558;
		port = get_sysconfig("snapshot-service.fusion-service.port", port);
		std::string update_uri = "ztcp://";
		update_uri += ip + ":";
		update_uri += std::to_string(port) + "/digdup/jos/v1/fusion";
		printf("datacollect creat ztcp req %s\n", update_uri.c_str());
		uri url(update_uri);
		url.add_query("uid", junc_id.c_str());
		printf("datacollect creat ztcp req %s\n", url.tostring().c_str());
		// todo regist and error 
		_device_sender = new device_wa_request(url, this);
		assert(nullptr != _device_sender);
		printf("creat finished, url %s\n", url.tostring().c_str());
	} else {
		_device_sender->set_header("uid", junc_id.c_str(), true);
	}
	_device_sender->send((const uint8_t*)data, sz, "REQ-REP");
	return 0;
}

int device_snapshot::generate_veh_frame(device_junction_item *item,
	junction_fusion_package &pkg, uint64_t endtime)
{
	if (endtime == 0) {
		return 0;
	}

	listnode_t* nd = item->veh_list.next;
	for (; nd != &item->veh_list; nd = nd->next){
		auto* vitem = LIST_ENTRY(vehicle_snapshot_item, junctionownerlist, nd);
		assert(nullptr != vitem);
		if (vitem->index >= VEHICLE_SNAPSHOT_HISTORY_MAX) {
			continue;
		}
		auto vspkg = vitem->snapshot[vitem->index].package();
		uint64_t timestamp = vspkg.timestamp_sec() * 1000
			+ vspkg.timestamp_usec() / 1000;
		if (timestamp < endtime - 100) {
			continue;
		}
		if (timestamp < endtime) {
			auto* vehi = pkg.add_vehicles();
			vehi->set_id(vitem->snapshot[vitem->index].vincode());
			vehi->set_timestamp(timestamp);
			vehi->set_lat(vspkg.gpsinfo().latitude());
			vehi->set_lon(vspkg.gpsinfo().longtitude());
			vehi->set_heading(vspkg.gpsinfo().heading());
			vehi->set_speed(vspkg.vehicle_speed());
			continue;
		}
		int start = (vitem->index + VEHICLE_SNAPSHOT_HISTORY_MAX - 1)
			% VEHICLE_SNAPSHOT_HISTORY_MAX;
		for (; start != vitem->index;
			start = (start + VEHICLE_SNAPSHOT_HISTORY_MAX - 1)
			% VEHICLE_SNAPSHOT_HISTORY_MAX) {
			auto tpkg = vitem->snapshot[start].package();
			timestamp = tpkg.timestamp_sec() * 1000
			+ tpkg.timestamp_usec() / 1000;
			if (timestamp <= endtime && timestamp > (endtime - 100)) {
				auto* vehi = pkg.add_vehicles();
				vehi->set_id(vitem->snapshot[start].vincode());
				vehi->set_timestamp(timestamp);
				vehi->set_lat(tpkg.gpsinfo().latitude());
				vehi->set_lon(tpkg.gpsinfo().longtitude());
				vehi->set_heading(tpkg.gpsinfo().heading());
				vehi->set_speed(tpkg.vehicle_speed());
				break;
			}
		}
	}

	nd = item->veh_prev_list.next;
	for (; nd != &item->veh_prev_list; nd = nd->next){
		auto* vitem = LIST_ENTRY(vehicle_snapshot_item, junctionownerlist, nd);
		assert(nullptr != vitem);
		if (vitem->index >= VEHICLE_SNAPSHOT_HISTORY_MAX) {
			continue;
		}
		auto vspkg = vitem->snapshot[vitem->index].package();
		uint64_t timestamp = vspkg.timestamp_sec() * 1000
			+ vspkg.timestamp_usec() / 1000;
		if (timestamp < endtime - 100) {
			continue;
		}
		if (timestamp < endtime) {
			auto* vehi = pkg.add_vehicles();
			vehi->set_id(vitem->snapshot[vitem->index].vincode());
			vehi->set_timestamp(timestamp);
			vehi->set_lat(vspkg.gpsinfo().latitude());
			vehi->set_lon(vspkg.gpsinfo().longtitude());
			vehi->set_heading(vspkg.gpsinfo().heading());
			vehi->set_speed(vspkg.vehicle_speed());
			continue;
		}
		int start = (vitem->index + VEHICLE_SNAPSHOT_HISTORY_MAX - 1)
			% VEHICLE_SNAPSHOT_HISTORY_MAX;
		for (; start != vitem->index;
			start = (start + VEHICLE_SNAPSHOT_HISTORY_MAX - 1)
			% VEHICLE_SNAPSHOT_HISTORY_MAX) {
			auto tpkg = vitem->snapshot[start].package();
			timestamp = tpkg.timestamp_sec() * 1000
			+ tpkg.timestamp_usec() / 1000;
			if (timestamp <= endtime && timestamp > (endtime - 100)) {
				auto* vehi = pkg.add_vehicles();
				vehi->set_id(vitem->snapshot[start].vincode());
				vehi->set_timestamp(timestamp);
				vehi->set_lat(tpkg.gpsinfo().latitude());
				vehi->set_lon(tpkg.gpsinfo().longtitude());
				vehi->set_heading(tpkg.gpsinfo().heading());
				vehi->set_speed(tpkg.vehicle_speed());
				break;
			}
		}
	}
	return 0;
}
static int snapshot_handle_cnt = 0;
static uint64_t tmptime1 = 0;

int device_snapshot::update_fusion_data(device_junction_item *item,
	device_info* radar_info)
{
	assert (nullptr != item);
	assert (nullptr != radar_info);

	if (snapshot_handle_cnt < 10000) {
		if (0 == snapshot_handle_cnt) {
			timeval tv;
			gettimeofday(&tv, nullptr);
			tmptime1 = tv.tv_sec*1000 + tv.tv_usec /1000;
		}
		snapshot_handle_cnt++;
	}
	else {
		timeval tv;
		gettimeofday(&tv, nullptr);
		uint64_t tmptime2 = tv.tv_sec*1000 + tv.tv_usec /1000;
		char timebuf[256];
		tm t;
		memset(timebuf, 0, 256);
	  	localtime_r(&tv.tv_sec, &t);
		snprintf(timebuf, 255, "%d/%02d/%02d %02d:%02d:%02d:%06ld",
		t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
		t.tm_hour, t.tm_min, t.tm_sec,
		tv.tv_usec);
		printf("[%s]snapshot handle 10000 data USE time %lu\n", timebuf, tmptime2 - tmptime1);
		snapshot_handle_cnt = 0;
		tmptime1 = 0;
	}


	uint32_t currid = (radar_info->index
		+ DEVICE_SNAPSHOT_CACHER - 1) % DEVICE_SNAPSHOT_CACHER;
	uint64_t endtime = radar_info->info[currid].timestamp();

	junction_fusion_package junc_fusion_pkg;
	auto* junc_pkg = junc_fusion_pkg.mutable_jun_pkg();
	auto* rainfo = junc_pkg->add_radar_info();
	*rainfo = radar_info->info[currid];
	generate_veh_frame(item, junc_fusion_pkg, endtime);
	junc_fusion_pkg.set_fusion_endtime(endtime);
	std::string senddata;
	junc_fusion_pkg.SerializeToString(&senddata);
	send_to_fusion_service(item->jun_info.id,
		(const uint8_t*)senddata.c_str(), senddata.length());
	return 0;
}

int device_snapshot::add_vehicle_to_junction(vehicle_snapshot_item* item)
{
	if (listnode_isempty(_device_junction_list)) {
		return -ENOTFOUND;
	}
	auto gps = item->snapshot[item->index].package().gpsinfo();
	point3d prjret = _proj->transform(llh(gps.longtitude(), gps.latitude()));
	prjret -= _projoffset;

	vector<rendermap_nearest_junctions> junc;
	_hdmap->find_junctions(prjret.xyz.x, prjret.xyz.y, 1, junc);
	if (junc.size() == 0) { return -ENOTFOUND;}
	auto jitem = junc[0];
	if (jitem.distance > VEHICLE_IN_JUNCTION_MAX_DISTANCE) {
		return -ENOTFOUND;
	}

	std::string juncid = std::to_string(jitem.junction->getid());
	auto* jucitem = find_junction(juncid);
	if (!jucitem) { return -ENOTFOUND;}

	listnode_add(jucitem->veh_list, item->junctionownerlist);

	return 0;
}

int device_snapshot::get_traffice_info(vehicle_snapshot_item* item,
	distribute_package &dist_pkg)
{
	if (listnode_isempty(_device_junction_list)) {
		return -ENOTFOUND;
	}
	auto gps = item->snapshot[item->index].package().gpsinfo();
	point3d prjret = _proj->transform(llh(gps.longtitude(), gps.latitude()));
	prjret -= _projoffset;

	vector<rendermap_nearest_junctions> junc;
	_hdmap->find_junctions(prjret.xyz.x, prjret.xyz.y, 2, junc);

	double yaw_rad = gps.heading() * DEG2RAD;
	Eigen::Vector3d t1(prjret.v[0], prjret.v[1], prjret.v[2]);
	Eigen::AngleAxisd rotation_vector(yaw_rad, Eigen::Vector3d::UnitZ());
	Eigen::Matrix3d rotate_mat = Eigen::Matrix3d::Identity();
	rotate_mat = rotation_vector.toRotationMatrix();
	Eigen::Isometry3d T1 = Eigen::Isometry3d::Identity();
	T1.rotate(rotate_mat);
	T1.pretranslate(t1);

	for (auto& item : junc) {
		double x, y, r;
		item.junction->center_point(x, y, r);
		Eigen::Vector3d junpos(x, y, 0);
		Eigen::Vector3d junvehpos = T1.inverse()* junpos;
		if (junvehpos(1) <= 0) {
			continue;
		}
		if (item.distance > JUNCTION_MAX_DISTANCE) {
			continue;
		}

		std::string junid = std::to_string(item.junction->getid());
		auto* jucitem = find_junction(junid);
		if (!jucitem) { continue;}
		auto* jpkg = dist_pkg.add_junctions();
		jpkg->set_junction_id(jucitem->jun_info.id);
		size_t sz = jucitem->info[jucitem->index - 1].targets_size();
		for (size_t i = 0; i < sz; i++) {
			auto* tget = jpkg->add_targets();
			*tget = jucitem->info[jucitem->index - 1].targets(i);
		}
	}
	
	return 0;
}

device_info* 
device_snapshot::find_device(device_junction_item *item, std::string &uid)
{
	assert(nullptr != item);
	auto* ret = avl_find(item->device_tree, 
		MAKE_FIND_OBJECT(uid, device_info, dev_id, avlnode),
		device_info::device_avl_compare);
	if (nullptr == ret) {
		return nullptr;
	}
	return AVLNODE_ENTRY(device_info, avlnode, ret);
}

device_junction_item* 
device_snapshot::find_junction(std::string &uid)
{
	auto* ret = avl_find(_device_junction_tree, 
		MAKE_FIND_OBJECT(uid, device_junction_item, jun_info.id, avlnode),
		device_junction_item::device_junction_avl_compare);
	if (nullptr == ret) {
		return nullptr;
	}
	return AVLNODE_ENTRY(device_junction_item, avlnode, ret);
}

int device_snapshot::remove_device(device_junction_item *item, std::string &uid)
{
	auto* info = find_device(item, uid);
	if (!info) {
		return -ENOTFOUND;
	}
	avl_remove(&item->device_tree, &info->avlnode);
	listnode_del(info->ownerlist);
	delete info;
	return 0;
}

int device_snapshot::remove_all_snapshot(void)
{
	while (!listnode_isempty(_device_junction_list)) {
		auto* item = LIST_ENTRY(device_junction_item,	\
			ownerlist, _device_junction_list.next);
		if (!item) {
			return -ELOGIC;
		}
		listnode_del(item->ownerlist);
		while (!listnode_isempty(item->veh_list)) {
			listnode_del(*item->veh_list.next);
		}
		while (!listnode_isempty(item->veh_prev_list)) {
			listnode_del(*item->veh_prev_list.next);
		}
		while (!listnode_isempty(item->device_list)) {
			auto* info= LIST_ENTRY(device_info,	\
				ownerlist, item->device_list.next);
			listnode_del(info->ownerlist);
			delete info;
		}
		delete item;
	}
	_device_junction_tree = nullptr;
	return 0;
}

int device_snapshot::remove_all_request(void)
{
	for (;;) {
		avl_node_t* node = avl_first(_request_jucntion_tree);
		if (nullptr == node) break;
		auto* item = AVLNODE_ENTRY(request_jd_item, avlnode, node);
		avl_remove(&_request_jucntion_tree, &item->avlnode);
		delete item;
	}
	for (;;) {
		avl_node_t* node = avl_first(_request_device_tree);
		if (nullptr == node) break;
		auto* item = AVLNODE_ENTRY(request_jd_item, avlnode, node);
		avl_remove(&_request_device_tree, &item->avlnode);
		delete item;
	}
	return 0;
}

int device_snapshot::request_device(std::string &id, bool subscribe)
{
	if (subscribe) {
		if (add_request_item(&_request_device_tree, id)){
			return 0;
		}
		return -ELOGIC;
	} else {
		return remove_request_item(&_request_device_tree, id);
	}

	return 0;
}

int device_snapshot::send_junction_to_kafka(device_junction_item* item)
{
	assert(nullptr != item);
	center_kafka_data kafka_data;
	auto* jos_data = kafka_data.add_junction();
	jos_data->set_junction_id(item->jun_info.id);
	jos_data->set_lat(item->jun_info.lat);
	jos_data->set_lon(item->jun_info.lon);
	int index = (item->index + DEVICE_SNAPSHOT_CACHER - 1)
		% DEVICE_SNAPSHOT_CACHER;

	jos_data->set_timestamp(item->info[index].timestamp());
	auto* timeinfo = jos_data->mutable_timeinfo();
	*timeinfo = item->info[index].timeinfo();
	timeval tv;
	gettimeofday(&tv, nullptr);
	timeinfo->set_esend_timestamp_sec(tv.tv_sec);
	timeinfo->set_esend_timestamp_usec(tv.tv_usec);
	int64_t timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	size_t fsz = item->info[index].targets_size();
	for (size_t i = 0; i < fsz; i++) {
		auto* fusiontgt = jos_data->add_targets();
		*fusiontgt = item->info[index].targets(i);
	}

	auto* dnd = item->device_list.next;
	for (; dnd != &item->device_list; dnd = dnd->next) {
		auto* info = LIST_ENTRY(device_info, ownerlist, dnd);
		assert(nullptr != info);
		auto* dev_info = jos_data->add_devices();
		int windex = (info->index + DEVICE_SNAPSHOT_CACHER - 1)
			% DEVICE_SNAPSHOT_CACHER;
		dev_info->set_type(info->info[windex].type());
		dev_info->set_id(std::to_string(info->info[windex].id()));
		dev_info->set_lon(info->info[windex].lon());
		dev_info->set_lat(info->info[windex].lat());
		dev_info->set_rdir(info->info[windex].rdir());
		auto* tinfo = dev_info->mutable_timeinfo();
		*tinfo = *timeinfo;
		dev_info->set_timestamp(item->info[index].timestamp());
		
		size_t tsz = info->info[windex].targets_size();
		for (size_t i = 0; i < tsz; i++) {
			auto* tgobj = dev_info->add_targets();
			*tgobj = info->info[windex].targets(i);
		}
	}

	size_t in_count = 0;
	size_t out_count = 0;
	if (item->fjunc) {
		if (item->fjunc->junc_count.invs_size() > 0
			|| item->fjunc->junc_count.outvs_size() > 0) {
			auto* cntjun = kafka_data.add_count();
			*cntjun = item->fjunc->junc_count;
			cntjun->set_timestamp(timestamp);
			item->fjunc->junc_count.clear_invs();
			item->fjunc->junc_count.clear_outvs();
		}
	}
	std::string cdata;
	kafka_data.SerializeToString(&cdata);
	service_backend::inst()->send_kafka_data(cdata.c_str(), cdata.length());

	return 0;
}

int device_snapshot::request_junction(std::string &id, bool subscribe)
{
	if (subscribe) {
		if (add_request_item(&_request_jucntion_tree, id)){
			return 0;
		}
		return -ELOGIC;
	} else {
		return remove_request_item(&_request_jucntion_tree, id);
	}

	return 0;
}

request_jd_item* device_snapshot::add_request_item(avl_node_t** root,
	std::string &id)
{
	auto* item = find_request_item(root, id);
	if (item) {
		item->refcnt++;
		return item;
	}
	item  = new request_jd_item();
	item->refcnt = 1;
	item->id = id;
	if (avl_insert(root, &item->avlnode,
		request_jd_item::request_jd_avl_compare)) {
		delete item;
		log.e(SNAPSHOT_SNAPSHOT_TAG, "update requeset device insert error\n");
		return nullptr;
	}
	return item;
}

request_jd_item* device_snapshot::find_request_item(avl_node_t** root,
	std::string &id)
{
	auto* ret = avl_find(*root, 
		MAKE_FIND_OBJECT(id, request_jd_item, id, avlnode),
		request_jd_item::request_jd_avl_compare);
	if (nullptr == ret) {
		return nullptr;
	}
	return AVLNODE_ENTRY(request_jd_item, avlnode, ret);
}

int device_snapshot::remove_request_item(avl_node_t** root,
	std::string &id)
{
	auto* item = find_request_item(root, id);
	if (!item) {
		log.d(SNAPSHOT_SNAPSHOT_TAG, "remove_request_item not find %s\n", id.c_str());
		return 0;
	}
	item->refcnt--;
	if (item->refcnt > 0) {
		log.d(SNAPSHOT_SNAPSHOT_TAG, "remove_request_item refcnt %d\n", item->refcnt);
		return 0;
	}
	avl_remove(root, &item->avlnode);
	delete item;
	log.w(SNAPSHOT_SNAPSHOT_TAG, "delete device item\n");
	return 0;
}

int device_junction_item::device_junction_avl_compare(
	avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(device_junction_item, avlnode, a);
	auto* bb = AVLNODE_ENTRY(device_junction_item, avlnode, b);
	int ret = aa->jun_info.id.compare(bb->jun_info.id);
	if (ret > 0) { return 1; }
	else if (ret < 0) { return -1; }
	else { return 0; }
	return 0;
}

int device_info::device_avl_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(device_info, avlnode, a);
	auto* bb = AVLNODE_ENTRY(device_info, avlnode, b);
	int ret = aa->dev_id.compare(bb->dev_id);
	if (ret > 0) { return 1; }
	else if (ret < 0) { return -1; }
	else { return 0; }
	return 0;
}

int request_jd_item::request_jd_avl_compare(avl_node_t* a,
	avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(request_jd_item, avlnode, a);
	auto* bb = AVLNODE_ENTRY(request_jd_item, avlnode, b);
	int ret = aa->id.compare(bb->id);
	if (ret > 0) { return 1; }
	else if (ret < 0) { return -1; }
	else { return 0; }
	return 0;
}

}}	//zas::vehicle_snapshot_service




