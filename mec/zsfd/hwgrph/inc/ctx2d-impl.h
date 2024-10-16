/** @file ctx2d-impl.h
 * declaration of the 2d context
 */

#ifndef __CXX_ZAS_HWGRPH_CONTEXT2D_IMPL_H__
#define __CXX_ZAS_HWGRPH_CONTEXT2D_IMPL_H__

#include <GLES2/gl2.h>

#include "hwgrph/context2d.h"
#include "gbuf.h"

namespace zas {
namespace hwgrph {

class ctx2d_impl;

struct gvertex
{
	gvertex() : x(0), y(0) {}
	gvertex(GLfloat xx, GLfloat yy)
	: x(xx), y(yy) {
	}
	GLfloat x, y;
};

inline static const gvertex& vtx(GLfloat xx, GLfloat yy)
{
	extern gvertex _gvtx;
	_gvtx.x = xx, _gvtx.y = yy;
	return _gvtx;
}

typedef gbuf_<gvertex> gvertexbuf;
typedef gbuf_<short> gindicebuf;

zas_interface shape
{
	// placement allocator
	void* operator new(size_t sz, ctx2d_impl* ctx);
	void operator delete(void* ptr, size_t sz);

	// return the current status
	virtual int getinfo(int* type = NULL) = 0;

	virtual int stroke(void) = 0;
	virtual int fill(void) = 0;
	virtual int shadow(void) = 0;
	virtual void release(void) = 0;
};

#define TEXTURE_WIDTH(w)	(((w) > 64) ? (w) : 64)
#define TEXTURE_HEIGHT(h)	(((h) > 64) ? (h) : 64)

class ctx2d_target_impl
{
public:
	ctx2d_target_impl(int type, int width, int height);
	~ctx2d_target_impl();

	int addref(void) {
		return __sync_add_and_fetch(&_refcnt, 1);
	}
	int release(void);

	GLuint get_texture(int *w = NULL, int *h = NULL) {
		if (w) *w = _width;
		if (h) *h = _height;
		return _texture;
	}

	void get_viewport(int& x, int& y, int& width, int& height) {
		x = _x; y = _y; width = _curwidth; height = _curheight;
	}

	int gettype(void) {
		return _f.target_type;
	}

private:
	int _refcnt;
	union {
		uint32_t _flags;
		struct {
			uint32_t target_type : 2;
		} _f;
	};

	GLuint _texture;
	// view pos, width and height
	int _x, _y, _width, _height, _curwidth, _curheight;
	// texture width and height
	int _texwidth, _texheight;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(ctx2d_target_impl);
};

class solidfill_impl : public fillstyle_base
{
public:
	solidfill_impl();
	~solidfill_impl();

	void* getobject(int* type);
	void setcolor(color c) {
		_c = c;
	}

	color getcolor(void) {
		return _c;
	}

private:
	color _c;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(solidfill_impl);
};

class shadow_impl
{
	friend class shadow_object;
public:
	shadow_impl();
	~shadow_impl();

	int addref(void) {
		return __sync_add_and_fetch(&_refcnt, 1);
	}

	int release(void);	

public:
	bool enabled(void) {
		return (_f.enabled) ? true : false;
	}

	bool enable(void) {
		_f.enabled = 1;
	}

	bool disable(void) {
		_f.enabled = 0;
	}

	void getsize(float& xsize, float& ysize) {
		xsize = _xsize;
		ysize = _ysize;
	}

	color getcolor(void) {
		return _color;
	}

	int get_blurlevel(void) {
		return _f.blurlevel;
	}

private:
	color _color;
	union {
		uint32_t _flags;
		struct {
			uint32_t enabled : 1;
			uint32_t blurlevel : 8;
			uint32_t offset_x : 8;
			uint32_t offset_y : 8;
		} _f;
	};
	float _xsize;
	float _ysize;
	int _refcnt;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(shadow_impl);
};

struct wraprect
{
	float x, y;
	float width, height;
};

class ctx2d_impl
{
	friend class shape;
	friend class context2d;
	friend class image_impl;
public:
	ctx2d_impl(bool thdsafe);
	~ctx2d_impl();

	int activate_program(void);
	int update_viewmodel(int width, int height);
	bool valid_check(void);

	int enable_offscreen(void);
	void reset_offscreen(void);
	int attach_target(void);

	int enable_FBO(GLuint texture, GLuint fbo);
	int restore_FBO(void);

	int gauss_blur(GLuint texture[2], int width, int height,
		int start, int radius, GLuint fbo);

	int draw_shadow(void);

	static GLuint create_texture(int width, int height,
		int type = GL_RGBA, const void* buf = NULL);

public:
	gvertexbuf& get_vertexbuf(void) {
		return _vertexbuf;
	}

	gindicebuf& get_indicebuf(void) {
		return _indicebuf;
	}

	void set_context(void* ctx) {
		_ctx = ctx;
	}

	void* get_context(void) {
		return _ctx;
	}

	void set_shape_wraprect(float x, float y, float w, float h) {
		_rect.x = x, _rect.y = y;
		_rect.width = w, _rect.height = h;
	}

private:
	int activate_solidfill_program(void* object);
	int paint_shadow(GLuint texture, float left,
		float top, float width, float height, color c);

	void lock(void);
	void unlock(void);

private:
	shape* _curr_shape;
	size_t _shp_size;
	union {
		uint32_t _flags;
		struct {
			uint32_t attached : 1;
			uint32_t shape_released : 1;
			uint32_t modelview_reload : 1;
			uint32_t thread_safe : 1;
		} _f;
	};

	// context info
	unsigned long int _threadid;
	void* _ctx;

	// render target
	ctx2d_target _target;
	GLuint _offscr_fbo;

	// fillstyle
	fillstyle _fillstyle;

	// shadow
	shadowstyle _shadowstyle;

	// font object
	font _font;

	// shape wrap rect
	wraprect _rect;

	gvertexbuf _vertexbuf;
	gindicebuf _indicebuf;
	GLfloat _modelview[4][4];

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(ctx2d_impl);
};

enum shape_type
{
	shape_type_unknown = 0,
	shape_type_rect,
	shape_type_image,
};

// all shape creator
shape* shape_create_rect(ctx2d_impl* ctx, float x, float y,
	float w, float h, float radius = 0);

}} // end of namespace zas::hwgrph
#endif // __CXX_ZAS_HWGRPH_CONTEXT2D_IMPL_H__
/* EOF */
