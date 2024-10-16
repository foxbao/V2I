/** @file view-impl.h
 * Definition of view_impl, which is the essential object of any UI objects
 */

#ifndef __CXX_ZAS_UICORE_VIEW_IMPL_H__
#define __CXX_ZAS_UICORE_VIEW_IMPL_H__

#include "uicore.h"
#include "std/list.h"
#include "utils/avltree.h"

#include "uicore/view.h"
#include "uicore/region.h"

namespace zas {
namespace uicore {

struct view_impl;
struct view_context_t;

// box model info
struct boxmdl_prop_t
{
	float left;
	float right;
	float top;
	float bottom;

	void reset(void) {
		left = 0., right = 0.;
		top = 0., bottom = 0.;
	}
};

struct boxmdl_info_t
{
	boxmdl_prop_t padding;
	boxmdl_prop_t border;
	boxmdl_prop_t margin;

	void reset(void) {
		padding.reset();
		border.reset();
		margin.reset();
	}
};

struct generic_info_t
{
	// density independent pixel rect
	rect_t dpr;

	// margin property
	boxmdl_prop_t margin;

	// pointer to view_context_t
	view_context_t* ctx;

	union {
		struct {
			// indicate the current view is visible
			uint32_t visible : 1;
		} flags;
		uint32_t _f;
	};
	void* extend_info;

	void reset(void) {
		margin.reset();
		ctx = NULL;
		_f = 0;
		extend_info = NULL;
	}

	void get_clientrect(rect_t& r) {
		r = dpr;
	}

	void get_wraprect(rect_t& r) {
		r = dpr;
		r.left -= margin.left;
		r.top -= margin.top;
		r.width += margin.left + margin.right;
		r.height += margin.right + margin.bottom;
	}

	// move the view to the position
	void setpos(view_impl* nd, const rect_t& r) {
		dpr.left = r.left, dpr.top = r.top;
		if (r.width >= 0.) dpr.width = r.width;
		if (r.height >= 0.) dpr.height = r.height;
	}

	// relatively move the view by dx, dy
	void move(float dx, float dy) {
		dpr.left += dx, dpr.top += dy;
	}

	void setvisible(bool v) {
		flags.visible = (v) ? 1 : 0;
	}
};

struct view_impl
{
	view_impl* parents;
	listnode_t children;
	listnode_t siblings;

	union {
		size_t id;
		const char name_str[sizeof(void*)];
		const char* name_ptr;
	};

	union {
		struct {
			// indicate if the current view is a group
			uint32_t as_group : 1;

			// indicate if we need to resize the siblings
			// if the size of view changed
			uint32_t need_resize : 1;

			// indicate if the current view has an embedded name
			uint32_t use_embedded_name : 1;

			// indicate the current view use id
			uint32_t useid : 1;

			// indicate the name reference shall not be released
			uint32_t name_norel : 1;

			// indicate which ginfo is the "previously submitted" one
			// we call it "read only"
			uint32_t readonly_ginfo_pos : 2;

			// indicate if the object is ready
			uint32_t ready : 1;

			// indicate if the object is in deleting
			uint32_t deleting : 1;
		} flags;
		uint32_t _f;
	};

	// one is "previously submiited"
	// another is "submitted this time"
	// there two items will be used to do comparison
	// to find out the difference that may help to 
	// create dirty rect and optimize rendering
	// the third items is "currently modifying" one
	generic_info_t* ginfo[3];
};

static inline void view_resetflag(view_impl* nd) {
	nd->_f = 0;
}

// no lock, carefully check
static inline void set_readonly_ginfo(view_impl* nd, generic_info_t* ginfo) {
	nd->ginfo[nd->flags.readonly_ginfo_pos] = ginfo;
}

static inline void view_addchild(view_impl* parents, view_impl* nd) {
	listnode_add(parents->children, nd->siblings);
	nd->parents = parents;
}

// the destructor for memcache
void view_dtor(view_impl* nd);

// get the client rect of the ginfo
void ginfo_get_clientrect(generic_info_t* ginfo, irect_t& r);

// get the wrap rect of the ginfo
void ginfo_get_wraprect(generic_info_t* ginfo, rect_t& r);
void ginfo_get_wraprect(generic_info_t* ginfo, irect_t& r);

}}; // end of namespace zas::uicore */
#endif /* __CXX_ZAS_UICORE_VIEW_IMPL_H__ */
/* EOF */
