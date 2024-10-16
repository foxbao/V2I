/** @file wl-utils.cpp
 * Implementation of wayland utilities. it is the essential part of wayland applications
 */

#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>

#include <GLES2/gl2.h>

#include "hwgrph/hwgrph.h"

#include "inc/wl-utils.h"
#include "inc/wl-surface.h"

namespace zas {
namespace uicore {

static int epoll_fdctrl(int epollfd, int fd, int op, uint32_t events)
{
	epoll_event ep;
	ep.events = events;
	ep.data.fd = fd;
	return epoll_ctl(epollfd, op, fd, &ep);
}

wayland_client::wayland_client()
: _epollfd(-1)
, _dispfd(-1)
, _flags(0)
, _tmrmgr(NULL)
{
	init_evfd();
	memset(&_wayland_state, 0, sizeof(wcl_state));

	// create the timer manager
	_tmrmgr = create_timermgr(20);
	assert(NULL != _tmrmgr);

	_epollfd = epoll_create1(EPOLL_CLOEXEC);
	assert(_epollfd >= 0);

	int tmrfd = _tmrmgr->getfd();
	assert(tmrfd != -1);
	int ret = epoll_fdctrl(_epollfd, tmrfd, EPOLL_CTL_ADD,
		EPOLLET | EPOLLIN | EPOLLOUT
		| EPOLLERR | EPOLLHUP | EPOLLRDHUP);
	assert(!ret);

	// set the wayland client thread as detached
	_thd.setattr(thread_attr_detached);
}

wayland_client::~wayland_client()
{
	destroy_evfd();
	if (_epollfd >= 0) {
		close(_epollfd);
		_epollfd = -1;
	}
	destroy();
}

void wayland_client::init_evfd(void)
{
	int* buf = reinterpret_cast<int*>(&_evfd);
	int* end = buf + sizeof(wcl_eventfd) / sizeof(int);
	for (; buf != end; *buf++ = -1);
}

void wayland_client::destroy_evfd(void)
{
	int* buf = reinterpret_cast<int*>(&_evfd);
	int* end = buf + sizeof(wcl_eventfd) / sizeof(int);
	for (; buf != end; ++buf) {
		// todo: release from epollfd
		close(*buf);
	}
}

int wayland_client::notify_event(wcl_evfd_e ev)
{
	if (ev < 0) return -1;
	if (int(ev) >= sizeof(wcl_eventfd) / sizeof(int))
		return -2;
	int fd = reinterpret_cast<int*>(&_evfd)[ev];
	if (fd < 0) return -3;

	uint64_t val = 1;
	for (;;) {
		int ret = write(fd, &val, sizeof(uint64_t));
		if (ret <= 0) {
			if (errno == EINTR)
				continue;
			else return -4;
		}
		assert(ret == 8);
		break;
	}
	return 0;
}

void wayland_client::destroy(void)
{
	wcl_state& s = _wayland_state;
	if (s.egl_context) {
		eglDestroyContext(s.egl_display, s.egl_context);
		s.egl_context = EGL_NO_CONTEXT;
	}
	if (s.egl_display) {
		eglTerminate(s.egl_display);
		eglReleaseThread();
	}

	if (s.wl_registry) {
		wl_registry_destroy(s.wl_registry);
		s.wl_registry = NULL;
	}

	if (s.wl_display) {
		wl_display_flush(s.wl_display);
		wl_display_disconnect(s.wl_display);
		s.wl_display = NULL;
	}
}

static void xdg_wm_base_ping(void *data, xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void registry_global(void *data, wl_registry *wl_registry,
	uint32_t name, const char *interface, uint32_t version)
{
	wcl_state *state = reinterpret_cast<wcl_state*>(data);
	if (!strcmp(interface, wl_compositor_interface.name)) {
		state->wl_compositor = reinterpret_cast<wl_compositor*>
			(wl_registry_bind(wl_registry,
				name, &wl_compositor_interface, 4));
		assert(NULL != state->wl_compositor);
	}
	else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		state->xdg_wm_base = reinterpret_cast<xdg_wm_base*>
			(wl_registry_bind(
				wl_registry, name, &xdg_wm_base_interface, 1));
		assert(NULL != state->xdg_wm_base);

		xdg_wm_base_add_listener(state->xdg_wm_base,
			&xdg_wm_base_listener, state);
    }
}

static void registry_global_remove(void *data,
	wl_registry *wl_registry, uint32_t name)
{
    /* This space deliberately left blank */
}

static const wl_registry_listener wl_registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

bool wayland_client::initialize(void)
{
	// indicate we are start running
	_f.running = 1;

	wcl_state& state = _wayland_state;
	if (state.wl_display && state.xdg_wm_base)
		return true;

	state.wl_display = wl_display_connect(NULL);
	assert(NULL != state.wl_display);

	_dispfd = wl_display_get_fd(state.wl_display);
	assert(_dispfd >= 0);

	state.wl_registry = wl_display_get_registry(state.wl_display);
	assert(NULL != state.wl_registry);

	wl_registry_add_listener(state.wl_registry,
		&wl_registry_listener, &state);
	wl_display_roundtrip(state.wl_display);

	init_egl();
	return true;
}

static bool wl_check_egl_extension(const char *extensions, const char *extension)
{
	size_t extlen = strlen(extension);
	const char *end = extensions + strlen(extensions);

	while (extensions < end) {
		size_t n = 0;

		/* Skip whitespaces, if any */
		if (*extensions == ' ') {
			extensions++;
			continue;
		}

		n = strcspn(extensions, " ");

		/* Compare strings */
		if (n == extlen && strncmp(extension, extensions, n) == 0)
			return true; /* Found */

		extensions += n;
	}
	/* Not found */
	return false;
}

static inline void * wl_platform_get_egl_proc_address(const char *address)
{
	const char *extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

	if (extensions &&
	    (wl_check_egl_extension(extensions, "EGL_EXT_platform_wayland") ||
	     wl_check_egl_extension(extensions, "EGL_KHR_platform_wayland"))) {
		return (void *) eglGetProcAddress(address);
	}
	return NULL;
}

static inline EGLSurface wl_platform_create_egl_surface(EGLDisplay dpy,
	EGLConfig config, void *native_window,
	const EGLint *attrib_list)
{
	static PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC
		create_platform_window = NULL;

	if (!create_platform_window) {
		create_platform_window = (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)
			wl_platform_get_egl_proc_address("eglCreatePlatformWindowSurfaceEXT");
	}

	if (create_platform_window)
		return create_platform_window(dpy, config,
			native_window, attrib_list);

	return eglCreateWindowSurface(dpy, config,
		(EGLNativeWindowType) native_window, attrib_list);
}

static inline EGLDisplay wl_get_egl_display(EGLenum platform,
	void *native_display, const EGLint *attrib_list)
{
	static PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display = NULL;

	if (!get_platform_display) {
		get_platform_display = (PFNEGLGETPLATFORMDISPLAYEXTPROC)
            wl_platform_get_egl_proc_address("eglGetPlatformDisplayEXT");
	}

	if (get_platform_display)
		return get_platform_display(platform,
					    native_display, attrib_list);

	return eglGetDisplay((EGLNativeDisplayType) native_display);
}

void wayland_client::init_egl(void)
{
	static const struct {
		const char *extension, *entrypoint;
	} swap_damage_ext_to_entrypoint[] = {
		{
			.extension = "EGL_EXT_swap_buffers_with_damage",
			.entrypoint = "eglSwapBuffersWithDamageEXT",
		},
		{
			.extension = "EGL_KHR_swap_buffers_with_damage",
			.entrypoint = "eglSwapBuffersWithDamageKHR",
		},
	};

	const char *extensions;

	EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_SAMPLES, 4,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	EGLint major, minor, n, count, i, size;
	EGLConfig *configs;
	EGLBoolean ret;
	wcl_state& state = _wayland_state;

//	if (window->opaque || window->buffer_size == 16)
//		config_attribs[9] = 0;

	state.egl_display = wl_get_egl_display(EGL_PLATFORM_WAYLAND_KHR,
		state.wl_display, NULL);
	assert(NULL != state.egl_display);

	ret = eglInitialize(state.egl_display, &major, &minor);
	assert(ret == EGL_TRUE);
	ret = eglBindAPI(EGL_OPENGL_ES_API);
	assert(ret == EGL_TRUE);

	if (!eglGetConfigs(state.egl_display, NULL, 0, &count)
		|| count < 1) assert(0);

	configs = (EGLConfig*)calloc(count, sizeof *configs);
	assert(configs);

	ret = eglChooseConfig(state.egl_display, config_attribs,
			      configs, count, &n);
	assert(ret && n >= 1);

	EGLConfig conf = NULL;
	for (i = 0; i < n; i++) {
		eglGetConfigAttrib(state.egl_display,
			configs[i], EGL_BUFFER_SIZE, &size);
		if (ZAS_BPP == size) {
			conf = configs[i]; break;
		}
	}
	free(configs);
	assert(conf != NULL);
	state.egl_conf = conf;

	state.swap_buffers_with_damage = NULL;
	extensions = eglQueryString(state.egl_display, EGL_EXTENSIONS);
	if (extensions && wl_check_egl_extension(extensions, "EGL_EXT_buffer_age"))
	{
		for (i = 0; i < (int) ARRAY_LENGTH(swap_damage_ext_to_entrypoint); i++)
		{
			if (wl_check_egl_extension(extensions,
				swap_damage_ext_to_entrypoint[i].extension)) {
				// The EXTPROC is identical to the KHR one
				state.swap_buffers_with_damage =
					(PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC)
					eglGetProcAddress(swap_damage_ext_to_entrypoint[i].entrypoint);
				break;
			}
		}
	}
}

int wayland_client::assign_eglcontext(surface_impl* surf)
{
	if (NULL == surf) return -1;
	if (surf->_gmtry.width <= 0 || surf->_gmtry.height <= 0)
		return -2;

	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	wcl_state& s = _wayland_state;

	// create the egl context
	if (NULL == s.egl_context) {
		s.egl_context = eglCreateContext(s.egl_display,
			s.egl_conf, s.egl_resource, context_attribs);
		assert(s.egl_context);
	}

	surf->_eglsurf.native = wl_egl_window_create(surf->_wl_surface,
		surf->_gmtry.width, surf->_gmtry.height);

	surf->_eglsurf.egl_surface = wl_platform_create_egl_surface(s.egl_display,
		s.egl_conf, surf->_eglsurf.native, NULL);

	EGLBoolean ret = eglMakeCurrent(s.egl_display, surf->_eglsurf.egl_surface,
		surf->_eglsurf.egl_surface, s.egl_context);
	assert(EGL_TRUE == ret);

	s.egl_resource = zas::hwgrph::bind_resource(s.egl_display,
		s.egl_conf, s.egl_context);
	assert(EGL_NO_CONTEXT != s.egl_resource);
	return 0;
}

int wayland_client::detach_eglcontext(surface_impl* surf)
{
	wcl_state& s = _wayland_state;
	assert(NULL != s.egl_display);
	eglMakeCurrent(s.egl_display, EGL_NO_SURFACE,
		EGL_NO_SURFACE, EGL_NO_CONTEXT);

	if (surf->_eglsurf.egl_surface) {
		eglDestroySurface(s.egl_display, surf->_eglsurf.egl_surface);
		surf->_eglsurf.egl_surface = NULL;
	}
	if (!surf->_eglsurf.native) {
		wl_egl_window_destroy(surf->_eglsurf.native);
		surf->_eglsurf.native = NULL;
	}
	return 0;
}

eventfd_handler wayland_client::_evhdr_tbl[] = {
	wayland_client::eventfd_handle_surface,
};

bool wayland_client::handle_eventfd(int fd)
{
	int i, cnt = sizeof(wcl_eventfd) / sizeof(int),
		*buf = reinterpret_cast<int*>(&_evfd);
	for (i = 0; i < cnt && buf[i] != fd; ++i);

	// not an eventfd, return
	if (i == cnt) return false;

	// drain the eventfd
	uint64_t val = 0;
	for (;;) {
		int ret = read(fd, &val, sizeof(uint64_t));
		if (ret <= 0 && errno == EINTR)
			continue;
		assert(ret == 8);
		break;
	}

	_evhdr_tbl[i](val);	// handle the eventfd
	return true;
}

void wayland_client::eventfd_handle_surface(uint64_t) {
	surfacemgr_impl::inst()->dispatch_pending_tasks();
}

void wayland_client::epoll_init(void)
{
	int ret;
	assert(_epollfd >= 0 && _dispfd >= 0);

	ret = epoll_fdctrl(_epollfd, _dispfd, EPOLL_CTL_ADD,
		EPOLLIN | EPOLLERR | EPOLLHUP);
	assert(!ret);

	// create the eventfd for surface
	_evfd.surface = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	assert(_evfd.surface >= 0);
	ret = epoll_fdctrl(_epollfd, _evfd.surface, EPOLL_CTL_ADD,
		EPOLLET | EPOLLIN | EPOLLERR | EPOLLHUP);
	assert(!ret);
}

#define EPOLL_MAX_EVENTS	16

using namespace zas::utils;

class wayland_timer : public timer
{
public:
	wayland_timer(timermgr* mgr, uint32_t interval)
	: timer(mgr, interval) {
	}

	void on_timer(void) {
		printf("wayland_timer\n");
		start();
	}
};

int wayland_client::run(void)
{
	int ret;
	wcl_state& s = _wayland_state;
	epoll_event ep[EPOLL_MAX_EVENTS];

	// add all necessary fds to epoll
	epoll_init();

	// indicate we are ready
	_f.ready = 1;

	wayland_timer* wtmr = new wayland_timer(_tmrmgr, 1000);
	wtmr->start();

	int tmrfd = _tmrmgr->getfd();
	for (display_start() ;;)
	{
		// todo: do any task
		wl_display_dispatch_pending(s.wl_display);

		if (!is_started())
			break;

		ret = wl_display_flush(s.wl_display);
		if (ret < 0 && errno == EAGAIN) {
			ret = epoll_fdctrl(_epollfd, _dispfd,
				EPOLL_CTL_MOD, 
				EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP);
			assert(!ret);
		}
		else if (ret < 0) {
			destroy(); return -2;
		}

		int count = epoll_wait(_epollfd, ep, EPOLL_MAX_EVENTS, -1);
		for (int i = 0; i < count; i++) {
			int fd = ep[i].data.fd;
			if (fd == _dispfd) {
				handle_display_data(ep[i].events);
			}
			else if (fd == tmrfd) {
				handle_timer();
			}
			else handle_eventfd(fd);
		}
	}
	destroy();
	return 0;
}

void wayland_client::handle_display_data(uint32_t events)
{
	int ret;
	epoll_event ep;
	wcl_state& s = _wayland_state;

	if (events & EPOLLERR || events & EPOLLHUP) {
		display_exit();
		return;
	}

	if (events & EPOLLIN) {
		ret = wl_display_dispatch(s.wl_display);
		if (ret == -1) {
			display_exit();
			return;
		}
	}

	if (events & EPOLLOUT) {
		ret = wl_display_flush(s.wl_display);
		if (ret == 0) {
			ret = epoll_fdctrl(_epollfd, _dispfd,
				EPOLL_CTL_MOD,
				EPOLLIN | EPOLLERR | EPOLLHUP);
			assert(!ret);
		}
		else if (ret == -1 && errno != EAGAIN) {
			display_exit();
			return;
		}
	}
}

void wayland_client::handle_timer(void)
{
	// drain the eventfd
	uint64_t val = 0;
	int fd = _tmrmgr->getfd();

	for (;;) {
		int ret = read(fd, &val, sizeof(uint64_t));
		if (ret <= 0 && errno == EINTR)
			continue;
		assert(ret == 8);
		break;
	}
	_tmrmgr->periodic_runner();
}

int wcl_thread::run(void)
{
	wayland_client* wcl = wayland_client::inst();
	if (!wcl || !wcl->initialize()) {
		return -1;
	}
	return wcl->run();
}

wayland_client* wayland_client::inst(void)
{
	static wayland_client* _inst = NULL;
	// todo: need lock
	if (NULL == _inst) {
		_inst = new wayland_client();
		assert(NULL != _inst);
	}
	return _inst;
}

// todo: need removed
void UICORE_EXPORT wayland_client_start(void)
{
	wayland_client::inst()->start();
}


}} // end of namespace zas::uicore
/* EOF */
