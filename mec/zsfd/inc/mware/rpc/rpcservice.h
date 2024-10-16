#ifndef __CXX_ZAS_RPC_SERVICE_IF_H__
#define __CXX_ZAS_RPC_SERVICE_IF_H__

#include "mware/mware.h"
#include "std/smtptr.h"
#include "utils/uri.h"

namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;

class MWARE_EXPORT service_object
{
public:
	service_object() = delete;
	~service_object() = delete;
	int addref();
	int release();

	/**
	 * @brief add service instance
	 * @param  instname		instance name
	 * @return int 		0:success
	 */
	int add_instance(const char* instname);

	/**
	 * @brief remove service instance
	 * @param  inst		instance name
	 * @return int 		0:success
	 */
	int remove_instance(const char* instname);
};

typedef zas::smtptr<service_object> service;

class MWARE_EXPORT service_mgr
{
public:
	service_mgr();
	~service_mgr();

	/**
	 * @brief load service config from file
	 * @param  file			config file
	 * @return int 		0:success
	 */
	int load_config_file(uri &file);

	/**
	 * @brief load service from manifest file
	 * @param  file			manifest file
	 * @return int 		0:success
	 */
	int load_manifest_file(uri &file);

	/**
	 * @brief save service config to file
	 * @param  file			dest config file
	 * @return int 		0:success
	 */
	int save_config_file(uri &file);

	/**
	 * @brief add service to manager
	 * @param  pkgname			service package name
	 * @param  servicename			service name
	 * @param  version			service version
	 * @param  attribute		service attirbutes
	 * global, shared, signleton
	 * @return service 
	 */
	service add_service(const char* pkgname, const char* servicename, 
		const char* executive, const char* ipaddr, uint32_t port, uint32_t version, uint32_t attributes);

	/**
	 * @brief remove service
	 * @param  pkgname			service package name
	 * @param  servicename			service name
	 * @return int 
	 */
	int remove_service(const char* pkgname, const char* servicename);

	/**
	 * @brief get service of package & service name
	 * @param  pkgname			service package name
	 * @param  servicename			service name
	 * @return service 
	 */
	service get_service(const char* pkgname, const char* servicename);

private:
	void* _data;
};

}}} // end of namespace zas::mware::zrpc

#endif /*__CXX_ZAS_ZRPC_SERVICE_IF_H__*/