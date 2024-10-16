/** @file context2d.cpp
 * implementation of the 2d context
 */

#include "utils/thread.h"

#include "inc/shader.h"
#include "inc/image.h"
#include "inc/grphgbl.h"
#include "inc/ctx2d-impl.h"

using namespace zas::utils;

namespace zas {
namespace hwgrph {

gvertex _gvtx;

#define to_shape_object_tail(T, base, shpsz)	\
	((T*)(((size_t)(base)) + (shpsz)))

// shape placement allocator
void* shape::operator new(size_t sz, ctx2d_impl* ctx)
{
	// release the previous shape
	if (ctx->_curr_shape && !ctx->_f.shape_released) {
		ctx->_curr_shape->release();
		assert(ctx->_f.shape_released);
	}
	size_t allsz = (sz + sizeof(ctx2d_impl*) + 15) & ~15;
	if (ctx->_shp_size < allsz) {
		if (ctx->_curr_shape) 
			free(ctx->_curr_shape);
		ctx->_curr_shape = (shape*)malloc(allsz);
		assert(NULL != ctx->_curr_shape);
		ctx->_shp_size = allsz;
	}
	to_shape_object_tail(ctx2d_impl*, ctx->_curr_shape, sz)[0] = ctx;
	ctx->_f.shape_released = 0;
	return ctx->_curr_shape;
}

void shape::operator delete(void* ptr, size_t sz)
{
	ctx2d_impl* ctx = to_shape_object_tail(ctx2d_impl*, ptr, sz)[0];
	ctx->_f.shape_released = 1;
}

color::color(float r, float g, float b)
{
	uint64_t rr, gg, bb;
	if (r < 0 || g < 0 || b < 0)
		goto error;
	if (r <= 1 && g <= 1 && b <= 1)
	{
		rr = uint64_t(r * 65535);
		gg = uint64_t(g * 65535) << 16;
		bb = uint64_t(b * 65535) << 32;
	}
	else if (r > 65535 || g > 65535 || b > 65535)
		goto error;
	else {
		rr = uint64_t(r);
		gg = uint64_t(g) << 16;
		bb = uint64_t(b) << 32;
	}
	m_color = rr | gg | bb;
	return;

error: m_color = 0;
}

color::color(float r, float g, float b, float a)
{
	uint64_t rr, gg, bb, aa;
	if (r < 0 || g < 0 || b < 0 || a < 0)
		goto error;
	if (r <= 1 && g <= 1 && b <= 1 && a <= 1)
	{
		rr = uint64_t(r * 65535);
		gg = uint64_t(g * 65535) << 16;
		bb = uint64_t(b * 65535) << 32;
		aa = uint64_t(a * 65535) << 48;
	}
	else if (r > 65535 || g > 65535 || b > 65535 || a > 65535)
		goto error;
	else {
		rr = uint64_t(r);
		gg = uint64_t(g) << 16;
		bb = uint64_t(b) << 32;
		aa = uint64_t(a) << 48;
	}
	m_color = rr | gg | bb | aa;
	return;

error: m_color = 0;
}

ctx2d_impl::ctx2d_impl(bool thdsafe)
: _vertexbuf(512)
, _indicebuf(512)
, _curr_shape(NULL)
, _shp_size(0)
, _flags(0)
, _threadid(0)
, _ctx(NULL)
, _offscr_fbo(0)
{
	if (thdsafe) _f.thread_safe = 1;
	// create default solid fill
	solidfill_impl* sfi = new solidfill_impl();
	sfi->setcolor(rgba(0, 0, 0, 1));
	_fillstyle = sfi;
}

ctx2d_impl::~ctx2d_impl()
{
	// release the FBO, if needed
	if (_offscr_fbo) {
		glDeleteFramebuffers(1, &_offscr_fbo);
		_offscr_fbo = 0;
	}

	if (_curr_shape)
	{
		// release the shape object if necessary
		if (!_f.shape_released)
			_curr_shape->release();
		free(_curr_shape);
		_curr_shape = NULL;
		_shp_size = 0;
	}
}

// return 0: active program remain no change
// return > 0: active program changed
// return < 0: error happened
int ctx2d_impl::activate_program(void)
{
	// todo: need rewrite
	int type, ret = -EINVALID;
	void* obj = _fillstyle->getobject(&type);
	if (NULL == obj) return -EINVALID;

	switch (type)
	{
	case solid_fill:
		ret = activate_solidfill_program(obj);
		break;

	default: break;
	}
	return ret;
}

int ctx2d_impl::activate_solidfill_program(void* object)
{
	shader_mgr* sdrmgr = shader_mgr::inst();
	shader* sdr = sdrmgr->getshader(0, "solidfill-shader");
	assert(NULL != sdr);
	int ret = sdr->activate();
	if (ret < 0) return ret;

	// load uniform if necessary
	if (ret > 0 || _f.modelview_reload) {
		glUniformMatrix4fv(sdr->get_varible(glslvar_u_modelview),
		1, GL_FALSE, (GLfloat *)_modelview);
		_f.modelview_reload = 0; // loaded
	}

	// set solid fill color
	solidfill_impl* sf = reinterpret_cast<solidfill_impl*>(object);
	color c = sf->getcolor();
	GLint ufillclr = sdr->get_varible(glslvar_u_fillcolor);
	if (ufillclr >= 0) glUniform4f(ufillclr, c.r(), c.g(), c.b(), c.a());
	return ret;
}

bool ctx2d_impl::valid_check(void)
{
	if (_threadid != gettid())
		return false;
	if (!_f.attached) return false;
	return true;
}

int ctx2d_impl::update_viewmodel(int iwidth, int iheight)
{
	float width = float(iwidth);
	float height = float(iheight);
	GLfloat modelview[4][4] =
	{
		{ 2 / width, 0, 0, 0 },
		{ 0, -2 / height, 0, 0 },
		{ 0, 0, 0, 0 },
		{ -1, 1, 0, 1 }
	};
	memcpy(_modelview, modelview, sizeof(modelview));
	_f.modelview_reload = 1;
	return 0;
}

context2d* context2d::create(bool thdsafe)
{
	ctx2d_impl* ctx = new ctx2d_impl(thdsafe);
	assert(NULL != ctx);
	return reinterpret_cast<context2d*>(ctx);
}

void context2d::release(void)
{
	ctx2d_impl* ctx = reinterpret_cast<ctx2d_impl*>(this);
	delete ctx;
}

int ctx2d_impl::enable_FBO(GLuint texture, GLuint fbo)
{
	if (gblconfig::inst()->RTT_multisampleing_supported())
	{
		// todo: check!
		//glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
		//	GL_RENDERBUFFER, fbo);
		gblconfig::inst()->glFramebufferTexture2DMultisample(GL_FRAMEBUFFER,
			GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0, 4);
	}
	else {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, texture, 0);
	}
	
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status == GL_FRAMEBUFFER_COMPLETE) return 0;
	else if (status == GL_FRAMEBUFFER_UNSUPPORTED) {
		return -ENOTSUPPORT;
	}
	else return -EINVALID;
}

int ctx2d_impl::enable_offscreen(void)
{
	assert(_target);

	ctx2d_target_impl* tar = reinterpret_cast
		<ctx2d_target_impl*>(_target.get());
	int width, height;
	GLuint texture = tar->get_texture(&width, &height);

	if (!_offscr_fbo) {
		glGenFramebuffers(1, &_offscr_fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, _offscr_fbo);
	}

	int ret = enable_FBO(texture, _offscr_fbo);
	if (ret == -ENOTSUPPORT) {
		printf("Offscreen render: unsupported "
			"framebuffer format.\n");
		exit(9970);
	}
	else if (ret == -EINVALID) {
		printf("Offscreen render: invalid framebuffer "
			"configuration.\n");
		exit(9969);
	}
	return 0;
}

int ctx2d_impl::restore_FBO(void)
{
	ctx2d_target_impl* target = reinterpret_cast
		<ctx2d_target_impl*>(_target.get());
	if (!target) return -EINVALID;

	int type = target->gettype();
	if (type == render_target_to_screen) {
		// switch back to default on-screen rendering
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	else if (type == render_target_to_image) {
		enable_offscreen();
	}

	int x, y, width, height;
	target->get_viewport(x, y, width, height);
	glViewport(x, y, width, height);
	return 0;
}

GLuint ctx2d_impl::create_texture(int width, int height,
	int type, const void* buf)
{
	GLuint texture;
	// we can create texture in any context
	// since all context are shared with each other
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, type, width, height, 0,
		type, GL_UNSIGNED_BYTE, buf);
	return texture;
}

int ctx2d_impl::gauss_blur(GLuint texture[2], int width, int height,
	int start, int radius, GLuint fbo)
{
	if (radius < GAUSS_BLUR_RADIUS_MIN
		|| radius > GAUSS_BLUR_RADIUS_MAX)
		return -EBADPARM;

	bool new_fbo = false;
	int next = 1 - start;
	shader_mgr* sdrmgr = shader_mgr::inst();

	if (!fbo) {
		// create a fbo
		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		new_fbo = true;
	}

	GLfloat vertexs[] = {
		-1, -1,  0,  0,
		-1,  1,  0,  1,
		1,  1,  1,  1,
		1, -1,  1,  0,
	};

	// two passes for gauss blur
	for (int i = 0; i < 2; ++i, start = 1 - start)
	{
		if (enable_FBO(texture[1 - start], fbo)) {
			if (new_fbo) glDeleteFramebuffers(1, &fbo);
			return -2;
		}
		glViewport(0, 0, width, height);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);

		shader* sdr = sdrmgr->getshader(0, i ? "shadow-gaussblur-pass2"
			: "shadow-gaussblur-pass1");
		if (sdr->activate() < 0) {
			if (new_fbo) glDeleteFramebuffers(1, &fbo);
			return -2;
		}

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture[start]);

		GLint utex = sdr->get_varible(glslvar_u_srctex);
		assert(utex >= 0);
		glUniform1i(utex, 0);

		GLint apos = sdr->get_varible(glslvar_a_position);
		assert(apos >= 0);
		glVertexAttribPointer(apos, 2, GL_FLOAT, GL_FALSE,
			4 * sizeof(GLfloat), vertexs);
		glEnableVertexAttribArray(apos);

		GLint atex = sdr->get_varible(glslvar_a_srctexcord);
		assert(atex >= 0);
		glVertexAttribPointer(atex, 2, GL_FLOAT, GL_FALSE,
			4 * sizeof(GLfloat), (GLfloat*)vertexs + 2);
		glEnableVertexAttribArray(atex);

		GLint ucount = sdr->get_varible(glslvar_u_count);
		assert(ucount >= 0);
		glUniform1i(ucount, radius);

		double *off, *wei;
		float coefficent = float(i ? height : width);
		float offset[GAUSS_BLUR_RADIUS_MAX];
		float weight[GAUSS_BLUR_RADIUS_MAX];
		gblconfig::inst()->get_gauss_weight(radius, off, wei);
		for (int i = 0; i < radius; ++i) {
			offset[i] = float(off[i] / coefficent);
			weight[i] = float(wei[i]);
		}

		GLint uoff = sdr->get_varible(glslvar_u_offset);
		assert(uoff >= 0);
		glUniform1fv(uoff, radius, (GLfloat*)offset);
		GLint uwei = sdr->get_varible(glslvar_u_weight);
		assert(uwei >= 0);
		glUniform1fv(uwei, radius, (GLfloat*)weight);

		GLshort indices[] = { 0, 1, 2, 2, 3 };
		glDrawElements(GL_TRIANGLE_FAN, 5, GL_UNSIGNED_SHORT, (GLshort*)indices);

		glDisableVertexAttribArray(apos);
		glDisableVertexAttribArray(atex);
	}
	if (new_fbo) glDeleteFramebuffers(1, &fbo);
	return 0;
}

int ctx2d_impl::draw_shadow(void)
{
	if (!_shadowstyle || !_shadowstyle->enabled()) {
		return -ENOTAVAIL;
	}

	shadow_impl* sdw = reinterpret_cast
		<shadow_impl*>(_shadowstyle.get());

	float h_magnify, v_magnify;
	sdw->getsize(h_magnify, v_magnify);
	if (h_magnify < 1.0f || v_magnify < 1.0f) {
		return -EBADPARM;
	}

	float width = _rect.width + 10;
	float height = _rect.height + 10;
	h_magnify = width / _rect.width;
	v_magnify = height / _rect.height;

	int blurlevel = sdw->get_blurlevel();
	float r = (blurlevel - 1) * 2 + 1;
	width += r * 2; height += r * 2;

	GLfloat modelview[4][4] =
	{
		{ h_magnify * 2 / width, 0, 0, 0 },
		{ 0, -v_magnify * 2 / height, 0, 0 },
		{ 0, 0, 0, 0 },
		{ -(width + _rect.x * 2 * h_magnify - r * 2) / width,
		(height + _rect.y * 2 * v_magnify - r * 2) / height, 0, 1 }
	};

	// create 2 temp textures used for gauss blur
	// since gauss blur need 2 rounds (horizal + vertical)
	GLuint texture[2];
	texture[0] = ctx2d_impl::create_texture(width, height);
	texture[1] = ctx2d_impl::create_texture(width, height);
	assert(texture[0] != 0 && texture[1] != 0);

	// create an FBO as render target so that we can
	// "draw" the shape for creating the final shadow	
	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	if (enable_FBO(texture[0], fbo)) {
		glDeleteTextures(2, texture);
		glDeleteFramebuffers(1, &fbo);
		return -1;
	}

	// select the shader for drawing shadow
	shader_mgr* sdrmgr = shader_mgr::inst();
	shader* sdr = sdrmgr->getshader(0, "solidfill-shader");
	sdr->activate();
	
	glUniformMatrix4fv(sdr->get_varible(glslvar_u_modelview),
		1, GL_FALSE, (GLfloat *)modelview);

	// set solid fill color
	GLint ufillclr = sdr->get_varible(glslvar_u_fillcolor);
	if (ufillclr >= 0) glUniform4f(ufillclr, 1, 1, 1, 1);

	// draw the shadow
	glViewport(0, 0, width, height);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	_curr_shape->shadow();
	
	// run the gauss blur with specified blurlevel
	gauss_blur(texture, width, height, 0, blurlevel, fbo);

	restore_FBO();
	paint_shadow(texture[0], _rect.x - (width - _rect.width) / 2,
		_rect.y - (height - _rect.height) / 2, width, height,
		sdw->getcolor());

	glDeleteTextures(2, texture);
	glDeleteFramebuffers(1, &fbo);
	return 0;
}

int ctx2d_impl::paint_shadow(GLuint texture, float left,
	float top, float width, float height, color c)
{
	int vx, vy, vw, vh;
	ctx2d_target_impl* tar = reinterpret_cast
		<ctx2d_target_impl*>(_target.get());
	tar->get_viewport(vx, vy, vw, vh);

	float cw = float(vw);
	float ch = float(vh);

	// this is equal to modelview matrix
	left = left * (2 / cw) - 1;
	top = -top * (2 / ch) + 1;
	float w = float(width) * (2 / cw);
	float h = -float(height) * (2 / ch);

	//	-1, -1,  0,  1,
	//	-1,  1,  0,  0,
	//	1,  1,  1,  0,
	//	1, -1,  1,  1,

	GLfloat vertexs[] = {
		left, top + h,  0,  1,
		left, top,  0,  0,
		left + w, top,  1,  0,
		left + w, top + h,  1,  1,
	};

	shader_mgr* sdrmgr = shader_mgr::inst();
	shader* s = sdrmgr->getshader(NULL, "shadow-texshader");
	s->activate();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	GLint utex = s->get_varible(glslvar_u_srctex);
	assert(utex >= 0);
	glUniform1i(utex, 0);

	GLint ufillclr = s->get_varible(glslvar_u_fillcolor);
	if (ufillclr >= 0) glUniform4f(ufillclr, c.r(), c.g(), c.b(), c.a());

	GLint apos = s->get_varible(glslvar_a_position);
	assert(apos >= 0);
	glVertexAttribPointer(apos, 2, GL_FLOAT, GL_FALSE,
		4 * sizeof(GLfloat), vertexs);
	glEnableVertexAttribArray(apos);

	GLint atex = s->get_varible(glslvar_a_srctexcord);
	assert(atex >= 0);
	glVertexAttribPointer(atex, 2, GL_FLOAT, GL_FALSE,
		4 * sizeof(GLfloat), (GLfloat*)vertexs + 2);
	glEnableVertexAttribArray(atex);

	GLshort indices[] = { 0, 1, 2, 2, 3 };

	glEnable(GL_BLEND); 
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDrawElements(GL_TRIANGLE_FAN, 5, GL_UNSIGNED_SHORT, (GLshort*)indices);
	glDisable(GL_BLEND);
	return 0;
}

void ctx2d_impl::reset_offscreen(void)
{
	ctx2d_target_impl* target = reinterpret_cast
		<ctx2d_target_impl*>(_target.get());
	if (!target) return;

	if (target->gettype() == render_target_to_screen) {
		// switch back to default on-screen rendering
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
}

int context2d::drawimage(image img, int left, int top,
	int width, int height)
{
	ctx2d_impl* ctx = reinterpret_cast<ctx2d_impl*>(this);
	image_impl* imgimp = reinterpret_cast<image_impl*>(img.get());
	if (!ctx || !imgimp) return -EBADPARM;

	return imgimp->render(ctx, left, top, width, height);
}

int context2d::stroke(void)
{
	ctx2d_impl* ctx = reinterpret_cast<ctx2d_impl*>(this);
	if (!ctx->valid_check()) {
		return -EINVALID;
	}
	if (NULL == ctx->_curr_shape) {
		return -ENOTEXISTS;
	}
	// see if the object status is ok
	// getinfo return the current status of
	// the shape object
	int stat = ctx->_curr_shape->getinfo();
	if (stat != 0) {
		return stat;
	}
	ctx->_curr_shape->stroke();
	return 0;
}

void context2d::clear(const color c)
{
	ctx2d_impl* ctx = reinterpret_cast<ctx2d_impl*>(this);
	if (!ctx->valid_check()) return;

	glClearColor(c.r(), c.g(), c.b(), c.a());
	glClear(GL_COLOR_BUFFER_BIT);
}

int context2d::shadow(void)
{
	ctx2d_impl* ctx = reinterpret_cast<ctx2d_impl*>(this);
	if (!ctx->valid_check()) {
		return -EINVALID;
	}
	if (NULL == ctx->_curr_shape
		|| ctx->_f.shape_released) {
		return -ENOTAVAIL;
	}
	int stat = ctx->_curr_shape->getinfo();
	if (stat != 0) {
		return stat;
	}
	ctx->draw_shadow();
	return 0;
}

int context2d::fill(void)
{
	ctx2d_impl* ctx = reinterpret_cast<ctx2d_impl*>(this);
	if (!ctx->valid_check()) {
		return -EINVALID;
	}
	if (NULL == ctx->_curr_shape) {
		return -ENOTEXISTS;
	}
	int stat = ctx->_curr_shape->getinfo();
	if (stat != 0) {
		return stat;
	}
	ctx->_curr_shape->fill();
	return 0;
}

int context2d::rect(float left, float top, float width,
	float height, float radius)
{
	ctx2d_impl* ctx = reinterpret_cast<ctx2d_impl*>(this);
	if (!ctx->valid_check()) {
		return -EINVALID;
	}
	shape_create_rect(ctx, left, top, width, height, radius);
	assert(NULL != ctx->_curr_shape);
	return ctx->_curr_shape->getinfo();
}

ctx2d_target context2d::set_target(ctx2d_target_type type, int width, int height)
{
	ctx2d_impl* ctx = reinterpret_cast<ctx2d_impl*>(this);
	ctx2d_target ret = ctx->_target;
	if (width <= 0 || height <= 0 || type == ctx2d_target_type_unknown) {
		return ret;
	}

	ctx2d_target_impl* tar = new ctx2d_target_impl(type, width, height);
	assert(NULL != tar);
	ctx->_target = reinterpret_cast<ctx2d_target_object*>(tar);
	ctx->attach_target();
	return ret;
}

int ctx2d_impl::attach_target(void)
{
	int x, y, width, height;

	// check if we are binding with the same thread
	unsigned long int thdid = gettid();
	if (_threadid) {
		if (thdid != _threadid)
			return -1;
	}
	else if (!__sync_bool_compare_and_swap(
		&_threadid, 0, thdid)) {
		return -2;
	}

	ctx2d_target_impl* tar = reinterpret_cast
		<ctx2d_target_impl*>(_target.get());

	reset_offscreen();
	if (tar->gettype() == render_target_to_image) {
		if (enable_offscreen())
			return -3;
	}

	tar->get_viewport(x, y, width, height);
	glViewport(x, y, width, height);

	// set the modelview
	int ret = update_viewmodel(width, height);
	if (ret < 0) return ret;

	_f.attached = 1;
	return 0;
}

void ctx2d_impl::lock(void)
{
	if (!_f.thread_safe)
		return;
	gblconfig::inst()->gles_lock();
}

void ctx2d_impl::unlock(void)
{
	if (!_f.thread_safe)
		return;
	gblconfig::inst()->gles_unlock();
}

ctx2d_target_impl::ctx2d_target_impl(int type, int width, int height)
: _refcnt(0)
, _flags(0)
, _texture(0)
, _x(0), _y(0), _width(width), _height(height)
, _curwidth(width), _curheight(height)
, _texwidth(-1)
, _texheight(-1)
{
	_f.target_type = type;
	if (type == render_target_to_image)
	{
		_texwidth = TEXTURE_WIDTH(width);
		_texheight = TEXTURE_WIDTH(height);
		_texture = ctx2d_impl::create_texture(_texwidth, _texheight);
	}
}

ctx2d_target_impl::~ctx2d_target_impl()
{
	if (_f.target_type == render_target_to_image) {
		glDeleteTextures(1, &_texture);
	}
}

int ctx2d_target_object::addref(void)
{
	ctx2d_target_impl* target = reinterpret_cast
		<ctx2d_target_impl*>(this);
	return target->addref();
}

int ctx2d_target_impl::release(void)
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0) delete this;
	return cnt;
}

int ctx2d_target_object::release(void)
{
	ctx2d_target_impl* target = reinterpret_cast
		<ctx2d_target_impl*>(this);
	return target->release();
}

}} // end of namespace zas::hwgrph
/* EOF */
