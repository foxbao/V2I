/** @file wl-window.h
 * Definition of the window serface implementation
 */

#ifndef __CXX_ZAS_UICORE_WAYLAND_WINDOW_H__
#define __CXX_ZAS_UICORE_WAYLAND_WINDOW_H__

#include <string>
#include "inc/wl-surface.h"

namespace zas {
namespace uicore {

class window_surface : public surface_impl
{
	friend class window_surface_attach_task;
	friend class window_surface_close_task;
public:
	window_surface();
	~window_surface();

public:
	int show(bool sync);
	int close(bool sync);

	void set_size(igeometry_t g);
	void set_position(iposition_t p, igeometry_t g);

public:
	void addchild(window_surface* child) {
		listnode_add(_children, child->_sibling);
		child->_parents = this;
	}

public:
	// handler
	void handle_toplevel_configure(xdg_toplevel *toplevel,
		int32_t width, int32_t height, wl_array *states);
	void handle_xdg_surface_configure(xdg_surface *xdg_surface,
		uint32_t serial);

private:

	// attach the wayland context
	int attach_surface(void);
	int attach_xdg_toplevel(void);

	// desotry the surface
	void destroy_surface(void);

	void handle_show_request(void);

private:
	listnode_t _sibling;
	listnode_t _children;
	window_surface* _parents;

	xdg_surface *_xdg_surface;
	xdg_toplevel *_xdg_toplevel;

	std::string _title;
	iposition_t _pos;

	union {
		struct {
			uint32_t initialized : 1;
			uint32_t showed : 1;
			uint32_t fullscreen : 1;
			uint32_t maximized : 1;
			uint32_t minimized : 1;
			uint32_t pos_valid : 1;
			uint32_t setpos_pending : 1;
		} _f;
		uint32_t _flags;
	};

	ZAS_DISABLE_EVIL_CONSTRUCTOR(window_surface);
};

}} // end of namespace zas::uicore
#endif // __CXX_ZAS_UICORE_WAYLAND_WINDOW_H__
/* EOF */
