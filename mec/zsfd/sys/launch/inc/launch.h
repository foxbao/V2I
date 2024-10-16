#ifndef __CXX_ZAS_SYSTEM_LAUNCH_H__
#define __CXX_ZAS_SYSTEM_LAUNCH_H__


#include "userinfo.h"
#include "mware/rpc/rpcservice.h"
#include "launch_pkg_mgr.h"

namespace zas {
namespace system {

using namespace zas::mware::rpc;

class launch
{
public: 
	launch();
	~launch();

public:
	int run(int argc, char *argv[]);
	int launch_system_app(void);
	int launch_app(const char* lauri);

private:
	int init_filesystem(void);
	int init_signal(void);
	int start_syssvr(void);
	int start_evloop(void);
	int launch_system_app_thread(void);
	int load_package_config(const char* pkgname, bool system);
	int load_service_config(void);
	int load_service_by_appconfig(const char* file);

	int init_mssdev_mounter(void);
	
private:
	appinfo_mgr _appinfo_mgr;
	service_mgr	_service_mgr;
	launch_pkg_mgr	_launch_pkg_mgr;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(launch);
};


}} // end of namespace zas::system

#endif /*__CXX_ZAS_SYSTEM_LAUNCH_H__*/
