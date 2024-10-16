#ifndef __CXX_ZAS_SYSSVR_PKG_MGR_H__
#define __CXX_ZAS_SYSSVR_PKG_MGR_H__

#include <string>
#include "utils/evloop.h"
#include "utils/wait.h"

namespace zas{
namespace syssvr{

using namespace zas::utils;

class syssvr_pkg_mgr{
public:
	syssvr_pkg_mgr();
	~syssvr_pkg_mgr();

public:
	bool handle_daemons_package(evlclient sender,
		const package_header& pkghdr,
		const triggered_pkgevent_queue& queue);
	int launch_app(const char* pkg_name,
		const char* inst_name, const char* pkg_info);
	int init_syssvr_package(void);
	int datapool_noitfy(void* data, size_t sz);
	
private:
	bool handle_svr_app_reply(evlclient sender,
		const package_header& pkghdr,
		const triggered_pkgevent_queue& queue);

private: 
	bool check_package_id(evlclient sender, uint32_t pkgid);
};

}} // end of namespace zas::syssvr


#endif /* __CXX_ZAS_SYSSVR_PKG_MGR_H__
/* EOF */
