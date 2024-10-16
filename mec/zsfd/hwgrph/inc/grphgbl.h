/** @file grphgbl.h
 * declaration of the global data and object for grph library
 */

#ifndef __CXX_ZAS_HWGRPH_GRPH_GLOBAL_H__
#define __CXX_ZAS_HWGRPH_GRPH_GLOBAL_H__

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "utils/wait.h"
#include "utils/evloop.h"
#include "utils/mutex.h"
#include "fontcache.h"

using namespace zas::utils;

namespace zas {
namespace hwgrph {

struct textchar;

#define GAUSS_BLUR_RADIUS_MIN	(2)
#define GAUSS_BLUR_RADIUS_MAX	(15)

class gblconfig
{
public:
	gblconfig();
	~gblconfig();
	static gblconfig* inst(void);

public:
	int bind(EGLDisplay dpy, EGLConfig cfg, EGLContext ctx);
	int get_gauss_weight(int count, double*& offsets, double*& weights);

public:
	EGLContext get_shared_context(void) {
		return _rsrc_ctx;
	}

	void glRenderbufferStorageMultisample(GLenum target, GLsizei samples,
		GLenum internalformat, GLsizei width, GLsizei height)
	{
		if (!_f.RTT_multisampleing_supported) return;
		if (_f.IMG_render_to_texture) glRenderbufferStorageMultisampleIMG
			(target, samples, internalformat, width, height);
		else if (_f.EXT_render_to_texture) glRenderbufferStorageMultisampleEXT
			(target, samples, internalformat, width, height);
	}

	void glFramebufferTexture2DMultisample(GLenum target, GLenum attachment,
		GLenum textarget, GLuint texture, GLint level, GLsizei samples)
	{
		if (!_f.RTT_multisampleing_supported) return;
		if (_f.IMG_render_to_texture) glFramebufferTexture2DMultisampleIMG
			(target, attachment, textarget, texture, level, samples);
		else if (_f.EXT_render_to_texture) glFramebufferTexture2DMultisampleEXT
			(target, attachment, textarget, texture, level, samples);
	}

	bool RTT_multisampleing_supported(void) {
		return _f.RTT_multisampleing_supported;
	}

	bool binary_shader_supported(void) {
		return _f.binary_shader_supported;
	}

	void glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary) {
		if (_glGetProgramBinaryOES) _glGetProgramBinaryOES(program, bufSize, length, binaryFormat, binary);
	}

	void glProgramBinary(GLuint program, GLenum binaryFormat, const void *binary, GLint length) {
		if (_glProgramBinaryOES) _glProgramBinaryOES(program, binaryFormat, binary, length);
	}

	fontcacher_impl* get_global_fontcacher(void) {
		return &_gblfontcacher;
	}

	void gles_lock(void) {
		_gles_mut.lock();
	}

	void gles_unlock(void) {
		_gles_mut.unlock();
	}

	zas::utils::mutex& get_gles_mutex(void) {
		return _gles_mut;
	}

private:
	int create_rsrc_context(void);
	bool check_FBO_multisample_support(void);
	bool check_binary_shader_support(void);

	class rsrc_context_creator : public evloop_task
	{
	public:
		rsrc_context_creator() : _ret(0) {}

		void run(void);
		int execute(void);
		void release(void);

	private:
		int _ret;
		waitobject _wobj;
		gblconfig* self(void) {
			return (gblconfig*)((size_t)this - 
			(size_t)(&(((gblconfig*)0)->_rcc)));
		}
	} _rcc;

private:
	EGLContext _shared_ctx;
	EGLConfig _shared_cfg;
	EGLDisplay _shared_dpy;

	EGLContext _rsrc_ctx;
	zas::utils::mutex _gles_mut;

	union {
		uint32_t _flags;
		struct {
			uint32_t RTT_multisampleing_supported : 1;
			uint32_t IMG_render_to_texture : 1;
			uint32_t EXT_render_to_texture : 1;
			uint32_t binary_shader_supported : 1;
		} _f;
	};
	union {
		PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC glRenderbufferStorageMultisampleIMG;
		PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT;
	};
	union {
		PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC glFramebufferTexture2DMultisampleIMG;
		PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT;
	};
	PFNGLGETPROGRAMBINARYOESPROC _glGetProgramBinaryOES;
	PFNGLPROGRAMBINARYOESPROC _glProgramBinaryOES;
	static double _guass_weight_table[];
	fontcacher_impl _gblfontcacher;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(gblconfig);
};

}} // end of namespace zas::hwgrph
#endif // __CXX_ZAS_HWGRPH_GRPH_GLOBAL_H__
/* EOF */
