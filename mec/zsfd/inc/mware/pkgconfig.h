/** @file pkgconfig.h
 * defintion of package configuration
 */

#ifndef __CXX_ZAS_MWARE_PKGCONFIG_H__
#define __CXX_ZAS_MWARE_PKGCONFIG_H__

#include "mware/mware.h"
#include "std/smtptr.h"
#include "utils/uri.h"

namespace zas {
namespace mware {

enum singleton_type
{
	singleton_type_unknown = 0,
	singleton_type_none,
	singleton_type_globally,
	singleton_type_device_only,
};

/**
  Compile a json style package configure file
  to the binary format one
 */
class MWARE_EXPORT pkgcfg_compiler
{
public:
	pkgcfg_compiler(const zas::utils::uri& cfgfile);
	~pkgcfg_compiler();

	/**
	  Compile the loaded json-style pakcage
	  configuration file to binary format
	  @param tarfile the target file to be generated
	  @return 0 for success
	 */
	int compile(const zas::utils::uri& tarfile);

private:
	void* _data;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(pkgcfg_compiler);
};

class MWARE_EXPORT pkgcfg_service_info_object
{
public:
	pkgcfg_service_info_object() = delete;
	~pkgcfg_service_info_object() = delete;
	int addref(void);
	int release(void);
	
	/**
	 * @brief get service name
	 * @return const char* 
	 */
	const char* get_name(void);

	/**
	 * @brief get service version 
	 * @return uint32_t 		0: service not exist
	 */
	uint32_t get_version(void);

	/**
	 * @brief get service executive
	 * @return const char* 		executive
	 */
	const char* get_executive(void);

	/**
	 * @brief get service ipaddr
	 * @return const char* 		ipaddr
	 */
	const char* get_ipaddr(void);

	/**
	 * @brief get service port
	 * @return uint32_t 		
	 */
	uint32_t get_port(void);

	/**
	 * @brief get service attributes
	 * @return uint32_t 		
	 */
	uint32_t get_attributes(void);

	/**
	 * @brief get service signleton type
	 * @return singleton_type 		
	 */
	singleton_type get_singleton(void);

	/**
	 * @brief check if service is shared
	 * @return true means shared service
	 */
	bool is_shared(void);

	/**
	 * @brief get instance count of service
	 * @return uint32_t 
	 */
	uint32_t get_instance_count(void);

	/**
	 * @brief get instance name by index
	 * @param  index           instance index
	 * @return const char* 
	 */
	const char* get_instance_by_index(uint32_t index);

	/**
	 * @brief check instance in service
	 * @param  inst_name        intance name
	 * @return true 
	 * @return false 
	 */
	bool check_instance(const char* inst_name);
	
	ZAS_DISABLE_EVIL_CONSTRUCTOR(pkgcfg_service_info_object);
};

typedef zas::smtptr<pkgcfg_service_info_object> pkgcfg_service_info;

/**
 * the package config object, use this to retrive
 * the configure info for the current process or from
 * a configure file (.json)
 */
class MWARE_EXPORT pkgconfig
{
public:
	pkgconfig();
	~pkgconfig();

	/**
	  Load the package config from a file
	  @param file the file name to be loaded
	  	(absfile style uri)
	 */
	pkgconfig(const zas::utils::uri& file);

	/**
	  Directly bind to the config info of the
	  current process
	 */
	static const pkgconfig& getdefault(void);

	/**
	 * @brief get current package name
	 * @return const char* 		pkgname
	 */
	const char* get_package_name(void) const;

	/**
	 * @brief get current package version
	 * @return uint32_t 	version
	 * if ver is a.b.c.d
	 * version = a << 24 | b << 16 | c << 8 | d
	 */
	uint32_t get_version(void) const;

	/**
	 * @brief get current package api level
	 * @return uint32_t 		apilevel
	 */
	uint32_t get_api_level(void) const;

	/**
	 * @brief get curent package accesss permision
	 * @return uint32_t 		1: private
	 */
	uint32_t get_access_permission(void) const;

	/**
	 * @brief check current package has service of name
	 * @param  name				service name
	 * @return true 			have this servcie
	 * @return false 
	 */
	bool check_service(const char* name) const;

	/**
	 * @brief get the service count of package count
	 * @return uint32_t 
	 */
	uint32_t get_service_count(void) const;

	/**
	 * @brief get service info by service name
	 * @param  name      service name
	 * @return pkgcfg_service_info 
	 */
	pkgcfg_service_info get_service_by_name(const char* name) const;

	/**
	 * @brief get service info by index
	 * @param  index            service index
	 * index start from 0
	 * @return pkgcfg_service_info 
	 */
	pkgcfg_service_info get_service_by_index(uint32_t index) const;

private:
	void* _config;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(pkgconfig);
};

}} // end of namespace zas::mware
#endif /* __CXX_ZAS_MWARE_PKGCONFIG_H__ */
/* EOF */

