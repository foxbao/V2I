#include "inc/launch_pkg_mgr.h"
#include "utils/evloop.h"
#include "utils/buffer.h"
#include "launch_pkg.h"
#include "inc/launch.h"


namespace zas{
namespace system{
using namespace zas::utils;

bool package_notify(void* owner, evlclient sender,
	const package_header& pkghdr,
	const triggered_pkgevent_queue& queue)
{
	auto *handle = reinterpret_cast<launch_pkg_mgr*> (owner);
	return handle->handle_evl_package(sender, pkghdr);
}

launch_app_info::launch_app_info()
: pkg_name(0), inst_name(0) 
, cmd(0), bufsize(0)
{
	validity.all = 0;
}

launch_pkg_mgr::launch_pkg_mgr()
:_launch(NULL)
{

}

launch_pkg_mgr::~launch_pkg_mgr()
{
	auto* evl = evloop::inst();
	evl->remove_package_listener(SYSSVR_LAUNCH_CTRL_REQ, package_notify, this);
}

int launch_pkg_mgr::init_launch_pkg(launch *lch)
{
	_launch = lch;
	auto* evl = evloop::inst();
	evl->add_package_listener(SYSSVR_LAUNCH_CTRL_REQ, package_notify, this);
	return 0;
}

bool launch_pkg_mgr::handle_evl_package(evlclient sender,
    const package_header& pkghdr)
{
	if (!check_package_id(sender, pkghdr.pkgid))
		return false;

	if (SYSSVR_LAUNCH_CTRL_REQ == pkghdr.pkgid)
		return handle_app_request(sender, pkghdr);
	return false;
}

// handle message from syssvr
bool launch_pkg_mgr::handle_app_request(evlclient sender,
	const package_header& pkghdr)
{
	launch_app_request* reqinfo = (launch_app_request*)
		alloca(pkghdr.size);
	assert(NULL != reqinfo);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(NULL != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)reqinfo, pkghdr.size);
	assert(sz == pkghdr.size);

	auto reqtype = (app_request_type)reqinfo->request;
	int ret = 0;

	// launch package
	if (app_request_launch == reqtype)
	{
		std::string exec = (reqinfo->appinfo.buf + reqinfo->appinfo.cmd);
		if (_launch)
			ret = _launch->launch_app(exec.c_str());
		else
			ret = -EINVALID;
	}

	size_t replysz = strlen(reqinfo->appinfo.buf
		+ reqinfo->appinfo.pkg_name) + 1
		+ strlen(reqinfo->appinfo.buf + reqinfo->appinfo.inst_name) + 1
		+ strlen(reqinfo->appinfo.buf + reqinfo->appinfo.cmd) + 1;
		
	launch_app_reply_pkg* svrrep = new(alloca(sizeof(*svrrep) + replysz))
		launch_app_reply_pkg(replysz);
	launch_app_reply& rinfo = svrrep->payload();

	rinfo.result = ret;
	svrrep->header().seqid = pkghdr.seqid;
	rinfo.appinfo.pkg_name = rinfo.appinfo.bufsize;
	strcpy((rinfo.appinfo.buf + rinfo.appinfo.pkg_name),
		reqinfo->appinfo.buf + reqinfo->appinfo.pkg_name);
	rinfo.appinfo.validity.m.pkg_name = 1;
	rinfo.appinfo.bufsize += strlen(reqinfo->appinfo.buf
		+ reqinfo->appinfo.pkg_name) + 1;

	rinfo.appinfo.inst_name = rinfo.appinfo.bufsize;
	strcpy((rinfo.appinfo.buf + rinfo.appinfo.inst_name),
		reqinfo->appinfo.buf + reqinfo->appinfo.inst_name);
	rinfo.appinfo.validity.m.inst_name = 1;
	rinfo.appinfo.bufsize += strlen(reqinfo->appinfo.buf
		+ reqinfo->appinfo.inst_name) + 1;

	rinfo.appinfo.cmd = rinfo.appinfo.bufsize;
	strcpy((rinfo.appinfo.buf + rinfo.appinfo.cmd),
		reqinfo->appinfo.buf + reqinfo->appinfo.cmd);
	rinfo.appinfo.validity.m.cmd = 1;
	rinfo.appinfo.bufsize += strlen(reqinfo->appinfo.buf
		+ reqinfo->appinfo.cmd) + 1;

	sender->write((void*)svrrep, sizeof(*svrrep) + replysz);	
	return true;
}


bool launch_pkg_mgr::check_package_id(evlclient sender, uint32_t pkgid)
{
	//pkgid is must be regist
	switch (pkgid)
	{
	case SYSSVR_LAUNCH_CTRL_REQ:
		break;
	default:
		return false;
	}

	// TODO: check pakcage sender name
	// switch((pkgid >> 22))
	// {

	// }

	return true;
}

}} // end of namespace zas::syssvr

/* EOF */
