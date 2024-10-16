/** @file wl-utils.h
 * Definition of wayland utilities. it is the essential part of wayland applications
 */

#ifndef __CXX_ZAS_UICORE_WL_UTILS_H__
#define __CXX_ZAS_UICORE_WL_UTILS_H__

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "wayland-egl.h"

#include "utils/thread.h"
#include "utils/timer.h"

#include "uicore/uicore.h"
#include "wayland-client.h"
#include "wl-protocol/xdg-shell-client-protocol.h"

namespace zas {
namespace uicore {

using namespace zas::utils;

class surface_impl;

class wcl_thread : public thread
{
public:
	wcl_thread() {}
	int run(void);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(wcl_thread);
};

// wayland client global state
struct wcl_state
{
	struct wl_display *wl_display;
	struct wl_registry *wl_registry;
	struct wl_compositor *wl_compositor;
	struct xdg_wm_base *xdg_wm_base;

	EGLDisplay egl_display;
	EGLConfig egl_conf;
	EGLContext egl_resource;
	EGLContext egl_context;
	PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC swap_buffers_with_damage;
};

struct wcl_eglsurface
{
	wl_egl_window* native;
	EGLSurface egl_surface;
};

struct wcl_eventfd
{
	int surface;
};

enum wcl_evfd_e
{
	wcl_evfd_unknown = -1,

	// used for surface operations
	wcl_evfd_surface_ops,
};

typedef void (*eventfd_handler)(uint64_t count);

class wayland_client
{
public:
	wayland_client();
	~wayland_client();
	static wayland_client* inst(void);
	
public:

	/**
	  Check if the wayland client has been initialized.
	  if not, try initialize the client
	  @return true for initialized or false for fail to initialize
	  */
	bool initialize(void);

	// check if we have initialized
	bool initialized(void) {
		wcl_state& state = _wayland_state;
		return (state.wl_display && state.xdg_wm_base)
			? true : false;
	}

	bool is_wlthread(void) {
		return _thd.is_myself();
	}

	int notify_event(wcl_evfd_e ev);

	// create and assign the egl context for
	// specific surface
	int assign_eglcontext(surface_impl* surf);

	// detach the egl context for specific surface
	int detach_eglcontext(surface_impl* surf);

	// run procedure in wayland thread
	int run(void);

	// start running the wayland client
	void start(void) {
		_thd.start();
	}

	// exit running the wayland client
	void exit(void) {
		display_exit();
	}

	// check if the wayland client is ready
	// to handle requests
	bool ready(void) {
		return (_f.ready) ? true : false;
	}

	const wcl_state& get_wclstate(void) {
		return _wayland_state;
	}

	timermgr* get_wcltimermgr(void) {
		return _tmrmgr;
	}

private:

	// handle the display fd in event loop
	void handle_display_data(uint32_t events);

	// destroy the wayland client
	void destroy(void);

	// initialize the display egl
	void init_egl(void);

	// init and destroy the event fds
	void init_evfd(void);
	void destroy_evfd(void);

	void epoll_init(void);

	// handle the eventfd in epoll loop
	bool handle_eventfd(int fd);

	// handle the timer events
	void handle_timer(void);

private:

	void display_start(void) {
		_f.running = 1;
	}

	void display_exit(void) {
		_f.running = 0;
	}

	bool is_started(void) {
		return _f.running ? true : false;
	}

private:	// for handler

	// handle surface operations
	static void eventfd_handle_surface(uint64_t count);

private:

	wcl_state _wayland_state;
	wcl_eventfd _evfd;

	int _epollfd;
	int _dispfd;
	wcl_thread _thd;
	timermgr* _tmrmgr;

	union {
		struct {
			uint32_t running : 1;
			uint32_t ready : 1;
		} _f;
		uint32_t _flags;
	};
	
	static eventfd_handler _evhdr_tbl[];
	ZAS_DISABLE_EVIL_CONSTRUCTOR(wayland_client);
};

}} // end of namespace zas::uicore
#endif /* __CXX_ZAS_UICORE_WL_UTILS_H__ */
/* EOF */
