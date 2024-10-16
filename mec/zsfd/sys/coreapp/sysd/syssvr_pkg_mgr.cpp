
#include "inc/launch_pkg.h"
#include "inc/syssvr_pkg_mgr.h"
#include "utils/evloop.h"
#include "utils/buffer.h"
#include "utils/datapool.h"

namespace zas {
namespace syssvr {

#define ZAS_SYSSVR_LAUNCH_ELE "zas.system.worker"
#define ZAS_SYSSVR_LAUNCH_PKG "zas.system"
#define ZAS_SYSSVR_LAUNCH_SERVICE "worker"

using namespace zas::utils;

bool package_notify(void* owner, evlclient sender,
	const package_header& pkghdr,
	const triggered_pkgevent_queue& queue)
{
	auto *handle = reinterpret_cast<syssvr_pkg_mgr*> (owner);
	assert(NULL != handle);
	return handle->handle_daemons_package(sender, pkghdr, queue);
}

void datapool_notify(void* owner, void* data, size_t sz)
{
	auto *handle = reinterpret_cast<syssvr_pkg_mgr*> (owner);
	assert(NULL != handle);
	int ret = handle->datapool_noitfy(data, sz);
}

struct launch_pkg_info{
	uint16_t pkg_name;
	uint16_t inst_name;
	uint16_t pkg_info;
	uint16_t bufsz;
	char buf[0];
};

launch_app_info::launch_app_info()
: pkg_name(0), inst_name(0) 
, cmd(0), bufsize(0)
{
	validity.all = 0;
}

syssvr_pkg_mgr::syssvr_pkg_mgr()
{
}

syssvr_pkg_mgr::~syssvr_pkg_mgr()
{
	auto* evl = evloop::inst();
	evl->remove_package_listener(LAUNCH_SYSSVR_CTRL_REP,
		package_notify, this);	
	auto dpele = datapool::inst()->get_element(ZAS_SYSSVR_LAUNCH_ELE,
		false);
	if (dpele.get())
		dpele->remove_listener(datapool_notify, this);
}

int syssvr_pkg_mgr::init_syssvr_package(void)
{
	evloop *evl = evloop::inst();
	evl->add_package_listener(LAUNCH_SYSSVR_CTRL_REP,
		package_notify, this);
	auto dpele = datapool::inst()->create_element(ZAS_SYSSVR_LAUNCH_ELE,
		false, false);
	return dpele->add_listener(datapool_notify, this);
}

int syssvr_pkg_mgr::datapool_noitfy(void* data, size_t sz)
{
	assert(NULL != data);
	assert(sz >= 0);
	auto* lpkg = reinterpret_cast<launch_pkg_info*>(data);
	if (sz <= sizeof(launch_pkg_info))
		return -EBADPARM;
	return launch_app(lpkg->buf + lpkg->pkg_name,
		lpkg->buf + lpkg->inst_name,
		lpkg->buf + lpkg->pkg_info);
}

bool syssvr_pkg_mgr::handle_daemons_package(evlclient sender,
    const package_header& pkghdr,
	const triggered_pkgevent_queue& queue)
{
	// check message need handle
	if (!check_package_id(sender, pkghdr.pkgid))
		return false;
	if (LAUNCH_SYSSVR_CTRL_REP == pkghdr.pkgid)
		return handle_svr_app_reply(sender, pkghdr, queue);
	return false;
}


/**
 * @brief handle sever replay package
 * @param  sender			sender
 * @param  pkghdr			package hdr
 * @return true 			handled
 * @return false 			no handle
 */
bool syssvr_pkg_mgr::handle_svr_app_reply(evlclient sender,
	const package_header& pkghdr,
	const triggered_pkgevent_queue& queue)
{
	auto* ev = queue.dequeue();
	if (NULL == ev) return false;

	launch_app_reply* reply = (launch_app_reply*)
		alloca(pkghdr.size);
	assert(NULL != reply);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(NULL != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)reply, pkghdr.size);
	assert(sz == pkghdr.size);

	int status = reply->result;
	// update info of event
	ev->write_outputbuf(&status, sizeof(status));
	return true;
}


int syssvr_pkg_mgr::launch_app(const char* pkg_name,
	const char* inst_name, const char* pkg_info)
{
	size_t sz = strlen(pkg_name) + 1
		+ strlen(inst_name) + 1;
	if (pkg_info && *pkg_info) {
		sz += strlen(pkg_info) + 1;
	}

	//create package
	launch_app_request_pkg* svrreq = new(alloca(sizeof(*svrreq) + sz))
		launch_app_request_pkg(sz);
	launch_app_request& info = svrreq->payload();
	// fill package type
	info.request = app_request_launch;

	// fill pkg name
	info.appinfo.pkg_name = info.appinfo.bufsize;
	strcpy((info.appinfo.buf + info.appinfo.pkg_name), pkg_name);
	info.appinfo.validity.m.pkg_name = 1;
	info.appinfo.bufsize += strlen(pkg_name) + 1;

	// fill inst name
	info.appinfo.inst_name = info.appinfo.bufsize;
	strcpy((info.appinfo.buf + info.appinfo.inst_name), inst_name);
	info.appinfo.validity.m.inst_name = 1;
	info.appinfo.bufsize += strlen(inst_name) + 1;

	// fill package command
	if (pkg_info && *pkg_info) {
		info.appinfo.cmd = info.appinfo.bufsize;
		strcpy((info.appinfo.buf + info.appinfo.cmd), pkg_info);
		info.appinfo.validity.m.cmd = 1;
		info.appinfo.bufsize += strlen(pkg_info) + 1;
	}
	//get launch cli
	evlclient launchcli = evloop::inst()->getclient(ZAS_SYSSVR_LAUNCH_PKG,
		ZAS_SYSSVR_LAUNCH_SERVICE);

	evpoller poller;
	auto* ev = poller.create_event(evp_evid_package_with_seqid,
		LAUNCH_SYSSVR_CTRL_REP, svrreq->header().seqid);
	assert(NULL != ev);
	ev->submit();
	printf("launch app before send %p\n", launchcli.get());
	if (nullptr == launchcli.get()) {
		return -ENOTAVAIL;
	}
	size_t sendsz = launchcli->write((void*)svrreq, sizeof(*svrreq) + sz);	
	if (sendsz < sizeof(*svrreq) + sz) {
		return -EBADOBJ;
	}
	printf("syssvr send launchinfo ,wait for reply\n");
	if (poller.poll(5000)) {
		return -4;
	}
	ev = poller.get_triggered_event();
	assert(!poller.get_triggered_event());
	int status = -1;
	ev->read_outputbuf(&status, sizeof(status));
	return status;
}

bool syssvr_pkg_mgr::check_package_id(evlclient sender, uint32_t pkgid)
{
	//pkgid is must be regist
	switch (pkgid)
	{
	case LAUNCH_SYSSVR_CTRL_REP:
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
