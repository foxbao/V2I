/** @file pkgconfig.h
 * defintion of package configuration
 */

#ifndef __CXX_ZAS_MWARE_MSS_DEVICE_H__
#define __CXX_ZAS_MWARE_MSS_DEVICE_H__

#include "mware/mware.h"

namespace zas {
namespace mware {

class mssdevice;
class mssdev_monitor;

enum mssdev_eventid
{
	mssdev_evid_unknown = 0,

	// for the first time, enumerate
	// all existed mass-storage devices
	mssdev_evid_enumerate,

	// an existed mass-storage device has
	// changed and need to be re-mounted
	// this shall never happen
	mssdev_evid_updated,

	// a new mass-storage device has
	// been connected
	mssdev_evid_connected,

	// a new connected mass-storage
	// device has been mounted
	mssdev_evid_mounted,

	// an existed mass-storage device
	// has been disconnected
	mssdev_evid_disconnected,
};

/**
  We only enumulate all supported
  file system type
 */
enum fs_type
{
	fs_type_unknown = 0,
	fs_type_vfat,
	fs_type_ntfs,
	fs_type_ext,
	fs_type_ext2,
	fs_type_ext3,
	fs_type_ext4,
};

struct MWARE_EXPORT mssdev_listener
{
	/**
	  User defined destructor
	 */
	virtual void release(void);

	/**
	  The method will be called if any
	  status has changed 
	 */
	virtual void on_changed(mssdev_monitor*,
		mssdev_eventid, mssdevice*);

	/**
	  The method will be called if a new
	  mass storage device has connected
	 */
	virtual void on_connected(mssdev_monitor*, mssdevice*);

	/**
	  The method will be called if a new
	  mass storage device has mounted
	 */
	virtual void on_mounted(mssdev_monitor*, mssdevice*);

	/**
	  The method will be called if an existed
	  mass storage device has been disconnected
	 */
	virtual void on_disconnected(mssdev_monitor*, mssdevice*);
};

class MWARE_EXPORT mssdevice
{
public:
	mssdevice() = delete;
	~mssdevice() = delete;

	/**
	  Check if the mass-storage device
	  has been mounted
	  @return true means it is already mounted
	 */
	bool mounted(void);

	/**
	  mount the mess-storage device
	  note the method will return immediately
	  before it is successfully mounted
	  you could ask to notify when successfully
	  mounted. failure will not be notified
	  @param opt specify the notify operation
	  	1) keep: keep the current status
		2) notify: set as notify when mounted
		3) not_notify: set as not notify (ignore) when mounted
	  @return 0 for success
	 */
	enum mnt_notify_opt { keep, notify, not_notify };
	int mount(mnt_notify_opt opt = keep);

	/**
	  Get the mounted path, nullptr will be returned
	  if the device is not mounted or mount failed
	  @return the path or nullptr
	 */
	const char* get_mounted_path(void);

	bool set_mount_notify(bool notify);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(mssdevice);
};

class MWARE_EXPORT mssdev_monitor
{
public:
	mssdev_monitor() = delete;
	~mssdev_monitor() = delete;

	/**
	  get the singleton instance of mssdev_monitor
	  @return return the singleton object
	 */
	static mssdev_monitor* inst(void);

	/**
	  activate the monitor
	  @return 0 for success
	 */
	int activate(void);

	/**
	  Add a listener
	  @return 0 for success
	 */
	int add_listener(mssdev_listener* lnr);
};

}} // end of namespace zas::mware
#endif // __CXX_ZAS_MWARE_MSS_DEVICE_H__
/* EOF */
