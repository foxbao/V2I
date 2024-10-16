#include "bridge.h"

#include <signal.h>

#include "utils/cmdline.h"
#include "utils/evloop.h"
#include "utils/thread.h"
#include "utils/timer.h"
#include "utils/uri.h"

#include "mware/rpc/rpcmgr.h"

#include "bridge_servicemgr.h"
#include "mqtt_mgr.h"

namespace zas {
namespace servicebridge {

using namespace zas::utils;
using namespace zas::mware::rpc;

class proxy_birdge_thread : public thread
{
public:
	proxy_birdge_thread(proxy_birdge* sysl)
	: _sysl(sysl) {}

	~proxy_birdge_thread() {

	}

	int run(void) {
		return _sysl->start_host_service();
	}
	
private:
	proxy_birdge* _sysl;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(proxy_birdge_thread);
};

proxy_birdge::proxy_birdge()
{
}

proxy_birdge::~proxy_birdge()
{
}

int proxy_birdge::run(int argc, char *argv[])
{
	uri hostcmd(argv[2]);
	_client_name = hostcmd.get_fullpath();
	_inst_name = hostcmd.query_value(HOST_CONTAINER_ID);

	start_evloop();
	return 0;
}

int proxy_birdge::start_host_service(void)
{
	evloop *evl = evloop::inst();
	while (1) {
		// todo, check evloop connect server.
		// is_running return evloop start. tobe change.
		if (evl->is_running()) {
			break;
		}
		msleep(5);
	}
	mqtt_client::inst();
	rpcmgr::inst()->register_bridge(rpcbridge_service::create_instance,
		rpcbridge_service::destory_instance);

	rpcbridge zrpchost = rpcmgr::inst()->load_bridge(BRIDGE_SERVICE_PKG,
		BRIDGE_SERVICE_NAME, nullptr);
	return 0;
}

int proxy_birdge::start_service_thread(void)
{
	auto *phost = new proxy_birdge_thread(this);
	phost->start();
	phost->release();
	return 0;
}

int proxy_birdge::start_evloop()
{
	evloop *evl = evloop::inst();
	evl->setrole(evloop_role_client);

	// set the name of launch as "zas.system.sysd"
	evl->updateinfo(evlcli_info_client_name, _client_name.c_str())
		->updateinfo(evlcli_info_instance_name, _inst_name.c_str())
		->updateinfo(evlcli_info_commit);

	// launch sysd after evloop initialize
	start_service_thread();

	// start the system server
	int ret = 0;
	while (1) {
		ret = evl->start(false);
		if (-ELOGIC == ret || 0 == ret) {
			break;
		}
		msleep(5);
	}
	return ret;
}

}}

int main(int argc, char *argv[])
{
	return 	zas::servicebridge::proxy_birdge().run(argc, argv);
}