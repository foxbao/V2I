/** @file view.h
 * Definition of the ui view, which is the essential object of any UI objects
 */

#ifndef __CXX_ZAS_UICORE_UIVIEW_H__
#define __CXX_ZAS_UICORE_UIVIEW_H__

#include "uicore/uicore.h"

namespace zas {
namespace uicore {

template <typename T> struct rect_;
typedef rect_<int32_t> irect_t;
typedef rect_<float> rect_t;

class UICORE_EXPORT view_t
{
public:

	/**
	  Create a view

	  @param parents the parents for the new created view, NULL means no parents
	  @param name represents the name of the view, NULL or "" mean no name
	  @param isgrp represents if the view is a group
	  @return true means the view is visilbe while false means invisible
	  */
	static view_t* create(view_t* parents = NULL, const char* name = NULL,
		bool isgrp = false);

	/**
	  Create a view

	  @param parents the parents for the new created view, NULL means no parents
	  @param r set the rect as position and size of the view
	  @param visible set if the view is visible or not
	  @param name represents the name of the view, NULL or "" mean no name
	  @param isgrp represents if the view is a group
	  @return true means the view is visilbe while false means invisible
	  */
	static view_t* create(view_t* parents, const rect_t& r,
		bool visible = true, const char* name = NULL,
		bool isgrp = false);

	/**
	  release the view and all its sub views
	  */
	void release(void);

	/**
	  Set the position of the view

	  @param left left of the view to be set
	  @param top top of the view to be set
	  @param width width of the view to be set
	  note: border, margin, padding is not included
	  @param height height of the view to be set
	  note: border, margin, padding is not included
	  
	  @return the pointer of the view
	  Note: if width or height is less than 0, both width and height
	  will not be set (means size not changed)
	  */
	view_t* setpos(float left, float top, float width = -1., float height = -1.);

	/**
	  Set the name of the current view

	  @param n specify the name of the view, NULL and "" will clear the name
	  @return 0 for success or an error happen
	    -EEXISTS: means a view with same name exists
	  */
	int set_name(const char* name);

	/**
	  Get the name of current view

	  @param name: the buffer to room the name
	  @param sz: indicate the size of name buffer
	  @return 0 means there is no name for this view
	          > 0: means the size of the nmae for this view
			  < 0: means the buffer size is not enough or buffer is NULL
			  the abs(return val) is the size for the buffer
			  to room the name of this view
	  */
	int get_name(char* name, size_t sz);

	/**
	  Move the view to specific position

	  @param left left pos to be moved to
	  @param top top pos to be moved to
	  @return the pointer of the view
	  */
	inline view_t* move_to(float left, float top) {
		return setpos(left, top);
	}

	/**
	  Get the client area size

	  @param r the rect to hold the client size
	  */
	void get_clientrect(rect_t& r);

	/**
	  Get the wrap rect size
	  wrap rect means the clieat area + margin

	  @param r the rect to hold the client size
	  */
	void get_wraprect(rect_t& r);
};

}}; // end of namespace zas::uicore
#endif /* __CXX_ZAS_UICORE_UIVIEW_H__ */
/* EOF */
