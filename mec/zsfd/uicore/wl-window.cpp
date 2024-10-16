/** @file wl-window.h
 * implmentation of the window serface
 */

#include <GLES2/gl2.h>	// todo: remove
#include "utils/uuid.h" // todo: remove

#include "utils/wait.h"
#include "uicore/window.h"
#include "inc/wl-window.h"
#include "inc/wl-surface.h"

#include "hwgrph/context2d.h"

using namespace zas::hwgrph;

namespace zas {
namespace uicore {

surface_impl* __create_window_surface(void) {
	return new window_surface();
}

window_surface::window_surface()
: _parents(NULL)
, _xdg_surface(NULL)
, _xdg_toplevel(NULL)
, _flags(0)
{
	listnode_init(_sibling);
	listnode_init(_children);
}

window_surface::~window_surface()
{
	destroy_surface();
}

void window_surface::destroy_surface(void)
{
	if (!_f.showed) return;

	wayland_client* wcl = wayland_client::inst();
	const wcl_state& s = wcl->get_wclstate();

	if (_xdg_toplevel) {
		xdg_toplevel_destroy(_xdg_toplevel);
		_xdg_toplevel = NULL;
	}
	if (_xdg_surface) {
		xdg_surface_destroy(_xdg_surface);
		_xdg_surface = NULL;
	}
	surface_impl::destroy_surface();
	_flags = 0;
}

static void toplevel_configure(void *data, struct xdg_toplevel *toplevel,
	int32_t width, int32_t height, struct wl_array *states)
{
	window_surface* surface = reinterpret_cast<window_surface*>(data);
	surface->handle_toplevel_configure(toplevel, width, height, states);
}

static void toplevel_close(void *data,
	xdg_toplevel *xdg_toplevel)
{
}

static const xdg_toplevel_listener xdg_toplevel_listener = {
	toplevel_configure,
	toplevel_close,
};

static void xdg_surface_configure(void *data,
	xdg_surface *xdg_surface, uint32_t serial)
{
	window_surface* surface = reinterpret_cast<window_surface*>(data);
	surface->handle_xdg_surface_configure(xdg_surface, serial);
}

static const xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

int window_surface::attach_xdg_toplevel(void)
{
	if (_xdg_toplevel) {
		return 0;
	}

	_xdg_toplevel = xdg_surface_get_toplevel(_xdg_surface);
	if (NULL == _xdg_toplevel) {
		destroy_surface(); return -1;
	}

	xdg_toplevel_add_listener(_xdg_toplevel,
		&xdg_toplevel_listener, this);

	xdg_toplevel_set_title(_xdg_toplevel, _title.c_str());
	return 0;
}

int window_surface::attach_surface(void)
{
	int ret = surface_impl::attach_surface();
	if (ret) return ret;

	wayland_client* wcl = wayland_client::inst();
	const wcl_state &s = wcl->get_wclstate();

	_xdg_surface = xdg_wm_base_get_xdg_surface(
		s.xdg_wm_base, _wl_surface);
	if (NULL == _xdg_surface) {
		destroy_surface(); return -1;
	}

	xdg_surface_add_listener(_xdg_surface,
		&xdg_surface_listener, this);

	ret = attach_xdg_toplevel();
	if (ret) return -1 + ret;

	// After creating a role-specific object and setting it up,
	// the client must perform an initial commit without any
	// buffer attached.
	wl_surface_commit(_wl_surface);
	return 0;
}

void window_surface::handle_xdg_surface_configure(
	xdg_surface *xdg_surface, uint32_t serial)
{
	xdg_surface_ack_configure(xdg_surface, serial);

	// make sure it is attached
	_f.showed = 1;

	wl_region *region;
	EGLint rect[4];
	EGLint buffer_age = 0;

	wayland_client* wcl = wayland_client::inst();
	const wcl_state& s = wcl->get_wclstate();
	
	if (s.swap_buffers_with_damage)
		eglQuerySurface(s.egl_display, _eglsurf.egl_surface,
			EGL_BUFFER_AGE_EXT, &buffer_age);

//	EGLBoolean ret = eglMakeCurrent(s.egl_display, _eglsurf.egl_surface,
//		_eglsurf.egl_surface, _eglsurf.egl_context);
//	assert(EGL_TRUE == ret);

//	zas::hwgrph::test(_gmtry.width, _gmtry.height);

	context2d* ctx = context2d::create();
	ctx->set_target(render_target_to_screen, _gmtry.width, _gmtry.height);

	solidfill sf = ctx->create_solidfill();
	sf->setcolor(rgba(0,1,1,1));
	ctx->set_fillstyle(sf);

	shadowstyle sdw = ctx->create_shadowstyle();
	sdw->setcolor(rgba(0, 0, 0, 1));
	sdw->setsize(1.15);
	sdw->setblur(10);
	sdw->enable();
	ctx->set_shadowstyle(sdw);

	ctx->clear(rgba(1, 1, 1, 1));
	ctx->rect(30, 30, 150, 80, 20);
	ctx->shadow();

	ctx->fill();
//	ctx->rect(30, 30, 100, 100, 50);
//	ctx->stroke();

/*
	static image img;
	img = loadimage("file:///home/jimview/test.bmp");
	ctx->drawimage(img, 0, 0, 0, 0);
*/

/*
	region = wl_compositor_create_region(s.wl_compositor);
	wl_region_add(region, 0, 0, _gmtry.width, _gmtry.height);
	wl_surface_set_opaque_region(_wl_surface, region);
	wl_region_destroy(region);
*/


	wl_surface_set_opaque_region(_wl_surface, NULL);


	if (s.swap_buffers_with_damage && buffer_age > 0)
	{
		rect[0] = _gmtry.width / 4 - 1;
		rect[1] = _gmtry.height / 4 - 1;
		rect[2] = _gmtry.width / 2 + 2;
		rect[3] = _gmtry.height / 2 + 2;
		s.swap_buffers_with_damage(s.egl_display, _eglsurf.egl_surface, rect, 1);
	} else {
		eglSwapBuffers(s.egl_display, _eglsurf.egl_surface);
	}

	ctx->release();
}

void window_surface::handle_toplevel_configure(xdg_toplevel *toplevel,
	int32_t width, int32_t height, wl_array *states)
{
	// set as window initialized
	_f.initialized = 1;

	uint32_t *p;
	int32_t fullscreen = 0;
	int32_t maximized = 0;

	if (_f.setpos_pending)
	{
		if (_f.pos_valid) {
			xdg_toplevel_set_position(_xdg_toplevel, _pos.x, _pos.y,
				_gmtry.width, _gmtry.height);
		}
		else if (_gmtry.valid()) {
			xdg_toplevel_set_size(_xdg_toplevel,
				_gmtry.width, _gmtry.height);
		}
		_f.setpos_pending = 0;
	}

	for (uint32_t* p = (uint32_t*)states->data;
		(char*) p < ((char*)
		states->data + (states)->size); p++) {
		uint32_t state = *p;
		switch (state) {
		case XDG_TOPLEVEL_STATE_FULLSCREEN:
			fullscreen = 1;
			break;
		case XDG_TOPLEVEL_STATE_MAXIMIZED:
			maximized = 1;
			break;
		}
	}

	if (fullscreen != _f.fullscreen) {
		// todo: notification
		_f.fullscreen = fullscreen;
	}
	if (maximized != _f.maximized) {
		// todo: notification
		_f.maximized = maximized;
	}

	// check size
	if (width > 0 && height > 0) {
		assert(NULL != _eglsurf.native);
		wl_egl_window_resize(_eglsurf.native,
			_gmtry.width, _gmtry.height, 0, 0);
	}
}

class window_surface_attach_task : public surface_task
{
public:
	window_surface_attach_task(window_surface* surf,
		waitobject* wobj, bool brel)
	: _surface(surf)
	, _wobj(wobj)
	, _brel(brel) {}

	void do_task(void)
	{
		int ret = _surface->attach_surface();
		// todo:
		assert(ret == 0);
		if (_wobj) {
			_wobj->lock();
			_wobj->notify();
			_wobj->unlock();
		}
		if (_brel) delete this;
	}

private:
	window_surface* _surface;
	waitobject* _wobj;
	bool _brel;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(window_surface_attach_task);
};

int window_surface::show(bool sync)
{
	int ret;
	wayland_client* wcl = wayland_client::inst();
	if (!wcl->ready()) return -ENOTREADY;

	if (_f.showed) return 0;

	if (sync) {
		waitobject wobj;
		window_surface_attach_task tsk(this, &wobj, false);

		wobj.lock();
		ret = surfacemgr_impl::inst()->submit_task(&tsk);
		if (ret) {
			wobj.unlock();
			return ret;
		}
		bool wret = wobj.wait(1000);
		wobj.unlock();
		if (!wret) return -ETIMEOUT;
	}
	else {
		wayland_client* wcl = wayland_client::inst();
		if (!wcl->ready()) return -ENOTREADY;

		window_surface_attach_task* tsk = new
			window_surface_attach_task(this, NULL, true);
		assert(NULL != tsk);
		ret = surfacemgr_impl::inst()->submit_task(tsk);
		if (ret) return ret;
	}
	return 0;
}

class window_surface_close_task : public surface_task
{
public:
	window_surface_close_task(window_surface* surf,
		waitobject* wobj, bool brel)
	: _surface(surf)
	, _wobj(wobj)
	, _brel(brel) {}

	void do_task(void)
	{
		_surface->destroy_surface();
		_surface->_f.showed = false;
		if (_wobj) {
			_wobj->lock();
			_wobj->notify();
			_wobj->unlock();
		}
		if (_brel) delete this;
	}

private:
	window_surface* _surface;
	waitobject* _wobj;
	bool _brel;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(window_surface_close_task);
};

int window_surface::close(bool sync)
{
	int ret;
	wayland_client* wcl = wayland_client::inst();
	if (!wcl->ready()) return -ENOTREADY;

	if (!_f.showed) return 0;

	if (sync) {
		waitobject wobj;
		window_surface_close_task tsk(this, &wobj, false);

		wobj.lock();
		ret = surfacemgr_impl::inst()->submit_task(&tsk);
		if (ret) {
			wobj.unlock();
			return ret;
		}
		bool wret = wobj.wait(1000);
		wobj.unlock();
		if (!wret) return -ETIMEOUT;
	}
	else {
		window_surface_close_task* tsk = new
			window_surface_close_task(this, NULL, true);
		assert(NULL != tsk);
		ret = surfacemgr_impl::inst()->submit_task(tsk);
		if (ret) return ret;
	}
	return 0;
}

class window_surface_setpos_task : public surface_task
{
public:
	window_surface_setpos_task(xdg_toplevel* xdg,
		iposition_t* pos, igeometry_t* gm,
		bool bsetpos)
	: _xdg_toplevel(xdg)
	, _pos(*pos), _gmtry(*gm)
	, _setpos(bsetpos) {}

	void do_task(void)
	{
		if (_setpos) {
			xdg_toplevel_set_position(_xdg_toplevel,
				_pos.x, _pos.y, _gmtry.width, _gmtry.height);
		}
		else {
			xdg_toplevel_set_size(_xdg_toplevel,
				_gmtry.width, _gmtry.height);
		}
		delete this;
	}

private:
	xdg_toplevel* _xdg_toplevel;
	iposition_t _pos;
	igeometry_t _gmtry;
	bool _setpos;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(window_surface_setpos_task);
};

void window_surface::set_size(igeometry_t g)
{
	if (!g.valid() || _gmtry == g) return;

	// set the size
	_gmtry = g;
	if (_f.initialized) {
		window_surface_setpos_task* tsk = new
			window_surface_setpos_task(_xdg_toplevel,
			&_pos, &_gmtry, true);
		assert(NULL != tsk);
		surfacemgr_impl::inst()->submit_task(tsk);
	}
	else _f.setpos_pending = 1;
}

void window_surface::set_position(iposition_t p, igeometry_t g)
{
	_pos = p;
	_f.pos_valid = 1;
	if (_gmtry == g) return;

	// set the pos and size
	_gmtry = g;
	if (_f.initialized) {
		window_surface_setpos_task* tsk = new
			window_surface_setpos_task(_xdg_toplevel,
			&_pos, &_gmtry, false);
		assert(NULL != tsk);
		surfacemgr_impl::inst()->submit_task(tsk);
	}
	else _f.setpos_pending = 1;
}

int window::show(bool sync)
{
	window_surface* ws = reinterpret_cast<window_surface*>(this);
	return ws->show(sync);
}

int window::close(bool sync)
{
	window_surface* ws = reinterpret_cast<window_surface*>(this);
	return ws->close(sync);
}

void window::set_size(igeometry_t g)
{
	window_surface* ws = reinterpret_cast<window_surface*>(this);
	ws->set_size(g);
}

void window::set_position(iposition_t p, igeometry_t g)
{
	window_surface* ws = reinterpret_cast<window_surface*>(this);
	ws->set_position(p, g);
}

window* create_window(window* parents)
{
	window_surface* ws = reinterpret_cast<window_surface*>
		(surfacemgr_impl::inst()->create_surface(
			surfaceid_window_surface));
	if (NULL == ws) return NULL;

	if (parents) {
		window_surface* p = reinterpret_cast<window_surface*>(parents);
		p->addchild(ws);
	}
	return reinterpret_cast<window*>(ws);
}

}} // end of namespace zas::uicore
/* EOF */
