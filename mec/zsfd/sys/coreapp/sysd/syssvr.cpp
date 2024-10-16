#include <getopt.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <grp.h>

#include "inc/syssvr.h"

#include "utils/evloop.h"
#include "utils/datapool.h"
#include "utils/uri.h"
#include "utils/mutex.h"
#include "utils/thread.h"
#include "mware/rpc/rpcmgr.h"

namespace zas {
namespace syssvr {

#define ZRPC_SERVER_ENTRY_CONFIG_FILE	\
	"file:///zassys/etc/syssvrconfig/hostconfig.bin"
#define ZRPC_SERVER_ENTRY_SEM	"zrpcserverconfig"
#define ZRPC_PROXY_BRIDGE_PKG	"zas.system"
#define ZRPC_PROXY_BRIDGE_NAME	"rpcbridge"
#define ZRPC_SERVER_TCPIP_LISTEN_PORT	5559
#define ZRPC_PROXY_BRIDGE_INFO	"exec://zas.system?host=daemons/rpcbridge&hostid=_zas.system_rpcbridge"

using namespace zas::utils;
using namespace zas::mware::rpc;

static semaphore zrpc_service(ZRPC_SERVER_ENTRY_SEM, 1);

static void init_daemon(void)
{
	printf("zsyssvr: running as daemon.\n");

	// disable signals having impact to control terminal
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	// run in background
	int pid = fork();
	if (pid)
	{

		// this is parents
		exit(0);
	}
	else if (pid < 0)
	{
		perror("fork");
		exit(2);
	}

	if (setsid() < 0)
	{
		perror("setsid");
		exit(3);
	}

	chdir("/tmp");

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	umask(0);
	signal(SIGCHLD, SIG_IGN);
}

static void help(const char *name)
{
	fprintf(stderr, "Usage: %s [args...] [-- [zsyssvr args..]]\n", name);
	fprintf(stderr, "  --reset         Reset all global objects.\n");
	fprintf(stderr, "  -d, --daemon    Run the system_server as a daemon.\n");
	fprintf(stderr, "  -h, --help      Display this help message\n");
}

class syssvr_thread : public thread
{
public:
	syssvr_thread(syssvr_mgr* sysl)
	: _sysl(sysl) {}

	~syssvr_thread() {

	}

	int run(void) {
		_sysl->syssvr_thread_run();
		return 0;
	}
	
private:
	syssvr_mgr* _sysl;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(syssvr_thread);
};

syssvr_mgr::syssvr_mgr()
: _client_listener(nullptr), _init_started(false)
{

}

syssvr_mgr::~syssvr_mgr()
{

}

int syssvr_mgr::syssvr_thread_run()
{
	evloop::inst()->remove_listener("sysd");

	printf("syssvr_thread_run\n");
	// start zrpc server
	start_zrpc_server();

	// init syssvr pakcage mgr
	_syssvr_pkg_mgr.init_syssvr_package();
	
//	rpcmgr::inst()->start_service("zas.system", "device-monitor");
//	rpcmgr::inst()->start_service("zas.system", "updater");
	rpcmgr::inst()->start_service("zas.digdup", "dataprovider");
	// rpcmgr::inst()->start_service("zas.system", "rpc.service2");

	// start proxy bridge service
	start_rpcproxy_bridge();

	// for create dataprovider manifest
	// _syssvr_pkg_mgr.launch_app("zas.digdup",
	// 	"dataprovider", "exec://zas.digdup?hostid=__shared_contiander_zas.digdup&service=dataprovider&executive=services/libdigdup_dataprovider.so&mode=start");
	_syssvr_pkg_mgr.launch_app("zas.digtwins",
		"digital_twins", "exec://zas.digtwins?host=zdigtwins");
	return 0;
}

int syssvr_mgr::start_rpcproxy_bridge()
{
	return _syssvr_pkg_mgr.launch_app(ZRPC_PROXY_BRIDGE_PKG,
		ZRPC_PROXY_BRIDGE_NAME, ZRPC_PROXY_BRIDGE_INFO);
}

int syssvr_mgr::syssvr_thread_start()
{
	printf("syssvr_thread_start\n");
	if (_init_started) return -EEXISTS;
	_init_started = true;
	auto *syssvrthd = new syssvr_thread(this);
	syssvrthd->start();
	syssvrthd->release();
	return 0;
}

int syssvr_mgr::start_server(void)
{
	evloop *evl = evloop::inst();
	evl->setrole(evloop_role_server);

	evl->updateinfo(evlcli_info_client_name, "zas.system")
		->updateinfo(evlcli_info_instance_name, "sysd")
		->updateinfo(evlcli_info_listen_port, ZRPC_SERVER_TCPIP_LISTEN_PORT)
		->updateinfo(evlcli_info_commit);
	//initial server datapool
	datapool *dp = datapool::inst();

	if (!_client_listener) 
		_client_listener = new syssvr_client_listener(this);
	evl->add_listener("sysd", _client_listener);

	// start the system server
	return evl->start(false);
}

int syssvr_mgr::start_zrpc_server(void)
{
	auto_semaphore as(zrpc_service);
	// start rpc server
	rpcserver zserver = rpcmgr::inst()->get_server();

	uri file(ZRPC_SERVER_ENTRY_CONFIG_FILE);
	zserver->load_config_file(file);
	zserver->run();
	return 0;
}

int syssvr_mgr::run(int argc, char *argv[])
{
	int i, c;
	struct option opts[] = {
		{"daemon", no_argument, NULL, 'd'},
		{"help", no_argument, NULL, 'h'},
		{"reset", no_argument, NULL, 0},
		{0, 0, NULL, 0}};

	printf("argc server num %d, %s\n", argc, argv[1]);
	while ((c = getopt_long(argc, argv, "dh", opts, &i)) != -1)
	{
		switch (c)
		{
		case 0: /* no need to handle here */
			break;
		case 'd':
			init_daemon();
			break;
		case 'h':
			help("zsyssvr");
			return 0;
		default:
			return 1;
		}
	}

	if (start_server())
	{
		return -1;
	}
	return 0;
}

}} // end of namespace zas::syssvr

int main(int argc, char *argv[])
{
    return zas::syssvr::syssvr_mgr().run(argc, argv);
}
/* EOF */
