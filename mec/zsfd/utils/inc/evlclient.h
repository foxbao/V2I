/** @file evlclient.h
 * definition of evloop client object
 */

#include "utils/utils.h"
#if (defined(UTILS_ENABLE_FBLOCK_BUFFER) && defined(UTILS_ENABLE_FBLOCK_EVLCLIENT))

#ifndef __CXX_ZAS_UTILS_EVL_CLIENT_H__
#define __CXX_ZAS_UTILS_EVL_CLIENT_H__

#include <string>
#include "std/list.h"

#include "utils/avltree.h"
#include "utils/mutex.h"

#include "fifobuffer.h"
#include "evlmsg.h"

namespace zas {
namespace utils {

class evloop_impl;

// evlclient type max number is 15.
enum evlclient_type
{
	client_type_unknown = 0,
	client_type_unix_socket,
	client_type_socket,
	client_type_unix_socket_listenfd,
	client_type_socket_listenfd,
	client_type_timer,
	client_type_event,

	// the start of user defined evlclient
	client_type_user = 15,
};

class evlclient_impl
{
	friend class evloop_impl;

public:
	evlclient_impl() = default;
	evlclient_impl(uint32_t type, int fd);
	virtual ~evlclient_impl();

	int destroy(void);
	int release(void);
	int getfd(void);
	int gettype(void);
	virtual void getinfo(uint32_t& type, int& fd);

	const char* get_clientname(void);
	const char* get_instname(void);
	size_t write(void* buffer, size_t sz);

	static evlclient_impl* create(int fd, uint32_t type);
	int update_info(evl_bctl_client_info* info, clinfo_validity_u& val);
	static evlclient_impl* getclient(int fd);
	static evlclient_impl* getclient(const char* name, const char* instname);
	size_t clientinfo_size(void);
	void load_clientinfo(evl_bctl_client_info& cinfo);

public:
	int addref(void) {
		return __sync_add_and_fetch(&_refcnt, 1);
	}

	fifobuffer_impl* get_writebuffer(void) {
		return &_writebuf;
	}

	fifobuffer_impl* get_readbuffer(void) {
		return &_readbuf;
	}

	int getpid(void) { return _pid; }

	mutex* getmutex(void) {
		return &_mut;
	}

	int set_syssvr(bool issvr) {
		int ret = _f.is_syssvr;
		_f.is_syssvr = issvr ? 1 : 0;
		return ret;
	}

	bool issyssvr(void) {
		return _f.is_syssvr ? true : false;
	}

	bool is_service_container(void) {
		return _f.is_servicecontainer ? true : false;
	}

	bool is_service_shared(void) {
		return  _f.is_servicecontainer ? (_f.is_serviceshared ? true : false) : false;
	}

	bool destroyed(void) {
		return _f.destroyed ? true : false;
	}

protected:
	virtual bool handle_datain(void);
	static int add_client(evlclient_impl* cli);
	static int avl_fd_compare(avl_node_t* a, avl_node_t* b);
	static int avl_name_compare(avl_node_t* a, avl_node_t* b);

private:
	static evlclient_impl* getclient_unlocked(int fd);
	static evlclient_impl* getclient_unlocked(const char* name, const char* instname);
	static evlclient_impl* getclient_bypid(int pid);

	void destroy_without_release(void);
	int set_detached(bool isdetached) {
		int ret = _f.detached;
		_f.detached = isdetached ? 1 : 0;
		return ret;
	}
	bool isdetached(void) {
		return _f.detached ? true : false;
	}
	int insert_client_unlock(evlclient_impl *cli);
	bool detach_client_unlock(evlclient_impl *cli);

protected:
	union {
		avl_node_t _avlnode;
		listnode_t _inst_ownerlist;		
	};
	avl_node_t _uuid_avlnode;
	listnode_t _ownerlist;
	listnode_t _instlist;
	union {
		struct {
			uint32_t destroyed : 1;
			//detached for global list 'clilst'
			uint32_t detached : 1;
			uint32_t client_type : 4;
			uint32_t is_syssvr : 1;
			uint32_t is_servicecontainer : 1;
			//when client_attr is service_container, shared is vaild.
			uint32_t is_serviceshared : 1;
		} _f;
		uint32_t _flags;
	};

	guid_t _uuid;
	std::string _name;
	std::string _inst_name;
	uint32_t _ipv4addr;
	uint32_t _port;
	fifobuffer_impl _readbuf, _writebuf;
	int _fd;
	int _pid;
	int _refcnt;
	mutex _mut;
};

zas_interface evloop_task;

#define TASKLET_CLIENT_APP_NAME "zas.system.rpc.client.tasklet"

class tasklet_client : public evlclient_impl
{
public:
	tasklet_client(const char* name);
	~tasklet_client();

	static tasklet_client* create(const char* name);
	// add a task
	int addtask(evloop_task* task);

protected:
	bool handle_datain(void);

private:
	int notify_event(void);
	
private:
	listnode_t _tasklist;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(tasklet_client);
};

class connmgr_client : public evlclient_impl
{
public:
	connmgr_client(int listenfd);
	~connmgr_client();

	static connmgr_client* create(int listenfd);

protected:
	bool handle_datain(void);
	ZAS_DISABLE_EVIL_CONSTRUCTOR(connmgr_client);
};

class tcp_connmgr_client : public evlclient_impl
{
public:
	tcp_connmgr_client(int listenfd);
	~tcp_connmgr_client();

	static tcp_connmgr_client* create(int listenfd);

protected:
	bool handle_datain(void);
	ZAS_DISABLE_EVIL_CONSTRUCTOR(tcp_connmgr_client);
};

class client_info : public tasklet_client
{
public:
	client_info(evloop_impl* evl);

	evloop_impl* get_evloop(void);
	bool name_avail(void);
	int update(int cmd, va_list vl);
	size_t getsize(void);
	void load(evl_bctl_client_info& clinfo);
	bool equal(const char* client_name, const char* inst_name);
	const char* get_name();
	const char* get_instance_name();
	const char* get_server_ipv4(void);
	uint32_t get_server_port(void);
	uint32_t get_listen_port(void);

private:
	bool is_server(void);

private:
	clinfo_validity_u _validity;
	std::string _client_name;
	std::string _instance_name;
	std::string _server_ipv4;
	uint32_t _server_port;
	uint32_t _client_ipv4;
	uint32_t _client_port;
	evloop_impl* _evl;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(client_info);
};

#define USER_CLIENT_NAME "zas.userdef.client"

class userdef_evlclient_impl : public evlclient_impl
{
public:
	userdef_evlclient_impl(userdef_evlclient* client);
	~userdef_evlclient_impl();

	int activate(void);
	int handle_input(void);
	int detach(void);
	virtual void getinfo(uint32_t& type, int& fd);

public:
	userdef_evlclient* get_userdef_client(void) {
		return _client;
	}

protected:
	bool handle_datain(void);

private:
	userdef_evlclient* _client;
	uint32_t	_user_client_type;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(userdef_evlclient_impl);
};

}} // end of namespace zas::utils
#endif /*  __CXX_ZAS_UTILS_EVL_CLIENT_H__ */
#endif // (defined(UTILS_ENABLE_FBLOCK_BUFFER) && defined(UTILS_ENABLE_EVLCLIENT))
/* EOF */
