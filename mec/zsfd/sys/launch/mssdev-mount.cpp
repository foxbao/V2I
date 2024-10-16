/** @file mssdev-mount.cpp
 * implementation of automatically mass-storage devices
 * mounting and umount in zlaunch (need privilage)
 */

#include "utils/evloop.h"
#include "mware/mssdevice.h"
#include "inc/launch.h"

namespace zas {
namespace system {

using namespace zas::mware;

class mssdev_action_listener : public mssdev_listener
{
public:
	void on_changed(mssdev_monitor* monitor,
		mssdev_eventid evid, mssdevice* device)
	{
		if (evid != mssdev_evid_enumerate
			&& evid != mssdev_evid_updated) {
			return;
		}
		// only handle "enumerate" and "updated" case
		handle_mount(device);
	}

	void on_connected(mssdev_monitor*, mssdevice* device)
	{
		handle_mount(device);
	}

	void on_mounted(mssdev_monitor*, mssdevice*)
	{
		printf("mounted\n");
	}

	void on_disconnected(mssdev_monitor*, mssdevice*)
	{
	}

private:
	void handle_mount(mssdevice* device)
	{
		assert(nullptr != device);
		printf("do_mount\n");	
		device->mount(mssdevice::not_notify);
	}
};

class mssdev_evloop_listener : public evloop_listener
{
public:
	mssdev_evloop_listener()
	: _lnr(nullptr) {
		_lnr = new mssdev_action_listener();
		assert(nullptr != _lnr);
	}

	~mssdev_evloop_listener()
	{
		if (_lnr) {
			delete _lnr;
			_lnr = nullptr;
		}
	}

	void connected(evlclient client)
	{
		assert(nullptr != _lnr);
		auto* mssm = mssdev_monitor::inst();
		mssm->add_listener(_lnr);
		mssm->activate();
	}

private:
	mssdev_action_listener* _lnr;
};

int launch::init_mssdev_mounter(void)
{
	evloop::inst()->add_listener("launch-mssdev",
		new mssdev_evloop_listener());
	return 0;
}

}} // end of namespace zas::system