#include "inc/host.h"

#include <signal.h>

#include "utils/cmdline.h"
#include "utils/evloop.h"
#include "utils/thread.h"
#include "utils/timer.h"
#include "utils/uri.h"

#include "mware/rpc/rpcmgr.h"

namespace zas {
namespace host {

#define HOST_CONTAINER_ID "hostid"
#define HOST_SERVICE_ID "service"
#define HOST_SERVICE_INST "inst"
#define HOST_SERVICE_EXEC "executive"
#define HOST_SERVICE_MODE "mode"
#define HOST_SERVICE_LISTEN_PORT "listenport"
#define HOST_SERVICE_IPADDR "ipaddr"

using namespace zas::utils;
using namespace zas::mware::rpc;

class host_thread : public thread
{
public:
	host_thread(host* sysl)
	: _sysl(sysl) {}

	~host_thread() {

	}

	int run(void) {
		return _sysl->start_host_service();
	}
	
private:
	host* _sysl;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(host_thread);
};

host::host()
: _listen_port(0)
{
	_client_ipv4.clear();
}

host::~host()
{
}

int host::run(int argc, char *argv[])
{
	uri hostcmd(argv[2]);
	_client_name = hostcmd.get_fullpath();
	_inst_name = hostcmd.query_value(HOST_CONTAINER_ID);
	if (_client_name.empty() || _inst_name.empty()) return -1;
	_service_name = hostcmd.query_value(HOST_SERVICE_ID);
	_service_inst = hostcmd.query_value(HOST_SERVICE_INST);
	_executive = hostcmd.query_value(HOST_SERVICE_EXEC);
	_startmode = hostcmd.query_value(HOST_SERVICE_MODE);
	if (_service_name.empty() || _executive.empty()) return -2;
	_client_ipv4 = hostcmd.query_value(HOST_SERVICE_IPADDR);
	std::string port = hostcmd.query_value(HOST_SERVICE_LISTEN_PORT);
	if (port.empty()) {
		_listen_port = 0;
	} else {
		_listen_port = atoi(port.c_str());
		printf("host start ip %s, port %u\n", _client_ipv4.c_str(), _listen_port);
	}
	start_evloop();
	return 0;
}

int host::start_host_service(void)
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

	printf("host loading service: ");
	if (_service_name.length())
		printf("service name %s, ", _service_name.c_str());
	if (_service_name.length())
		printf("service inst %s, ", _service_inst.c_str());
	if (_service_name.length())
		printf("executive %s.", _executive.c_str());
	printf("\n");

	rpchost zrpchost = rpcmgr::inst()->load_service(
		_service_name.c_str(), _service_inst.c_str(),
		_executive.c_str(), _startmode.c_str());
	return 0;
}

int host::start_service_thread(void)
{
	auto *phost = new host_thread(this);
	phost->start();
	phost->release();
	return 0;
}

class host_evloop_client_listener: public evloop_listener
{
public:
	host_evloop_client_listener(host* sysl)
	:_sysl(sysl){
	}
	void connected(evlclient client) {
		// launch sysd after evloop initialize
		_sysl->start_service_thread();
	}
private:
	host* _sysl;

};
int host::start_evloop()
{
	evloop *evl = evloop::inst();
	evl->setrole(evloop_role_client);

	// set the name of launch as "zas.system.sysd"
	evl->updateinfo(evlcli_info_client_name, _client_name.c_str())
		->updateinfo(evlcli_info_instance_name, _inst_name.c_str())
		->updateinfo(evlcli_info_client_ipv4, _client_ipv4.c_str())
		->updateinfo(evlcli_info_listen_port, _listen_port)
		->updateinfo(evlcli_info_commit);

	host_evloop_client_listener lnr(this);
	evl->add_listener("host_evloop_listener", &lnr);

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
	return 	zas::host::host().run(argc, argv);
}