
#include "rpc/rpcservice.h"
#include "inc/serviceinfo.h"

#include "std/list.h"
#include "utils/avltree.h"
#include "utils/dir.h"
#include "utils/mutex.h"

namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;

service_info::service_info()
: _service_id(0), _version(0)
, _flags(0), _refcnt(1), _port(0)
{
	_inst_name.clear();
	_ipaddr.clear();
	listnode_init(_inst_list);
}

service_info::service_info(const char* pkgname,
	const char* servicename,
	const char* instname,
	const char* executive,
	const char* ipaddr, uint32_t port,
	uint32_t version, uint32_t attributes)
: _service_id(0), _version(version)
, _flags(attributes), _refcnt(1), _port(port)
{
	_pkg_name = pkgname;
	_service_name = servicename;
	if (executive && *executive)
		_executive = executive;
	if (!instname || !*instname) {
		_f.is_default = 1;
		listnode_init(_inst_list);
	} else {
		_inst_name = instname;
		_f.is_default = 0;
	}
	if (!ipaddr || !*ipaddr) {
		_ipaddr.clear();
	} else {
		_ipaddr = ipaddr;
	}
}

service_info::~service_info()
{
	if (is_default_service()) {	
		release_all_instance_unlock();
	}
}

int service_info::addref()
{
	return __sync_add_and_fetch(&_refcnt, 1);
}

int service_info::release()
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0) delete this;
	return cnt;
}

int service_info::add_instance(const char* instname)
{
	return -1;
}

int service_info::add_instance_unlock(service_info* inst)
{
	if (!inst) return -EBADPARM;
	//default service can add instance
	if (!is_default_service())
		return -ENOTAVAIL;
	//check service can multiple instance
	if (!check_multi_instance())
		return -ENOTALLOWED;
	//check instance is exist
	if (find_inst_unlock(inst->_inst_name.c_str()))
		return -EEXISTS;

	listnode_add(_inst_list, inst->ownerlist);
	return 0;
}

int service_info::remove_instance_unlock(const char* instname)
{
	if (!instname || !*instname) return -EBADPARM;
	//default servce can add instance
	if (!is_default_service())
		return -ENOTAVAIL;
	service_info* inst = find_inst_unlock(instname);
	if (!inst) return -ENOTEXISTS;
	listnode_del(inst->ownerlist);
	delete inst;
	return 0;
}

service_info* service_info::find_inst_unlock(const char* instname)
{
	if (listnode_isempty(_inst_list)) return NULL;
	listnode_t *nd = _inst_list.next;
	service_info* inst = NULL;;
	for (; nd != &_inst_list; nd = nd->next) {
		inst = LIST_ENTRY(service_info, ownerlist, nd);
		if (!strcmp(inst->_inst_name.c_str(), instname))
			return inst;
	}
	return NULL;
}

int service_info::release_all_instance_unlock(void)
{
	//default servce can add instance
	if (!is_default_service())
		return -ENOTAVAIL;
	while (!listnode_isempty(_inst_list)) {
		service_info* nd = LIST_ENTRY(service_info,	\
			ownerlist, _inst_list.next);
		assert(NULL != nd);
		listnode_del(nd->ownerlist);
		delete nd;
	}
	return 0;
}

uint32_t service_info::get_string_size(void)
{
	uint32_t sz = _pkg_name.length() + 1
		+ _service_name.length() + 1
		+ _executive.length() + 1;
	
	if (!is_default_service())
		sz += _inst_name.length() + 1;
		
	if (_ipaddr.length() > 0)
		sz += _ipaddr.length() + 1;
	return sz;
}

int service_info::load_info(service_pkginfo* info)
{
	if (_pkg_name.length() < 1 || _service_name.length() < 1)
		return -ENOTAVAIL;

	if (_pkg_name.length() > 0){
		info->pkgname = info->bufsize;
		strcpy(info->buf + info->pkgname,
			_pkg_name.c_str());
		info->validity.m.pkgname = 1;
		info->bufsize +=_pkg_name.length() + 1;
	}
	if (_service_name.length() > 0){
		info->name = info->bufsize;
		strcpy(info->buf + info->name,
			_service_name.c_str());
		info->validity.m.name = 1;
		info->bufsize += _service_name.length() + 1;
	}
	
	if (!is_default_service()) {
		info->inst_name = info->bufsize;
		strcpy(info->buf + info->inst_name,
			_inst_name.c_str());
		info->validity.m.inst_name = 1;
		info->bufsize += _inst_name.length() + 1;
	}

	if (_executive.length() > 0){
		info->executive = info->bufsize;
		strcpy(info->buf + info->executive,
			_executive.c_str());
		info->validity.m.executive = 1;
		info->bufsize += _executive.length() + 1;
	}

	if (_ipaddr.length() > 0){
		info->ipaddr = info->bufsize;
		strcpy(info->buf + info->ipaddr,
			_ipaddr.c_str());
		info->validity.m.ipaddr = 1;
		info->bufsize += _ipaddr.length() + 1;
	}

	info->port = _port;
	info->version = _version;
	info->validity.m.version = 1;
	if (_service_id > 0) {
		info->id = _service_id;
		info->validity.m.id = 1;
	}
	info->accessible = _f.global_accessible;
	info->shared = _f.shared;
	info->signleton = _f.singleton;
	info->validity.m.accessible = 1;
	info->validity.m.shared = 1;
	info->validity.m.signleton = 1;
	return 0;
}

int service_info::check_info(service_pkginfo &info,
	service_pkginfo_validity_u &val)
{
	if (!info.validity.m.pkgname || !info.validity.m.name)
		return -EBADPARM;
	std::string tmppkgname = info.buf + info.pkgname;
	std::string tmpname = info.buf + info.name;

	if (strcmp(info.buf + info.pkgname, _pkg_name.c_str())
		|| strcmp(info.buf + info.name, _service_name.c_str()))
		return -ELOGIC;
	if (!is_default_service() && info.validity.m.inst_name) {
		if (strcmp(info.buf + info.inst_name, _inst_name.c_str()))
			return -ELOGIC;
	}
	val.m.pkgname = 1;
	val.m.name = 1;
	val.m.inst_name = 1;
	if (info.validity.m.version) {
		if (info.version == _version)
			val.m.version = 1;
	}
	if (info.validity.m.executive) {
		if (!strcmp(info.buf + info.executive, _executive.c_str()))
			val.m.executive = 1;
	}
	if (info.validity.m.ipaddr) {
		if (!strcmp(info.buf + info.ipaddr, _ipaddr.c_str()))
			val.m.ipaddr = 1;
	}
	if (info.validity.m.port) {
		if (info.port == _port)
			val.m.port = 1;
	}
	if (info.accessible == _f.global_accessible)
		val.m.accessible = 1;
	if (info.shared == _f.shared)
		val.m.shared = 1;
	if (info.signleton == _f.singleton)
		val.m.signleton = 1;
	return 0;
}

int service_info::update_info(service_pkginfo &info,
	service_pkginfo_validity_u &val)
{
	if (_pkg_name.length() > 0 || _service_name.length() > 0)
		return -EEXISTS;

	if (!info.validity.m.pkgname || !info.validity.m.name)
		return -EBADPARM;

	_pkg_name = info.buf + info.pkgname;
	_service_name = info.buf + info.name;
	val.m.pkgname = 1;
	val.m.name = 1;
	if (info.validity.m.inst_name) {
		_inst_name = info.buf + info.inst_name;
		val.m.inst_name = 1;
		_f.is_default = 0;
	} else {
		_f.is_default = 1;
	}

	if (info.validity.m.version) {
		_version = info.version;
		val.m.version = 1;
	}

	if (info.validity.m.executive) {
		_executive = info.buf + info.executive;
		val.m.executive = 1;
	}

	if (info.validity.m.ipaddr) {
		_ipaddr = info.buf + info.ipaddr;
		val.m.ipaddr = 1;
	}

	if (info.validity.m.port) {
		_port = info.port;
		val.m.port = 1;
	}
	
	_f.global_accessible = info.accessible;
	val.m.accessible = 1;
	_f.shared = info.shared;
	val.m.shared = 1;
	_f.singleton = info.signleton;
	val.m.signleton = 1;
	return 0;
}

int service_info::update_info(cmd_service_info &info,
	cmd_service_info_validity_u &val)
{
	if (_pkg_name.length() > 0 || _service_name.length() > 0)
		return -EEXISTS;

	if (!info.validity.m.pkgname || !info.validity.m.name)
		return -EBADPARM;

	_pkg_name = info.buf + info.pkgname;
	_service_name = info.buf + info.name;
	val.m.pkgname = 1;
	val.m.name = 1;
	if (info.validity.m.inst_name) {
		_inst_name = info.buf + info.inst_name;
		val.m.inst_name = 1;
		_f.is_default = 0;
	} else {
		_f.is_default = 1;
	}
	return 0;
}

service_mgr_base::service_mgr_base()
: _service_info_tree(NULL)
{
	listnode_init(_service_info_list);
}

service_mgr_base::~service_mgr_base()
{

}

int service_mgr_base::add_service_unlock(service_info *info)
{
	if (avl_insert(&_service_info_tree, &(info->avlnode),
		service_info_compare)) {
		return -ELOGIC;
	}
	listnode_add(_service_info_list, info->ownerlist);
	return 0 ;
}
service_info* service_mgr_base::find_service_unlock(const char* pkgname,
	const char* servicename)
{
	service_info tmpimpl(pkgname, servicename, NULL, NULL, nullptr, 0, 0, 0);
	avl_node_t* nd = avl_find(_service_info_tree, &(tmpimpl.avlnode),
		service_info_compare);
	if (!nd) return NULL;
	return AVLNODE_ENTRY(service_info, avlnode, nd);
}

int service_mgr_base::service_info_compare(avl_node_t*aa, avl_node_t *bb)
{
	service_info* aimpl = AVLNODE_ENTRY(service_info, avlnode, aa);
	service_info* bimpl = AVLNODE_ENTRY(service_info, avlnode, bb);
	int ret = strcmp(aimpl->_pkg_name.c_str(), bimpl->_pkg_name.c_str());
	if (0 == ret)
		ret = strcmp(aimpl->_service_name.c_str(),
			bimpl->_service_name.c_str());
	if (ret > 0) return 1;
	else if (ret < 0) return -1;
	else return 0;
}

int service_mgr_base::remove_service_unlock(const char* pkgname,
	const char* servicename)
{
	service_info* impl = find_service_unlock(pkgname, servicename);
	if (!impl) return -ENOTEXISTS;

	avl_remove(&_service_info_tree, &impl->avlnode);
	listnode_del(impl->ownerlist);
	delete impl;
	return 0;
}

int service_mgr_base::release_all_service_unlock(void)
{
	service_info *nd;
	while (!listnode_isempty(_service_info_list)) {
		nd = LIST_ENTRY(service_info, ownerlist, _service_info_list.next);
		listnode_del(nd->ownerlist);
		delete nd;
	}
	_service_info_tree = NULL;
	return 0;
}

}}}
