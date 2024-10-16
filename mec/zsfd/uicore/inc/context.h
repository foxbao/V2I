/** @file context.h
 * Definition of global context object for the uicore
 */

#ifndef __CXX_ZAS_UICORE_CONTEXT_H__
#define __CXX_ZAS_UICORE_CONTEXT_H__

#include "uicore.h"
#include "std/list.h"

#include "utils/mutex.h"
#include "utils/mcache.h"

namespace zas {
namespace uicore {

using namespace zas::utils;

struct view_impl;
struct generic_info_t;

struct view_context_t
{
	listnode_t ownerlist;
	view_impl* view;
	generic_info_t* ginfo;
};

struct display_metrics
{
	float density;
};

// to directly expose the structure details to others
// is trying to help inline and improve efficiency
struct context_t
{
	// flags
	union {
		struct {
		} flags;
		uint32_t _f;
	};

	display_metrics metrics;

	// indicate that it is in drawing
	// be aware about the difference between
	// "drawing" and "rendering"
	uint32_t draw_refcnt;

	// the pending list for all drawing views
	listnode_t draw_pendings;

	// memcache
	// for view allocation
	memcache* view_mch;

	// for view global info allocation
	memcache ginfo_mch;

	// for view context allocation
	memcache viewctx_mch;
	
	// mutex
	// view operatoins
	mutex view_mut;

	// view context operations
	mutex view_ctx_mut;

	// used to lock the render
	mutex render_mut;

	context_t();
};

extern context_t* gblctx;

/**
  Add a generic info as pending, a view_context_t object
  will be created and appended to draw_pending list

  if this is used to record the operation of creating a new view,
  just leave the parameter ginfo as NULL

  @param nd the view with the generic info
  @param ginfo the generic info
  @return the view_context_t
  */
view_context_t* gblctx_add_pending_ginfo(view_impl* nd, generic_info_t* ginfo = NULL);

/**
  remove the pending ginfo
  @param ctx the pending ginfo context
  */
void gblctx_remove_pending_ginfo(view_context_t* ctx);

/**
  merge the view changes to the dirty rect

  @param r the rect for merging changes
  @return 0 for success or error happened
  */
int gblctx_merge_view_changes(irect_t& r);

}} // end of namespace zas::uicore
#endif /* __CXX_ZAS_UICORE_CONTEXT_H__ */
/* EOF */
