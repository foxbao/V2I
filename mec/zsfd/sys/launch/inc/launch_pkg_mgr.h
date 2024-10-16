#ifndef __CXX_ZAS_LAUNCH_PKG_MGR_H__
#define __CXX_ZAS_LAUNCH_PKG_MGR_H__

#include <string>
#include "utils/evloop.h"
#include "utils/wait.h"

namespace zas{
namespace system{

using namespace zas::utils;

class launch;

class launch_pkg_mgr
{
public:
	launch_pkg_mgr();
	~launch_pkg_mgr();

public:
	int init_launch_pkg(launch *lch);
	bool handle_evl_package(evlclient sender,
		const package_header& pkghdr);
private:
	bool handle_app_request(evlclient sender,
		const package_header& pkghdr);

	bool check_package_id(evlclient sender, uint32_t pkgid);

private:
	launch *_launch;
};

}} // end of namespace zas::syssvr


#endif /* __CXX_ZAS_LAUNCH_PKG_MGR_H__
/* EOF */
