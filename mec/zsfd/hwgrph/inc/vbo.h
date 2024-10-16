/** @file vbo.h
 * declaration of vbo object
 */

#ifndef __CXX_ZAS_HWGRPH_VBO_H__
#define __CXX_ZAS_HWGRPH_VBO_H__

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include "hwgrph/hwgrph.h"

namespace zas {
namespace hwgrph {

extern EGLContext rsrc_ctx;

enum rsrcbuf_const {
	ARRBUF_SIZE = 512,
	ELEM_ARRBUF_SIZE = 512,
};

struct rsrcbuf
{
	rsrcbuf();
	~rsrcbuf();

	GLfloat* arrbuf;
	short* element_arrbuf;

	int arrbuf_cursz;
	int arrbuf_totalsz;
	int elem_arrbuf_cursz;
	int elem_arrbuf_totalsz;

	int arrbuf_add(float* buf, int count);
	int elem_arrbuf_add(short* buf, int count);

	int arrbuf_pos(void) {
		return arrbuf_cursz;
	}

	int elem_arrbuf_pos(void) {
		return elem_arrbuf_cursz;
	}
};

class bscvbo
{
public:
	bscvbo();
	~bscvbo();
	static bscvbo* inst(void);

	void init();

private:
	void register_rectdata(rsrcbuf& buf);

private:
	struct {
		int rect_arr;	// pos for rect array offset
		int rect_elem;	// pos for rect element offset
	} _pos;

	// [0] - vertex_buffer
	// [1] - indice_buffer
	GLuint _vboid[2];
	ZAS_DISABLE_EVIL_CONSTRUCTOR(bscvbo);
};

}} // end of namespace zas::hwgrph
#endif // __CXX_ZAS_HWGRPH_VBO_H__
/* EOF */
