/** @file view.cpp
 * implementation of ui view operations
 */

#include <math.h>
#include "inc/view-impl.h"
#include "inc/region-impl.h"
#include "inc/context.h"
#include "utils/mutex.h"
#include "utils/mcache.h"

using namespace zas::utils;

namespace zas {
namespace uicore {

static void init_view(view_impl* p)
{		
	p->parents = NULL;
	listnode_init(p->children);
	listnode_init(p->siblings);
	
	p->name_ptr = NULL;
	view_resetflag(p);
	p->ginfo[0] = p->ginfo[1] = NULL;
}

// this function is not locked
static bool check_deleting(view_impl* p)
{
	for (; p; p = p->parents) {
		if (p->flags.deleting) return true;
	}
	return false;
}

// this function is not locked
static bool check_set_deleting(view_impl* p)
{
	view_impl* q = p;
	// check if the view is already in deleting
	for (; p; p = p->parents) {
		if (p->flags.deleting) return true;
	}

	// set the current view as deleting
	q->flags.ready = 0;
	q->flags.deleting = 1;
	return false;
}

static void view_reltree(view_impl* p)
{
	// when rendering, we cannot relase the view tree
	gblctx->render_mut.lock();
	gblctx->view_mut.lock();
	if (check_set_deleting(p)) {
		gblctx->view_mut.unlock();
		gblctx->render_mut.unlock();
		return;
	}

	// remove from the tree
	if (!listnode_isempty(p->siblings))
		listnode_del(p->siblings);
	gblctx->view_mut.unlock();
	gblctx->render_mut.unlock();

	// release all items
	// todo: check if we need lock

	for (p->parents = NULL; p;)
	{
		while (!listnode_isempty(p->children))
		{
			listnode_t* item = p->children.next;
			p = list_entry(view_impl, siblings, item);
		}

		// delete the current view
		view_impl* q = p->parents;
		listnode_del(p->siblings);
		gblctx->view_mch->free(p);
		p = q;
	}
}

void view_dtor(view_impl* nd)
{
	assert(listnode_isempty(nd->children));
	assert(listnode_isempty(nd->siblings));

	// relase the name
	if (nd->name_ptr && !nd->flags.use_embedded_name)
		delete [] nd->name_ptr;

	// relase all ginfo
	for (int i = 0; i < 3; ++i) {
		generic_info_t* ginfo = nd->ginfo[i];
		if (!ginfo) continue;

		// check if the ginfo has a context
		if (ginfo->ctx)
			gblctx_remove_pending_ginfo(ginfo->ctx);
		else gblctx->viewctx_mch.free(ginfo);
	}
}

/**
  find the view with a specific name
  Note: this function is not locked

  @param parents the pointer to the view
  @param name the name to be set for the view
  @return the pointer of the view found, or NULL as not found
  */
static view_impl* find_view(view_impl* parents, const char* name)
{
	if (!parents || !name || !*name)
		return NULL;

	listnode_t* item = parents->children.next;
	for (; item != &parents->children; item = item->next)
	{
		view_impl* nd = list_entry(view_impl, siblings, item);
		if (nd->flags.useid)
			continue;

		else if (nd->flags.use_embedded_name)
		{
			// see if the embedded name occupies all space, if so, we may
			// not able to use strcpy for string comparison
			if (nd->name_str[sizeof(void*) - 1]) {
				if (strlen(name) == sizeof(void*) &&
					!memcmp(name, nd->name_str, sizeof(void*)))
					return nd;
			}
			else if (!strcmp(name, nd->name_str))
				return nd;
		}
		else if (!nd->name_ptr) continue;
		else if (!strcmp(name, nd->name_ptr))
			return nd;
	}
	return NULL;
}

/**
  find the view with a specific name
  Note: this function is not locked

  @param parents the pointer to the view
  @param id the id you want to find
  @return the pointer of the view found, or NULL as not found
  */
static view_impl* find_view(view_impl* parents, size_t id)
{
	if (!parents || !id) return NULL;
	listnode_t* item = parents->children.next;
	for (; item != &parents->children; item = item->next)
	{
		view_impl* nd = list_entry(view_impl, siblings, item);
		if (!(nd->flags.useid))
			continue;

		else if (nd->id == id)
			return nd;
	}
	return NULL;
}

/**
  Get the name of current view
  this function is not locked

  @param name: the buffer to room the name
  @param sz: indicate the size of name buffer
  @return 0 means there is no name for this view
          > 0: means the size of the nmae for this view
		  < 0: means the buffer size is not enough or buffer is NULL
		  the abs(return val) is the size for the buffer
		  to room the name of this view
  */
static int get_view_name(view_impl* nd, char* name, size_t sz)
{
	assert(NULL != nd);

	// check if we have a name
	if (nd->name_ptr == NULL) {
		return 0;
	}

	size_t nlen;
	if (nd->flags.use_embedded_name)
	{
		// as we may lost the '\0' in embedded buffer
		// we copy it out and add the last '\0'
		char buf[sizeof(void*) + 1];
		memcpy(buf, nd->name_str, sizeof(void*));
		buf[sizeof(void*)] = '\0';

		// here we find the length of the embedded name
		nlen = strlen(buf) + 1;
		if (nlen > sz || !name) return -int(nlen);

		// copy out the name string to buffer
		strcpy(name, buf);
	}
	else {
		nlen = strlen(nd->name_ptr) + 1;
		if (nlen > sz || !name) return -int(nlen);

		// copy out the name string to buffer
		strcpy(name, nd->name_ptr);
	}
	return int(nlen);
}

/**
  Set the reference name for a view
  Note: this function is not locked and will not check
        if there is an existed sibling with a same name
		an empty name means clear the previous name of the view
  
  @param nd the pointer to the view
  @param name the name to be set for the view
  @param dup if we need to duplicate the name or not
  @return 0 for success, or an error occurred
  */
static int set_view_name(view_impl* nd, const char* name, bool dup)
{
	// see if we need to release the previous name
	if (nd->name_ptr && !nd->flags.useid && !nd->flags.use_embedded_name) {
		delete [] nd->name_ptr;
	}

	// clear previous name or id
	nd->flags.use_embedded_name = 0;
	nd->flags.useid = 0;
	nd->id = 0;
	

	// an empty name means clear name of the view
	if (!name || !*name) return 0;

	size_t nlen = strlen(name);
	if (nlen <= sizeof(void*)) {
		// make the name as embedded
		memcpy((void*)nd->name_str, name, nlen);
		nd->flags.use_embedded_name = 1;
	}
	else {
			// do we need to duplicate the name
		if (dup) {
			nd->name_ptr = new char [nlen + 1];
			assert(NULL != nd->name_ptr);
			strcpy((char*)nd->name_ptr, name);
		}
		else {
			nd->name_ptr = name;
			nd->flags.name_norel = 1;
		}
		nd->flags.use_embedded_name = 0;
	}
	return 0;
}

/**
  Set the reference name for a view
  Note: this function is not locked and will not check
        if there is an existed sibling with a same id
		set id = 0 means clear the previous id of the view
  
  @param nd the pointer to the view
  @param id the id to be set for the view
  @return 0 for success, or an error occurred
  */
static int set_viewid(view_impl* nd, size_t id)
{
	// see if we need to release the previous name
	if (nd->name_ptr && !nd->flags.useid && ! nd->flags.use_embedded_name) {
		delete [] nd->name_ptr;
	}

	// clear previous name or id
	nd->flags.use_embedded_name = 0;
	nd->flags.useid = 0;
	nd->id = 0;

	// an empty name means clear name of the view
	if (id) {
		nd->id = id;
		nd->flags.useid = 1;
	}
	return 0;
}

/**
  get the current readonly generic info

  @param nd the pointer to the view
  @return the pointer to the ginfo, NULL will be
  returned if the ginfo not exists. this could be
  used for checking if the ginfo exists
  */
static inline generic_info_t* check_get_readonly_ginfo(view_impl* nd)
{
	assert(NULL != nd);
	return nd->ginfo[nd->flags.readonly_ginfo_pos];
}

/**
  get the current writable generic info

  @param nd the pointer to the view
  @return the pointer to the ginfo, NULL will be
  returned if the ginfo not exists. this could be
  used for checking if the ginfo exists
  */
static inline generic_info_t* check_get_writable_ginfo(view_impl* nd)
{
	assert(NULL != nd);
	unsigned int writable = (nd->flags.readonly_ginfo_pos + 1) % 3;
	return nd->ginfo[writable];
}

/**
  get the current generic info
  try to get the read only generic info first. if there is no
  read only ginfo, we return writable ginfo

  @param nd the pointer to the view
  @return the pointer to the ginfo, NULL will be
  returned if the ginfo not exists. this could be
  used for checking if the ginfo exists
  */
static inline generic_info_t* check_get_ginfo(view_impl* nd)
{
	assert(NULL != nd);
	generic_info_t* ret = nd->ginfo[nd->flags.readonly_ginfo_pos];
	if (ret) return ret;

	// check if we have writable ginfo
	ret = nd->ginfo[(nd->flags.readonly_ginfo_pos + 1) % 3];
	return ret;
}

/**
  get the current writable generic info, we will
  create the ginfo if not exist

  @param nd the pointer to the view
  @return the pointer to the ginfo
  */
static generic_info_t* get_writable_ginfo(view_impl* nd)
{
	generic_info_t* prev;

	assert(NULL != nd);
	unsigned int writable = (nd->flags.readonly_ginfo_pos + 1) % 3;
	if (nd->ginfo[writable]) {
		return nd->ginfo[writable];
	}

	// create the ginfo object and its context
	generic_info_t* inf = reinterpret_cast<generic_info_t*>
		(gblctx->ginfo_mch.alloc());
	if (NULL == inf) return NULL;

	// initialize the info context
	view_context_t* ctx = gblctx_add_pending_ginfo(nd, inf);
	if (!ctx) {
		// relase the ginfo object
		gblctx->ginfo_mch.free(inf);
		return NULL;
	}

	// check if we need a new ginfo again (for racing condition)
	gblctx->view_mut.lock();
	writable = (nd->flags.readonly_ginfo_pos + 1) % 3;

	if (!nd->ginfo[writable]) {
		prev = nd->ginfo[nd->flags.readonly_ginfo_pos];
		nd->ginfo[writable] = inf;
	}
	else {
		gblctx->view_mut.unlock();
		// relase the allocated ginfo since we don't need it any more
		gblctx_remove_pending_ginfo(ctx);
		return nd->ginfo[writable];
	}

	// copy the previous info to the current writable info
	if (prev) {
		memcpy(inf, prev, sizeof(generic_info_t));
	} else inf->reset();
	gblctx->view_mut.unlock();
	return inf;
}

void ginfo_get_clientrect(generic_info_t* ginfo, irect_t& r)
{
	assert(NULL != ginfo);
	float d = gblctx->metrics.density;
	r.left = (int32_t)roundf(ginfo->dpr.left * d);
	r.top = (int32_t)roundf(ginfo->dpr.top * d);
	r.width = (int32_t)roundf(ginfo->dpr.width * d);
	r.height = (int32_t)roundf(ginfo->dpr.height * d);
}

void ginfo_get_wraprect(generic_info_t* ginfo, rect_t& r)
{
	assert(NULL != ginfo);
	r = ginfo->dpr;
	r.left -= ginfo->margin.left;
	r.top -= ginfo->margin.top;
	r.width += ginfo->margin.left + ginfo->margin.right;
	r.height += ginfo->margin.top + ginfo->margin.bottom;
}

void ginfo_get_wraprect(generic_info_t* ginfo, irect_t& r)
{
	assert(NULL != ginfo);
	ginfo_get_clientrect(ginfo, r);
	float d = gblctx->metrics.density;
	
	int32_t mleft = (int32_t)roundf(ginfo->margin.left * d);
	int32_t mtop = (int32_t)roundf(ginfo->margin.top * d);
	int32_t mright = (int32_t)roundf(ginfo->margin.right * d);
	int32_t mbottom = (int32_t)roundf(ginfo->margin.bottom * d);

	r.left -= mleft, r.top -= mtop;
	r.width += mleft + mright;
	r.height += mtop + mbottom;
}

/**
  check if the view is visible
  Note: this function is not locked

  @param nd the pointer to the view
  @return true means the view is visilbe while false means invisible
  */
static bool visible(view_impl* nd)
{
	if (NULL == nd) return false;
	for (; nd; nd = nd->parents) {
		generic_info_t* ginfo = nd->ginfo[nd->flags.readonly_ginfo_pos];
		if (!ginfo->flags.visible)
			return false;
	}
	return true;
}

// this function is not locked
static void calc_group_wraprect(view_impl* nd, rect_t& res)
{
	assert(NULL != nd);
	if (!nd->flags.as_group) {
		return;
	}

	// check all siblings
	rect_t r;
	res.clear();
	listnode_t* item = nd->children.next;

	for (; item != &nd->children; item = item->next)
	{
		view_impl* sn = list_entry(view_impl, siblings, item);
		generic_info_t* gi = check_get_writable_ginfo(sn);
		if (!gi) continue;

		ginfo_get_wraprect(gi, r);
		rect_merge(&r, res);
	}
}

// this funciton is not locked
static void group_adjust_sibling_pos(view_impl* nd, float dx, float dy)
{
	// handle the writable ginfo
	generic_info_t* gwinf = check_get_writable_ginfo(nd);
	if (gwinf) {
		gwinf->move(dx, dy);
		return;
	}

	// handle the read only ginfo
	// we must make sure there is at least
	// one ginfo exists (read only or writable)
	// otherwise there is no way to store the dx, dy
	generic_info_t* grinf = check_get_readonly_ginfo(nd);
	if (grinf) grinf->move(dx, dy);
	else if (!gwinf) {
		// create the read only ginfo
		grinf = reinterpret_cast<generic_info_t*>(gblctx->ginfo_mch.alloc());
		assert(NULL != grinf);

		grinf->reset();
		grinf->dpr.set(dx, dy, 0, 0);
		set_readonly_ginfo(nd, grinf);
	}
}

// this function is not locked
static void view_update_group(view_impl* nd, generic_info_t* ginfo)
{
	rect_t r;
	if (ginfo) ginfo_get_wraprect(ginfo, r);
	else return;

	for (view_impl* p = nd->parents; p; p = p->parents)
	{
		if (!p->flags.as_group)
			return;
		
		generic_info_t* ginfo = check_get_ginfo(p);
		if (NULL == ginfo) {
			ginfo = get_writable_ginfo(p);
			assert(NULL != ginfo);
		}

		// backup the ginfo rect
		rect_t dpr(ginfo->dpr);
		dpr.setpos(0., 0.);
		
		// make the merge
		rect_merge(&dpr, r);
		ginfo->dpr.move(dpr.left, dpr.top);
		ginfo->dpr.setsize(dpr.width, dpr.height);

		// if the position of the group changed, adjust the
		// position of all its siblings
		if (dpr.left < 0 || dpr.top < 0)
		{
			listnode_t* it = p->children.next;
			for (; it != &p->children; it = it->next)
			{
				view_impl* sn = list_entry(view_impl, siblings, it);
				group_adjust_sibling_pos(sn, -dpr.left, -dpr.top);
			}
		}
		r = ginfo->dpr;
	}
}

/**
  Create a view

  @param parents the parents for the new created view, NULL means no parents
  @param name represents the name of the view, NULL or "" mean no name
  @param isgrp represents if the view is a group
  @return true means the view is visilbe while false means invisible
  */
view_t* view_t::create(view_t* parents, const char* name, bool isgrp)
{
	view_impl* p = reinterpret_cast<view_impl*>(parents);

	// if we have a name, see if it exists
	if (name) {
		// we need lock before searching
		auto_mutex m(gblctx->view_mut);
		if (find_view(p, name))
			return NULL;
	}

	view_impl* view = reinterpret_cast<view_impl*>(gblctx->view_mch->alloc());
	if (NULL == view) return NULL;
	init_view(view);
	set_view_name(view, name, true);

	// set as group if necessary
	if (isgrp) view->flags.as_group = 1;

	if (NULL != p) {
		// we need lock before inserting
		gblctx->view_mut.lock();
		// make sure the parent is not in "deleting" status
		if (check_deleting(p)) {
			gblctx->view_mut.unlock();
			gblctx->view_mch->free(view);
			return NULL;
		}
		view_addchild(p, view);
		if (p->flags.as_group) {
			view_update_group(view, NULL);
		}
		gblctx->view_mut.unlock();
	}
	return reinterpret_cast<view_t*>(view);
}

/**
  Create a view

  @param parents the parents for the new created view, NULL means no parents
  @param r set the rect as position and size of the view
  @param visible set if the view is visible or not
  @param name represents the name of the view, NULL or "" mean no name
  @param isgrp represents if the view is a group
  @return true means the view is visilbe while false means invisible
  */
view_t* view_t::create(view_t* parents, const rect_t& r, bool visible,
	const char* name, bool isgrp)
{
	view_impl* p = reinterpret_cast<view_impl*>(parents);

	// if we have a name, see if it exists
	if (name) {
		// we need lock before searching
		auto_mutex m(gblctx->view_mut);
		if (find_view(p, name))
			return NULL;
	}

	view_impl* view = reinterpret_cast<view_impl*>(gblctx->view_mch->alloc());
	if (NULL == view) return NULL;
	init_view(view);
	set_view_name(view, name, true);

	// set as group if necessary
	if (isgrp) view->flags.as_group = 1;

	generic_info_t* ginfo = get_writable_ginfo(view);
	if (NULL == ginfo) {
		gblctx->view_mch->free(view);
		return NULL;
	}

	gblctx->view_mut.lock();

	// make sure the parent is not in "deleting" status
	if (check_deleting(p)) {
		gblctx->view_mut.unlock();
		gblctx->view_mch->free(view);
		return NULL;
	}
	
	// we must add the view first
	// since the following operation rely on
	// the parents of the view
	if (p) view_addchild(p, view);
	ginfo->setpos(view, r);
	// todo: visible

	if (p && p->flags.as_group) {
		view_update_group(view, ginfo);
	}
	gblctx->view_mut.unlock();
	return reinterpret_cast<view_t*>(view);
}

/**
  release the view and all its sub views
  */
void view_t::release(void)
{
	view_impl* p = reinterpret_cast<view_impl*>(this);
	view_reltree(p);
}

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
view_t* view_t::setpos(float left, float top, float width, float height)
{
	view_impl* nd = reinterpret_cast<view_impl*>(this);
	// we cannot change pos for a group
	if (nd->flags.as_group) return this;

	generic_info_t* ginfo = get_writable_ginfo(nd);
	if (NULL == ginfo) return this;

	// make sure the parent is not in "deleting" status
	gblctx->view_mut.lock();
	if (check_deleting(nd)) {
		gblctx->view_mut.unlock();
		return NULL;
	}

	// change the pos and update the parent groups
	ginfo->setpos(nd, rect_t(left, top, width, height));
	view_update_group(nd, ginfo);

	gblctx->view_mut.unlock();
	return this;
}

/**
  Set the name of the current view

  @param n specify the name of the view, NULL and "" will clear the name
  @return 0 for success or an error happen
    -EEXISTS: means a view with same name exists
  */
int view_t::set_name(const char* name)
{
	// check if the name is valid
	// a valid name shall not have '.'
	if (name) for (const char* t = name; *t; ++t) {
		if (*t == '.') return -EINVALID;
	}

	view_impl* nd = reinterpret_cast<view_impl*>(this);

	// we need lock before searching
	auto_mutex m(gblctx->view_mut);
	view_impl* p = nd->parents;

	// if we have a name, see if it exists
	if (!p && find_view(p, name)) {
		return -EEXISTS;
	}

	return set_view_name(nd, name, true);
}

/**
  Get the name of current view

  @param name: the buffer to room the name
  @param sz: indicate the size of name buffer
  @return 0 see get_view_name()
  */
int view_t::get_name(char* name, size_t sz)
{
	view_impl* nd = reinterpret_cast<view_impl*>(this);

	// we need lock before reading name
	auto_mutex m(gblctx->view_mut);
	return get_view_name(nd, name, sz);
}

/**
  Get the client area size

  @param r the rect to hold the client size
  */
void view_t::get_clientrect(rect_t& r)
{
	view_impl* nd = reinterpret_cast<view_impl*>(this);
	auto_mutex m(gblctx->view_mut);
	generic_info_t* ginfo = check_get_ginfo(nd);
	if (ginfo) {
		ginfo->get_clientrect(r);
	} else r.clear();
}

/**
  Get the wrap rect size
  wrap rect means the clieat area + margin

  @param r the rect to hold the client size
  */
void view_t::get_wraprect(rect_t& r)
{
	view_impl* nd = reinterpret_cast<view_impl*>(this);
	auto_mutex m(gblctx->view_mut);
	generic_info_t* ginfo = check_get_ginfo(nd);
	if (ginfo) {
		ginfo->get_wraprect(r);
	} else r.clear();
}

}};	// end of namespace zas::uicore
/* EOF */
