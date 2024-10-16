/** @file vbo.cpp
 * implementation of vbo object
 */

#include "inc/vbo.h"

namespace zas {
namespace hwgrph {

rsrcbuf::rsrcbuf() {
	memset(this, 0, sizeof(rsrcbuf));
}

rsrcbuf::~rsrcbuf()
{
	free(arrbuf);
	free(element_arrbuf);
}

static void buffer_add(void* src, void* &dst, int count,
	const int minsz, const int elesz, int& cursz, int& totalsz)
{
	int newsz = cursz + count;
	if (newsz > totalsz)
	{
		// expend the buffer
		for (; newsz > totalsz; totalsz += minsz);

		void* newbuf = (void*)malloc(totalsz * elesz);
		assert(NULL != newbuf);
		if (cursz) {
			memcpy(newbuf, dst, cursz * elesz);
			free(dst);
		}		
		dst = newbuf;
	}
	// copy the data if the space is enough
	memcpy((char*)dst + cursz * elesz, src, count * elesz);
	cursz = newsz;
}

int rsrcbuf::arrbuf_add(GLfloat* buf, int count)
{
	if (count <= 0) return -1;
	buffer_add(buf, (void*&)arrbuf, count, ARRBUF_SIZE,
		sizeof(GLfloat), arrbuf_cursz, arrbuf_totalsz);
	return 0;
}

int rsrcbuf::elem_arrbuf_add(short* buf, int count)
{
	if (count <= 0) return -1;
	buffer_add(buf, (void*&)element_arrbuf, count, ELEM_ARRBUF_SIZE,
		sizeof(short), elem_arrbuf_cursz, elem_arrbuf_totalsz);
	return 0;
}

bscvbo::bscvbo()
{
	memset(&_pos, 0, sizeof(_pos));
	_vboid[0] = _vboid[1] = 0;
}

bscvbo::~bscvbo()
{
}

bscvbo* bscvbo::inst(void)
{
	static bscvbo* _inst = NULL;

	if (!_inst) {
		// todo: need lock
		_inst = new bscvbo();
		assert(NULL != _inst);
	}
	return _inst;
}

void bscvbo::init(void)
{
	rsrcbuf buf;
	register_rectdata(buf);

	// create the vbo
	glGenBuffers(2, _vboid);

	// vertex buffer
	glBindBuffer(GL_ARRAY_BUFFER, _vboid[0]);
	glBufferData(GL_ARRAY_BUFFER, buf.arrbuf_cursz * sizeof(GLfloat),
		buf.arrbuf, GL_STATIC_DRAW);
	
	// indice buffer
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _vboid[1]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, buf.elem_arrbuf_cursz * sizeof(short),
		buf.element_arrbuf, GL_STATIC_DRAW);

	printf("%d, %d\n", _vboid[0], _vboid[1]);
}

void bscvbo::register_rectdata(rsrcbuf& buf)
{
	static const GLfloat rect_verts[4][2] = {
		{ -1, -1 },
		{ -1,  1 },
		{  1,  1 },
		{  1, -1 },
	};
	_pos.rect_arr = buf.arrbuf_pos();
	buf.arrbuf_add((GLfloat*)rect_verts[0], 4 * 2);

	static const short rect_indices[] = {
		0, 2, 1, 0, 3, 2,	// for fill
		0, 1, 2, 3,			// for stoking
	};
	_pos.rect_elem = buf.elem_arrbuf_pos();
	buf.elem_arrbuf_add((short*)rect_indices,
		sizeof(rect_indices));
}

}} // end of namespace zas::hwgrph
/* EOF */
