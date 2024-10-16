/** @file inc/mssdev-monitor.h
 * definition of device monitors
 */

#ifndef __CXX_ZAS_MWARE_MSSDEV_MONITOR_IMPL_H__
#define __CXX_ZAS_MWARE_MSSDEV_MONITOR_IMPL_H__

#include <string>
#include <libudev.h>
#include "std/list.h"
#include "utils/mutex.h"
#include "utils/evloop.h"
#include "utils/thread.h"

#include "mware/mssdevice.h"

namespace zas{
namespace mware {

using namespace zas::utils;

#define client_type_device_monitor	(100)
#define working_thread_max_timeout_threshold	(15000)
#define working_thread_max_timeout	\
	(working_thread_max_timeout_threshold + 2000)

class mssdev_monitor_impl;

class udev_evlclient : public userdef_evlclient
{
public:
	udev_evlclient(mssdev_monitor_impl*);
	~udev_evlclient();

	void getinfo(uint32_t& type, int& fd);
	bool handle_datain(void);
	int on_common_datain(readonlybuffer* buf, int& discard);

private:
	int init_udev(void);
	void destroy_udev(void);
	int udev_scan(void);

private:
	udev *_udev;
	udev_monitor *_monitor;
	mssdev_monitor_impl* _mssdev_mon;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(udev_evlclient);
};

class mssdev_monitor_impl
{
public:
	struct supported_fs_node
	{
		listnode_t ownerlist;
		std::string fstype;
	};

	struct listener_node
	{
		listnode_t ownerlist;
		mssdev_listener* lnr;
	};

	struct mssdev_node
	{
		enum {
			mntstat_mounting = 1,
			mntstat_mounted = 2,
			mntstat_mount_failed = 3,
		};

		listnode_t ownerlist;
		std::string sys_path;
		std::string dev_name;
		std::string fs_type;
		std::string sys_name;
		std::string sys_num;
		std::string mounted_dir;
		union {
			uint32_t flags;
			struct {
				uint32_t mount_state : 3;
				uint32_t notify_mounted : 1;
			} f;
		};
		mssdev_node() : flags(0) {
			f.notify_mounted = 1;
		}
	};

	class notify_request : public looper_task
	{
	public:
		notify_request(mssdev_eventid evid,
			mssdev_node* node, mssdev_monitor_impl* mon)
			: _eventid(evid), _node(node), _monitor(mon)
		{
		}
		
		void do_action(void);
	
	private:
		mssdev_eventid _eventid;
		mssdev_node* _node;
		mssdev_monitor_impl* _monitor;
	};

	class mssdev_mount_request : public looper_task
	{
	public:
		mssdev_mount_request(mssdev_node* node, mssdev_monitor_impl* mon)
		: _node(node), _monitor(mon) {
			assert(nullptr != node && nullptr != mon);
		}

		void do_action(void);

	private:
		mssdev_monitor_impl::mssdev_node* _node;
		mssdev_monitor_impl* _monitor;
	};

	class check_mounted_timer : public timer
	{
	public:
		enum {
			check_mounted_count = 10000 / 200,
		};
		check_mounted_timer(mssdev_monitor_impl* monitor,
			mssdev_monitor_impl::mssdev_node* node);

		void on_timer(void);

	private:
		int _count;
		mssdev_monitor_impl* _monitor;
		mssdev_monitor_impl::mssdev_node* _node;
		std::string _mntdir;
	};

	mssdev_monitor_impl();
	~mssdev_monitor_impl();

	static mssdev_monitor_impl* inst(void);

	int activate(void);
	int update_supported_fs_list(void);
	int update_device(udev_device* dev);

	int add_listener(mssdev_listener* lnr);
	int mount(mssdev_node* node);

private:
	void release_fslist(void);
	void release_lnrlist(void);
	void release_mssdev_list(void);

	supported_fs_node* get_fsnode_unlocked(
		const char* start,
		const char* end);

	bool fs_supported(
		const char* start, const char* end);

	bool fstype_inlist_unlocked(const char* fs);

	mssdev_node* find_unlocked(const char* syspath);
	listener_node* find_unlocked(mssdev_listener* lnr);

	mssdev_node* create_mssdev_node(udev_device* dev);
	int update_mssdev_node(mssdev_node* node, udev_device* dev);

	int act_mount(mssdev_node* node);
	void notify(mssdev_eventid evid, mssdev_node* node);
	int async_notify(mssdev_eventid evid, mssdev_node* node);
	
	int check_notify_mounted(mssdev_node* node);
	int do_check_notify_mounted(mssdev_node* node, std::string& mntdir);
	bool check_mssdev_node_exists_unlocked(mssdev_node* node);

	int check_run_working_thread(void);

private:
	mutex _mut;
	listnode_t _fs_list;
	listnode_t _lnr_list;
	listnode_t _mssdev_list;
	udev_evlclient* _client;
	static mssdev_monitor_impl* _inst;

	looper_thread* _working_thread;
	long _wkthd_start_timestamp;

	union {
		uint32_t _flags;
		struct {
			uint32_t notifying : 1;
		} _f;
	};
	static const char* _supported_fs[];
	ZAS_DISABLE_EVIL_CONSTRUCTOR(mssdev_monitor_impl);
};

}} // end of namespace zas::coreapp::servicebundle
#endif // __CXX_ZAS_MWARE_MSSDEV_MONITOR_IMPL_H__
/* EOF */
