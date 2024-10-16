
#ifndef __CXX_ZAS_LAUNCH_PACKAGE_ID_H__
#define __CXX_ZAS_LAUNCH_PACKAGE_ID_H__

#include "mware/pkg_id_def.h"
#include "utils/evloop.h"

namespace zas{
namespace syssvr{

using namespace zas::utils;
using namespace zas::mware;

//communicate with syssvr
#define SYSSVR_LAUNCH_CTRL_REQ			\
	EVL_MAKE_PKGID(SYSTEM_LAUNCH_CLSID_CTRL, 1)
#define LAUNCH_SYSSVR_CTRL_REP			\
	EVL_MAKE_PKGID(SYSTEM_LAUNCH_CLSID_CTRL, 2)

enum app_request_type
{
	app_request_launch = 0,
};

union launch_app_validity_u
{
	uint32_t all;
	struct {
		uint32_t pkg_name			: 1;
		uint32_t inst_name			: 1;
		uint32_t cmd				: 1;
	} m;
};

struct launch_app_info
{
	launch_app_info();
	launch_app_validity_u validity;
	uint16_t pkg_name;
	uint16_t inst_name;
	uint16_t cmd;
	uint16_t bufsize;
	char buf[0];
};

struct launch_app_request
{
	uint16_t request;
	launch_app_info  appinfo;
};

struct launch_app_reply
{
	int result;
	launch_app_info  appinfo;
};

EVL_DEFINE_PACKAGE(launch_app_request, SYSSVR_LAUNCH_CTRL_REQ);
EVL_DEFINE_PACKAGE(launch_app_reply, LAUNCH_SYSSVR_CTRL_REP);

}}

#endif  /*__CXX_ZAS_LAUNCH_PACKAGE_ID_H__*/