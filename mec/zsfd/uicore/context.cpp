/** @file context.cpp
 * implementation of global context operations
 */

#include <math.h>
#include "uicore/region.h"

#include "inc/region-impl.h"
#include "inc/context.h"
#include "inc/view-impl.h"

namespace zas {
namespace uicore {

class view_memcache : public memcache
{
public:
	view_memcache(const char* name, size_t sz)
	: memcache(name, sz) {
	}

	~view_memcache() {
	}

protected:
	void dtor(void* buf, size_t sz) {
		view_dtor(reinterpret_cast<view_impl*>(buf));
	}
};

struct context_full_t : public context_t
{
	context_full_t()
	: view_mch_object("uic-view", sizeof(view_impl)) {
		view_mch = &view_mch_object;
	}

	// we are not going to expose the following
	// objects to the final caller
	view_memcache view_mch_object;
}
gblctx_full;

context_t* gblctx = &gblctx_full;

static void reset_metrics(context_t* ctx)
{
	display_metrics& m = ctx->metrics;
	m.density = 1.;
}

context_t::context_t()
: _f(0), draw_refcnt(0)
, view_mch(NULL)
, ginfo_mch("uic-ginf", sizeof(generic_info_t))
, viewctx_mch("uic-vwctx", sizeof(view_context_t))
{
	listnode_init(draw_pendings);
	reset_metrics(this);
}

// add a generic info as pending
view_context_t* gblctx_add_pending_ginfo(view_impl* nd, generic_info_t* ginfo)
{
	assert(NULL != nd);
	view_context_t* ctx = reinterpret_cast<view_context_t*>
		(gblctx->viewctx_mch.alloc());
	if (NULL == ctx) return NULL;

	ctx->view = nd;
	ctx->ginfo = ginfo;
	ginfo->ctx = ctx;

	auto_mutex(gblctx->view_ctx_mut);
	listnode_add(gblctx->draw_pendings, ctx->ownerlist);

	return ctx;
}

// remove pending view
void gblctx_remove_pending_ginfo(view_context_t* ctx)
{
	gblctx->view_ctx_mut.lock();
	listnode_del(ctx->ownerlist);
	gblctx->view_ctx_mut.unlock();

	// release all items
	if (ctx->ginfo)
		gblctx->ginfo_mch.free(ctx->ginfo);
	gblctx->viewctx_mch.free(ctx);
}

// this function is not locked
static int merge_dirtyrect(view_impl* nd, generic_info_t* curr, irect_t& r)
{
	generic_info_t* prev = nd->ginfo[nd->flags.readonly_ginfo_pos];
	return 0;
}

// this function is not locked
static void swap_view_uinfo(listnode_t* tmp)
{
	while (!listnode_isempty(gblctx->draw_pendings))
	{
		// add the item to a temp list
		listnode_t* item = gblctx->draw_pendings.next;
		listnode_del(*item);
		listnode_add(*tmp, *item);
	
		// get the pending view context
		view_context_t* nc = list_entry(view_context_t, ownerlist, item);
		if (!nc->ginfo) continue;

		// we have a ginfo, swap it
		view_impl* nd = nc->view;
		assert(NULL != nd);
		nd->flags.readonly_ginfo_pos = (nd->flags.readonly_ginfo_pos + 1) % 3;
	}
}

/**
  merge the view changes to the dirty rect

  @param r the rect for merging changes
  @return 0 for success or error happened
  */
int gblctx_merge_view_changes(irect_t& r)
{
	r.clear();
	
	listnode_t tmplst = LISTNODE_INITIALIZER(tmplst);
	gblctx->view_ctx_mut.lock();
	
	// swap to apply all uinfo
	swap_view_uinfo(&tmplst);

	while (!listnode_isempty(tmplst))
	{
		listnode_t* p = tmplst.next;
		listnode_del(*p);
		gblctx->view_ctx_mut.unlock();

		// get a pending view context
		view_context_t* nc = list_entry(view_context_t, ownerlist, p);
		view_impl* nd = nc->view;
		assert(NULL != nd);
		if (nc->ginfo) {
			irect_t wr;
			get_visible_dirtyrect(nc->view, wr);
			if (!wr.isnull()) {
				rect_merge(&r, wr);
			}
		}

		// finalize the view
		nc->view->flags.ready = 1;

		// release the view context
		gblctx->viewctx_mch.free(nc);
	}
	return 0;
}

iposition_t px(position_t& p)
{
	iposition_t ret;
	float d = gblctx->metrics.density;
	ret.x = (int32_t)roundf(p.x * d);
	ret.y = (int32_t)roundf(p.y * d);
	return ret;
}

igeometry_t px(geometry_t& g)
{
	igeometry_t ret;
	float d = gblctx->metrics.density;
	ret.width = (int32_t)roundf(g.width * d);
	ret.height = (int32_t)roundf(g.height * d);
	return ret;
}

}} // end of namespace zas::uicore
/* EOF */
