/** @file inc/mssdev-monitor.cpp
 * implementation of device monitors
 */

#include <unistd.h>
#include <sys/mount.h>

#include "utils/dir.h"
#include "utils/cert.h"
#include "utils/timer.h"
#include "mware/sysconfig.h"
#include "inc/mssdev-monitor.h"

namespace zas{
namespace mware {

using namespace std;
using namespace zas::mware;

udev_evlclient::udev_evlclient(mssdev_monitor_impl* mm)
: _udev(nullptr), _monitor(nullptr), _mssdev_mon(mm)
{
	assert(nullptr != mm);
	mm->update_supported_fs_list();
	int ret = init_udev();
	assert(ret == 0);
}

udev_evlclient::~udev_evlclient()
{
	destroy_udev();
}

int udev_evlclient::init_udev(void)
{
	int ret = 0;
	_udev = udev_new();
	if (nullptr == _udev) return -1;

	_monitor = udev_monitor_new_from_netlink(_udev, "udev");
	if (nullptr == _monitor) { ret = -2; goto error; }

	// scan to get the initial status
	if (udev_scan()) { ret = -3; goto error; }

	if (udev_monitor_filter_add_match_subsystem_devtype(
		_monitor, "block", "partition")) {
		ret = -4; goto error;
	}
	if (udev_monitor_enable_receiving(_monitor)) {
		ret = -5; goto error;
	}
	return 0;

error:
	if (_monitor) udev_monitor_unref(_monitor);
	if (_udev) udev_unref(_udev);
	return ret;
}

void udev_evlclient::destroy_udev(void)
{
	if (_monitor) {
		udev_monitor_unref(_monitor);
		_monitor = nullptr;
	}
	if (_udev) {
		udev_unref(_udev);
		_udev = nullptr;
	}
}

void udev_evlclient::getinfo(uint32_t& type, int& fd)
{
	type = client_type_device_monitor;
	fd = udev_monitor_get_fd(_monitor);
	assert(fd >= 0);
}

int udev_evlclient::udev_scan(void)
{
	udev_list_entry *first, *list;
	udev_enumerate* enumerate = udev_enumerate_new(_udev);
	if (nullptr == enumerate) return -1;

	// add matching conditions and enmuerte
	if (udev_enumerate_add_match_subsystem(enumerate, "block")) {
		udev_enumerate_unref(enumerate); return -2;
	}
	if (udev_enumerate_scan_devices(enumerate)) {
		udev_enumerate_unref(enumerate); return -3;
	}

	first = udev_enumerate_get_list_entry(enumerate);
	udev_list_entry_foreach(list, first) {
		const char* syspath = udev_list_entry_get_name(list);
		udev_device* device = udev_device_new_from_syspath(
			_udev, syspath);
		if (nullptr == device) continue;

		_mssdev_mon->update_device(device);
		udev_device_unref(device);
	}
	udev_enumerate_unref(enumerate);
	return 0;
}

bool udev_evlclient::handle_datain(void)
{
	if (nullptr == _monitor) {
		return true;
	}
	udev_device* dev;
	while (nullptr != (dev = udev_monitor_receive_device(_monitor))) {
		_mssdev_mon->update_device(dev);
		udev_device_unref(dev);
	}
	return true;
}

int udev_evlclient::on_common_datain(
	readonlybuffer* buf, int& discard)
{
	assert(0);
	return 0;
}

const char* mssdev_monitor_impl::_supported_fs[] = {
	"vfat", "ntfs", "ext", "ext2", "ext3", "ext4", nullptr,
};

mssdev_monitor_impl::mssdev_monitor_impl()
: _client(nullptr)
, _working_thread(nullptr)
, _wkthd_start_timestamp(0)
, _flags(0)
{
	listnode_init(_fs_list);
	listnode_init(_lnr_list);
	listnode_init(_mssdev_list);
}

mssdev_monitor_impl::~mssdev_monitor_impl()
{
	release_fslist();
	release_lnrlist();
	release_mssdev_list();
}

int mssdev_monitor_impl::activate(void)
{
	auto_mutex am(_mut);
	if (nullptr != _client) {
		return 0;
	}
	_client = new udev_evlclient(this);
	assert(nullptr != _client);
	return _client->activate();
}

void mssdev_monitor_impl::release_fslist(void)
{
	auto_mutex am(_mut);
	while (!listnode_isempty(_fs_list)) {
		auto* node = list_entry(supported_fs_node, ownerlist, \
		_fs_list.next);
		listnode_del(node->ownerlist);
		delete node;
	}
}

void mssdev_monitor_impl::release_lnrlist(void)
{
	auto_mutex am(_mut);
	if (_f.notifying) {
		assert(0);
		return;
	}
	while (!listnode_isempty(_lnr_list)) {
		auto* node = list_entry(listener_node, ownerlist, \
		_lnr_list.next);
		listnode_del(node->ownerlist);
		if (node->lnr) {
			node->lnr->release();
		}
		delete node;
	}
}

void mssdev_monitor_impl::release_mssdev_list(void)
{
	auto_mutex am(_mut);
	while (!listnode_isempty(_mssdev_list)) {
		auto* node = list_entry(mssdev_node,
			ownerlist, _mssdev_list.next);
		listnode_del(node->ownerlist);
		delete node;
	}
}

mssdev_monitor_impl::supported_fs_node*
mssdev_monitor_impl::get_fsnode_unlocked(
	const char* start, const char* end)
{
	if (!start || !*start) {
		return nullptr;
	}
	auto* item = _fs_list.next;
	for (; item != &_fs_list; item = item->next) {
		auto* node = list_entry(supported_fs_node, ownerlist, item);
		if (nullptr == end) {
			if (!strcmp(node->fstype.c_str(), start))
				return node;
		} else if (!strncmp(node->fstype.c_str(),
			start, end - start)) {
			return node;
		}
	}
	return nullptr;
}

int mssdev_monitor_impl::update_supported_fs_list(void)
{
	// release all existed filesystem types
	release_fslist();
	const char* fslist = get_sysconfig(
		"mass-storage.supported-filesystems", nullptr);
	if (nullptr == fslist) {
		return 0;
	}

	// we need lock
	auto_mutex am(_mut);

	for (;;) {
		const char* end = strchr(fslist, ' ');
		if (fs_supported(fslist, end) &&
			nullptr == get_fsnode_unlocked(fslist, end)) {
			// create a new object and add to list
			auto* node = new supported_fs_node;
			if (nullptr == node) {
				return -1;
			}
			// save the file system type
			if (nullptr == end) {
				node->fstype.assign(fslist);
			} else {
				node->fstype.assign(fslist, end - fslist);
			}
			listnode_add(_fs_list, node->ownerlist);
		}
		if (nullptr == end) break;

		// omit rest spaces if we have
		for (; *end && *end == ' '; ++end);
		if (!*end) break;
		fslist = end;
	}
	return 0;
}

bool mssdev_monitor_impl::fstype_inlist_unlocked(const char* fs)
{
	if (!fs || !*fs) return false;

	if (listnode_isempty(_fs_list)) {
		const char* end = fs + strlen(fs);
		return fs_supported(fs, end) ? true : false;
	}

	auto* item = _fs_list.next;
	for (; item != &_fs_list; item = item->next) {
		auto* node = list_entry(supported_fs_node, ownerlist, item);
		if (!strcmp(node->fstype.c_str(), fs)) {
			return true;
		}
	}
	return false;
}

int mssdev_monitor_impl::update_device(udev_device* dev)
{
	if (nullptr == dev) {
		return -1;
	}

	const char *id_bus = udev_device_get_property_value(
		dev, "ID_BUS");
	const char* syspath = udev_device_get_syspath(dev);
	if (id_bus && strcmp(id_bus, "usb")) {
		// check if it contains "usb" in the syspath
		if (!syspath || !strstr(syspath, "usb")) {
			return -2;
		}
	}

	const char* devtype = udev_device_get_devtype(dev);
	if (!devtype || strcmp(devtype, "partition")) {
		return -3;
	}

	// need lock
	auto_mutex am(_mut);

	// check if the file system is supported
	bool avail = fstype_inlist_unlocked(
		udev_device_get_property_value(dev, "ID_FS_TYPE"));
	if (!avail) return -4;

	// check the action
	auto* node = find_unlocked(syspath);
	auto *action = udev_device_get_action(dev);

	if (action) {
		if (!strcmp(action, "remove")) {
			if (node) {
				// remove device
				printf("remove device: %s\n", syspath);
				listnode_del(node->ownerlist);
				async_notify(mssdev_evid_disconnected, node);
				delete node;
				return 0;
			}
			else {
				// remove an item that is not
				// in out list, ignore it
				return 0;
			}
		}
		else if (!strcmp(action, "add")) {
			if (node) {
				// the node exists, just update it
				if (update_mssdev_node(node, dev) > 0) {
					async_notify(mssdev_evid_updated, node);
				}
				return 0;
			} else {
				// create a new node
				printf("add device: %s\n", syspath);
				if (nullptr == (node = create_mssdev_node(dev))) {
					return -5;
				}
				async_notify(mssdev_evid_connected, node);
				return 0;
			}
		}
		else {
			// unrecognized action
			// do nothing and return
			return -6;
		}
	} else {
		// no action
		if (node) {
			// the node exists, just update it
			if (update_mssdev_node(node, dev) > 0) {
				async_notify(mssdev_evid_updated, node);
			}
			return 0;
		}
		else {
			// create a new node
			if (nullptr == (node = create_mssdev_node(dev))) {
				return -7;
			}
			async_notify(mssdev_evid_enumerate, node);
			return 0;
		}
	}
	// shall never be here
	return -EINVALID;
}

// this method will not check if the node exists
mssdev_monitor_impl::mssdev_node*
mssdev_monitor_impl::create_mssdev_node(udev_device* dev)
{
	auto* node = new mssdev_node;
	if (nullptr == node) {
		return nullptr;
	}
	if (update_mssdev_node(node, dev) < 0) {
		delete node;
		return nullptr;
	}

	auto_mutex am(_mut);
	listnode_add(_mssdev_list, node->ownerlist);
	return node;
}

int mssdev_monitor_impl::update_mssdev_node(
	mssdev_monitor_impl::mssdev_node* node,
	udev_device* dev)
{
	assert(nullptr != node);
	auto* syspath = udev_device_get_syspath(dev);
	if (node->sys_path == syspath) {
		// it is the same device
		return 0;
	}

	if (node->f.mount_state > mssdev_node::mntstat_mounting)
	{
		// unmount the node first
		assert(node->mounted_dir.length());
		if (umount2(node->mounted_dir.c_str(), MNT_DETACH)) {
			return -EINVALID;
		}
		node->f.mount_state = 0;
	}

	node->sys_path = syspath;
	node->dev_name = udev_device_get_property_value(
		dev, "DEVNAME");
	node->fs_type = udev_device_get_property_value(
		dev, "ID_FS_TYPE");
	node->sys_num = udev_device_get_sysnum(dev);
	node->sys_name = udev_device_get_sysname(dev);
printf("devname: %s, sysname: %s\n",
	node->dev_name.c_str(),
	node->sys_name.c_str());
	return 1;
}

mssdev_monitor_impl::mssdev_node*
mssdev_monitor_impl::find_unlocked(const char* syspath)
{
	auto* item = _mssdev_list.next;
	for (; item != &_mssdev_list; item = item->next) {
		auto* node = list_entry(mssdev_node, ownerlist, item);
		if (!strcmp(node->sys_path.c_str(), syspath)) {
			return node;
		}
	}
	return nullptr;
}

int mssdev_monitor_impl::act_mount(mssdev_node* node)
{
	int ret = 0;
	char tmp[8];
	string folder, tmpdir;
	string& dir = node->mounted_dir;

	// set the current state as "mounting"
	node->f.mount_state = mssdev_node::mntstat_mounting;

	// generate a unique directory name
	digest dgst("md5");
	dgst.append((void*)node->sys_name.c_str(),
		node->sys_name.length());

	size_t sz;
	const uint8_t* dgstret = dgst.getresult(&sz);
	assert(nullptr != dgstret && sz == 16);

	// get the mount root directory
	const char* mntdir = get_sysconfig("mass-storage.mount-root", nullptr);
	if (nullptr == mntdir) { ret = -EINVALID; goto all_ret; }

	// generate a temporary directory name
	dir = mntdir;
	if (dir[dir.length() - 1] != '/') {
		dir += "/";
	}

	for (int i = 0; i < 16; ++i) {
		sprintf(tmp, "%02x", dgstret[i]);
		folder += tmp;
	}

	// use a temp directory first
	tmpdir = dir + "original/";

	// that's the final filename
	dir += folder;
	tmpdir += folder + ".org";

	if (!fileexists(tmpdir.c_str())) {
		printf("creating dir: %s\n", tmpdir.c_str());
		createdir(tmpdir.c_str());
	}

	if (fileexists(dir.c_str())) {
		ret = -EINVALID; goto all_ret;
	}

	// mount the directory
	if (::mount(node->dev_name.c_str(),
		tmpdir.c_str(), node->fs_type.c_str(), 0,
		"errors=continue")) {
		printf("fail in mount: %s\n", strerror(errno));
		rmdir(tmpdir.c_str());
		dir.clear();
		ret = -ENOTALLOWED; goto all_ret;
	}
	if (symlink(tmpdir.c_str(), dir.c_str())) {
		printf("symlink error: %s\n", strerror(errno));
		ret = -ENOTALLOWED; goto all_ret;
	}

all_ret:
	node->f.mount_state = (!ret) ? mssdev_node::mntstat_mounted
		: mssdev_node::mntstat_mount_failed;
	return ret;
}

void mssdev_monitor_impl::mssdev_mount_request::do_action(void)
{
	// are we already mounted?
	if (_node->f.mount_state == mssdev_node::mntstat_mounted
		|| _node->f.mount_state == mssdev_node::mntstat_mount_failed) {
		return;
	}
	
	assert(nullptr != _monitor);
	if (!_monitor->act_mount(_node)) {
		// successfully mounted, see if we need notify
		if (_node->f.notify_mounted) {
			_monitor->notify(mssdev_evid_mounted, _node);
			// todo: _node->f.mnt_notified = 1;
		}
	}
}

int mssdev_monitor_impl::async_notify(mssdev_eventid evid,
	mssdev_monitor_impl::mssdev_node* node)
{
	int ret = check_run_working_thread();
	if (ret) return ret;

	auto* task = new notify_request(evid, node, this);
	if (nullptr == task) return -ENOMEMORY;

	return task->addto(_working_thread);
}

void mssdev_monitor_impl::notify_request::do_action(void)
{
	// notify to user
	_monitor->notify(_eventid, _node);

	// check if we need to notify "mounted"
	if (_eventid == mssdev_evid_enumerate
		|| _eventid == mssdev_evid_connected) {
		_monitor->check_notify_mounted(_node);
	}
}

void mssdev_monitor_impl::notify(mssdev_eventid evid,
	mssdev_monitor_impl::mssdev_node* node)
{
	auto_mutex am(_mut);
	_f.notifying = 1;
	auto* item = _lnr_list.next;
	for (; item != &_lnr_list; item = item->next) {
		auto* obj = list_entry(listener_node, ownerlist, item);
		if (obj->lnr) {
			auto* device = reinterpret_cast<mssdevice*>(node);
			auto* monitor = reinterpret_cast<mssdev_monitor*>(this);
			obj->lnr->on_changed(monitor, evid, device);
			
			switch (evid) {
			case mssdev_evid_connected:
				obj->lnr->on_connected(monitor, device);
				break;

			case mssdev_evid_mounted:
				obj->lnr->on_mounted(monitor, device);
				break;

			case mssdev_evid_disconnected:
				obj->lnr->on_disconnected(monitor, device);
				break;
			}
		}
	}
	// finished
	_f.notifying = 0;
}

mssdev_monitor_impl::check_mounted_timer
::check_mounted_timer(mssdev_monitor_impl* monitor,
	mssdev_monitor_impl::mssdev_node* node)
	: timer(200), _monitor(monitor), _node(node)
	, _count(check_mounted_count)
{
	assert(nullptr != node);
	// get the mount root directory
	const char* mntdir = get_sysconfig(
		"mass-storage.mount-root", nullptr);
	if (nullptr == mntdir) {
		return;
	}

	// generate a temporary directory name
	_mntdir = mntdir;
	if (_mntdir[_mntdir.length() - 1] != '/') {
		_mntdir += "/";
	}

	// generate a unique directory name
	digest dgst("md5");
	dgst.append((void*)node->sys_name.c_str(),
		node->sys_name.length());

	size_t sz;
	const uint8_t* dgstret = dgst.getresult(&sz);
	assert(nullptr != dgstret && sz == 16);

	char tmp[8];
	for (int i = 0; i < 16; ++i) {
		sprintf(tmp, "%02x", dgstret[i]);
		_mntdir += tmp;
	}
}

void mssdev_monitor_impl::check_mounted_timer::on_timer(void)
{printf("check_mounted_timer: count: %d\n", _count);
	int ret;
	// if we are timeout?
	if (_count-- <= 0) {
		goto release_myself;
	}
	ret = _monitor->do_check_notify_mounted(_node, _mntdir);
	if (ret < 0) {
		// an error occurred
		goto release_myself;
	}
	else if (ret > 0) {
		// continue poll again
		start();
		return;
	}

	// ret = 0, successfully mounted and notified
release_myself:
	delete this;
}

int mssdev_monitor_impl::check_notify_mounted(
	mssdev_monitor_impl::mssdev_node* node)
{
	if (!node->f.notify_mounted) {
		return 0;
	}

	// create a timer and poll mount status
	// by check if the mounting directory
	// has been created
	auto* tmr = new mssdev_monitor_impl::check_mounted_timer(
		this, node);
	assert(nullptr != tmr);
	tmr->start();
	return 0;
}

int mssdev_monitor_impl::do_check_notify_mounted(
	mssdev_monitor_impl::mssdev_node* node,
	string& mntdir)
{
	auto_mutex am(_mut);
	if (!check_mssdev_node_exists_unlocked(node)) {
		return -ENOTEXISTS;
	}
	// check if it is successfully mounted
	if (!fileexists(mntdir.c_str())) {
		return 1; // still pending
	}
	// set as mounted
	node->f.mount_state = mssdev_node::mntstat_mounted;
	node->mounted_dir = mntdir;

	// notify
	if (!node->f.notify_mounted) {
		return 0;
	}
	async_notify(mssdev_evid_mounted, node);
	return 0;
}

bool mssdev_monitor_impl::check_mssdev_node_exists_unlocked(
	mssdev_monitor_impl::mssdev_node* node)
{
	auto* item = _mssdev_list.next;
	for (; item != &_mssdev_list; item = item->next) {
		auto* obj = list_entry(mssdev_node, ownerlist, item);
		if (obj == node) return true;
	}
	return false;
}

int mssdev_monitor_impl::check_run_working_thread(void)
{
	if (nullptr == _working_thread)
	{
		_wkthd_start_timestamp = gettick_millisecond();

		// this is the first time creating the working thread
		_working_thread = new looper_thread(
			"mssdev-worker",
			0, working_thread_max_timeout);

		if (nullptr == _working_thread) {
			return -ENOMEMORY;
		}
	}
	else {
		assert(_wkthd_start_timestamp != 0);

		// create a new looper thread (not the first time)
		// since the last one is going to exit
		// in a very short time
		long curr = gettick_millisecond();
		if (curr - _wkthd_start_timestamp
			> working_thread_max_timeout_threshold)
		{
			_wkthd_start_timestamp = gettick_millisecond();
			_working_thread = new looper_thread(
				"mssdev-worker",
				0, working_thread_max_timeout);

			if (nullptr == _working_thread) {
				return -ENOMEMORY;
			}
		}
		else return 0;
	}
	// start the new thread
	_working_thread->start();
	_working_thread->release();
	return 0;
}

int mssdev_monitor_impl::mount(mssdev_node* node)
{
	int ret = check_run_working_thread();
	if (ret) return ret;

	// send request
	mssdev_mount_request* req = new mssdev_mount_request(node, this);
	assert(nullptr != req);
	req->addto(_working_thread);
	return 0;
}

mssdev_monitor_impl* mssdev_monitor_impl::inst(void)
{
	static mssdev_monitor_impl _monitor;
	return &_monitor;
}

bool mssdev_monitor_impl::fs_supported(
	const char* start, const char* end)
{
	if (!start || !*start) {
		return false;
	}
	for (int i = 0 ;; ++i) {
		const char* item = _supported_fs[i];
		if (nullptr == item) {
			break;
		}
		if (nullptr == end) {
			if (!strcmp(start, item)) return true;
		} else if (!strncmp(start,
			item, end - start)) {
			return true;
		}
	}
	return false;
}

int mssdev_monitor_impl::add_listener(mssdev_listener* lnr)
{
	if (nullptr == lnr) {
		return -EBADPARM;
	}
	auto_mutex am(_mut);
	if (_f.notifying) {
		assert(0);
		return -EINVALID;
	}
	if (find_unlocked(lnr)) {
		return -EEXISTS;
	}

	// add a new node
	listener_node* node = new listener_node;
	if (nullptr == node) {
		return -ENOMEMORY;
	}
	node->lnr = lnr;
	listnode_add(_lnr_list, node->ownerlist);
	return 0;
}

mssdev_monitor_impl::listener_node*
mssdev_monitor_impl::find_unlocked(mssdev_listener* lnr)
{
	if (nullptr == lnr) {
		return nullptr;
	}
	auto* item = _lnr_list.next;
	for (; item != &_lnr_list; item = item->next) {
		auto* node = list_entry(listener_node, ownerlist, item);
		if (node->lnr == lnr) return node;
	}
	return nullptr;
}

void mssdev_listener::release(void) {
}

void mssdev_listener::on_changed(mssdev_monitor*,
	mssdev_eventid, mssdevice*) {
}

void mssdev_listener::on_connected(mssdev_monitor*, mssdevice*)
{
}

void mssdev_listener::on_mounted(mssdev_monitor*, mssdevice*)
{
}

void mssdev_listener::on_disconnected(mssdev_monitor*, mssdevice*)
{
}

mssdev_monitor* mssdev_monitor::inst(void)
{
	auto* obj = mssdev_monitor_impl::inst();
	assert(nullptr != obj);
	return reinterpret_cast<mssdev_monitor*>(obj);
}

int mssdev_monitor::add_listener(mssdev_listener* lnr)
{
	auto* obj = reinterpret_cast<mssdev_monitor_impl*>(this);
	if (nullptr == obj) return -EINVALID;
	return obj->add_listener(lnr);
}

int mssdev_monitor::activate(void)
{
	auto* obj = reinterpret_cast<mssdev_monitor_impl*>(this);
	if (nullptr == obj) return -EINVALID;
	return obj->activate();
}

bool mssdevice::mounted(void)
{
	auto* dev = reinterpret_cast<
		mssdev_monitor_impl::mssdev_node*>(this);
	if (nullptr == dev) return false;
	
	return (dev->f.mount_state == mssdev_monitor_impl
	::mssdev_node::mntstat_mounted) ? true : false;
}

const char* mssdevice::get_mounted_path(void)
{
	auto* dev = reinterpret_cast<
		mssdev_monitor_impl::mssdev_node*>(this);
	if (nullptr == dev) return nullptr;

	if (dev->f.mount_state != mssdev_monitor_impl
		::mssdev_node::mntstat_mounted) {
		return nullptr;
	}
	return dev->mounted_dir.c_str();
}

int mssdevice::mount(mnt_notify_opt opt)
{
	auto* dev = reinterpret_cast<
		mssdev_monitor_impl::mssdev_node*>(this);
	if (nullptr == dev) return -EINVALID;

	uint32_t backup = dev->f.notify_mounted;
	if (opt != keep) {
		dev->f.notify_mounted =
			(opt == notify) ? 1 : 0;
	}
	int ret = mssdev_monitor_impl::inst()->mount(dev);
	if (ret) {
		dev->f.notify_mounted = backup;
		return ret;
	}
	return 0;
}

bool mssdevice::set_mount_notify(bool notify)
{
	auto* dev = reinterpret_cast<
		mssdev_monitor_impl::mssdev_node*>(this);
	if (nullptr == dev) return -EINVALID;

	bool ret = (dev->f.notify_mounted) ? true : false;
	dev->f.notify_mounted = (notify) ? 1 : 0;

	return ret;
}

}} // end of namespace zas::coreapp::servicebundle
/* EOF */
