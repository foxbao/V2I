/** @file evloop.cpp
 * implementation of the main loop for events
 */

#include "utils/utils.h"
#if (defined(UTILS_ENABLE_FBLOCK_EVLOOP) && defined(UTILS_ENABLE_FBLOCK_EVLCLIENT) && defined(UTILS_ENABLE_FBLOCK_BUFFER))

#include <stddef.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <string>

#include "std/list.h"
#include "utils/dir.h"
#include "utils/wait.h"
#include "utils/mutex.h"
#include "utils/evloop.h"
#include "utils/timer.h"
#include "utils/thread.h"
#include "utils/avltree.h"

#include "inc/evloop-impl.h"
#include "inc/evlclient.h"
#include "inc/timer-impl.h"
#include "inc/fifobuffer.h"

namespace zas {
namespace utils {

using namespace std;
using namespace zas::utils;
#define SYSTEM_NET_GID		(1050)

static mutex evlmut;

bool handle_client_info_ack(void* owner, evlclient sender,
	const package_header& pkghdr,
	const triggered_pkgevent_queue& queue)
{
	auto *handle = reinterpret_cast<evloop_impl*> (owner);
	return handle->handle_client_info_ack(sender, pkghdr, queue);
}

bool handle_retrive_client_ack(void* owner, evlclient sender,
	const package_header& pkghdr,
	const triggered_pkgevent_queue& queue)
{
	auto *handle = reinterpret_cast<evloop_impl*> (owner);
	return handle->handle_retrive_client_ack(sender, pkghdr, queue);
}

static int setnonblocking(int sockfd)
{
	int flag = fcntl(sockfd, F_GETFL, 0);
	if (flag < 0) {
		return -1;
	}
	if (fcntl(sockfd, F_SETFL, flag | O_NONBLOCK) < 0) {
		return -2;
	}
	return 0;
}

static int evl_unix_socket_dir_set_group(const char* file)
{
	int ret = chmod(file, 0770);
	ret = chown(file, -1,SYSTEM_NET_GID);
	return 0;
}

static int epoll_add(int efd, int sockfd, void* ptr,
	uint32_t flags = EPOLLIN | EPOLLOUT)
{
	struct epoll_event ev;
	ev.events = EPOLLET | flags | EPOLLRDHUP;
	ev.data.ptr = ptr;
	if (-1 == epoll_ctl(efd, EPOLL_CTL_ADD, sockfd, &ev)) {
		if (errno == EEXIST) return 0;
		return -1;
	}
	return 0;
}

/*
static void epoll_write(int efd, int sockfd, bool enable)
{
	struct epoll_event ev;
	ev.events = EPOLLIN | (enable ? EPOLLOUT : 0);
	ev.data.fd = sockfd;
	epoll_ctl(efd, EPOLL_CTL_MOD, sockfd, &ev);
}
*/

static int epoll_del(int efd, int sockfd)
{
	int ret = epoll_ctl(efd, EPOLL_CTL_DEL, sockfd , NULL);
	if (ret == -1 && errno == ENOENT) return 0;
	return ret;
}

size_t nonblk_read(int fd, void *vptr, size_t n)
{
	int nread;
	size_t nleft = n;
	char *ptr = (char*)vptr;
 
    while (nleft > 0)
	{
		nread = (int)read(fd, ptr, nleft);
		if (nread < 0) {
			if (errno == EINTR)
				nread = 0; /* call was interrupted, call read() again */
            else
				return (n - nleft); /* maybe errno == EAGAIN */
		} else if (0 == nread) {
			break; /* EOF */
		}
		nleft -= nread;
		ptr += nread;
	}
	return (n - nleft); /* return >= 0 */
}

size_t nonblk_write(int fd, const void *vptr, size_t n)
{
	int nwritten;
	size_t nleft = n;
	const char *ptr = (char*)vptr;

	while (nleft > 0)
	{
		nwritten = write(fd, ptr, nleft);
		if (nwritten <= 0) {
			if (nwritten < 0 && errno == EINTR)
				nwritten = 0; /* call was interrupted, call write() again */
			else
				return (n - nleft); /* error */
		}
		nleft -= nwritten;
		ptr += nwritten;
	}
    return n;
}

#define QLEN	16
#define EVL_PATH_ROOT	"/tmp/var/zas"
#define EVL_PATH		"/tmp/var/zas/unix_sockets"
#define EVL_SVR_FILE	"/tmp/var/zas/unix_sockets/rpc.svr"
#define EVL_CLI_FILE	"/tmp/var/zas/unix_sockets/rpc.cli"
#define EVL_CLI_LISTEN_FILE	"/tmp/var/zas/unix_sockets/rpc.listen"

/**
  Create the unix domain socket for the server
  @param fd the listen fd
  @return 0 for success
  */
static int server_init_socket(int& fd)
{
	size_t len;
	struct sockaddr_un un;

	/* create a UNIX domain stream socket */
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return -1;
	setnonblocking(fd);
	unlink(EVL_SVR_FILE);   /* in case it already exists */

	/* fill in socket address structure */
	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, EVL_SVR_FILE);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(EVL_SVR_FILE);

	/* bind the name to the descriptor */
	if (bind(fd, (struct sockaddr *)&un, len) < 0) {
		close(fd);
		return -2;
	}
	evl_unix_socket_dir_set_group(EVL_SVR_FILE);
	if (listen(fd, QLEN) < 0) { /* tell kernel we're a server */
		close(fd);
		return -3;
	}
	return 0;
}

/**
  Create the TCP/IP socket for the server
  @param fd the listen fd
  @return 0 for success
  */
static int server_init_tcpip_socket(int& fd, uint32_t port)
{
	size_t len;
	struct sockaddr_in addr;

	/* create a INET stream socket */
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;
	setnonblocking(fd);

	// set sender and listener can use same port
	int on = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
		(const char*)&on, sizeof(on)) == -1) {
		return -2;
	}

	/* fill in socket address structure */
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);
	len = sizeof(addr);
	/* bind the name to the descriptor */
	if (bind(fd,(struct sockaddr*)&addr, len) < 0) {
		close(fd);
		return -3;
	}

	if (listen(fd, QLEN) < 0) { /* tell kernel we're a server */
		close(fd);
		return -4;
	}
	return 0;
}

static int client_init_socket(int& fd, int& listenfd, pid_t pid)
{
	int len;
	struct sockaddr_un un;

	/* create a UNIX domain stream socket */
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return -1;

	/* fill socket address structure with our address */
	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;

	if (pid) sprintf(un.sun_path, "%s.%05d-peer.%05d", EVL_CLI_FILE, getpid(), pid);
	else sprintf(un.sun_path, "%s.%05d", EVL_CLI_FILE, getpid());
	
	len = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);

	unlink(un.sun_path);        /* in case it already exists */
	if (bind(fd, (struct sockaddr *)&un, len) < 0) {
		close(fd); return -2;
	}

	/* fill socket address structure with server's address */
	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	
	if (!pid) strcpy(un.sun_path, EVL_SVR_FILE);
	else sprintf(un.sun_path, "%s.%05d", EVL_CLI_LISTEN_FILE, pid);
	
	len = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);

	int ret = connect(fd, (struct sockaddr *)&un, len);
	if (ret < 0) {
		perror("client_init_socket");
		close(fd); return -3;
	}
	setnonblocking(fd);
	if (pid) return 0;

	/* this socket will be used to listen to any direct 
	 * connection request
	 * create a UNIX domain stream socket */
	if ((listenfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return -4;
	setnonblocking(listenfd);

	/* fill in socket address structure */
	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	sprintf(un.sun_path, "%s.%05d", EVL_CLI_LISTEN_FILE, getpid());
	len = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);

	unlink(un.sun_path);   /* in case it already exists */

	/* bind the name to the descriptor */
	if (bind(listenfd, (struct sockaddr *)&un, len) < 0) {
		close(listenfd); return -5;
	}
	evl_unix_socket_dir_set_group(un.sun_path);
	if (listen(listenfd, QLEN) < 0) {
		close(listenfd); return -6;
	}
	return 0;
}

static int client_init_tcpip_socket(int& fd, int& listenfd,
	const char* svr_ip, uint32_t svr_port, uint32_t cli_port)
{
	size_t len;
	struct sockaddr_in addr;
	if (cli_port) {
		/* create a INET stream socket */
		if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			return -1;
		setnonblocking(listenfd);

		// set sender and listener can use same port
		int on = 1;
		if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
			(const char*)&on, sizeof(on)) == -1) {
			return -2;
		}

		/* fill in socket address structure */
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(cli_port);
		len = sizeof(addr);
		if (bind(listenfd,(struct sockaddr*)&addr, len) < 0) {
			close(listenfd);
			return -3;
		}

		if (listen(listenfd, QLEN) < 0) { /* tell kernel we're a server */
			close(listenfd);
			return -4;
		}
	}

	if (svr_ip && svr_port) {
		/* this socket will be used to listen to any direct 
		* connection request
		* create a tcp/ip stream socket */
		if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			return -4;

		addr.sin_family = AF_INET;
		addr.sin_port = htons(svr_port);
		int status = inet_pton(AF_INET, svr_ip, &addr.sin_addr);
		if ( errno == EAFNOSUPPORT ) { return -5; }
		len = sizeof(addr);

		int ret = connect(fd, (sockaddr*)&addr, len);
		if (ret < 0) {
			perror("client_init_tcpip_socket");
			close(fd); return -6;
		}
		setnonblocking(fd);
	}

	return 0;
}

static int read(int fd, fifobuffer_impl* fifobuf)
{
	return 0;
}

static size_t fifo_write(int fd, fifobuffer_impl* fifobuf, mutex* mut)
{
	const size_t bufsz = 256;
	uint8_t buf[bufsz];
	size_t pos = 0, nrd = 0, nwri;

	auto_mutex am(mut);
	for (;;) {
		nrd = fifobuf->peekdata(pos, buf, bufsz);
		if (!nrd) break;
		nwri = nonblk_write(fd, buf, nrd);
		pos += nwri;
		if (nwri < nrd) break;
	}
	fifobuf->discard(pos);
	return pos;
}

// 0 is reserved
static uint32_t gblseqcounter = 1;

package_header::package_header()
: magic(PKG_HEADER_MAGIC)
, pkgid(0), seqid(0)
, size(0), sender(0)
{
}

package_header::package_header(uint32_t pkid, uint32_t sz)
: magic(PKG_HEADER_MAGIC)
, pkgid(pkid)
, seqid(gblseqcounter)
, size(sz)
, sender(0)
{
	if (!++gblseqcounter) ++gblseqcounter;
}

package_header::package_header(uint32_t pkid, uint32_t sqid, uint32_t sz)
: magic(PKG_HEADER_MAGIC)
, pkgid(pkid)
, seqid(sqid)
, size(sz)
, sender(NULL)
{
}

void package_header::init(void)
{
	magic = PKG_HEADER_MAGIC;
	pkgid = 0;
	seqid = 0;
	size = 0;
	sender = NULL;
}

void package_header::init(uint32_t pkid, uint32_t sz)
{
	magic = PKG_HEADER_MAGIC;
	pkgid = pkid;
	seqid = gblseqcounter;
	size = sz;
	sender = NULL;
	if (!++gblseqcounter) ++gblseqcounter;
}

void package_header::init(uint32_t pkid, uint32_t sqid, uint32_t sz)
{
	magic = PKG_HEADER_MAGIC;
	pkgid = pkid;
	seqid = sqid;
	size = sz;
	sender = NULL;
}

package_footer::package_footer(package_header& hdr)
{
	init(hdr);
}

void package_footer::init(package_header& hdr)
{
	magic = PKG_FOOTER_MAGIC;
	// todo:
}

evl_bctl_client_info::evl_bctl_client_info()
: bufsz(0), pid(0), ipv4(0), port(0), need_reply(true)
, client_name(0), inst_name(0)
{
	memset(&client_uuid, 0, sizeof(client_uuid));
	validity.all = 0;
	flags.type = 0;
	flags.service_container = 0;
	flags.service_shared = 0;
}

package_header::~package_header()
{
	if (!sender) return;
	evlclient_impl* cl = reinterpret_cast<evlclient_impl*>(sender);
	cl->release();
}

package_header& package_header::operator=(const package_header& ph)
{
	if (&ph == this) return *this;

	magic = ph.magic;
	pkgid = ph.pkgid;
	seqid = ph.seqid;
	size = ph.size;

	evlclient_impl* cl;
	if (sender) {
		cl = reinterpret_cast<evlclient_impl*>(sender);
		cl->release();
	}
	sender = ph.sender;
	if (sender) {
		cl = reinterpret_cast<evlclient_impl*>(sender);
		cl->addref();
	}
	return *this;
}

readonlybuffer* package_header::get_readbuffer(void) const
{
	evlclient_impl *cl = reinterpret_cast<evlclient_impl*>(sender);
	if (NULL == cl) return NULL;
	fifobuffer_impl* buf = cl->get_readbuffer();
	return reinterpret_cast<readonlybuffer*>(buf);
}

void package_header::finish(void) const
{
	evlclient_impl *cl = reinterpret_cast<evlclient_impl*>(sender);
	if (NULL == cl) return;
	fifobuffer_impl* buf = cl->get_readbuffer();
	buf->discard(size + sizeof(package_footer));
	const_cast<package_header*>(this)->sender = NULL;
	cl->release();
}

#define MAX_EVENTS	16

evloop_thread::evloop_thread(evloop_impl* evl)
: _evl(evl)
, _retval(0)
, _wait(NULL) {}

evloop_thread::~evloop_thread()
{
	if (_wait) {
		delete _wait;
		_wait = NULL;
	}
}

int evloop_thread::start_thread(bool waitres)
{
	if (!waitres) return start();
	if (!_wait) _wait = new waitobject();
	_wait->lock();
	int ret = start();
	if (ret) {
		_wait->unlock();
		return ret;
	}
	_wait->wait(15000);
	_wait->unlock();
	return _retval;
}

void evloop_thread::notify_success(void)
{
	if (!_wait) return;
	
	_retval = 0;	// set as success
	_wait->lock();
	_wait->notify();
	_wait->unlock();
}

int evloop_thread::run(void)
{
	if (!_evl) return -ELOGIC;
	_retval = _evl->run();
	if (_retval && _wait) {
		_wait->lock();
		_wait->notify();
		_wait->unlock();
	}
	return _retval;
}

static bool package_notifier_to_listener(void *owner, evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	evloop_pkglistener *pkglnr = reinterpret_cast<evloop_pkglistener*>(owner);
	return pkglnr->on_package(sender, pkghdr, queue);
}

pkglistener_mgr::pkglistener_mgr()
: _pkgid_tree(NULL)
{
	listnode_init(_pkgid_list);
}

pkglistener_mgr::~pkglistener_mgr()
{
	release_all_node();
}

int pkglistener_mgr::add_package_listener(uint32_t pkgid, 
	evloop_pkglistener *lnr)
{
	auto_mutex am(_mut);
	return add_package_listener_unlocked(pkgid, 
		package_notifier_to_listener, (void*)lnr);
}

int pkglistener_mgr::add_package_listener(uint32_t pkgid,
	pkgnotifier notify, void* owner)
{
	auto_mutex am(_mut);
	return add_package_listener_unlocked(pkgid, notify, owner);
}

int pkglistener_mgr::add_package_event(uint32_t pkgid,
	evpoller_event* event)
{
	auto_mutex am(_mut);

	// find the package listener object
	auto* pkglnr = find_package_id_unlocked(pkgid);
	if (NULL == pkglnr) return -ENOTEXISTS;

	return pkglnr->evp_pendings.append(
		reinterpret_cast<evpoller_event_impl*>(event));
}

int pkglistener_mgr::add_package_listener_unlocked(uint32_t pkgid,
	pkgnotifier notify, void* owner)
{
	// check package id exist
	pkglistener* pkglnr = find_package_id_unlocked(pkgid);
	if (pkglnr) return -1;

	//init pkg_listener
	pkglnr = new pkglistener();
	pkglnr->pkg_id = pkgid;
	pkglnr->owner = owner;
	pkglnr->notifier = notify;

	// add to avltree
	if (avl_insert(&_pkgid_tree, &(pkglnr->avlnode),
		package_id_compare)) {
		delete pkglnr; 
		return -2;
	}
	// add to list
	listnode_add(_pkgid_list, pkglnr->ownerlist);
	return 0;
}

int pkglistener_mgr::remove_package_listener(uint32_t pkgid, 
	evloop_pkglistener *lnr)
{
	auto_mutex am(_mut);
	return remove_package_listener_unlocked(pkgid,
		package_notifier_to_listener, (void*)lnr);
}

int pkglistener_mgr::remove_package_listener(uint32_t pkgid,
	pkgnotifier notify, void* owner)
{
	auto_mutex am(_mut);
	return remove_package_listener_unlocked(pkgid, notify, owner);
}

int pkglistener_mgr::remove_package_listener_unlocked(uint32_t pkgid,
	pkgnotifier notify, void* owner)
{
	// check package id exist
	pkglistener* pkglnr = find_package_id_unlocked(pkgid);
	if (!pkglnr) return -1;

	//package id listener is empty, then remove package id node
	avl_remove(&_pkgid_tree, &(pkglnr->avlnode));
	listnode_del(pkglnr->ownerlist);
	delete pkglnr;

	return 0;
}

void pkglistener_mgr::release_all_node(void)
{
	listnode_t* nd;
	while(!listnode_isempty(_pkgid_list)) {
		nd = _pkgid_list.next;
		pkglistener* pkglnr = LIST_ENTRY(pkglistener, ownerlist, nd);
		assert(NULL != pkglnr);
		listnode_del(pkglnr->ownerlist);
		delete pkglnr;
	}
	_pkgid_tree = NULL;
}

int pkglistener_mgr::package_id_compare(avl_node_t* a, avl_node_t* b)
{
	pkglistener* anode = AVLNODE_ENTRY(pkglistener, avlnode, a);
	pkglistener* bnode = AVLNODE_ENTRY(pkglistener, avlnode, b);
	if (anode->pkg_id > bnode->pkg_id)  return 1;
	else if(anode->pkg_id < bnode->pkg_id) return -1;
	else return 0;
}

class triggered_pkgevent_queue_impl
{
public:
	triggered_pkgevent_queue_impl(
		evp_pkgevent_list& q, package_header& hdr)
	: _queue(q), _pkghdr(hdr) {
		listnode_init(_triggered_poller_list);
	}

	~triggered_pkgevent_queue_impl()
	{
		release_triggered_poller_list();
	}

	evpoller_event_impl* dequeue(void)
	{
		// all generic events to be triggered
		auto* ret = _queue.dequeue();
		if (ret) goto next_action;
	
		// all sepcific events to be triggered
		ret = _queue.dequeue_specific(_pkghdr);
		if (ret) goto next_action;
		return NULL;

	next_action:
		auto* poller = ret->getpoller();
		add_poller_node(poller);
		return ret;
	}

	void finalize(void)
	{
		evpoller_event_impl* ev;
		while (NULL != (ev = dequeue()));
		_queue.finalize();

		// trigger all pollers
		trigger_pollers();
	}

private:

	void add_poller_node(evpoller_impl* poller)
	{
		assert(NULL != poller);
		auto* node = new triggered_poller_node;
		assert(NULL != node);
		node->poller = poller;
		listnode_add(_triggered_poller_list, node->ownerlist);
	}

	void release_triggered_poller_list()
	{
		while (!listnode_isempty(_triggered_poller_list)) {
			auto* node = list_entry(triggered_poller_node, ownerlist, \
				_triggered_poller_list.next);
			listnode_del(node->ownerlist);
			delete node;
		}
	}

	void trigger_pollers(void)
	{
		while (!listnode_isempty(_triggered_poller_list)) {
			auto* node = list_entry(triggered_poller_node, ownerlist, \
				_triggered_poller_list.next);

			listnode_del(node->ownerlist);
			assert(NULL != node->poller);
			node->poller->trigger_submit();
			delete node;
		}
	}

private:
	evp_pkgevent_list& _queue;
	package_header& _pkghdr;

	struct triggered_poller_node {
		listnode_t ownerlist;
		evpoller_impl* poller;
	};
	listnode_t _triggered_poller_list;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(triggered_pkgevent_queue_impl);
};

evpoller_event* triggered_pkgevent_queue::dequeue(void) const
{
	auto* q = (triggered_pkgevent_queue_impl*)this;
	return reinterpret_cast<evpoller_event*>(q->dequeue());
}

bool pkglistener_mgr::handle_package(const package_header &pkghdr)
{
	// check msg head client_impl
	evlclient_impl* cl = reinterpret_cast
	<evlclient_impl*>(pkghdr.sender);
	assert(NULL != cl);

	//check package listener
	pkglistener* pkglnr = find_package_id(pkghdr.pkgid);
	if (NULL == pkglnr) return false;

	// make evlclient object
	evlclient client;
	cl->addref();
	client.set(reinterpret_cast<evlclient_object*>(cl));

	// call on_package
	triggered_pkgevent_queue_impl queue(pkglnr->evp_pendings,
		const_cast<package_header&>(pkghdr));
	auto* q = reinterpret_cast<triggered_pkgevent_queue*>(&queue);
	bool ret = pkglnr->notifier(pkglnr->owner, client, pkghdr, *q);

	queue.finalize();
	return true;
}

pkglistener_mgr::pkglistener* 
pkglistener_mgr::find_package_id(uint32_t pkgid)
{
	auto_mutex am(_mut);
	return find_package_id_unlocked(pkgid);
}

pkglistener_mgr::pkglistener* 
pkglistener_mgr::find_package_id_unlocked(uint32_t pkgid)
{
	avl_node_t* anode = avl_find(_pkgid_tree, 
		MAKE_FIND_OBJECT(pkgid, pkglistener, pkg_id, avlnode),
		package_id_compare);
	if (NULL == anode)
		return NULL;
	return AVLNODE_ENTRY(pkglistener, avlnode, anode);
}

evllistener_mgr::evllistener_mgr()
: _lnr_tree(NULL)
{
	listnode_init(_lnr_list);
	listnode_init(_pending_evclients);
}

evllistener_mgr::~evllistener_mgr()
{
	release_all_nodes();
}

int evllistener_mgr::add_listener(const char* name,
	evloop_listener* lnr)
{
	auto_mutex am(_mut);

	if (!name || !*name || !lnr) return -EBADPARM;

	if (find_listener_unlock(name))
		return -EEXISTS;

	auto* info = new listener_info();
	info->name = name;
	info->lnr = lnr;
	if (avl_insert(&_lnr_tree, &info->avlnode,
		evloop_listener_compare)) {
		delete info; return -ELOGIC;
	}
	listnode_add(_lnr_list, info->ownerlist);
	return 0;
}

int evllistener_mgr::remove_listener(const char* name)
{
	auto_mutex am(_mut);

	if (!name || !*name) return -EBADPARM;

	auto *info = find_listener_unlock(name);
	if (!info) return -ENOTEXISTS;

	avl_remove(&_lnr_tree, &info->avlnode);
	listnode_del(info->ownerlist);
	delete info;
	return 0;
}

void evllistener_mgr::accepted(evlclient client)
{
	if (listnode_isempty(_lnr_list)) return;

	auto_mutex am(_mut);
	listnode_t * nd = _lnr_list.next;
	listener_info* info = NULL;
	for (; nd != &_lnr_list; nd = nd->next) {
		info = LIST_ENTRY(listener_info, ownerlist, nd);
		if (!info) return;
		info->lnr->accepted(client);
	}
}

void evllistener_mgr::connected(evlclient client)
{

	// we need lock
	auto_mutex am(_mut);

	// all pending evlclient shall be add
	auto* evl = reinterpret_cast<evloop_impl*>(evloop::inst());
	while (!listnode_isempty(_pending_evclients)) {
		auto* pending = list_entry(pending_evlclient, ownerlist,\
			_pending_evclients.next);
		assert(nullptr != pending->client);
		evl->register_client(pending->client);
		
		// release pending node
		pending->client->release();
		listnode_del(pending->ownerlist);
		delete pending;
	}

	// notify all listeners
	if (listnode_isempty(_lnr_list)) return;
	listnode_t * nd = _lnr_list.next;
	listener_info* info = NULL;
	for (; nd != &_lnr_list; nd = nd->next) {
		info = LIST_ENTRY(listener_info, ownerlist, nd);
		if (!info) return;
		info->lnr->connected(client);
	}
}

void evllistener_mgr::disconnected(const char* cliname, const char* instname)
{
	if (listnode_isempty(_lnr_list)) return;

	auto_mutex am(_mut);
	listnode_t * nd = _lnr_list.next;
	listener_info* info = NULL;
	for (; nd != &_lnr_list; nd = nd->next) {
		info = LIST_ENTRY(listener_info, ownerlist, nd);
		if (!info) return;
		info->lnr->disconnected(cliname, instname);
	}
}

evllistener_mgr::pending_evlclient*
evllistener_mgr::get_evlclient_pending_unlocked(
	evlclient_impl* cli)
{
	auto* node = _pending_evclients.next;
	for (; node != &_pending_evclients; node = node->next) {
		auto* cliobj = list_entry(pending_evlclient, ownerlist, node);
		if (cliobj->client == cli) return cliobj;
	}
	return nullptr;
}

int evllistener_mgr::add_evlclient_pending_unlocked(evlclient_impl* cli)
{
	auto* cliobj = new pending_evlclient;
	if (nullptr == cliobj) return -1;

	cli->addref();
	cliobj->client = cli;
	listnode_add(_pending_evclients, cliobj->ownerlist);
	return 0;
}

int evllistener_mgr::add_pending_evlclient(evlclient_impl* cli)
{
	assert(nullptr != cli);

	// we need lock
	auto_mutex am(evlmut);

	// check if we already have this client in pending list
	if (get_evlclient_pending_unlocked(cli)) {
		return 0;
	}
	// add it into pending list
	if (add_evlclient_pending_unlocked(cli)) {
		return -1;
	}
	return 0;
}

int evllistener_mgr::remove_pending_evlclient(evlclient_impl* cli)
{
	assert(nullptr != cli);

	// we need lock
	auto_mutex am(evlmut);

	// failed if the specified client is not in the pending list
	auto* pending = get_evlclient_pending_unlocked(cli);
	if (nullptr == pending) return -1;

	// remove it from the pending list
	assert(nullptr != pending->client);
	pending->client->release();
	listnode_del(pending->ownerlist);
	delete pending;

	return 0;
}


int evllistener_mgr::evloop_listener_compare(avl_node_t* aa, avl_node_t* bb)
{
	auto* infoa = AVLNODE_ENTRY(listener_info, avlnode, aa);
	auto* infob = AVLNODE_ENTRY(listener_info, avlnode, bb);
	int ret = strcmp(infoa->name.c_str(), infob->name.c_str());
	if (ret > 0) return 1;
	else if (ret < 0) return -1;
	else return 0;
}

evllistener_mgr::listener_info* 
evllistener_mgr::find_listener_unlock(const char* nm)
{
	if (!nm || !*nm) return NULL;

	auto* infond = avl_find(_lnr_tree,
		MAKE_FIND_OBJECT(nm, listener_info, name, avlnode),
		evloop_listener_compare);
	if (!infond) return NULL;

	return AVLNODE_ENTRY(listener_info, avlnode, infond);
}

int evllistener_mgr::release_all_nodes(void)
{
	auto_mutex am(_mut);

	listener_info* info = NULL;
	while(!listnode_isempty(_lnr_list)) {
		info = LIST_ENTRY(listener_info, avlnode, _lnr_list.next);
		listnode_del(info->ownerlist);
		delete info;
	}
	_lnr_tree = NULL;

	// release all pending evlclients
	while (!listnode_isempty(_pending_evclients)) {
		auto* pending = list_entry(pending_evlclient, ownerlist,\
			_pending_evclients.next);
		listnode_del(pending->ownerlist);

		assert(nullptr != pending->client);
		pending->client->release();
		delete pending;
	}
	return 0;
}


evloop_impl::evloop_impl()
: _epollfd(-1)
, _server(NULL)
, _tcp_server(NULL)
, _connmgr(NULL)
, _tcp_connmgr(NULL)
, _flags(0)
, _clientinfo(NULL)
, _evlthd(NULL)
, _container_tid(0) {
	_f.state = evloop_state_created;
	regist_client_info_ack_listen();
}

evloop_impl::~evloop_impl()
{
	// we'll not close the fd here
	// we let the run() routine to
	// handle it
	if (_evlthd) {
		delete _evlthd;
		_evlthd = NULL;
	}
	if (_clientinfo) {
		_clientinfo->release();
		_clientinfo = NULL;
	}
}

evloop_state evloop_impl::get_state(void) {
	return (evloop_state)(_f.state);
}

int evloop_impl::setrole(evloop_role r)
{
	// check if we already set the role
	if (role_avail()) {
		return -EEXISTS;
	}
	_f.role = r;
	return 0;
}

evloop_role evloop_impl::getrole(void)
{
	if (evloop_role_server == _f.role)
		return evloop_role_server;
	else if (evloop_role_client == _f.role)
		return  evloop_role_client;
	
	return evloop_role_unknown;
}
	
bool evloop_impl::is_server(void) {
	return (_f.role == evloop_role_server)
	? true : false;
}

bool evloop_impl::is_client(void) {
	return (_f.role == evloop_role_client)
	? true : false;
}

int evloop_impl::run(void)
{
	int ret;
	if (!role_avail() || _f.finished) {
		return -ELOGIC;
	}
	// showing that we are running
	setrunning();

	/* the num is ignored by kernel but must be
		large than 0 */
	_epollfd = epoll_create1(EPOLL_CLOEXEC);
	if (-1 == _epollfd) return seterror() - 1;

	// create the server/clicent socket
	if(_f.role == evloop_role_server) {
		ret = init_server();
	} else {
		if (_clientinfo && _clientinfo->get_listen_port()
			&& _clientinfo->get_server_port()) {
			ret = init_tcpip_client();
		} else {
			ret = init_client();
		}
	}

	if (ret) return seterror() + ret;

	// register timerfd
	int tmrfd = timermgr_impl::getdefault()->getfd();
	assert(-1 != tmrfd);

	// create the client object
	evlclient_impl* cl = evlclient_impl::create(tmrfd,
		client_type_timer);
	assert(NULL != cl);

	ret = epoll_add(_epollfd, tmrfd, cl);
	if (-1 == ret && errno != EEXIST) {
		return seterror() + ret;
	}
	// create the event client
	cl = tasklet_client::create("generic-tasklet");
	assert(NULL != cl);
	ret = epoll_add(_epollfd, cl->getfd(), cl, EPOLLIN | EPOLLHUP | EPOLLERR);
	if (-1 == ret && errno != EEXIST) {
		return seterror() + ret;
	}
	// enable the client_info (which is a tasklet client)
	if (NULL == _clientinfo) {
		_clientinfo = new client_info(this);
		assert(NULL != _clientinfo);
	}
	ret = epoll_add(_epollfd, _clientinfo->getfd(), cl, EPOLLIN | EPOLLHUP | EPOLLERR);
	if (-1 == ret && errno != EEXIST) {
		return seterror() + ret;
	}
	setinitialized();

	// modify by wyq
	// use wait_handle instead of wait_get_package
	for (;;) wait_handle(1000);

	// finally we finished
	setfinish();
	return 0;
}

bool evloop_impl::is_evloop_thread(void)
{
	if (_evlthd) return _evlthd->is_myself();
	return (gettid() == _container_tid)
		? true : false;
}

int evloop_impl::start(bool septhd, bool wait)
{
	if (!role_avail() || _f.finished) {
		return -ELOGIC;
	}

	if (septhd) {
		if (_evlthd) return -EEXISTS;
		_evlthd = new evloop_thread(this);
		assert(NULL != _evlthd);

		// start the event loop
		int ret = _evlthd->start_thread(wait);
		if (ret) {
			delete _evlthd;
			_evlthd = NULL;
		}
		return ret;
	}

	// we will run the event loop directly
	// in the current thread
	_container_tid = gettid();
	return run();
}

evlclient_impl* evloop_impl::get_serverclient(void)
{
	if (is_running()) {
		if (is_server()) {
			//return evlclient of itself
			_clientinfo->addref();
			return _clientinfo;
		} else {
			//client return evlclient of server
			_server->addref();
			return _server;
		}
	}
	return NULL;
}

bool evloop_impl::is_running(void) {
	return (_f.running) ? true : false;
}

bool evloop_impl::is_finished(void) {
	return (_f.finished) ? true : false;
}

bool evloop_impl::is_initialized(void) {
	return (_f.running && _f.initialized)
		? true : false;
}

int evloop_impl::write_wait_comsume(int timeout)
{
	long start_time = gettick_millisecond();

	if (is_evloop_thread()) {
		// we are in evloop thread, check
		// if we are in a coroutine
		coroutine* cor = _cormgr.get_current();
		for (;;) {
			if (NULL == cor) {
				// we are in evloop main procedure
				// run epoll_wait to consume some data
				int ret = wait_handle(timeout);
				if (ret < 0) return ret;

				// check timeout
				if (timeout >= 0) {
					long curr = gettick_millisecond();
					timeout -= curr - start_time;
					if (timeout <= 0) return -ETIMEOUT;
					start_time = curr;
				}
			}
			else {
				// yield the coroutine, this may
				// finally return to the main evloop
				// procedure and consume some data
				cor->yield();

				// check timeout
				if (gettick_millisecond() - start_time >= timeout) {
					return -ETIMEOUT;
				}
			}
		}
	}
	else for (;;) {
		// we are not in the evloop thread
		// try waste some time (wait for evloop
		// to consume some data)
		msleep(2);
		if (gettick_millisecond() - start_time >= timeout) {
			return -ETIMEOUT;
		}
	}
	// shall never go here
	return -EINVALID;
}

size_t evloop_impl::write(int fd, const void* src,
	size_t nleft, mutex* mut, fifobuffer_impl* fifobuf,
	int timeout, bool prelock)
{
	if (!nleft) return 0;
	size_t nwri = 0;
	const char* s = (const char*)src;
	const char* o = s;

	// this is the start time stamp
	long start_time = gettick_millisecond();
	if (mut && !prelock) mut->lock();

	while (nleft > 0) {
		// fifobuf empty means all data transfered
		// we can try directly write data, otherwise
		// we need to put data into buffer
		if (fifobuf->empty()) {
			nwri = nonblk_write(fd, s, nleft);
			s += nwri, nleft -= nwri;
			// all written?
			if (!nleft) { break; }
		}

		// the kernel buffer is full, buffering the data
		// check if the fifobuffer has enough space
		// try lock the fifo first
		for (;;) {
			if (fifobuf->enter()) break;
			if (mut) mut->unlock();

			// wait 10ms for evloop to consume some data
			write_wait_comsume(10);
			if (mut) mut->lock();

			if (gettick_millisecond() - start_time > timeout) {
				nleft = 0; break;
			}
		}

		// write data to fifobuffer, which will be delivered later
		for (nwri = 0; nleft > 0;)
		{
			nwri = fifobuf->append((void*)s, nleft);
			nleft -= nwri, s += nwri;

			// check if we read all data
			if (!nleft) { break; }

			// check if we timed-out
			if (gettick_millisecond() - start_time > timeout) {
				nleft = 0; break;
			}
			// we have not finished buffering
			// this shall be "fifo buffer full", wait for
			// consuming some data
			if (mut) mut->unlock();
			write_wait_comsume(5);
			if (mut) mut->lock();

			if (fifobuf->empty()) {
				break;
			}
		}
		fifobuf->exit();
	}
	if (mut && !prelock) mut->unlock();
	return s - o;
}

int evloop_impl::write(evlclient_impl* cli, void* buf, size_t sz, int timeout)
{
	if (!cli || !buf || !sz) return -EBADPARM;

	fifobuffer_impl* fifobuf = cli->get_writebuffer();
	mutex* mut = cli->getmutex();
	size_t n = write(cli->getfd(), buf, sz, mut, fifobuf, timeout);

	if (n != sz) return -EBUSY;
	return 0;
}

int evloop_impl::write(int fd, void* buf, size_t sz, int timeout)
{
	if (!fd || !buf || !sz) return -EBADPARM;

	evlclient_impl* cli = evlclient_impl::getclient(fd);
	if (NULL == cli) return -ENOTEXISTS;

	fifobuffer_impl* fifobuf = cli->get_writebuffer();
	mutex* mut = cli->getmutex();
	size_t n = write(fd, buf, sz, mut, fifobuf, timeout);
	cli->release();

	if (n != sz) return -EBUSY;
	return 0;
}

pkglistener_mgr* evloop_impl::get_package_listener_manager(void) {
	return &_pkgid_mgr;
}

coroutine_mgr* evloop_impl::get_coroutine_manager(void) {
	return &_cormgr;
}

int evloop_impl::addtask(const char* tasklet_name, evloop_task* task)
{
	if (NULL == tasklet_name || NULL == task)
		return -EBADPARM;
	if (is_finished()) return -ELOGIC;

	if (!is_running()) {
		int ret = start(true, true); // start the evloop
		assert(!ret);
	}		
	if (!is_initialized()) return -ELOGIC;

	// make sure the evloop is initialized
	// if (!is_initialized()) {
	//	if (_evlthd->is_myself())
	//		return -ENOTREADY;
	//	for (; !is_initialized(); msleep(1));
	// }

	tasklet_client* tcl = reinterpret_cast<tasklet_client*>
		(evlclient_impl::getclient(TASKLET_CLIENT_APP_NAME, tasklet_name));
	if (NULL == tcl) {
		return -ENOTEXISTS;
	}
	return tcl->addtask(task);
}

int evloop_impl::update_clientinfo(int cmd, va_list vl)
{
	// check if the role has been set
	if (!role_avail()) return -ELOGIC;

	if (NULL == _clientinfo) {
		_clientinfo = new client_info(this);
		assert(NULL != _clientinfo);
	}
	int ret = _clientinfo->update(cmd, vl);
	if (ret) return ret;

	size_t sz = _clientinfo ?
		_clientinfo->getsize() : 0;

	evl_bctl_client_info_pkg* pkg = new(alloca(sizeof(*pkg) + sz))
		evl_bctl_client_info_pkg(sz);

	evl_bctl_client_info& clinfo = pkg->payload();

	clinfo_validity_u val;
	_clientinfo->load(clinfo);
	ret = _clientinfo->update_info(&clinfo, val);
	if (ret) return ret;

	if (is_client() && is_initialized()) {
		// todo: update the remote info
	}
	return 0;
}

evlclient_impl* evloop_impl::getclient(const char* cname, const char* iname)
{
	evlclient_impl* ret = NULL;
	
	// see if "myself" is requested
	if (_clientinfo && _clientinfo->equal(cname, iname)) {
		_clientinfo->addref();
		return _clientinfo;
	}

	// if we are the system server, return "not found"
	if (is_server()) return NULL;

	size_t cname_sz = strlen(cname) + 1;
	size_t sz = cname_sz + strlen(iname) + 1;
	evl_bctl_retrive_client_pkg* clinfo = new(alloca(sizeof(*clinfo) + sz))
		evl_bctl_retrive_client_pkg(sz);
	evl_bctl_retrive_client& cl = clinfo->payload();

	memcpy(cl.client_name, cname, cname_sz);
	strcpy(&cl.client_name[cname_sz], iname);
	cl.inst_name = uint16_t(cname_sz);

	sz += sizeof(*clinfo);
	if (write(_server, clinfo, sz, 500) < 0) {
		// todo: error handle
		return NULL;
	}

	// wait for reply
	evpoller poller;
	auto* ev = poller.create_event(evp_evid_package_with_seqid,
		EVL_BCTL_RETRIVE_CLIENT_ACK, clinfo->header().seqid);
	assert(NULL != ev);
	ev->write_inputbuf(&_server, sizeof(void*));
	// submit the event
	ev->submit();
	if (poller.poll(1000)) {
		return NULL;
	}
	ev = poller.get_triggered_event();
	assert(!poller.get_triggered_event());
	evlclient_impl* client = NULL;
	ev->read_outputbuf(&client, sizeof(void*));
	return client;
}

int evloop_impl::add_package_listener(uint32_t pkgid,
	evloop_pkglistener *lnr)
{
	return _pkgid_mgr.add_package_listener(pkgid, lnr);
}

int evloop_impl::add_package_listener(uint32_t pkgid,
	pkgnotifier notify, void* owner)
{
	return _pkgid_mgr.add_package_listener(pkgid, notify, owner);
}

int evloop_impl::add_listener(const char* name, evloop_listener *lnr)
{
	return _evloop_lnr_mgr.add_listener(name, lnr);
}

int evloop_impl::remove_package_listener(uint32_t pkgid,
	evloop_pkglistener *lnr)
{
	return _pkgid_mgr.remove_package_listener(pkgid, lnr);
}

int evloop_impl::remove_package_listener(uint32_t pkgid,
	pkgnotifier notify, void* owner)
{
	return _pkgid_mgr.remove_package_listener(pkgid, notify, owner);
}

int evloop_impl::remove_listener(const char* name)
{
	return _evloop_lnr_mgr.remove_listener(name);
}

int evloop_impl::register_client(evlclient_impl* cli)
{
	if (cli->getfd() <= 0) return -ENOTHANDLED;
	if (_epollfd < 0) {
		if (is_server()) {
			return -ENOTREADY;
		}
		// I'm a client
		if (get_state() == evloop_state_connected) {
			return -EINVALID;
		}
		// the evloop is not ready, put it into pending list
		_evloop_lnr_mgr.add_pending_evlclient(cli);
		return 0;
	}
	return  epoll_add(_epollfd, cli->getfd(), cli,
		EPOLLIN | EPOLLHUP | EPOLLERR);
}

int evloop_impl::deregister_client(evlclient_impl* cli)
{
	if (cli->getfd() <= 0) return -ENOTHANDLED;
	if (_epollfd < 0) {
		if (is_server()) {
			return -ENOTREADY;
		}
		// I'm a client
		if (_evloop_lnr_mgr.remove_pending_evlclient(cli)) {
			return -EINVALID;
		}
		return 0;
	}
		
	evloop_state st = get_state();
	if (st != evloop_state_connected
		&& st != evloop_state_disconnected) {
		return -EINVALID;
	}
	int ret = epoll_del(_epollfd, cli->getfd());
	cli->destroy();
	return ret;
}

int evloop_impl::init_server(void)
{		
	int fd;
	// initialize the unix socket path
	// this shall only be done by server
	// since we assume the server will be run
	// early than the client
	if (createdir(EVL_PATH_ROOT))
		return -2;
	if (removedir(EVL_PATH)) return -3;
	if (createdir(EVL_PATH)) return -4;
	evl_unix_socket_dir_set_group(EVL_PATH);
	// init the server socket
	if (server_init_socket(fd)) {
		close(_epollfd), _epollfd = -1;
		return -5;
	}

	// add the "unique server" as a client
	_server = evlclient_impl::create(fd,
		client_type_unix_socket_listenfd);
	assert(NULL != _server);

	// add the listen fd to epoll
	// _fd is the "listenfd"
	if (epoll_add(_epollfd, fd, _server))
	{
		close(_epollfd);
		_epollfd = -1;
		
		_server->release();
		_server = NULL;
		
		close(fd);
		return -6;
	}

	// set the server as ready
	_f.state = evloop_state_ready;

	//init tcpip sock
	if (_clientinfo && _clientinfo->get_listen_port()) {
		int tcp_fd;
		if (server_init_tcpip_socket(tcp_fd, _clientinfo->get_listen_port())){
			return -7;
		}

		// add the "unique server" as a client
		_tcp_server = evlclient_impl::create(tcp_fd,
			client_type_socket_listenfd);
		assert(NULL != _tcp_server);

		// add the listen fd to epoll
		// _fd is the "listenfd"
		if (epoll_add(_epollfd, tcp_fd, _tcp_server))
		{
			_tcp_server->release();
			_tcp_server = NULL;

			close(tcp_fd);
			return -8;
		}
	}
	return 0;
}

int evloop_impl::init_client(void)
{
	// set connecting status
	_f.state = evloop_state_connecting;

	// we only make sure the path exists
	if (!fileexists(EVL_PATH))
		return -1;

	int fd, listenfd;
	if (client_init_socket(fd, listenfd, 0)) {
		close(_epollfd), _epollfd = -1;
		return -2;
	}

	// add the "unique server" as a client
	_server = evlclient_impl::create(fd,
		client_type_unix_socket);
	assert(NULL != _server);

	// add the server fd to epoll
	// _fd is the "unique server fd"
	if (epoll_add(_epollfd, fd, _server))
	{
		close(_epollfd);
		_epollfd = -1;

		_server->release();
		_server = NULL;

		close(fd);
		return -3;
	}

	// add the listenfd as a client
	_connmgr = connmgr_client::create(listenfd);
	assert(NULL != _connmgr);

	int ret = -4;
	// add the listen fd to epoll
	if (epoll_add(_epollfd, listenfd, _connmgr))
	{
	final_error:
		close(_epollfd);
		_epollfd = -1;

		_server->release();
		_server = NULL;

		_connmgr->release();
		_connmgr = NULL;

		close(listenfd);
		close(fd);
		return ret;
	}
	if (_clientinfo && _clientinfo->get_listen_port()) {
		int tcpfd, tcplistenfd;
		if (client_init_tcpip_socket(tcpfd, tcplistenfd, 
			nullptr,
			0,
			_clientinfo->get_listen_port())) {
			// close(_epollfd), _epollfd = -1;
			return -2;
		}

		// add the listenfd as a client
		_tcp_connmgr = tcp_connmgr_client::create(tcplistenfd);
		assert(NULL != _tcp_connmgr);

		int ret = -4;
		// add the listen fd to epoll
		if (epoll_add(_epollfd, tcplistenfd, _tcp_connmgr))
		{
			close(tcplistenfd);
			return ret;
		}
	}
	
	// update the client info to server
	ret = act_update_remote_client_info(_server, true);
	if (ret) goto final_error;

	// set connected state
	_f.state = evloop_state_connected;

	//notify connect
	evlclient ccli;
	_server->addref();
	ccli.set(reinterpret_cast<evlclient_object*>(_server));
	
	assert(_epollfd >= 0);
	_evloop_lnr_mgr.connected(ccli);
	return ret;
}

int evloop_impl::init_tcpip_client(void)
{
	assert(nullptr != _clientinfo);
	// set connecting status
	_f.state = evloop_state_connecting;

	int fd, listenfd;
	if (client_init_tcpip_socket(fd, listenfd, 
		_clientinfo->get_server_ipv4(),
		_clientinfo->get_server_port(),
		_clientinfo->get_listen_port())) {
		close(_epollfd), _epollfd = -1;
		return -2;
	}

	// add the "unique server" as a client
	_server = evlclient_impl::create(fd,
		client_type_socket);
	assert(NULL != _server);

	// add the server fd to epoll
	// _fd is the "unique server fd"
	if (epoll_add(_epollfd, fd, _server))
	{
		close(_epollfd);
		_epollfd = -1;

		_server->release();
		_server = NULL;

		close(fd);
		return -3;
	}

	// add the listenfd as a client
	_tcp_connmgr = tcp_connmgr_client::create(listenfd);
	assert(NULL != _tcp_connmgr);

	int ret = -4;
	// add the listen fd to epoll
	if (epoll_add(_epollfd, listenfd, _tcp_connmgr))
	{
	final_error:
		close(_epollfd);
		_epollfd = -1;

		_server->release();
		_server = NULL;

		_tcp_connmgr->release();
		_tcp_connmgr = NULL;

		close(listenfd);
		close(fd);
		return ret;
	}
	
	// update the client info to server
	ret = act_update_remote_client_info(_server, true);
	if (ret) goto final_error;

	// set connected state
	_f.state = evloop_state_connected;

	//notify connect
	evlclient ccli;
	_server->addref();
	ccli.set(reinterpret_cast<evlclient_object*>(_server));
	
	assert(_epollfd >= 0);
	_evloop_lnr_mgr.connected(ccli);
	return ret;
}

bool evloop_impl::handle_client_info_ack(evlclient sender,
	const package_header& header,
	const triggered_pkgevent_queue& queue)
{
	int status;
	// get the package payload
	evl_bctl_client_info_ack* ack = (evl_bctl_client_info_ack*)
		alloca(header.size);
	readonlybuffer* buf = header.get_readbuffer();
	assert(NULL != buf);

	auto* ev = queue.dequeue();
	assert(NULL == queue.dequeue());
	evlclient_impl* peer = NULL;
	ev->read_inputbuf(&peer, sizeof(void*));
	assert(NULL != peer);
	if (buf->read(ack, header.size) == header.size) {
		clinfo_validity_u val;
		peer->update_info(&ack->info, val);
		status = ack->status;
	}
	else status = -3;

	// update info of event
	ev->write_outputbuf(&status, sizeof(status));
	return true;
}

bool evloop_impl::handle_retrive_client_ack(evlclient sender,
	const package_header& header,
	const triggered_pkgevent_queue& queue)
{

	evl_bctl_retrive_client_ack* cinfo = (evl_bctl_retrive_client_ack*)
		alloca(header.size);

	readonlybuffer* buf = header.get_readbuffer();
	assert(NULL != buf);

	auto* ev = queue.dequeue();
	assert(NULL == queue.dequeue());
	evlclient_impl* result = NULL;
	size_t n = buf->peekdata(0, cinfo, header.size);
	assert(n == header.size);

	if (cinfo->status < 0 || !cinfo->info.validity.m.client_pid) {
		ev->write_outputbuf(&result, sizeof(void*));
		return false;
	}

	// check if the client is the "syssvr"
	if (cinfo->info.flags.type == CLINFO_TYPE_SYSSVR) {
		_server->addref();
		ev->write_outputbuf(&_server, sizeof(void*));
		return false;
	}

	evlclient_impl* waitsender = nullptr;
	ev->read_inputbuf(&waitsender, sizeof(void*));
	assert(nullptr != waitsender);
	// connect to the peer
	evlclient_impl* peer = nullptr;
	if (client_type_unix_socket == waitsender->gettype()) {
		if (cinfo->info.validity.m.client_pid) {
			peer = init_peer(cinfo->info.pid);
		}
		else {
			ev->write_outputbuf(&result, sizeof(void*));
			return false;
		}
	} else if (client_type_socket == waitsender->gettype()) {
		if (cinfo->info.validity.m.client_ipv4addr
			&& cinfo->info.validity.m.client_port) {
			peer = init_tcpip_peer(cinfo->info.ipv4, cinfo->info.port);
		} else {
			ev->write_outputbuf(&result, sizeof(void*));
			return false;
		}
	} else {
		ev->write_outputbuf(&result, sizeof(void*));
		return false;
	}

	if (cinfo->info.validity.m.client_ipv4addr
		&& cinfo->info.validity.m.client_port
		&& !_connmgr) {
	 	peer = init_tcpip_peer(cinfo->info.ipv4, cinfo->info.port);
	} else {
	 	peer = init_peer(cinfo->info.pid);
	}
	if (peer) peer->addref();
	else { 
		ev->write_outputbuf(&result, sizeof(void*));
		return false;
	}

	clinfo_validity_u val;
	peer->update_info(&cinfo->info, val);
	ev->write_outputbuf(&peer, sizeof(void*));

	// update my information to the peer
	auto ret = act_update_remote_client_info(peer, false);
	assert(ret == 0);

	return true;
}

int evloop_impl::regist_client_info_ack_listen(void)
{
	_pkgid_mgr.remove_package_listener(EVL_BCTL_CLIENT_INFO_ACK,
		zas::utils::handle_client_info_ack, (void*)this);
	_pkgid_mgr.add_package_listener(EVL_BCTL_CLIENT_INFO_ACK,
		zas::utils::handle_client_info_ack, (void*)this);
	_pkgid_mgr.remove_package_listener(EVL_BCTL_RETRIVE_CLIENT_ACK,
		zas::utils::handle_retrive_client_ack, (void*)this);
	_pkgid_mgr.add_package_listener(EVL_BCTL_RETRIVE_CLIENT_ACK,
		zas::utils::handle_retrive_client_ack, (void*)this);
	return 0;
}

int evloop_impl::act_update_remote_client_info(evlclient_impl* svr, bool reply)
{
	size_t sz = _clientinfo ?
		_clientinfo->getsize() : 0;

	evl_bctl_client_info_pkg* clinfo = new(alloca(sizeof(*clinfo) + sz))
		evl_bctl_client_info_pkg(sz);
	evl_bctl_client_info& obj = clinfo->payload();

	if (_clientinfo) {
		_clientinfo->load(obj);
	}
	obj.need_reply = reply;

	sz += sizeof(*clinfo);
	if (write(svr, clinfo, sz, 500) < 0) {
		// todo: error handle
		return -1;
	}
	if (!reply) return 0;

	// wait for reply
	evpoller poller;
	auto* ev = poller.create_event(evp_evid_package_with_seqid,
		EVL_BCTL_CLIENT_INFO_ACK, clinfo->header().seqid);
	assert(NULL != ev);
	ev->write_inputbuf(&svr, sizeof(void*));
	// submit the event
	ev->submit();
	if (poller.poll(1000)) {
		return -4;
	}
	ev = poller.get_triggered_event();
	assert(!poller.get_triggered_event());
	int status = -1;
	ev->read_outputbuf(&status, sizeof(status));
	return status;
}

size_t evloop_impl::drain(int fd)
{
	char buf[256];
	size_t ret, out;
	for (ret = 0;;) {
		out = nonblk_read(fd, buf, 256);
		ret += out;
		if (out < 256) break;
	}
	return ret;
}

size_t evloop_impl::epoll_read_drain(int fd, fifobuffer_impl* buf)
{
	size_t ret = 0, sz;
	for (;;) {
		sz = buf->append_getsize();
		if (!sz) {
			ret += drain(fd);
			return ret;
		}
		size_t rsz = buf->append(fd, sz);
		ret += rsz;
		if (rsz < sz) break;
	}
	return ret;
}

// this function is not locked
size_t evloop_impl::move_to_package_start(fifobuffer_impl* fifobuf)
{
	uint32_t magic;
	size_t bufsz = fifobuf->getsize();
	if (bufsz < sizeof(package_header))
		return 1;

	size_t n = fifobuf->peekdata(0, &magic, sizeof(uint32_t));
	if (n != sizeof(uint32_t)) return 2;
	if (magic == PKG_HEADER_MAGIC) return 0;

	const size_t chksz = 256;
	uint8_t buf[chksz];
	size_t start = 0;

	fifobuf->seek(0, seek_set);
	while (bufsz)
	{
		size_t nread = (chksz > bufsz) ? bufsz : chksz;
		if (nread < sizeof(uint32_t)) return 2;
		n = fifobuf->read(buf, nread);
		assert(n == nread);

		for (size_t i = 0, end = nread - (sizeof(uint32_t) - 1); i < end; ++i)
		{
			magic = *((uint32_t*)&buf[i]);
			if (magic == PKG_HEADER_MAGIC) {
				// we finally find the start of package
				fifobuf->discard(start + i);
				return 0;
			}
		}
		start += nread;
		bufsz -= nread;
	}
	// there is no package found in the fifobuffer
	fifobuf->drain();
	return 3;
}

bool evloop_impl::check_footer(package_header& hdr, package_footer& footer)
{
	// todo: need revise
	return (footer.magic == PKG_FOOTER_MAGIC) ? true : false;
}

int evloop_impl::readbuf_get_package(evlclient_impl* cl, package_header& pkghdr)
{
	fifobuffer_impl* buf = cl->get_readbuffer();
	assert(NULL != buf);

	for (;;) {
		if (move_to_package_start(buf))
			return -1;

		// if the space can hold the package header
		size_t bufsz = buf->getsize();
		if (bufsz < sizeof(package_header)) {
			return 2;
		}
		size_t n = buf->peekdata(0, &pkghdr, sizeof(package_header));
		assert(n == sizeof(package_header));
		// check if the buffer data is ready
		if (bufsz < pkghdr.size + sizeof(package_header)
			+ sizeof(package_footer)) {
			return 3;
		}
		// check if the footer is valid
		package_footer footer;
		n = buf->peekdata(sizeof(package_header) + pkghdr.size,
			&footer, sizeof(package_footer));
		assert(n == sizeof(package_footer));

		if (!check_footer(pkghdr, footer)) {
			buf->discard(sizeof(uint32_t));
			continue;
		}

		// we found a package
		package_header_set_sender(pkghdr, cl);
		buf->discard(sizeof(package_header));
		return 0;
	}
	// shall never be here
	return -4;
}

void evloop_impl::package_header_set_sender(package_header& pkghdr, evlclient_impl* cl)
{
	if (pkghdr.sender) {
		evlclient_impl* tmp = reinterpret_cast<evlclient_impl*>(pkghdr.sender);
		tmp->release();
	}
	cl->addref();
	pkghdr.sender = cl;
}

int evloop_impl::handle_package(const package_header& pkghdr)
{
	int ret = -ENOTHANDLED;
	switch (pkghdr.pkgid)
	{
	case EVL_BCTL_CLIENT_INFO:
		printf("EVL_BCTL_CLIENT_INFO\n");
		ret = handle_package_bctl_client_info(pkghdr);
		break;

	case EVL_BCTL_RETRIVE_CLIENT:
		ret = handle_package_bctl_retrive_client(pkghdr);
		break;

	default:
		ret = _pkgid_mgr.handle_package(pkghdr);
	}
	pkghdr.finish();
	return ret;
}

int evloop_impl::handle_package_bctl_client_info(const package_header& pkghdr)
{
	evlclient_impl* cl = reinterpret_cast
		<evlclient_impl*>(pkghdr.sender);
	assert(NULL != cl);

	fifobuffer_impl* buf = cl->get_readbuffer();
	assert(NULL != buf);

	evl_bctl_client_info* cinfo =
		(evl_bctl_client_info*)alloca(pkghdr.size);

	size_t n = buf->peekdata(0, (void*)cinfo, pkghdr.size);
	assert(n == pkghdr.size);

	int ret;
	clinfo_validity_u val;
	ret = cl->update_info(cinfo, val);
	
	//notify listener accepted client
	evlclient acli;
	cl->addref();
	acli.set(reinterpret_cast<evlclient_object*>(cl));
	_evloop_lnr_mgr.accepted(acli);
	if (!cinfo->need_reply)
		return ret;

	// act reply
	assert(NULL != _clientinfo);
	size_t sz = _clientinfo->clientinfo_size();
	evl_bctl_client_info_ack_pkg *ack = new(alloca(sizeof(*ack) + sz))
		evl_bctl_client_info_ack_pkg(sz, pkghdr.seqid);
	evl_bctl_client_info_ack& obj = ack->payload();

	// set reply seqid with receive pkg
	ack->header().seqid = pkghdr.seqid;

	obj.status = ret;
	_clientinfo->load_clientinfo(obj.info);
	return write(cl, ack, sizeof(*ack) + sz, 500);
}

int evloop_impl::handle_package_bctl_retrive_client(const package_header& pkghdr)
{
	evlclient_impl* cl = reinterpret_cast
		<evlclient_impl*>(pkghdr.sender);
	assert(NULL != cl);

	fifobuffer_impl* buf = cl->get_readbuffer();
	assert(NULL != buf);

	evl_bctl_retrive_client* cinfo =
		(evl_bctl_retrive_client*)alloca(pkghdr.size);

	size_t n = buf->peekdata(0, (void*)cinfo, pkghdr.size);
	assert(n == pkghdr.size);

	// try to find the client in server
	evlclient_impl* peer = evlclient_impl::getclient(
		cinfo->client_name,
		&cinfo->client_name[cinfo->inst_name]);

	size_t sz = peer ? peer->clientinfo_size() : 0;
	evl_bctl_retrive_client_ack_pkg* ack = new(alloca(sizeof(*ack)
		+ sz))evl_bctl_retrive_client_ack_pkg(sz);
	evl_bctl_retrive_client_ack& obj = ack->payload();
	ack->header().seqid = pkghdr.seqid;
	if (peer) peer->load_clientinfo(obj.info);

	// set reply seqid with receive pkg
	ack->header().seqid = pkghdr.seqid;

	obj.status = peer ? 0 : -ENOTFOUND;
	return write(cl, ack, sizeof(*ack) + sz, 500);
}

evlclient_impl* evloop_impl::init_peer(int pid)
{
	int fd, dummy;
	if (client_init_socket(fd, dummy, pid))
		return NULL;

	// add the "unique server" as a client
	evlclient_impl* peer = evlclient_impl::create(fd,
		client_type_unix_socket);
	assert(NULL != peer);

	// add the peer fd to epoll
	if (epoll_add(_epollfd, fd, peer))
	{
		peer->release();
		close(fd);
		return NULL;
	}
	return peer;
}

evlclient_impl* evloop_impl::init_tcpip_peer(uint32_t ipv4, uint32_t port)
{
	int fd, dummy;
	struct sockaddr_in addr;
	addr.sin_addr.s_addr = ipv4;
	char ipchar[INET_ADDRSTRLEN];
	const char* pip = inet_ntop(AF_INET,
		&addr.sin_addr, ipchar, INET_ADDRSTRLEN);
	if (client_init_tcpip_socket(fd, dummy, pip, port, 0))
		return NULL;

	// add the "unique server" as a client
	evlclient_impl* peer = evlclient_impl::create(fd,
		client_type_socket);
	assert(NULL != peer);

	// add the peer fd to epoll
	if (epoll_add(_epollfd, fd, peer))
	{
		peer->release();
		close(fd);
		return NULL;
	}
	return peer;
}

const char* evloop_impl::get_name(void)
{
	if (!_clientinfo) return NULL;
	return _clientinfo->get_name();
}

const char* evloop_impl::get_instance_name(void)
{
	if (!_clientinfo) return NULL;
	return _clientinfo->get_instance_name();
}

void evloop_impl::evloop_addbuf(evlclient_impl** clients, evlclient_impl* cli, int& count)
{
	for (int i = 0; i < count; ++i) {
		if (clients[i] == cli) return;
	}
	cli->addref();
	clients[count++] = cli;
}

int evloop_impl::evloop_wait_data(int timeout, evlclient_impl** clients, int& count)
{
	struct epoll_event ev, events[MAX_EVENTS];
	int nfds = epoll_wait(_epollfd, events, MAX_EVENTS, timeout);
	if (nfds == -1) {
		if (errno == EINTR)
			return -EINTRUPT;
		else return -1;
	}
	else if (!nfds) return -ETIMEOUT;

	count = 0;
	for (int i = 0; i < nfds; ++i)
	{
		evlclient_impl* cl = reinterpret_cast<evlclient_impl*>
			(events[i].data.ptr);
		if (NULL == cl) continue;

		// handler server listen request
		if (is_server() && cl == _server) {
			accept_unix_connection(_server);
			continue;
		}

		if (is_server() && cl == _tcp_server) {
			accept_tcpip_connection(_tcp_server);
			continue;
		}
		
		// handle client listenfd request
		if (is_client() && cl == _connmgr) {
			accept_unix_connection(_connmgr);
			continue;
		}
		// handle client listenfd request
		if (is_client() && cl == _tcp_connmgr) {;
			accept_tcpip_connection(_tcp_connmgr);
			continue;
		}

		// if the hangup or error happened, we will not
		// handle data input/output even though there may
		// be some data left in the buffer to be read or write
		if (events[i].events & (EPOLLRDHUP | EPOLLHUP)) {
			epoll_release(cl);
			printf("EPOLLRDHUP/EPOLLHUP\n");
			continue;
		}
		if (events[i].events & EPOLLERR) {
			epoll_release(cl);
			printf("EPOLLERR\n");
			continue;
		}
	
		if (events[i].events & EPOLLPRI) {
			// pass
		}
		if (events[i].events & EPOLLIN) {
			epoll_data_in(cl, clients, count);
		}
		if (events[i].events & EPOLLOUT) {
			if (cl->gettype() > 20) {
				printf("unknown EPOLLOUT: client type: %u\n", cl->gettype());
			}
			size_t n = fifo_write(cl->getfd(), cl->get_writebuffer(), cl->getmutex());
		}
	}
	return 0;
}

void evloop_impl::epoll_data_in(evlclient_impl* cli, evlclient_impl** clients, int& count)
{
	assert(NULL != cli);
	if (cli->handle_datain()) return;

	int fd = cli->getfd();
	assert(fd);

	fifobuffer_impl* buf = cli->get_readbuffer();
	if (NULL == buf) return;

	epoll_read_drain(fd, buf);
	evloop_addbuf(clients, cli, count);
}

void evloop_impl::epoll_release(evlclient_impl* cl)
{
	assert(NULL != cl);
	// set disconnected status
	_f.state = evloop_state_disconnected;
	epoll_del(_epollfd, cl->getfd());
	cl->destroy();
	// notify status
	_evloop_lnr_mgr.disconnected(cl->_name.c_str(), cl->_inst_name.c_str());
}

int evloop_impl::readbuf_handle_userdef_client(evlclient_impl* cl)
{
	auto* ucl = zas_downcast(evlclient_impl, userdef_evlclient_impl, cl);
	return ucl->handle_input();
}

int evloop_impl::wait_handle(int timeout)
{
	package_header hdr;
	long prev = gettick_nanosecond();

	int count;
	evlclient_impl* clients[MAX_EVENTS];
	for (;;)
	{
		int ret = evloop_wait_data(timeout, clients, count);
		if (ret < 0 && ret != -EINTRUPT)
			return ret;	// including -ETIMEOUT

		if (ret != -EINTRUPT) for (int i = 0; i < count; ++i)
		{
			evlclient_impl* cl = clients[i];
			if (cl->gettype() >= client_type_user) {
				readbuf_handle_userdef_client(cl);
			}
			else while (!readbuf_get_package(cl, hdr)) {
				handle_package(hdr);
			}
			cl->release();
		}

		if (count < MAX_EVENTS) return 0;

		if (timeout >= 0) {
			// adjust the timeout
			long curr = gettick_nanosecond();
			long timedout = long(timeout) * 1000000;
			if (curr - prev < timedout) {
				timeout = (timedout + prev - curr) / 1000000;
				prev = curr;
			}
			else timeout = 0;
		}
	}
	return 0;
}

int evloop_impl::wait_get_package(evlclient_impl* cl, uint32_t pkgid,
	uint32_t seqid, package_header* pkghdr, int timeout, int* handle)
{
	package_header hdr;
	long prev = gettick_nanosecond();
	bool chkpkg = (cl && pkgid && seqid) ? true : false;

	int count;
	evlclient_impl* clients[MAX_EVENTS];
	for (;;)
	{
		int ret = evloop_wait_data(timeout, clients, count);
		if (ret < 0 && ret != -EINTRUPT)
			return ret;	// including -ETIMEOUT

		// check if a package in buffer
		bool ok = false;
		if (ret != -EINTRUPT) for (int i = 0; i < count; ++i)
		{
			//TODO: for fixed iusse, need to modify --start
			evlclient_impl* cl = clients[i];
			if (cl->gettype() >= client_type_user) {
				readbuf_handle_userdef_client(cl);
			}
			else while (!readbuf_get_package(cl, hdr))
			{
				if (chkpkg && cl == hdr.sender && pkgid == hdr.pkgid
					&& seqid == hdr.seqid) {
					if (pkghdr) *pkghdr = hdr;
					if (handle) *handle = handle_package(hdr);
					ok = true;
					break;
				}
				handle_package(hdr);
			}
			cl->release();
			//TODO: for fixed iusse, need to modify --end
		}
		if (ok) return 0;

		long curr = gettick_nanosecond();
		long timedout = long(timeout) * 1000000;
		if (curr - prev < timedout) {
			timeout = (timedout + prev - curr) / 1000000;
			prev = curr;
		}
		else return -ETIMEOUT;
	}
	return 0;
}

void evloop_impl::handle_packages(package_header& pkghdr) {
	// todo: check timeout (current 1000)
	wait_get_package(NULL, 0, 0, &pkghdr, 1000);
}

int evloop_impl::accept_unix_connection(evlclient_impl* svr)
{
	struct sockaddr_un cliun;
	socklen_t cliun_len = sizeof(cliun);
	int connfd = accept(svr->getfd(), (struct sockaddr*)&cliun, &cliun_len);
	if (connfd == -1) return -1;
	setnonblocking(connfd);
	evlclient_impl* cl = evlclient_impl::create(connfd,
		client_type_unix_socket);
	assert(NULL != cl);

	if (epoll_add(_epollfd, connfd, cl)) {
		close(connfd);
		return -2;
	}
	return 0;
}

int evloop_impl::accept_tcpip_connection(evlclient_impl* svr)
{
	sockaddr_in addr;
	socklen_t len = sizeof(addr);
	int connfd = accept(svr->getfd(), (struct sockaddr*)&addr, &len);
	if (connfd == -1) return -1;

	setnonblocking(connfd);
	evlclient_impl* cl = evlclient_impl::create(connfd,
		client_type_socket);
	assert(NULL != cl);

	if (epoll_add(_epollfd, connfd, cl)) {
		close(connfd);
		return -2;
	}
	return 0;
}

bool evloop_impl::role_avail(void) {
	return (_f.role == evloop_role_unknown)
	? false : true;
}

void evloop_impl::setrunning(void) {
	_f.finished = 0;
	_f.running = 1;
	_f.initialized = 0;
}

void evloop_impl::setinitialized(void) {
	_f.finished = 0;
	_f.running = 1;
	_f.initialized = 1;
	if (_evlthd) {
		_evlthd->notify_success();
	}
}

int evloop_impl::setfinish(void) {
	_f.running = 0;
	_f.initialized = 0;
	_f.finished = 1;
	return 0;
}

int evloop_impl::seterror(void){
	_f.running = 0;
	_f.initialized = 0;
	_f.finished = 0;
	_f.state = evloop_state_error;
	return 0;		
}

evloop* evloop::inst(void)
{
	static evloop_impl* _inst = NULL;
	if (_inst) return reinterpret_cast<evloop*>(_inst);

	auto_mutex am(evlmut);
	if (_inst) return reinterpret_cast<evloop*>(_inst);
	_inst = new evloop_impl();
	assert(NULL != _inst);
	return reinterpret_cast<evloop*>(_inst);
}

evloop_state evloop::get_state(void)
{
	evloop_impl* evl = reinterpret_cast<evloop_impl*>(this);
	if (nullptr == evl) return evloop_state_unknown;
	return evl->get_state();
}

int evloop::setrole(evloop_role role)
{
	evloop_impl* evl = reinterpret_cast<evloop_impl*>(this);
	if (nullptr == evl) return -ELOGIC;
	return evl->setrole(role);
}

evloop_role evloop::getrole(void)
{
	evloop_impl* evl = reinterpret_cast<evloop_impl*>(this);
	if (NULL == evl) return evloop_role_unknown;
	return evl->getrole();
}

int evloop::start(bool septhd, bool wait)
{
	evloop_impl* evl = reinterpret_cast<evloop_impl*>(this);
	if (NULL == evl) return -ELOGIC;
	return evl->start(septhd, wait);
}

bool evloop::is_running(void)
{
	evloop_impl* evl = reinterpret_cast<evloop_impl*>(this);
	if (NULL == evl) return false;
	return evl->is_running();
}

int evloop::addtask(const char* name, evloop_task* task)
{
	evloop_impl* evl = reinterpret_cast<evloop_impl*>(this);
	if (NULL == evl) return false;
	return evl->addtask(name, task);
}

evloop* evloop::updateinfo(int cmd, ...)
{
	evloop_impl* evl = reinterpret_cast<evloop_impl*>(this);

	va_list vl;
	va_start(vl, cmd);
	evl->update_clientinfo(cmd, vl);
	va_end(vl);
	return this;
}

const char* evloop::get_name(void)
{
	evloop_impl* evl = reinterpret_cast<evloop_impl*>(this);
	return evl->get_name();
}

const char* evloop::get_instance_name(void)
{
	evloop_impl* evl = reinterpret_cast<evloop_impl*>(this);
	return evl->get_instance_name();
}

evlclient evloop::getclient(const char* client_name,
	const char* inst_name)
{
	evlclient client;

	if (NULL == client_name && NULL == inst_name) {
		evloop_impl* evl = reinterpret_cast<evloop_impl*>(this);
		evlclient_impl* cl = evl->get_serverclient();
		if (NULL != cl) {
			// we use "set" to avoid another addref() of cl
			client.set(reinterpret_cast<evlclient_object*>(cl));
			return client;
		}
	}

	if (!client_name || !*client_name
		|| !inst_name || !*inst_name) {
		return client;
	}

	evlclient_impl* cl = evlclient_impl::getclient(client_name, inst_name);
	if (NULL != cl) {
		// we use "set" to avoid another addref() of cl
		client.set(reinterpret_cast<evlclient_object*>(cl));
		return client;
	}

	// let the event loop to retrive the client
	evloop_impl* evl = reinterpret_cast<evloop_impl*>(this);
	cl = evl->getclient(client_name, inst_name);

	// we use "set" to avoid another addref() of cl
	client.set(reinterpret_cast<evlclient_object*>(cl));
	return client;
}

int evloop::add_package_listener(uint32_t pkgid, evloop_pkglistener *lnr)
{
	evloop_impl* evl = reinterpret_cast<evloop_impl*>(this);
	if (NULL == evl) return -ENOTAVAIL;
	return evl->add_package_listener(pkgid, lnr);
}

int evloop::add_package_listener(uint32_t pkgid,
	pkgnotifier notify, void* owner)
{
	evloop_impl* evl = reinterpret_cast<evloop_impl*>(this);
	if (NULL == evl) return -ENOTAVAIL;
	return evl->add_package_listener(pkgid, notify, owner);
}

int evloop::remove_package_listener(uint32_t pkgid, evloop_pkglistener *lnr)
{
	evloop_impl* evl = reinterpret_cast<evloop_impl*>(this);
	if (NULL == evl) return -ENOTAVAIL;
	return evl->remove_package_listener(pkgid, lnr);
}

int evloop::remove_package_listener(uint32_t pkgid,
	pkgnotifier notify, void* owner)
{
	evloop_impl* evl = reinterpret_cast<evloop_impl*>(this);
	if (NULL == evl) return -ENOTAVAIL;
	return evl->remove_package_listener(pkgid, notify, owner);
}

int evloop::add_listener(const char* name, evloop_listener *lnr)
{
	if (!name || !*name || !lnr) return -EBADPARM;
	evloop_impl* evl = reinterpret_cast<evloop_impl*>(this);
	if (NULL == evl) return -ENOTAVAIL;
	return evl->add_listener(name, lnr);
}

int evloop::remove_listener(const char* name)
{
	if (!name || !*name) return -EBADPARM;
	evloop_impl* evl = reinterpret_cast<evloop_impl*>(this);
	if (NULL == evl) return -ENOTAVAIL;
	return evl->remove_listener(name);
}

void evloop_listener::accepted(evlclient client) {}
void evloop_listener::connected(evlclient client) {}
void evloop_listener::disconnected(const char* cliname, const char* instname) {}

void evloop_task::run(void) {}
void evloop_task::release(void) {}

int evlclient_object::addref(void)
{
	evlclient_impl* cl = reinterpret_cast<evlclient_impl*>(this);
	return cl->addref();
}

int evlclient_object::release(void)
{
	evlclient_impl* cl = reinterpret_cast<evlclient_impl*>(this);
	return cl->release();
}

const char* evlclient_object::get_clientname(void)
{
	evlclient_impl* cl = reinterpret_cast<evlclient_impl*>(this);
	return cl->get_clientname();
}

const char* evlclient_object::get_instname(void)
{
	evlclient_impl* cl = reinterpret_cast<evlclient_impl*>(this);
	return cl->get_instname();
}

size_t evlclient_object::write(void* buffer, size_t sz)
{
	evlclient_impl *cl = reinterpret_cast<evlclient_impl*>(this);
	if (NULL == cl) return 0;
	return cl->write(buffer, sz);
}

}} // end of namespace zas::utils
#endif // (defined(UTILS_ENABLE_FBLOCK_EVLOOP) && defined(UTILS_ENABLE_FBLOCK_EVLCLIENT) && defined(UTILS_ENABLE_FBLOCK_BUFFER))
/* EOF */
