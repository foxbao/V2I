/** @file evlclient.cpp
 * implementation of evloop client object
 */

#include "utils/utils.h"
#if (defined(UTILS_ENABLE_FBLOCK_BUFFER) && defined(UTILS_ENABLE_FBLOCK_EVLCLIENT))

#include <unistd.h>
#include <stdarg.h>

#include <sys/eventfd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "inc/evlclient.h"
#include "inc/evloop-impl.h"
#include "inc/timer-impl.h"

namespace zas {
namespace utils {

using namespace std;

static mutex cliset_mut;
static avl_node_t* cli_nametree = NULL;
static listnode_t clilst = LISTNODE_INITIALIZER(clilst);


static uint32_t get_curipaddr(void)
{
	char hostname[256];
	struct hostent* hent;

	if (gethostname(hostname, sizeof(hostname))) {
		return 0;
	}
	hent = gethostbyname(hostname);
	if (NULL == hent) return 0;

	if (AF_INET != hent->h_addrtype)
		return 0;

	if (!hent->h_addr_list[0])
		return 0;

	return *((unsigned int *)hent->h_addr_list[0]);
}

evlclient_impl::evlclient_impl(uint32_t type, int fd)
: _flags(0), _port(0) , _fd(fd), _ipv4addr(0)
, _pid(0), _refcnt(1) {
	memset(&_uuid, 0, sizeof(_uuid));
	_f.client_type = type;
	_f.detached = 1;
	listnode_init(_instlist);
}

evlclient_impl::~evlclient_impl()
{
}

void evlclient_impl::destroy_without_release(void)
{
	auto_mutex am(cliset_mut);
	if (_f.destroyed) return;

	if (!_name.empty() && !_inst_name.empty() && !isdetached())
		detach_client_unlock(this);

	if (!isdetached())
		listnode_del(_ownerlist);

	{
		string tmp;
		if (!_name.empty() || !_inst_name.empty())
			tmp = _name + "." + _inst_name;
		else tmp = "<Unkown>";
		printf("client: %s (pid=%05d) diconnected.\n", tmp.c_str(), _pid);
	}

	memset(&_uuid.uuid, 0, sizeof(_uuid));
	_flags = 0;
	_f.destroyed = 1;
	_port = 0;
	if (_fd >= 0) {
		close(_fd);
		_fd = -1;
	}
	_pid = 0;
}

int evlclient_impl::destroy(void)
{
	destroy_without_release();
	return _refcnt;
}

int evlclient_impl::release(void)
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0) {
		if (!_f.destroyed)
			destroy_without_release();
		delete this;
	}
	return cnt;
}

int evlclient_impl::getfd(void)
{
	uint32_t type = 0;
	int fd = -1;
	getinfo(type, fd);
	return fd;
}

int evlclient_impl::gettype(void)
{
	uint32_t type = 0;
	int fd = -1;
	getinfo(type, fd);
	return type;
}

void evlclient_impl::getinfo(uint32_t& type, int& fd)
{
	type = _f.client_type;
	fd = _fd;
}

const char* evlclient_impl::get_clientname(void)
{
	return _name.c_str();
}

const char* evlclient_impl::get_instname(void)
{
	return _inst_name.c_str();
}

evlclient_impl* evlclient_impl::create(int fd, uint32_t type)
{
	if (type == client_type_unknown || !fd)
		return NULL;

	evlclient_impl* cli = new evlclient_impl(type, fd);
	if (NULL == cli) return NULL;
	if (add_client(cli)) {
		delete cli;
		return NULL;
	}
	return cli;
}

int evlclient_impl::add_client(evlclient_impl* cli)
{
	assert(NULL != cli);
	auto_mutex am(cliset_mut);

	// see if the name exists
	if (!cli->_name.empty() && !cli->_inst_name.empty()) {
		return cli->insert_client_unlock(cli);
	} else {
		listnode_add(clilst, cli->_ownerlist);
		cli->set_detached(false);
	}
	return 0;
}

bool evlclient_impl::detach_client_unlock(evlclient_impl *cli)
{
	if (isdetached()) return true;

	avl_node_t *instavlnode =
	 avl_find(cli_nametree, &(cli->_avlnode), avl_name_compare);
	if (NULL == instavlnode) return false;

	evlclient_impl *instnode = AVLNODE_ENTRY(evlclient_impl, \
		_avlnode, instavlnode);
	//cli is client root node
	if (instavlnode == &(cli->_avlnode))
	{
		avl_remove(&cli_nametree, &(cli->_avlnode));

		if (!listnode_isempty(instnode->_instlist)) {
			//find client next point
			listnode_t *nextinstnode = instnode->_instlist.next;
			evlclient_impl* client = LIST_ENTRY(evlclient_impl, \
				_inst_ownerlist, nextinstnode);
			listnode_del(instnode->_instlist);
			if (listnode_isempty(*nextinstnode)) {
				assert(listnode_isempty(client->_instlist));
			} else {
				listnode_t *fristnode = nextinstnode->next;
				listnode_del(client->_inst_ownerlist);
				//*fristnode is inst list, _instlist is one node
				//so, _instlist need join *firstnode
				listnode_add(*fristnode, client->_instlist);
			}
			avl_insert(&cli_nametree, &(client->_avlnode), avl_name_compare);
		}
		// clilst list node, remove need clear
		listnode_del(cli->_ownerlist);
		set_detached(true);
		return true;
	} else {
		listnode_t* listnode = instnode->_instlist.next;
		for (; listnode != &(instnode->_instlist); listnode = listnode->next) {
			evlclient_impl* client = LIST_ENTRY(evlclient_impl, \
				_inst_ownerlist, listnode);
			int ret = strcmp(client->_inst_name.c_str(), cli->_inst_name.c_str());
			if (!ret) {
				listnode_del(client->_inst_ownerlist);
				listnode_del(cli->_ownerlist);
				set_detached(true);
				return true;
			}
		}
	}
	return false;
}

int evlclient_impl::insert_client_unlock(evlclient_impl *cli)
{
	if (cli->_name.empty() || cli->_inst_name.empty())
		return -1;

	avl_node_t *instavlnode =
	 avl_find(cli_nametree, &cli->_avlnode, avl_name_compare);

	if (instavlnode) {
		evlclient_impl *instnode = AVLNODE_ENTRY(evlclient_impl, \
			_avlnode, instavlnode);

		if (!strcmp(instnode->_inst_name.c_str(), cli->_inst_name.c_str())) {
			return -2;
		}
		listnode_t *listnode = instnode->_instlist.next;
		for (; listnode != &(instnode->_instlist); listnode = listnode->next) {
			evlclient_impl *clilistnode = LIST_ENTRY(evlclient_impl, \
				_inst_ownerlist, listnode);
			if (!strcmp(clilistnode->_inst_name.c_str(),
				cli->_inst_name.c_str())) {
				return -3;
			}			
		}
		listnode_add(instnode->_instlist, cli->_inst_ownerlist);
	} else {
		int ret = avl_insert(&cli_nametree, &cli->_avlnode, avl_name_compare);
		if (ret) return -4;
	}

	//if cli is detach from list, it need addto clilst
	if (isdetached()) {
		listnode_add(clilst, cli->_ownerlist);
		set_detached(false);
	}
	return 0;
}

int evlclient_impl::update_info(evl_bctl_client_info* info,
	clinfo_validity_u& val)
{
	val.all = 0;
	bool removed = false;
	string client_name, inst_name;

	// we need lock
	auto_mutex am(cliset_mut);

	// save the name in advance
	client_name = _name;
	inst_name = _inst_name;

	// update the name
	if (info->validity.m.client_name)
	{
		if (!_name.empty() && !_inst_name.empty()) {		
			detach_client_unlock(this);
			removed = true; // only allow to detach once
		}
		_name = &info->buf[info->client_name];
		val.m.client_name = 1;
	}
	if (info->validity.m.instance_name)
	{
		if (!_name.empty() && !_inst_name.empty() && !removed) {
			detach_client_unlock(this);
		}
		_inst_name = &info->buf[info->inst_name];
		val.m.instance_name = 1;
	}
	if (!_name.empty() && !_inst_name.empty()) {
		int ret = insert_client_unlock(this);
		if (ret) {
			// restore the original name
			_name = client_name;
			_inst_name = inst_name;
			if (!_name.empty() && !_inst_name.empty()) {
				int ret = insert_client_unlock(this);
				assert(ret == 0);
			}
			val.all = 0; 
			return -EEXISTS;
		}
	}

	// update pid info for client
	if (info->validity.m.client_pid
		&& info->pid != _pid
		&& !getclient_bypid(info->pid)) {
		_pid = info->pid;
		val.m.client_pid = 1;
	}
	if (info->validity.m.client_ipv4addr) {
		_ipv4addr = info->ipv4;
		val.m.client_ipv4addr = 1;
	}
	if (info->validity.m.client_port) {
		_port = info->port;
		val.m.client_port = 1;
	}

	if (info->validity.m.flags) {
		if (CLINFO_TYPE_UNKNOWN != info->flags.type) {
			if (CLINFO_TYPE_SYSSVR == info->flags.type)
				_f.is_syssvr = 1;
			else _f.is_syssvr = 0;
		}
		_f.is_serviceshared = info->flags.service_shared;		
		_f.is_servicecontainer = info->flags.service_container;	
		val.m.flags = 1;
	}
	return 0;
}

size_t evlclient_impl::clientinfo_size(void)
{
	size_t sz = _name.length() + _inst_name.length();
	if (!sz) return sz;
	return sz + 2;
}

void evlclient_impl::load_clientinfo(evl_bctl_client_info& cinfo)
{
	cinfo.validity.all = 0;
	cinfo.flags.service_shared = _f.is_serviceshared;
	cinfo.flags.service_container = _f.is_servicecontainer;
	cinfo.flags.type = issyssvr() ? CLINFO_TYPE_SYSSVR
		: CLINFO_TYPE_CLIENT;
	cinfo.validity.m.flags = 1;
	
	guid_t zero = {0};
	if (memcmp(&zero, &_uuid, sizeof(zero))) {
		cinfo.validity.m.client_uuid = 1;
		cinfo.client_uuid = _uuid;
	}

	if (_ipv4addr) {
		cinfo.ipv4 = _ipv4addr;
		cinfo.validity.m.client_ipv4addr = 1;
	}

	if (_pid) {
		cinfo.pid = _pid;
		cinfo.validity.m.client_pid = 1;
	}

	if (_port) {
		cinfo.port = _port;
		cinfo.validity.m.client_port = 1;
	}

	if (!_name.empty()) {
		strcpy(cinfo.buf, _name.c_str());
		cinfo.bufsz = _name.length() + 1;
		cinfo.client_name = 0;
		cinfo.validity.m.client_name = 1;
	}

	if (!_inst_name.empty()) {
		strcpy(&cinfo.buf[cinfo.bufsz], _inst_name.c_str());
		cinfo.inst_name = cinfo.bufsz;
		cinfo.validity.m.instance_name = 1;
	}
}

evlclient_impl* evlclient_impl::getclient(int fd)
{
	auto_mutex am(cliset_mut);
	evlclient_impl* ret = getclient_unlocked(fd);
	if (NULL == ret) return ret;
	ret->addref();
	return ret;
}

evlclient_impl* evlclient_impl::getclient(const char* name, const char* instname)
{
	if (!name || !*name) return NULL;
	if (!instname || !*instname) return NULL;

	auto_mutex am(cliset_mut);
	evlclient_impl* ret = getclient_unlocked(name, instname);
	if (NULL == ret) return ret;
	ret->addref();
	return ret;
}
	
bool evlclient_impl::handle_datain(void)
{
	size_t nonblk_read(int fd, void *vptr, size_t n);

	if (client_type_timer == _f.client_type) {
		uint64_t val;
		if (sizeof(val) != nonblk_read(_fd, &val, sizeof(val)))
			return false;
		timermgr_impl::getdefault()->periodic_runner();
		return true;
	}
	return false;
}

int evlclient_impl::avl_fd_compare(avl_node_t* a, avl_node_t* b)
{
	evlclient_impl* aa = AVLNODE_ENTRY(evlclient_impl, _avlnode, a);
	evlclient_impl* bb = AVLNODE_ENTRY(evlclient_impl, _avlnode, b);
	if (aa->_fd < bb->_fd) return -1;
	else if (aa->_fd > bb->_fd) return 1;
	else return 0;
}

int evlclient_impl::avl_name_compare(avl_node_t* a, avl_node_t* b)
{
	evlclient_impl* aa = AVLNODE_ENTRY(evlclient_impl, _avlnode, a);
	evlclient_impl* bb = AVLNODE_ENTRY(evlclient_impl, _avlnode, b);
	int ret = strcmp(aa->_name.c_str(), bb->_name.c_str());
	if (!ret) return 0;
	if (ret < 0) return -1;
	else return 1;
}

evlclient_impl* evlclient_impl::getclient_unlocked(int fd)
{
	if (fd < 0) return NULL;

	listnode_t* node = clilst.next;
	for (; node != &clilst; node = node->next) {
		evlclient_impl* cli = list_entry(evlclient_impl, _ownerlist, node);
		if (cli->_fd == fd) return cli;
	}
	return NULL;
}

evlclient_impl* evlclient_impl::getclient_unlocked(const char* name, const char* instname)
{
	evlclient_impl dummy;
	dummy._name.assign(name);
	avl_node_t* node = avl_find(cli_nametree, &dummy._avlnode, avl_name_compare);
	if (NULL == node) return NULL;

	evlclient_impl *instnode = AVLNODE_ENTRY(evlclient_impl, _avlnode, node);
	if (!strcmp(instnode->_inst_name.c_str(), instname)) {
		return instnode;
	}

	listnode_t* inst_node = instnode->_instlist.next;
	for (; inst_node != &(instnode->_instlist); inst_node = inst_node->next) {
		evlclient_impl* cli = LIST_ENTRY(evlclient_impl, _inst_ownerlist, inst_node);
		int ret = strcmp(cli->_inst_name.c_str(), instname);
		if (!ret) {
			return cli;
		}
	}	
	return NULL;
}

evlclient_impl* evlclient_impl::getclient_bypid(int pid)
{
	listnode_t* node = clilst.next;
	for (; node != &clilst; node = node->next) {
		evlclient_impl* cli = list_entry(evlclient_impl, _ownerlist, node);
		if (cli->_pid == pid) return cli;
	}
	return NULL;
}

tasklet_client::tasklet_client(const char* name)
: evlclient_impl(client_type_event, -1)
{
	if (name && *name) {
		_name = TASKLET_CLIENT_APP_NAME;
		_inst_name = name;
	}
	listnode_init(_tasklist);

	// create the eventfd for surface
	_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	assert(_fd >= 0);
}

tasklet_client::~tasklet_client()
{
	// todo: close fd
	// todo: remove all tasks
}

tasklet_client* tasklet_client::create(const char* name)
{
	if (NULL == name || *name == '\0')
		return NULL;

	tasklet_client* ecl = new tasklet_client(name);
	assert(NULL != ecl);

	if (add_client(ecl)) {
		delete ecl;
		return NULL;
	}
	return ecl;
}

size_t nonblk_read(int fd, void *vptr, size_t n);
size_t nonblk_write(int fd, const void *vptr, size_t n);

static size_t client_write(int fd, const void* src, size_t n, mutex* mut,
	fifobuffer_impl* fifobuf, int timeout, bool prelock = false)
{
	if (!n) return 0;
	size_t nwri = 0;

	// this is the start time stamp
	long start_time = gettick_millisecond();
	if (mut && !prelock) mut->lock();

	// fifobuf empty means all data transfered
	// we can try directly write data, otherwise
	// we need to put data into buffer
	if (fifobuf->empty()) {
		nwri = nonblk_write(fd, src, n);
		if (nwri == n) {
			if (mut && !prelock) mut->unlock();
			return n;
		}
	}
	// the kernel buffer is full, buffering the data
	// check if the fifobuffer has enough space
	size_t nleft = n - nwri;

	// try lock the fifo first
	for (;;)
	{
		if (fifobuf->enter()) break;
		if (mut) mut->unlock();
		msleep(2);
		if (gettick_millisecond() - start_time > timeout)
			return n;
		if (mut) mut->lock();
	}

	size_t fnwri = 0;
	const char* s = reinterpret_cast<const char*>(src);
	for (s += nwri; fnwri < nleft; s += fnwri)
	{
		fnwri = fifobuf->append((void*)s, nleft);
		nwri += fnwri;
		if (fnwri == nleft) break;

		// we have not finished buffering
		// this shall be "fifo buffer full", wait for
		// consuming some data
		if (mut) mut->unlock();
		msleep(2);

		if (mut) mut->lock();
		if (gettick_millisecond() - start_time > timeout)
			break;
		nleft -= fnwri;
	}
	fifobuf->exit();
	if (mut && !prelock) mut->unlock();
	return nwri;
}

size_t evlclient_impl::write(void* buffer, size_t sz)
{
	if (!buffer || !sz) return -EBADPARM;
	fifobuffer_impl* fifobuf = get_writebuffer();
	mutex* mut = getmutex();
	size_t n = zas::utils::client_write(getfd(), 
		buffer, sz, mut, fifobuf, 1000);

	if (n != sz) return -EBUSY;
	return n;
}

struct task_closure
{
	listnode_t ownerlist;
	evloop_task* task;
};

bool tasklet_client::handle_datain(void)
{
	assert(_f.client_type == client_type_event);

	// drain the eventfd
	uint64_t val = 0;
	if (sizeof(val) != nonblk_read(_fd, &val, sizeof(val)))
		return false;

	_mut.lock();
	while (!listnode_isempty(_tasklist))
	{
		task_closure* tc = list_entry(task_closure, \
			ownerlist, _tasklist.next);
		listnode_del(tc->ownerlist);
		_mut.unlock();

		// handle the task
		if (tc->task) {
			tc->task->run();
			tc->task->release();
		}

		delete tc;
		_mut.lock();		
	}
	_mut.unlock();
	return true;
}

int tasklet_client::notify_event(void)
{
	int fd = getfd();
	if (fd < 0) return -1;

	uint64_t val = 1;
	int ret = nonblk_write(fd, &val, sizeof(uint64_t));
	return (ret == 8) ? 0 : -2;
}

int tasklet_client::addtask(evloop_task* task)
{
	assert(NULL != task);
	task_closure* tc = new task_closure();
	tc->task = task;
	
	_mut.lock();
	listnode_add(_tasklist, tc->ownerlist);
	_mut.unlock();
	return notify_event();
}

connmgr_client::connmgr_client(int listenfd)
: evlclient_impl(client_type_unix_socket_listenfd, listenfd)
{
}

connmgr_client::~connmgr_client()
{
	// todo: close fd
}

connmgr_client* connmgr_client::create(int listenfd)
{
	connmgr_client* ecl = new connmgr_client(listenfd);
	if (NULL == ecl) return NULL;

	// set name
	ecl->_name = "zas.system";
	ecl->_inst_name = "connection_listener";

	if (add_client(ecl)) {
		delete ecl;
		return NULL;
	}
	return ecl;
}

bool connmgr_client::handle_datain(void)
{
	return false;
}

tcp_connmgr_client::tcp_connmgr_client(int listenfd)
: evlclient_impl(client_type_socket_listenfd, listenfd)
{
}

tcp_connmgr_client::~tcp_connmgr_client()
{
	// todo: close fd
}

tcp_connmgr_client* tcp_connmgr_client::create(int listenfd)
{
	tcp_connmgr_client* ecl = new tcp_connmgr_client(listenfd);
	if (NULL == ecl) return NULL;

	// set name
	ecl->_name = "zas.system";
	ecl->_inst_name = "tcp_connection_listener";

	if (add_client(ecl)) {
		delete ecl;
		return NULL;
	}
	return ecl;
}

bool tcp_connmgr_client::handle_datain(void)
{
	return false;
}

client_info::client_info(evloop_impl* evl)
: _evl(evl)
, _server_port(0)
, _client_ipv4(0)
, _client_port(0)
, tasklet_client(NULL) {
	_validity.all = 0;
	_server_ipv4.clear();
}

evloop_impl* client_info::get_evloop(void) {
	return _evl;
}

bool client_info::name_avail(void) {
	return (_client_name.empty() || _instance_name.empty())
		? false : true;
}

const char* client_info::get_name(void) {
	if (_client_name.empty()) return NULL;
	return _client_name.c_str();
}

const char* client_info::get_instance_name(void) {
	if (_instance_name.empty()) return NULL;
	return _instance_name.c_str();
}

const char* client_info::get_server_ipv4(void) {
	return _server_ipv4.c_str();
}

uint32_t client_info::get_server_port(void) {
	return _server_port;
}

uint32_t client_info::get_listen_port(void) {
	return _client_port;
}

int client_info::update(int cmd, va_list vl)
{
	switch (cmd)
	{
	case evlcli_info_commit:
		break;

	case evlcli_info_clear:
		_client_name.clear();
		_instance_name.clear();
		_validity.all = 0;
		break;

	case evlcli_info_client_name: {
		const char* client_name = va_arg(vl, const char*);
		if (!client_name || !*client_name) return -2;
		_client_name = client_name;
		_validity.m.client_name = 1;
		break;
	}

	case evlcli_info_instance_name: {
		const char* inst_name = va_arg(vl, const char*);
		if (!inst_name || !*inst_name) return -3;
		_instance_name = inst_name;
		_validity.m.instance_name = 1;
	}	break;

	case evlcli_info_client_ipv4: {
		const char* client_ip = va_arg(vl, const char*);
		if (!client_ip || !*client_ip) return -3;
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		int status = inet_pton(AF_INET, client_ip, &addr.sin_addr);
		if ( errno == EAFNOSUPPORT ) { return -5; }
		_client_ipv4 = addr.sin_addr.s_addr;
		_validity.m.client_ipv4addr = 1;
	}	break;

	case evlcli_info_listen_port: {
		_client_port = va_arg(vl, unsigned int);
		if (0 == _client_port) {
			return -4;
		}
		_validity.m.client_port = 1;
	}	break;

	case evlcli_info_server_ipv4: {
		const char* svr_ip = va_arg(vl, const char*);
		if (!svr_ip || !*svr_ip) {
			return -5;
		}
		_server_ipv4 = svr_ip;
	}	break;

	case evlcli_info_server_port: {
		_server_port = va_arg(vl, unsigned int);
		if (0 == _server_port) {
			return -5;
		}
	}	break;

	case evlcli_info_service_container: {
		bool isservicecontainer = (va_arg(vl, int) == 1) ? true : false;
		_f.is_servicecontainer = isservicecontainer ? 1 : 0;
	}	break;

	case evlcli_info_service_shared: {
		bool isshared = (va_arg(vl, int) == 1) ? true : false;
		_f.is_serviceshared = isshared ? 1 : 0;
	}	break;
	
	default: return -1;
	}
	return 0;
}

size_t client_info::getsize(void)
{
	return _client_name.size() + 1
		+ _instance_name.size() + 1;
}

void client_info::load(evl_bctl_client_info& clinfo)
{
	clinfo.validity.all = 0;

	if (_validity.m.client_name)
	{
		clinfo.client_name = clinfo.bufsz;
		clinfo.bufsz += _client_name.size() + 1;
		strcpy(clinfo.buf + clinfo.client_name, _client_name.c_str());
		clinfo.validity.m.client_name = 1;
	}
	if (_validity.m.instance_name)
	{
		clinfo.inst_name = clinfo.bufsz;
		clinfo.bufsz += _instance_name.size() + 1;
		strcpy(clinfo.buf + clinfo.inst_name, _instance_name.c_str());
		clinfo.validity.m.instance_name = 1;
	}
	if (_validity.m.client_port) {
		clinfo.port = _client_port;
		clinfo.validity.m.client_port = 1;
		if (!_validity.m.client_ipv4addr) {
			_client_ipv4 = get_curipaddr();
			_validity.m.client_ipv4addr = 1;
		}
	}
	if (_validity.m.client_ipv4addr) {
		clinfo.ipv4 = _client_ipv4;
		clinfo.validity.m.client_ipv4addr = 1;
	}
	if (_validity.m.client_uuid) {
		
	}
	clinfo.flags.type = is_server()
		? CLINFO_TYPE_SYSSVR : CLINFO_TYPE_CLIENT;
	clinfo.flags.service_container = _f.is_servicecontainer;
	clinfo.flags.service_shared = _f.is_serviceshared;
	clinfo.validity.m.flags = 1;

	// add the pid info to the package
	clinfo.pid = (int)::getpid();
	clinfo.validity.m.client_pid = 1;
}

bool client_info::equal(const char* client_name, const char* inst_name)
{
	if (client_name == _client_name
		&& inst_name == _instance_name)
		return true;
	else return false;
}

bool client_info::is_server(void)
{
	assert(NULL != _evl);
	return _evl->is_server();
}

userdef_evlclient_impl::userdef_evlclient_impl(userdef_evlclient* client)
: evlclient_impl(-1, -1)
, _client(client), _user_client_type(0)
{
	assert(NULL != client);
	_f.client_type = client_type_user;
	int ret = add_client(this);
	assert(ret == 0);
}

userdef_evlclient_impl::~userdef_evlclient_impl()
{
	detach();
}

int userdef_evlclient_impl::detach(void)
{
	// remove from the current evloop
	auto* evl = reinterpret_cast<evloop_impl*>(evloop::inst());
	
	// in case of the fd in epoll is hangup or error
	// the "deregister_client" will return -1. this is
	// something normal
	evl->deregister_client(this);
	_client = NULL;
	return 0;
}

int userdef_evlclient_impl::activate(void)
{
	// add it to the current evloop
	auto* evl = reinterpret_cast<evloop_impl*>(evloop::inst());
	return evl->register_client(this);
}

bool userdef_evlclient_impl::handle_datain(void)
{
	if (_client) {
		return _client->handle_datain();
	}
	return false;
}

void userdef_evlclient_impl::getinfo(uint32_t& type, int& fd)
{
	if (!_client) {
		type = 0; fd = -1;
		return;
	}
	if (_fd > 0) {
		type = _user_client_type;
		fd = _fd;
		return;
	}

	_client->getinfo(type, fd);
	_user_client_type = type;
	_fd = fd;
	_name = USER_CLIENT_NAME;
	_name += "_" + to_string(type);
	_inst_name = USER_CLIENT_NAME;
	_inst_name += "_" + to_string(_fd);
	add_client(this);
}

int userdef_evlclient_impl::handle_input(void)
{
	fifobuffer_impl* buf = get_readbuffer();
	if (buf->empty()) {
		return -EDATANOTAVAIL;
	}

	int discard_sz = -1;
	int ret = -ENOTHANDLED;
	if (_client) {
		auto* robuf = reinterpret_cast<readonlybuffer*>(buf);
		ret = _client->on_common_datain(robuf, discard_sz);
	}

	// if discard_sz = 0, we discard nothing since the callee
	// will handle the data discard
	if (discard_sz > 0 && (size_t)discard_sz < buf->getsize()) {
		buf->discard(discard_sz);
	}
	else if (discard_sz) {
		buf->drain();
	}
	return ret;
}

userdef_evlclient::userdef_evlclient()
: _data(NULL)
{
	auto* impl = new userdef_evlclient_impl(this);
	if (NULL == impl) return;

	// check if the userdef client is valid
	// since the new operator may not return
	// success/fail status, so we use the
	// get_userdef_client() to check validity
	auto* ucli = impl->get_userdef_client();
	if (!ucli || ucli != this) {
		return;
	}
	_data = reinterpret_cast<void*>(impl);
}

userdef_evlclient::~userdef_evlclient()
{
	if (_data) {
		auto* impl = reinterpret_cast<userdef_evlclient_impl*>(_data);
		impl->detach();
		impl->release();
		_data = NULL;
	}
}

int userdef_evlclient::activate(void)
{
	auto* userdef = reinterpret_cast<userdef_evlclient_impl*>(_data);
	if (nullptr == userdef) return -EINVALID;
	return userdef->activate();
}

evlclient userdef_evlclient::getclient(void)
{
	evlclient user;
	if (!_data) return user;
	auto* userdef = reinterpret_cast<userdef_evlclient_impl*>(_data);
	userdef->addref();
	evlclient_impl* usercli = (evlclient_impl*)userdef;
	user.set(reinterpret_cast<evlclient_object*>(usercli));
	return user;
}

bool userdef_evlclient::handle_datain(void)
{
	return false;
}

int userdef_evlclient::on_common_datain(readonlybuffer* buf, int& discard)
{
	discard = -1;	// discard all data in buffer
	return -ENOTHANDLED;
}

}} // end of namespace zas::utils
#endif // (defined(UTILS_ENABLE_FBLOCK_BUFFER) && defined(UTILS_ENABLE_FBLOCK_EVLCLIENT))
/* EOF */
