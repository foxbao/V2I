/** @file evloop.h
 * definition of evloop impl 
 *  */

#ifndef __CXX_ZAS_UTILS_EV_LOOP_IMPL_H__
#define __CXX_ZAS_UTILS_EV_LOOP_IMPL_H__

#include <string>
#include "std/list.h"

#include "utils/avltree.h"
#include "utils/evloop.h"
#include "utils/thread.h"
#include "utils/wait.h"
#include "utils/coroutine.h"

#include "evpoller.h"
#include "fifobuffer.h"

namespace zas {
namespace utils {

class client_info;
class evloop_impl;
class evpoller_event;

class evloop_thread : public thread
{
public:
	evloop_thread(evloop_impl* evl);
    ~evloop_thread();
	int start_thread(bool waitres);
	void notify_success(void);
	int run(void);
	
private:
	evloop_impl* _evl;
	int _retval;
	waitobject* _wait;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(evloop_thread);
};

class pkglistener_mgr
{
public:
	pkglistener_mgr();
	~pkglistener_mgr();

	int add_package_listener(uint32_t pkgid, evloop_pkglistener *lnr);
	int add_package_listener(uint32_t pkgid, pkgnotifier notify, void* owner);
	int remove_package_listener(uint32_t pkgid, evloop_pkglistener *lnr);
	int remove_package_listener(uint32_t pkgid, pkgnotifier notify, void* owner);
	bool handle_package(const package_header &pkghdr);

	int add_package_event(uint32_t pkgid, evpoller_event* event);

private:
	struct pkglistener
	{
		avl_node_t		avlnode;
		listnode_t		ownerlist;
		uint32_t		pkg_id;
		pkgnotifier		notifier;
		void*			owner;

		// evpoller pending event list
		evp_pkgevent_list evp_pendings;
	};

	static int package_id_compare(avl_node_t* a, avl_node_t* b);
	pkglistener* find_package_id(uint32_t pkgid);
	pkglistener* find_package_id_unlocked(uint32_t msgid);
	int add_package_listener_unlocked(uint32_t pkgid, pkgnotifier notify, void* owner);
	int remove_package_listener_unlocked(uint32_t pkgid, pkgnotifier notify, void* owner);
	void release_all_node(void);

private:
	avl_node_t*		_pkgid_tree;
	listnode_t		_pkgid_list;
	mutex _mut;
};

class evllistener_mgr
{
public:
	evllistener_mgr();
	~evllistener_mgr();

	int add_listener(const char* name, evloop_listener *lnr);
	int remove_listener(const char* name);

	void accepted(evlclient client);
	void connected(evlclient client);
	void disconnected(const char* cliname, const char* instname);

	int add_pending_evlclient(evlclient_impl* cli);
	int remove_pending_evlclient(evlclient_impl* cli);

private:
	struct listener_info {
		listnode_t	ownerlist;
		avl_node_t	avlnode;
		std::string name;
		evloop_listener *lnr;
	};

	struct pending_evlclient {
		listnode_t ownerlist;
		evlclient_impl* client;
	};

	static int evloop_listener_compare(avl_node_t* a, avl_node_t* b);
	listener_info* find_listener_unlock(const char* nm);
	int release_all_nodes(void);
	pending_evlclient* get_evlclient_pending_unlocked(evlclient_impl* cli);
	int add_evlclient_pending_unlocked(evlclient_impl* cli);

private:
	avl_node_t*		_lnr_tree;
	listnode_t		_lnr_list;
	listnode_t		_pending_evclients;
	mutex _mut;	
};

class evloop_impl
{
public:
	evloop_impl();
	~evloop_impl();

	evloop_state get_state(void);
	int setrole(evloop_role r);
	evloop_role getrole(void);
	int start(bool septhd, bool wait);
	int addtask(const char* tasklet_name, evloop_task* task);
	int update_clientinfo(int cmd, va_list vl);
	evlclient_impl* getclient(const char* cname, const char* iname);
	int add_package_listener(uint32_t pkgid, evloop_pkglistener *lnr);
	int add_package_listener(uint32_t pkgid,
		pkgnotifier notify, void* owner);
	int add_listener(const char* name, evloop_listener *lnr);
	int remove_package_listener(uint32_t pkgid, evloop_pkglistener *lnr);
	int remove_package_listener(uint32_t pkgid,
		pkgnotifier notify, void* owner);
	int remove_listener(const char* name);
	int register_client(evlclient_impl* cli);
	int deregister_client(evlclient_impl* cli);
	bool is_running(void);
	evlclient_impl* get_serverclient(void);
	int wait_handle(int timeout);
	bool handle_client_info_ack(evlclient sender,
		const package_header& pkghdr,
		const triggered_pkgevent_queue& queue);
	bool handle_retrive_client_ack(evlclient sender,
		const package_header& pkghdr,
		const triggered_pkgevent_queue& queue);

	const char* get_name(void);
	const char* get_instance_name(void);
	
public:
	bool is_server(void);
	bool is_client(void);
	int run(void);
	bool is_evloop_thread(void);
	bool is_finished(void);
	bool is_initialized(void);
	int write(evlclient_impl* cli, void* buf, size_t sz, int timeout);
	int write(int fd, void* buf, size_t sz, int timeout);
	pkglistener_mgr* get_package_listener_manager(void);
	coroutine_mgr* get_coroutine_manager(void);

public:
	int sendto_svr(void* buf, size_t sz, int timeout) {
		return write(_server, buf, sz, timeout);
	}

private:
	int init_server(void);
	int init_client(void);
	int init_tcpip_client(void);
	int regist_client_info_ack_listen(void);
	int act_update_remote_client_info(evlclient_impl* svr, bool reply);
	size_t drain(int fd);
	size_t epoll_read_drain(int fd, fifobuffer_impl* buf);
	size_t move_to_package_start(fifobuffer_impl* fifobuf);
	bool check_footer(package_header& hdr, package_footer& footer);
	int readbuf_get_package(evlclient_impl* cl, package_header& pkghdr);
	int readbuf_handle_userdef_client(evlclient_impl* cl);
	void package_header_set_sender(package_header& pkghdr, evlclient_impl* cl);
	int handle_package(const package_header& pkghdr);
	int handle_package_bctl_client_info(const package_header& pkghdr);
	int handle_package_bctl_retrive_client(const package_header& pkghdr);
	evlclient_impl* init_peer(int pid);
	evlclient_impl* init_tcpip_peer(uint32_t ipv4, uint32_t port);
	void evloop_addbuf(evlclient_impl** clients, evlclient_impl* cli, int& count);
	int evloop_wait_data(int timeout, evlclient_impl** clients, int& count);
	void epoll_data_in(evlclient_impl* cli, evlclient_impl** clients, int& count);
    void epoll_release(evlclient_impl* cl);
	int wait_get_package(evlclient_impl* cl, uint32_t pkgid, uint32_t seqid,
		package_header* pkghdr, int timeout, int* handle = NULL);
	void handle_packages(package_header& pkghdr);
	int accept_unix_connection(evlclient_impl* svr);
	int accept_tcpip_connection(evlclient_impl* svr);
	bool role_avail(void);
	void setrunning(void);
	void setinitialized(void);
	int setfinish(void);
	int seterror(void);

	size_t write(int fd, const void* src, size_t n, mutex* mut,
		fifobuffer_impl* fifobuf, int timeout, bool prelock = false);
	int write_wait_comsume(int timeout);

private:
	union {
		struct {
			// if the role of current evloop is server
			uint32_t role : 2;
			uint32_t running : 1;
			uint32_t initialized : 1;
			uint32_t finished : 1;
			uint32_t state : 4;
		} _f;
		uint32_t _flags;
	};

private:
	int _epollfd;
	evlclient_impl* _server;
	evlclient_impl* _tcp_server;
	evlclient_impl* _connmgr;
	evlclient_impl* _tcp_connmgr;
	client_info* _clientinfo;
	evloop_thread* _evlthd;
	pkglistener_mgr _pkgid_mgr;
	evllistener_mgr _evloop_lnr_mgr;
	coroutine_mgr _cormgr;
	unsigned long int _container_tid;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(evloop_impl);
};

}} // end of namespace zas::utils
#endif /*  __CXX_ZAS_UTILS_EV_LOOP_IMPL_H__ */
/* EOF */
