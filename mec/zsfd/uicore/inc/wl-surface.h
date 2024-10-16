/** @file wl-surface.h
 * Definition of the serface, window and surface manager implementation
 */

#ifndef __CXX_ZAS_UICORE_WAYLAND_SURFACE_H__
#define __CXX_ZAS_UICORE_WAYLAND_SURFACE_H__

#include "std/list.h"
#include "std/order-array.h"
#include "uicore/surface.h"
#include "uicore/region.h"
#include "inc/wl-utils.h"

namespace zas {
namespace uicore {

class surface_impl;
class surfacemgr_impl;

class surface_impl : public surface
{
	friend class wayland_client;
public:
	surface_impl();
	virtual ~surface_impl();

protected:
	int attach_surface(void);
	void destroy_surface(void);

protected:
	listnode_t _surface_list;
	igeometry_t _gmtry;
	wl_surface* _wl_surface;
	wcl_eglsurface _eglsurf;

	ZAS_DISABLE_EVIL_CONSTRUCTOR(surface_impl);
};

#define REGISTER_SURFACE_CREATOR(name)	\
	do {	\
		extern surface_impl* __create_##name(void);	\
		_factory.add(surfaceid_##name, __create_##name);	\
	} while (0)

typedef surface_impl* (*surface_creator)(void);

class surface_task
{
	friend class surfacemgr_impl;
public:
	surface_task();
	virtual ~surface_task();
	virtual void do_task(void);

private:
	listnode_t _ownerlist;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(surface_task);
};

class surfacemgr_impl
{
public:
	surfacemgr_impl();
	~surfacemgr_impl();
	static surfacemgr_impl* inst(void);

public:
	surface_impl* create_surface(surfaceid id);

	// run a surface task
	int submit_task(surface_task* tsk);

	// process all pending tasks
	void dispatch_pending_tasks(void);

private:
	zas::ordered_array<surface_creator> _factory;
	listnode_t _tasklist;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(surfacemgr_impl);
};

}} // end of namespace zas::uicore
#endif /* __CXX_ZAS_UICORE_WAYLAND_SURFACE_H__ */
/* EOF */
