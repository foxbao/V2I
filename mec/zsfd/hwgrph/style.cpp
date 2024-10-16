/** @file style.cpp
 * implementation of all kinds of fillstyle, shadow
 */

#include "inc/ctx2d-impl.h"

namespace zas {
namespace hwgrph {

fillstyle_base::fillstyle_base()
: _refcnt(0) {
}

fillstyle_base::~fillstyle_base()
{
}

int fillstyle_base::release(void)
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0) delete this;
	return cnt;
}

void* fillstyle_base::getobject(int* type)
{
	return NULL;
}

solidfill_impl::solidfill_impl()
{
}

solidfill_impl::~solidfill_impl()
{
}

void* solidfill_impl::getobject(int* type)
{
	if (type) *type = solid_fill;
	return reinterpret_cast<void*>(this);
}

solidfill_object::operator fillstyle_base*(void)
{
	solidfill_impl* sf = reinterpret_cast<solidfill_impl*>(this);
	return static_cast<fillstyle_base*>(sf);
}

int solidfill_object::addref(void)
{
	solidfill_impl* sf = reinterpret_cast<solidfill_impl*>(this);
	return sf->addref();
}

int solidfill_object::release(void)
{
	solidfill_impl* sf = reinterpret_cast<solidfill_impl*>(this);
	return sf->release();
}

solidfill_object* solidfill_object::setcolor(color c)
{
	solidfill_impl* sf = reinterpret_cast<solidfill_impl*>(this);
	sf->setcolor(c);
	return this;
}

solidfill context2d::create_solidfill(void)
{
	solidfill_impl* sf = new solidfill_impl();
	assert(NULL != sf);
	return solidfill(reinterpret_cast<solidfill_object*>(sf));
}

solidfill context2d::create_solidfill(color c)
{
	solidfill_impl* sf = new solidfill_impl();
	assert(NULL != sf);
	sf->setcolor(c);
	return solidfill(reinterpret_cast<solidfill_object*>(sf));
}

fillstyle context2d::set_fillstyle(solidfill sf)
{
	ctx2d_impl* ctx = reinterpret_cast<ctx2d_impl*>(this);
	fillstyle ret = ctx->_fillstyle;
	ctx->_fillstyle = (fillstyle_base*)sf.get();
	return ret;
}

shadowstyle context2d::set_shadowstyle(shadowstyle sdw)
{
	ctx2d_impl* ctx = reinterpret_cast<ctx2d_impl*>(this);
	shadowstyle ret = ctx->_shadowstyle;
	ctx->_shadowstyle = sdw;
	return ret;
}

font context2d::set_font(font fnt)
{
	ctx2d_impl* ctx = reinterpret_cast<ctx2d_impl*>(this);
	font ret = ctx->_font;
	ctx->_font = fnt;
	return ret;
}

shadowstyle context2d::create_shadowstyle(void)
{
	shadow_impl* sdw = new shadow_impl();
	assert(NULL != sdw);
	return shadowstyle(reinterpret_cast<shadow_object*>(sdw));
}

int shadow_impl::release(void)
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0) delete this;
	return cnt;
}

shadow_impl::shadow_impl()
: _flags(0)
, _xsize(0), _ysize(0)
, _refcnt(0)
{
}

shadow_impl::~shadow_impl()
{
}

int shadow_object::addref(void)
{
	shadow_impl* sdw = reinterpret_cast<shadow_impl*>(this);
	return sdw->addref();
}

int shadow_object::release(void)
{
	shadow_impl* sdw = reinterpret_cast<shadow_impl*>(this);
	return sdw->release();
}

bool shadow_object::enabled(void)
{
	shadow_impl* sdw = reinterpret_cast<shadow_impl*>(this);
	return sdw->enabled();
}

void shadow_object::enable(void)
{
	shadow_impl* sdw = reinterpret_cast<shadow_impl*>(this);
	sdw->enable();
}

void shadow_object::disable(void)
{
	shadow_impl* sdw = reinterpret_cast<shadow_impl*>(this);
	sdw->disable();
}

void shadow_object::setcolor(color c)
{
	shadow_impl* sdw = reinterpret_cast<shadow_impl*>(this);
	sdw->_color = c;
}

int shadow_object::setsize(float s)
{
	if (s < 1) return -EBADPARM;
	shadow_impl* sdw = reinterpret_cast<shadow_impl*>(this);
	sdw->_xsize = s;
	sdw->_ysize = s;
	return 0;
}

int shadow_object::setblur(int blurlevel)
{
	shadow_impl* sdw = reinterpret_cast<shadow_impl*>(this);
	// todo:
	sdw->_f.blurlevel = blurlevel;
	return 0;
}

}} // end of namespace zas::hwgrph
/* EOF */
