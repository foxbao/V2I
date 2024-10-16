
#include "utils/json.h"
#include "utils/mutex.h"

#include "userinfo.h"

namespace zas {
namespace system {

#define USERINFO_SYSTEM_APP_INFO_PATH   "file:///zassys/etc/zlaunch/sysapp-profile.json"
#define USERINFO_USER_APP_INFO_PATH "file:///zassys/etc/zalaunch/userprofile"

using namespace std;
using namespace zas::utils;

appinfo_mgr::appinfo_node::appinfo_node(const char* name, jsonobject& jobj, bool bsysapp)
: _flags(0)
, _package_name(name)
, _gids_count(0)
, _data_path(NULL)
{
	assert(NULL != _package_name);

	// load uid
	jsonobject& juid = jobj["uid"];
	if (!juid.is_number()) return;
	_uid = (uid_t)juid.to_number();
	
	// load gids
	jsonobject& jgids = jobj["gids"];
	int gids_cnt = jgids.count();
	memset(_gids, 0, sizeof(_gids));

	if (!jgids.is_array()) {
		_gids_count = 1;
		_gids[0] = _uid;
	}
	else {
		if (gids_cnt < 0 || gids_cnt > USERINFO_USER_MAX_GROUP)
			return;
		_gids_count = gids_cnt;
	
		for (int i = 0; i < gids_cnt; ++i) {
			jsonobject& gids_item = jgids[i];
			if (!gids_item.is_number()) return;
			_gids[i] = (gid_t)gids_item.to_number();
		}
	}
	
	// load the directory paths
	jsonobject& jpaths = jobj["app_paths"];
	_data_path = jpaths["data"].to_string();
	if (NULL == _data_path) return;
	_f.is_sysapp = bsysapp ? 1 : 0;
	_f.loaded = 1;
}

appinfo_mgr::appinfo_node::~appinfo_node()
{
}

appinfo_mgr::appinfo_mgr()
: _sysapp_info(NULL)
, _userapp_info(NULL)
, _appinfo_tree(NULL)
{
	listnode_init(_appinfo_list);
}

appinfo_mgr::~appinfo_mgr()
{
	if (_sysapp_info) {
		_sysapp_info->release();
		_sysapp_info = NULL;
	}
	if (_userapp_info) {
		_userapp_info->release();
		_userapp_info = NULL;
	}
	release_appinfo_tree();
}

void appinfo_mgr::release_appinfo_tree(void)
{
	auto_mutex am(_mut);
	while (!listnode_isempty(_appinfo_list)) {
		listnode_t* node = _appinfo_list.next;
		listnode_del(*node);

		auto* app_node = list_entry(appinfo_node, _ownerlist, node);
		delete app_node;
	}
}

int appinfo_mgr::appinfo_node_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(appinfo_node, _avlnode, a);
	auto* bb = AVLNODE_ENTRY(appinfo_node, _avlnode, b);
	int ret = strcmp(aa->_package_name, bb->_package_name);
	if (ret > 0) return 1;
	else if (ret < 0) return -1;
	else return 0;
}

int appinfo_mgr::load_appinfo_node(jsonobject& jobj, bool bsysapp)
{
	if (!jobj.is_object()) {
		return -1;
	}

	// check if the package existed
	const char* pkg_name = jobj["package_name"].to_string();
	if (NULL == pkg_name) return -2;

	_mut.lock();
	avl_node_t* tmp = avl_find(_appinfo_tree,
		MAKE_FIND_OBJECT(pkg_name, appinfo_node, _package_name, _avlnode),
		appinfo_node_compare);
	_mut.unlock();

	if (tmp) return -3;

	// create the appinfo_node object
	auto* node = new appinfo_node(pkg_name, jobj, bsysapp);
	if (!node || !node->_f.loaded) {
		delete node;
		return -4;
	}

	// add it to appinfo list
	auto_mutex am(_mut);
	if (avl_insert(&_appinfo_tree, &node->_avlnode,
		appinfo_node_compare)) {
		delete node;
		return -5;
	}
	listnode_add(_appinfo_list, node->_ownerlist);
	return 0;
}

int appinfo_mgr::load_appinfo(jsonobject& cfg, bool bsysapp)
{
	if (!cfg.is_object()) {
		return -1;
	}
	
	jsonobject &apps = cfg["apps"];
	if (!apps.is_array()) {
		return -2;
	}

	uint32_t count = apps.count();
	if (0 == count) {
		return -3;
	}

	for (int i = 0; i < count; ++i) {
		load_appinfo_node(apps[i], bsysapp);
	}
	return 0;
}

int appinfo_mgr::init()
{
	if (_sysapp_info) return 0;

	// load system app config file
	_sysapp_info = &json::loadfromfile(USERINFO_SYSTEM_APP_INFO_PATH);
	if (_sysapp_info->is_null()
		|| load_appinfo(*_sysapp_info, true)) {
		return -1;
	}

	// load user app config file
	// we allow user app config file as empty which means
	// no user app has been installed
	_userapp_info = &json::loadfromfile(USERINFO_USER_APP_INFO_PATH);
	if (!_userapp_info->is_null()
		&& load_appinfo(*_userapp_info, false)) {
		return -2;
	}
	return 0;
}

int appinfo_mgr::get_appinfo(const char* pkg_name, appinfo* info, bool lock)
{
	if (!pkg_name || !*pkg_name) {
		return -EBADPARM;
	}

	// we need lock
	auto_mutex am(_mut);
	// see if we can find the package
	avl_node_t* node = avl_find(_appinfo_tree,
		MAKE_FIND_OBJECT(pkg_name, appinfo_node, _package_name, _avlnode),
		appinfo_node_compare);
	if (NULL == node) return -ENOTFOUND;

	auto* app_node = AVLNODE_ENTRY(appinfo_node, _avlnode, node);

	// see if we need to lock the object
	if (lock) app_node->_f.locked = 1;

	// copy data
	if (!info) return 0;

	info->package_name = app_node->_package_name;
	info->uid = app_node->_uid;
	info->gid_count = app_node->_gids_count;
	memcpy(info->gids, app_node->_gids, sizeof(info->gids));
	info->data_path = app_node->_data_path;
	info->data_path = (1 == app_node->_f.is_sysapp) ? true : false;
	return 0;
}

}} // end of namespace zas::system
/* EOF */
