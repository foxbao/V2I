/** @file evloop.h
 * Definition of the main evloop routine
 */

#include "utils/utils.h"
#if (defined(UTILS_ENABLE_FBLOCK_EVLOOP) && defined(UTILS_ENABLE_FBLOCK_EVLCLIENT) && defined(UTILS_ENABLE_FBLOCK_BUFFER))

#ifndef __CXX_ZAS_UTILS_EVLOOP_H__
#define __CXX_ZAS_UTILS_EVLOOP_H__

#include "std/smtptr.h"
#include "utils/utils.h"

namespace zas {
namespace utils {

using namespace zas::utils;

class evloop;
class evlclient_impl;
class readonlybuffer;

enum evp_eventid
{
	evp_evid_unknown = 0,
	evp_evid_package,
	evp_evid_package_with_seqid,
	evp_evid_timeout,
};

/**
  The event poller event definition
 */
class UTILS_EXPORT evpoller_event
{
public:
	evpoller_event() = delete;
	~evpoller_event() = delete;

public:
	/**
	  submit the poller event, after this, the
	  event is able to be triggered
	 */
	int submit(void);

	/**
	  allocate the buffer for input
	  @param sz size to be allocated
	  @return the buffer allocated
	 */
	void* allocate_inputbuf(size_t sz);

	/**
	  allocate the buffer for output
	  @param sz size to be allocated
	  @return the buffer allocated
	 */
	void* allocate_outputbuf(size_t sz);

	/**
	  Get the allocated input buffer
	  @param sz the size of allocated buffer
	  @return the pointer to the buffer
	 */
	void* get_inputbuf(size_t* sz = NULL);

	/**
	  Get the allocated output buffer
	  @param sz the size of allocated buffer
	  @return the pointer to the buffer
	 */
	void* get_outputbuf(size_t* sz = NULL);

	/**
	  Read the input buffer
	  @param buf the buffer to hold the data
	  @param sz the size of buffer
	  @return 0 for success
	 */
	int read_inputbuf(void* buf, size_t sz);

	/**
	  Read the output buffer
	  @param buf the buffer to hold the data
	  @param sz the size of buffer
	  @return 0 for success
	 */
	int read_outputbuf(void* buf, size_t sz);

	/**
	  Write the data to the input buffer
	  @param buf the buffer to be written
	  @param sz the size of the buffer to be written
	  @return 0 for success
	 */
	int write_inputbuf(void* buf, size_t sz);

	/**
	  Write the data to the output buffer
	  @param buf the buffer to be written
	  @param sz the size of the buffer to be written
	  @return 0 for success
	 */
	int write_outputbuf(void* buf, size_t sz);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(evpoller_event);
};

class UTILS_EXPORT evpoller
{
public:
	evpoller();
	~evpoller();

	/**
	  Create an poller event via the event id
	  multiple parameters could be added for
	  creating the event
	 */
	evpoller_event* create_event(int evid, ...);

	/**
	  Reset the evpoller
	  we recommend calling reset() before loop calling
	  poll() for waiting events, because this will help
	  to clear all potential triggered event before, which
	  is not cared by user in most cases
	 */
	int reset(void);

	/**
	  poll the event
	  @return 0 for success
	 */
	int poll(int timeout);

	/** todo */
	evpoller_event* get_triggered_event(void);

private:
	void* _data;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(evpoller);
};

class UTILS_EXPORT triggered_pkgevent_queue
{
public:
	triggered_pkgevent_queue() = delete;
	~triggered_pkgevent_queue() = delete;

	/**
	  get triggered package evpoller event from the queue
	  @return NULL means no more pending events
	 */
	evpoller_event* dequeue(void) const;

	bool is_empty(void);

	ZAS_DISABLE_EVIL_CONSTRUCTOR(triggered_pkgevent_queue);
};

/**
  the header for a package
  this class also offers quick method that could simplify
  the process of creating a package
  Note: the package_header is not thread-safe
 */
struct UTILS_EXPORT package_header
{
	~package_header();
	package_header();
	package_header(uint32_t pkid, uint32_t sz);

	// this constructor is used for initialization of the reply package
	package_header(uint32_t pkid, uint32_t sqid, uint32_t sz);
	package_header& operator=(const package_header& ph);

	/**
	  Use to manually initialize a package header
	 */
	void init(void);
	void init(uint32_t pkid, uint32_t sz);
	void init(uint32_t pkid, uint32_t sqid, uint32_t sz);

	/**
	  Finish and finialize the package
	  this must be called when finish handling the package
	  so that the package will be released from the buffer
	  @param none
	  @return none
	  */
	void finish(void) const;

	/** 
	  Get the readonly buffer for reading data
	  @return readonly buffer pointer or NULL for error
	  */
	readonlybuffer* get_readbuffer(void) const;

	uint32_t magic;
	uint32_t pkgid;
	uint32_t seqid;		// sequence id
	uint32_t size;		// not including the header
	void* sender;
};

class UTILS_EXPORT evlclient_object
{
public:
	evlclient_object() = delete;
	~evlclient_object() = delete;
	int addref(void);
	int release(void);

	/**
	 * @brief Get client name
	 * @return const char* 		NULL is error
	 */
	const char* get_clientname(void);

	/**
	 * @brief Get the inst name 
	 * @return const char* 		NULL is error
	 */
	const char* get_instname(void);

	/**
	 * @brief this is temporary implementation for client write data
	 */
	size_t write(void* buffer, size_t sz);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(evlclient_object);
};

typedef zas::smtptr<evlclient_object> evlclient;

class UTILS_EXPORT userdef_evlclient
{
public:
	userdef_evlclient();
	virtual ~userdef_evlclient();

	evlclient getclient(void);

	/**
	  activate the client, after that, the client could
	  start handling
	  @return 0 for success
	 */
	int activate(void);

	/**
	  The interface to be implmented to get the
	  type and fd of the user defined evlclient
	  @param type the type of userdefined evlclient
	  @param fd the fd of the evlclient
	 */
	virtual void getinfo(uint32_t& type, int& fd) = 0;

	/**
	  To gvie a chance to the client to handle the data
	  input by itself
	  @return false means there is no handle, causing the
	  		evloop to handle the data input
	 */
	virtual bool handle_datain(void);

	/**
	  This is to give the chance to userdef evlclient
	  to handle common data input case if the evlclient
	  has not define its own handle_datain()
	  @param buf the read only buffer with inputted data
	  @param discard how many bytes shall we discard
	  		if 'discard' is less than 0 (by default), all data
			in buffer will be discard after the call
	  @return 0 for success
	 */
	virtual int on_common_datain(readonlybuffer* buf, int& discard);

private:
	void* _data;
};

#define EVL_MAKE_PKGID(clsid, pkgid)	(((clsid) << 22)	\
		| ((pkgid) & ((1 << 22) - 1)))

struct UTILS_EXPORT package_footer
{
	package_footer() = default;
	package_footer(package_header& hdr);
	void init(package_header& hdr);

	uint32_t magic;
	// todo:
};

#define EVL_DEFINE_PACKAGE(payload_type, pkgid)	\
	class payload_type##_pkg {	\
	public:	\
		payload_type##_pkg()	\
		: _header(pkgid, sizeof(payload_type)) {	\
			footer().init(_header);	\
		}	\
		payload_type##_pkg(size_t extra)	\
		: _header(pkgid, sizeof(payload_type) + extra) {	\
			footer().init(_header);	\
		}	\
		payload_type##_pkg(size_t extra, uint32_t seqid)	\
		: _header(pkgid, seqid	\
		, sizeof(payload_type) + extra) {	\
			footer().init(_header);	\
		}	\
		void* operator new(size_t sz, void* ptr) {	\
			return ptr;	\
		}	\
		void operator delete(void*, size_t) {}	\
		package_header& header(void) { return _header; }	\
		payload_type& payload(void) { return _payload; }	\
		package_footer& footer(void) {	\
			return *(package_footer*)(((size_t)&_payload)	\
			+ _header.size);	\
		}	\
	private:	\
		package_header _header;		\
		payload_type _payload;	\
		uint8_t _footer_buf[sizeof(package_footer)];	\
	}

enum evloop_role
{
	evloop_role_unknown = 0,
	evloop_role_server,
	evloop_role_client,
};

enum evloop_clientinfo
{
	evlcli_info_unknonwn = 0,
	evlcli_info_commit,
	evlcli_info_clear,
	evlcli_info_client_name,
	evlcli_info_instance_name,
	evlcli_info_client_ipv4,
	evlcli_info_listen_port,
	evlcli_info_server_ipv4,
	evlcli_info_server_port,
	evlcli_info_service_container,
	evlcli_info_service_shared,
};

zas_interface UTILS_EXPORT evloop_pkglistener
{
    /**
     * @brief it will be called when client receive new package
     * @param  client           sender of pacakge
     * @param  package_header   package header
	 * @param  queue			queue for triggered evpoller events
     * @return true             package handled
     * @return false            no handle
     */
    virtual bool on_package(evlclient sender,
        const package_header& pkghdr,
		const triggered_pkgevent_queue& queue) = 0;
};

typedef bool (*pkgnotifier)(void* owner, evlclient sender,
	const package_header& pkghdr,
	const triggered_pkgevent_queue& queue);

struct UTILS_EXPORT evloop_listener
{
	virtual void accepted(evlclient client);
	virtual void connected(evlclient client);
	virtual void disconnected(const char* cliname, const char* instname);
};

enum evloop_state
{
	evloop_state_unknown = 0,
	evloop_state_created,
	// ready is only for server
	evloop_state_ready,
	evloop_state_connecting,
	evloop_state_connected,
	evloop_state_disconnected,
	evloop_state_error,
};

zas_interface UTILS_EXPORT evloop_task
{
	/**
	 the precedure to be override for the
	 real task
	 */
	virtual void run(void);

	/**
	  Provide a way to destroy the task when
	  if finished. if nothing to be destroy,
	  simply not override this method
	 */
	virtual void release(void);
};

class UTILS_EXPORT evloop
{	
public:
	evloop() = delete;
	~evloop() = delete;

	/**
	  Get the singleton evloop object
	  @return evloop object
	  */
	static evloop* inst(void);

	/**
	  Get the current state of the evloop
	  @return the state of evloop
	  */
	evloop_state get_state(void);

	/**
	  Set role for the event loop
	  could be: server / client
	  note: the role shall be set only once

	  @param role the role to be set
	  @return 0 for success
	  */
	int setrole(evloop_role role);

	/**
	 * @brief get evloop rolse
	 * @return evloop_role_server 		server
	 * @return evloop_role_client 		client
	 * @return evloop_role_unknown 		error
	 */
	evloop_role getrole(void);

	/**
	  Start the event loop running
	  you can use the current thread as the running
	  thread for the evloop. this will immediately
	  block the current thread, or you can choose to
	  use a saperate thread to run the evloop. In this
	  case a new thread will be created for evloop

	  @param septhd true for a saperate thread
	         false will directly use current thread
	  @param wait true for waiting for the saperate
	  		 thread be ready for service. if it is failed
			 to establish evloop, an error will be generated
	  @return todo
	  */
	int start(bool septhd = true, bool wait = false);

	/**
	  Add a task to the evloop which will be
	  run in the earliest convenience
	  Note that the task must not be time critical
	  since there is no garantee when it will be
	  executed

	  @param tasklet_name the name of the tasklet
	  @param task the task to be run later
	  @return 0 for success
	  */
	int addtask(const char* tasklet_name, evloop_task* task);

	/**
	  Update the client info by parameter (cmd)
	  this will cause the server and all peers to update
	  the current client's information
	  @param cmd the command / parameters to execute
	  @return still the pointer to the evloop
	 */
	evloop* updateinfo(int cmd, ...);

	const char* get_name(void);

	const char* get_instance_name(void);

	/**
	  retrive the client by its name
	  the client name and instance name shall be passed
	  to retrive the client
	  client_name and inst_name are all NULL, return server client
	  @param client_name the client name
	  @param inst_name the instance name
	  @return the client object
	 */
	evlclient getclient(const char* client_name, const char* inst_name);

	/**
	  Check if the event loop is in running
	  @return true means running
	  */
	bool is_running(void);

	/**
	 * @brief  add evloop listener, listen package
	 * one package id only has one listener or notifier
	 * @param  pkgid            package id
	 * @param  lnr              listener
     * @return int == 0         success
     * @return int != 0         error
	 */
    int add_package_listener(uint32_t pkgid, evloop_pkglistener *lnr);

	/**
	 * @brief  add evloop notify, listen package
	 * one package id only has one listener or notifier
	 * @param  pkgid            package id
	 * @param  lnr              notifier
	 * @param  owner            userdata point
     * @return int == 0         success
     * @return int != 0         error
	 */
    int add_package_listener(uint32_t pkgid, 
		pkgnotifier notify, void* owner);

	/**
	 * @brief  remove evloop listener
	 * @param  pkgid            package id
	 * @param  lnr              listener
     * @return int == 0         success
     * @return int != 0         error
	 */
    int remove_package_listener(uint32_t pkgid, evloop_pkglistener *lnr);

	/**
	 * @brief  remove evloop notify
	 * @param  pkgid            package id
	 * @param  lnr              notifier
	 * @param  owner            userdata point
     * @return int == 0         success
     * @return int != 0         error
	 */
    int remove_package_listener(uint32_t pkgid, 
		pkgnotifier notify, void* owner);

	/**
	 * @brief add evloop client status listener
	 * name is unique
	 * @param  name             listener name
	 * @param  lnr              listener object
	 * @return int 				0:success
	 */
	int add_listener(const char* name, evloop_listener *lnr);

	/**
	 * @brief remove evloop client status listener
	 * @param  name             listener name
	 * @param  lnr              listener object
	 * @return int 				0:success
	 */
	int remove_listener(const char* name);

	ZAS_DISABLE_EVIL_CONSTRUCTOR(evloop);
};

}} // end of namespace zas::utils
#endif /*  __CXX_ZAS_UTILS_EVLOOP_H__ */
#endif // (defined(UTILS_ENABLE_FBLOCK_EVLOOP) && defined(UTILS_ENABLE_FBLOCK_EVLCLIENT) && defined(UTILS_ENABLE_FBLOCK_BUFFER))
/* EOF */
