/** @file wl-surface.cpp
 * Implementation of the serface and surface manager
 */

#include "utils/mutex.h"
#include "inc/wl-surface.h"

namespace zas {
namespace uicore {

using namespace zas::utils;

// mutex for surface manager
mutex sfmgr_mut;

surface_impl::surface_impl()
: _wl_surface(NULL)
{
	listnode_init(_surface_list);
	memset(&_eglsurf, 0, sizeof(_eglsurf));

	// todo: temp
	_gmtry.width = 300;
	_gmtry.height = 200;
	listnode_init(_surface_list);
}

surface_impl::~surface_impl()
{
	destroy_surface();
}

int surface_impl::attach_surface(void)
{
	wayland_client* wcl = wayland_client::inst();
	const wcl_state &s = wcl->get_wclstate();

	_wl_surface = wl_compositor_create_surface(s.wl_compositor);
	if (NULL == _wl_surface) {
		destroy_surface(); return -1;		
	}

	// prepare the surface
	wcl->assign_eglcontext(this);

	// here we are not binding a role to wl_surface
	// this will done in derived classy
	return 0;
}

void surface_impl::destroy_surface(void)
{
	wayland_client::inst()->detach_eglcontext(this);

	if (_wl_surface) {
		wl_surface_destroy(_wl_surface);
		_wl_surface = NULL;
	}

//	if (window->callback)
//		wl_callback_destroy(window->callback);
}

surfacemgr_impl::surfacemgr_impl()
: _factory(8)
{
	listnode_init(_tasklist);
	REGISTER_SURFACE_CREATOR(window_surface);
}

surfacemgr_impl::~surfacemgr_impl()
{
}

surface_impl* surfacemgr_impl::create_surface(surfaceid id)
{
	wayland_client* wcl = wayland_client::inst();
	if (!wcl->ready()) return NULL;

	surface_creator* creator = _factory.get(id);
	if (NULL == creator) return NULL;

	surface_impl* surf = (*creator)();
	assert(NULL != surf);
	return surf;
}

surface_task::surface_task() {}
surface_task::~surface_task() {}
void surface_task::do_task(void) {}

int surfacemgr_impl::submit_task(surface_task* tsk)
{
	if (NULL == tsk)
		return -1;

	// check if we are in the wayland_client thread
	wayland_client* wcl = wayland_client::inst();
	if (wcl->is_wlthread()) {
		// run the task
		// Note: we will not release the task object
		// the user shall determine if it is necessary
		// to release the object in do_task()
		tsk->do_task();
		return 0;
	}

	// submit the task
	sfmgr_mut.lock();
	listnode_add(_tasklist, tsk->_ownerlist);
	sfmgr_mut.unlock();

	return wcl->notify_event(wcl_evfd_surface_ops);
}

void surfacemgr_impl::dispatch_pending_tasks(void)
{
	sfmgr_mut.lock();
	while (!listnode_isempty(_tasklist)) {
		surface_task* tsk = list_entry(surface_task, \
			_ownerlist, _tasklist.next);
		listnode_del(tsk->_ownerlist);
		sfmgr_mut.unlock();

		// run the task
		// Note: we will not release the task object
		// the user shall determine if it is necessary
		// to release the object in do_task()
		tsk->do_task();
		sfmgr_mut.lock();
	}
	sfmgr_mut.unlock();
}

surfacemgr_impl* surfacemgr_impl::inst(void)
{
	static surfacemgr_impl* _inst = NULL;
	if (!_inst) {
		// todo: lock
		_inst = new surfacemgr_impl();
		assert(NULL != _inst);
	}
	return _inst;
}

surface_mgr* get_surfacemgr(void)
{
	return reinterpret_cast<surface_mgr*>
		(surfacemgr_impl::inst());
}

surface* surface_mgr::create_surface(surfaceid id)
{
	surfacemgr_impl* surfmgr = reinterpret_cast
		<surfacemgr_impl*>(this);
	if (NULL == surfmgr) return NULL;

	return static_cast<surface*>(surfmgr->create_surface(id));
}

}} // end of namespace zas:uicore
/* EOF */
