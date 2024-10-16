#include "vehicle-mgr.h"
#include "proto/register_request.pb.h"
#include "proto/register_reply.pb.h"
#include "webcore/logger.h"
#include "webcore/webapp.h"
#include "utils/uri.h"
#include "utils/uuid.h"

namespace zas {
namespace vehicle_indexing {

using namespace zas::utils;
using namespace zas::webcore;
using namespace vss;

vehicle_mgr::vehicle_mgr()
: _vehicle_vin_tree(nullptr)
, _vehicle_uid_tree(nullptr)
{
	listnode_init(_vehicle_list);
}

vehicle_mgr::~vehicle_mgr()
{
	remove_all_vehicle();
}

vehicle_mgr* vehicle_mgr::inst()
{
	static vehicle_mgr* _inst = NULL;
	if (_inst) return _inst;
	_inst = new vehicle_mgr();
	assert(NULL != _inst);
	return _inst;
}

vehicle_item* vehicle_mgr::add_vehicle(std::string& vid,
	std::string& acc_token)
{
	auto* item = find_vehicle_vid(vid);
	if (item) {
		item->acctoken = acc_token;
		return item;
	}
	item = new vehicle_item();
	item->vid = vid;
	item->acctoken = acc_token;
	item->uid = alloc_uid();

	if (avl_insert(&_vehicle_uid_tree, &item->avlnode,
		vehicle_item::vehicle_uid_compare)) {
		delete item;
		return nullptr;
	}
	int ret = avl_insert(&_vehicle_vin_tree, &item->avlvinnode,
		vehicle_item::vehicle_vin_compare);
	assert(!ret);

	listnode_add(_vehicle_list, item->ownerlist);
	return item;
}

int vehicle_mgr::remove_all_vehicle(void)
{
	while (!listnode_isempty(_vehicle_list)) {
		auto* item = LIST_ENTRY(vehicle_item, ownerlist, _vehicle_list.next);
		assert(nullptr != item);
		listnode_del(item->ownerlist);
		delete item;
	}
	_vehicle_uid_tree = nullptr;
	_vehicle_vin_tree = nullptr;
	return 0;
}

std::string vehicle_mgr::alloc_uid()
{
	uuid allocuid;
	allocuid.generate();
	std::string vuid;
	allocuid.to_hex(vuid);
	return vuid;
}

int vehicle_mgr::vehicle_register(std::string &key,
	uri &url, void* data, size_t sz)
{
	assert(nullptr != data);
	std::string vin = url.query_value(key.c_str());
	register_request req;
	req.ParseFromArray(data, sz);
	int ret = 0;
	bool bcreate = false;
	vehicle_item* item = nullptr;
	if (req.type() != register_objtype_vehicle
		&& req.type() != register_objtype_junction_device) {
		ret = register_status_error;
	} else {
		item = find_vehicle_vid(vin);
		if (item) {
			if(req.type() != register_objtype_junction_device) {
				item->acctoken = req.account_acstoken();
			}
			ret = register_status_success;
		} else {
			std::string acctoken;
			if(req.type() != register_objtype_junction_device) {
				acctoken = req.account_acstoken();
			}
			item = add_vehicle(vin, acctoken);
			if (!item) {
				ret = register_status_error;
			} else {
				ret = register_status_success;
				bcreate = true;
			}
		}
	}
	register_reply rep;
	rep.set_status(ret);
	if (item) {
		log.d(INDEXING_SNAPSHOT_TAG,
			"regist client [%s] ret %d, uid %s\n",
			vin.c_str(), ret, item->uid.c_str());
		rep.set_access_token(item->uid);
	}
	rep.set_new_created(bcreate);
	wa_response response;
	std::string rdata;
	rep.SerializeToString(&rdata);
	response.response((void*)rdata.c_str(), rdata.length());
	return 0;
}

int vehicle_mgr::vehicle_remove(std::string &key,
	uri &url, void* data, size_t sz)
{
	assert(nullptr != data);
	//todo
	// std::string vin = url.query_value(key.c_str());
	// snapshot_request req;
	// req.ParseFromArray(data, sz);
	return 0;
}

int vehicle_mgr::get_vehicle_id(std::string& uid, std::string &vid)
{
	if (uid.length() == 0) {
		return -EBADPARM;
	}
	auto* item = find_vehicle_uid(uid);
	if (!item) {
		return -ENOTFOUND;
	}
	vid = item->vid;
	return 0;
}

vehicle_item* 
vehicle_mgr::find_vehicle_uid(std::string& uid)
{
	auto* ret = avl_find(_vehicle_uid_tree, 
		MAKE_FIND_OBJECT(uid, vehicle_item, uid, avlnode),
		vehicle_item::vehicle_uid_compare);
	if (nullptr == ret) {
		return nullptr;
	}
	return AVLNODE_ENTRY(vehicle_item, avlnode, ret);
}

int vehicle_mgr::remove_vehicle_by_uid(std::string& uid)
{
	auto* item = find_vehicle_uid(uid);
	if (!item) {
		return -ENOTFOUND;
	}
	avl_remove(&_vehicle_uid_tree, &item->avlnode);
	avl_remove(&_vehicle_vin_tree, &item->avlvinnode);
	listnode_del(item->ownerlist);
	delete item;
	return 0;
}

int vehicle_item::vehicle_uid_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(vehicle_item, avlnode, a);
	auto* bb = AVLNODE_ENTRY(vehicle_item, avlnode, b);
	int ret = aa->uid.compare(bb->uid);
	if (ret > 0) { return 1; }
	else if (ret < 0) { return -1; }
	else { return 0; }
	return 0;
}

vehicle_item* 
vehicle_mgr::find_vehicle_vid(std::string &vin)
{
	auto* ret = avl_find(_vehicle_vin_tree, 
		MAKE_FIND_OBJECT(vin, vehicle_item, vid, avlvinnode),
		vehicle_item::vehicle_vin_compare);
	if (nullptr == ret) {
		return nullptr;
	}
	return AVLNODE_ENTRY(vehicle_item, avlvinnode, ret);
}

int vehicle_mgr::remove_vehicle_by_vid(std::string &vin)
{
	auto* item = find_vehicle_vid(vin);
	if (!item) {
		return -ENOTFOUND;
	}
	avl_remove(&_vehicle_uid_tree, &item->avlnode);
	avl_remove(&_vehicle_vin_tree, &item->avlvinnode);
	listnode_del(item->ownerlist);
	delete item;
	return 0;
}

int vehicle_item::vehicle_vin_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(vehicle_item, avlvinnode, a);
	auto* bb = AVLNODE_ENTRY(vehicle_item, avlvinnode, b);
	int ret = aa->vid.compare(bb->vid);
	if (ret > 0) { return 1; }
	else if (ret < 0) { return -1; }
	else { return 0; }
	return 0;
}


}}	//zas::vehicle_indexing




