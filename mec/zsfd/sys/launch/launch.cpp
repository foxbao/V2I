#include <getopt.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <errno.h> 
#include <sys/wait.h>
#include <sys/prctl.h>

#include "utils/absfile.h"
#include "utils/dir.h"
#include "utils/evloop.h"
#include "utils/mutex.h"
#include "utils/shmem.h"
#include "utils/timer.h"
#include "utils/thread.h"
#include "mware/pkgconfig.h"

#include "launch.h"

#define LAUNCH_SYSTEM_SERVER_URI	\
	"exec://zas.system?host=daemons/zsysd"

#define LAUNCH_SYSTEM_HOST			\
	"/zassys/bin/zhost"
#define LAUNCH_SYSTEM_APP_ROOT		"/zassys/sysapp"
#define LAUNCH_USER_APP_ROOT		"/zasdata/app"
#define LAUNCH_PACKAGE_CONFIG_ROOT	"/zassys/etc/manifest-bin/"
#define LAUNCH_PACKAGE_CONFIG_SUFFIX	".bin"
#define LAUNCH_PACKAGE_CONFIG_FILE_NAME	"manifest.json"
#define LAUNCH_PACKAGE_SHM_SUFFIX	".pkgcfg.area"
#define LAUNCH_SYSTEM_HOST_KEY		"host"

#ifndef ARM_LINUX
#define LAUNCH_SYSTEM_LIB_ENV		\
	"LD_LIBRARY_PATH=/zassys/lib/x86_64-linux-gnu"
#else
#define LAUNCH_SYSTEM_LIB_ENV		\
	"LD_LIBRARY_PATH=/zassys/lib/arm_64-linux-gnu"
#endif

#define ZRPC_SERVER_ENTRY_CONFIG_FILE	\
	"file:///zassys/etc/syssvrconfig/hostconfig.bin"
#define ZRPC_SERVER_ENTRY_SEM	"zrpcserverconfig"

#define LAUNCH_SYSTEM_USER			(1000)

// need fixed,  todo.for weston:
#define LAUNCH_SYSTEM_WESTON_URI	\
	"exec://zas.system.weston?host=/zassys/bin/weston"

static semaphore service_sem(ZRPC_SERVER_ENTRY_SEM, 1);

namespace zas {
namespace system {

using namespace zas::utils;
using namespace zas::mware;
using namespace zas::mware::rpc;

void childproc_handle(int sig)
{
	pid_t child_pid = 0;
	while ((child_pid = waitpid(-1, NULL, 0)) > 0) {
		//todo: send message to application mgr
		printf("launch child_pid %d, sig %d\n", child_pid, sig);
	}
}

class launch_thread : public thread
{
public:
	launch_thread(launch* sysl)
	: _sysl(sysl) {}

	~launch_thread() {

	}

	int run(void) {
		return _sysl->launch_system_app();
	}
	
private:
	launch* _sysl;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(launch_thread);
};


launch::launch()
{
}

launch::~launch()
{
}

int launch::run(int argc, char *argv[])
{
	// init system lib env
	putenv((char*)LAUNCH_SYSTEM_LIB_ENV);

	if (init_signal())
		return -1;

	// init user info
	if (_appinfo_mgr.init())
		return -2;

	// init service config info
	int ret = load_service_config();
	if (-ENOTEXISTS != ret && ret)
		return -3;

	// start system server
	if (start_syssvr())
		return -4;

	if (init_mssdev_mounter())
		return -5;

	if (start_evloop())
		return -6;

	return 0;
}

int launch::launch_system_app(void)
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

	return 0;
}

int launch::init_signal(void)
{
	signal(SIGCHLD, childproc_handle);
	return 0;
}

int launch::start_syssvr(void)
{
	int ret = launch_app(LAUNCH_SYSTEM_SERVER_URI);
	if (ret < 0) {
		printf("launch syssvr run error\n");
		return -1;
	}
	msleep(100);
	return 0;
}

int launch::start_evloop(void)
{
	evloop *evl = evloop::inst();
	evl->setrole(evloop_role_client);

	// set the name of launch as "zas.system"
	evl->updateinfo(evlcli_info_client_name, "zas.system")
		->updateinfo(evlcli_info_instance_name, "worker")
		->updateinfo(evlcli_info_commit);

	// launch sysd after evloop initialize
	launch_system_app_thread();

	// init pkg handle
	_launch_pkg_mgr.init_launch_pkg(this);

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

int launch::launch_system_app_thread(void)
{
	auto *plaunch = new launch_thread(this);
	plaunch->start();
	plaunch->release();
	return 0;
}


int launch::launch_app(const char* lauri)
{
	if (!lauri || !*lauri) {
		return -EINVALID;
	}

	uri app(lauri);
	if (!app.valid() || "exec" != app.get_scheme()) {
		return -EBADPARM;
	}

	string package_name = app.get_fullpath();
	string exefile = app.query_value(LAUNCH_SYSTEM_HOST_KEY);
	string fullpathexef;

	appinfo launchapp;
	int userret =  _appinfo_mgr.get_appinfo(package_name.c_str(), &launchapp);
	if (userret < 0) {
		printf("launch get userinfo error, errno: %d\n", userret);
		return -ENOTFOUND;
	}

	if (exefile.empty()) {
		fullpathexef = LAUNCH_SYSTEM_HOST;
	} else {
		if (launchapp.sysapp) {
			fullpathexef = LAUNCH_SYSTEM_APP_ROOT;
		} else {
			fullpathexef = LAUNCH_USER_APP_ROOT;
		}
		fullpathexef = fullpathexef + "/"+ package_name;
		fullpathexef = fullpathexef + "/"+ exefile;
	}

	//load package config
	load_package_config(package_name.c_str(), launchapp.sysapp);
	
	char* argv[4] = {0};
	argv[0] = (char*)fullpathexef.c_str();
	argv[1] = (char*)"-l";
	argv[2] = (char*)lauri;

	int pid = fork();
	if (pid == 0) {
		int ret = setgroups(launchapp.gid_count, launchapp.gids);
		ret = setresgid(launchapp.gids[0], 
			launchapp.gids[0], launchapp.gids[0]);
		ret = setresuid(launchapp.uid, launchapp.uid, launchapp.uid);
		// root process close need close all children process.
		prctl(PR_SET_PDEATHSIG, SIGKILL);

		// for test weston
		// putenv("XDG_RUNTIME_DIR=/run/user/1000");
		ret = execv(argv[0], argv);
		if (-1 == ret) {
			printf("launch execv error. exef: %s,\
				 errno: %d, strerr: %s\n",  argv[0], errno, strerror(errno));
		}

		// new process execv finished, exit
		exit(0);
	}
	else if (pid < 0) {
		perror("fork"); return -ENOTHANDLED;
	}

	printf("launch launch app finished. app: %s, pid %d\n", lauri, pid);
	return 0;
}

int launch::load_package_config(const char* pkgname, bool system)
{
	// generate pkgconfig bin file name
	std::string pkgbin = LAUNCH_PACKAGE_CONFIG_ROOT;
	pkgbin += pkgname;
	pkgbin += LAUNCH_PACKAGE_CONFIG_SUFFIX;

	// pkgconfig bin uri
	std::string uripkgbin = "file://";
	uripkgbin += pkgbin;

	if (!fileexists(pkgbin.c_str())) {
		std::string appconfig = "file://";
		if (system)
			appconfig += LAUNCH_SYSTEM_APP_ROOT;
		else 
			appconfig += LAUNCH_USER_APP_ROOT;
		appconfig += "/";
		appconfig += pkgname;
		appconfig += "/";
		appconfig += LAUNCH_PACKAGE_CONFIG_FILE_NAME;
		pkgcfg_compiler* cp = new pkgcfg_compiler(appconfig.c_str());
		int ret = cp->compile(uripkgbin.c_str());
		printf("re complie pkg : %s, config. result %d\n", pkgname, ret);
		delete cp;
		ret = load_service_by_appconfig(appconfig.c_str());
		printf("load_service_by_appconfig :"
			" %s, config. result %d\n", appconfig.c_str(), ret);
		if (ret) return ret;
	}

	
	// check pkg config file
	if (!fileexists(pkgbin.c_str())) {
		// not find pkgconfig
		return -ENOTEXISTS;
	}

	const pkgconfig& p = pkgconfig::getdefault();

	// generate pkgconfig bin sharememory name
	std::string pkgsh_name = pkgname;
	pkgsh_name += LAUNCH_PACKAGE_SHM_SUFFIX;

	size_t filesz = 0;
	shared_memory* pkgsh = NULL;
	void*	pkgadr = NULL;

	// load bin file
	absfile* afile = absfile_open(uripkgbin.c_str(), "rb");
	if (NULL == afile) return -ENOTEXISTS;
	
	int ret = -EINVALID;
	ret = afile->seek(0, absfile_seek_end);
	if (ret) goto error;

	filesz = afile->getpos();
	ret = afile->seek(0, absfile_seek_set);
	if (ret) goto error;

	// create shared memory
	pkgsh = shared_memory::create(pkgsh_name.c_str(),
		(filesz + ZAS_PAGESZ - 1) & ~(ZAS_PAGESZ - 1));
	if (NULL == pkgsh) goto error;

	// shared memory is already created
	if (!pkgsh->is_creator()) {
		afile->close();
		afile->release();
		return 0;
	}

	// load pkg config bin
	pkgadr = pkgsh->getaddr();
	if (NULL == pkgadr) goto error;
	afile->read(pkgadr, filesz);
	ret = 0;

error:
	afile->close();
	afile->release();
	return ret;
}

int launch::load_service_config(void)
{
	uri file(ZRPC_SERVER_ENTRY_CONFIG_FILE);
	auto_semaphore as(service_sem);
	return _service_mgr.load_config_file(file);
}

int launch::load_service_by_appconfig(const char* file)
{
	uri configfile(file);
	_service_mgr.load_manifest_file(configfile);
	uri savefile(ZRPC_SERVER_ENTRY_CONFIG_FILE);
	auto_semaphore as(service_sem);
	return _service_mgr.save_config_file(savefile);
}

}} // end of namespace zas::system

/* EOF */
 
