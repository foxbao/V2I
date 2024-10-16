
#ifndef __CXX_ZAS_RPC_SERVICE_INFO_H__
#define __CXX_ZAS_RPC_SERVICE_INFO_H__

#include <string>
#include "std/list.h"
#include "utils/avltree.h"
#include "rpc_pkg_def.h"

namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;

enum singleton_type
{
	singleton_type_unknown = 0,
	singleton_type_none,
	singleton_type_globally,
	singleton_type_device_only,
};

class service_info
{
public:
	service_info();
	service_info(const char* pkgname,
		const char* servicename,
		const char* instname,
		const char* executive,
		const char* ipaddr, uint32_t port,
		uint32_t version, uint32_t attributes);
	virtual ~service_info();
	
	int addref();
	int release();

	// service_info is not implement this fuction
	// it must be implement in derived class of service_info
	virtual int add_instance(const char* instname);

	int add_instance_unlock(service_info* inst);
	service_info* find_inst_unlock(const char* instname);
	int remove_instance_unlock(const char* instname);

	bool check_multi_instance(void) {
		return (singleton_type_none == _f.singleton) ? true : false;
	}

	bool is_default_service(void) {
		return (1 == _f.is_default) ? true : false;
	}

	bool is_shared_service(void) {
		return (1 == _f.shared) ? true : false;
	}

	bool is_starting(void) {
		return (1 == _f.starting || 1 == _f.running) ? true : false;
	}

	bool is_running(void) {
		return (1 == _f.running) ? true : false;
	}

	uint32_t get_string_size(void);

	int load_info(service_pkginfo* info);
	int check_info(service_pkginfo &info, service_pkginfo_validity_u &val);
	int update_info(service_pkginfo &info, service_pkginfo_validity_u &val);
	int update_info(cmd_service_info &info, cmd_service_info_validity_u &val);

public:
	listnode_t ownerlist;
	avl_node_t avlnode;
	std::string _pkg_name;
	std::string _service_name;
	std::string _executive;
	std::string _ipaddr;
	uint32_t _port;
	uint32_t _service_id;
	uint32_t _version;
	int _refcnt;
	union {
		uint32_t _flags;
		struct {
			uint32_t global_accessible : 1;
			uint32_t shared : 1;
			uint32_t singleton : 2;
			uint32_t is_global : 1;
			uint32_t is_default : 1;
			uint32_t starting : 1;
			uint32_t running : 1;
		} _f;
	};

	std::string _inst_name;
	listnode_t	_inst_list;

private:
	int release_all_instance_unlock(void);
};


class service_mgr_base
{
public:
	service_mgr_base();
	virtual ~service_mgr_base();

	int add_service_unlock(service_info *info);
	service_info* find_service_unlock(const char* pkgname,
		const char* servicename);
	static int service_info_compare(avl_node_t*aa, avl_node_t *bb);
	int remove_service_unlock(const char* pkgname,
		const char* servicename);
	int release_all_service_unlock(void);

	listnode_t _service_info_list;
	avl_node_t* _service_info_tree;
};


}}} // namespace  zas::mware::rpc

#endif /*__CXX_ZAS_ZRPC_SERVICE_INFO_H__*/