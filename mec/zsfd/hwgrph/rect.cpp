/** @file rect.cpp
 * implementation of drawing rectangle
 */

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include <math.h>
#include "inc/image.h"
#include "inc/shader.h"
#include "inc/grphgbl.h"
#include "inc/ctx2d-impl.h"

namespace zas {
namespace hwgrph {

class shape_rect : public shape
{
public:
	shape_rect(ctx2d_impl* ctx, float x, float y,
		float w, float h, float radius)
	: _ctx(ctx)
	, _status(0)
	, _flags(0)
	, _radius(radius)
	, _frag_cnt(0)
	{
		assert(NULL != _ctx);
		
		// check if the parameter is valid
		if (w <= 0 || h <= 0) {
			_status = -EBADPARM;
			return;
		}

		ctx->set_shape_wraprect(x, y, w, h);

		if (float_is0(radius)) {
			vertex_rect(x, y, w, h);
		}
		else {
			_f.roundrect = 1;
			vertex_roundrect(x, y, w, h, radius);
		}
	}

	~shape_rect()
	{
	}

	void release(void) {
		delete this;
	}

	int getinfo(int* type) {
		if (type) *type = shape_type_rect;
		return _status;
	}

	int shadow(void)
	{
		if (_status) return _status;
		if (!_f.data_committed) {
			vertexbuf_commit();
		}
		fillrect();
		return 0;
	}

	int stroke(void)
	{
		if (_status) return _status;
		int ret = _ctx->activate_program();
		if (ret < 0) return ret;
		if (ret > 0 || !_f.data_committed)
			vertexbuf_commit();

		// enable varible
		GLint apos = shader_mgr::inst()->current(_ctx->get_context())
			->get_varible(glslvar_a_position);
		assert(apos >= 0);
		glEnableVertexAttribArray(apos);

		if (_radius > 0) {
			// round rectangle
			assert(_frag_cnt != 0);
			roundrect_stroke();
		}
		else {
			// rectangle
			glDrawArrays(GL_LINE_LOOP, 0, 4);
		}

		// finished, disable the varible
		glDisableVertexAttribArray(apos);
		return 0;
	}

	int fill(void)
	{
		if (_status) return _status;
		int ret = _ctx->activate_program();
		if (ret < 0) return ret;
		if (ret > 0 || !_f.data_committed)
			vertexbuf_commit();

		fillrect();
		return 0;
	}

private:

	void fillrect(void)
	{
		// enable varible
		GLint apos = shader_mgr::inst()->current(_ctx->get_context())
			->get_varible(glslvar_a_position);
		assert(apos >= 0);
		glEnableVertexAttribArray(apos);

		if (_radius > 0) {
			assert(_frag_cnt != 0);
			roundrect_fill();
		}
		else {
			GLshort indices[] = { 0, 3, 2, 0, 2, 1 };
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, &indices);
		}

		// finished, disable the varible
		glDisableVertexAttribArray(apos);
	}

	void vertexbuf_commit(void)
	{
		assert(!_status);
		gvertexbuf& vbuf = _ctx->get_vertexbuf();
		shader_mgr* sdrmgr = shader_mgr::inst();
		shader* s = sdrmgr->current(_ctx->get_context());

		GLint apos = s->get_varible(glslvar_a_position);
		if (apos >= 0) glVertexAttribPointer(apos, 2, GL_FLOAT,
			GL_FALSE, 0, vbuf.buffer());
	
		// indicate that we have committed
		_f.data_committed = 1;
	}

	void vertex_rect(float x, float y, float w, float h)
	{
		gvertexbuf& vbuf = _ctx->get_vertexbuf();
		vbuf.clear();
		vbuf.add(vtx(x, y));
		vbuf.add(vtx(x + w, y));
		vbuf.add(vtx(x + w, y + h));
		vbuf.add(vtx(x, y + h));
	}

	void vertex_roundrect(float x, float y, float w, float h, float radius)
	{
		float d = radius * 2;
		if (d > w || d > h) {
			_status = -EBADPARM;
			return;
		}

		gvertexbuf& vbuf = _ctx->get_vertexbuf();
		gindicebuf& ibuf = _ctx->get_indicebuf();
		vbuf.clear();
		ibuf.clear();		

		if (float_equal(d, h)) {
			_f.hbar = 1;
			vbuf.add(vtx(x + radius, y + radius));
			vbuf.add(vtx(x + w - radius, y + radius));
		}
		if (float_equal(d, w)) {
			vbuf.clear();
			if (_f.hbar) {
				// it is a cycle
				_f.hbar = _f.vbar = 0;
				_f.circle = 1;
				vbuf.add(vtx(x + radius, y + radius));
				_frag_cnt = int(_radius);
				if (_frag_cnt < 12) _frag_cnt = 12;

				vertex_arc(x + radius, y + radius,
					radius, 0, M_PI * 2, _frag_cnt);

				// create line indices
				for (int i = 0; i <= _frag_cnt + 1; ++i)
					ibuf.add(short(i));
			}
			else {
				// it is a vbar
				_f.vbar = 1;
				vbuf.add(vtx(x + radius, y + radius));
				vbuf.add(vtx(x + radius, y + h - radius));

				// recheck the indice count
				_frag_cnt = int(_radius / 2);
				if (_frag_cnt < 7) _frag_cnt = 7;

				vertex_arc(x + radius, y + radius,
					radius, M_PI, 0, _frag_cnt);
				vertex_arc(x + radius, y + h - radius,
					radius, 0, -M_PI, _frag_cnt);

				// create line indices
				for (int i = 0, c = (_frag_cnt + 1) * 2; i < c; ++i)
					ibuf.add(short(i + 2));

				// create triangle indices
				_triangle_indice_start = ibuf.getsize();
				for (int i = 0; i < _frag_cnt; ++i) 
				{
					ibuf.add(0);
					ibuf.add(short(i + 2));
					ibuf.add(short(i + 3));
				}
				//  **
				//   *
				ibuf.add(short(2));
				ibuf.add(short(_frag_cnt + 2));
				ibuf.add(short(_frag_cnt + 3));
				//  *
				//  **
				ibuf.add(short(2));
				ibuf.add(short(_frag_cnt + 3));
				ibuf.add(short((_frag_cnt + 1) * 2 + 1));

				for (int i = 0, j = _frag_cnt + 3; i < _frag_cnt; ++i) 
				{
					ibuf.add(1);
					ibuf.add(short(i + j));
					ibuf.add(short(i + j + 1));
				}
			}
		}
		else if (_f.hbar) {
			// it is an hbar, and its vertex has added
			// recheck the indice count
			_frag_cnt = int(_radius / 2);
			if (_frag_cnt < 7) _frag_cnt = 7;

			vertex_arc(x + radius, y + radius,
				radius, M_PI + M_PI_2, M_PI_2, _frag_cnt);
			vertex_arc(x + w - radius, y + radius,
				radius, M_PI_2, -M_PI_2, _frag_cnt);

			// create line indices
			for (int i = 0, c = (_frag_cnt + 1) * 2; i < c; ++i)
				ibuf.add(short(i + 2));

			// create triangle indices
			_triangle_indice_start = ibuf.getsize();
			for (int i = 0; i < _frag_cnt; ++i) 
			{
				ibuf.add(0);
				ibuf.add(short(i + 2));
				ibuf.add(short(i + 3));
			}
			//  **
			//  *
			ibuf.add(short(2));
			ibuf.add(short(_frag_cnt + 2));
			ibuf.add(short(_frag_cnt + 3));
			//   *
			//  **
			ibuf.add(short(_frag_cnt + 3));
			ibuf.add(short((_frag_cnt + 1) * 2 + 1));
			ibuf.add(short(2));

			for (int i = 0, j = _frag_cnt + 3; i < _frag_cnt; ++i) 
			{
				ibuf.add(1);
				ibuf.add(short(i + j));
				ibuf.add(short(i + j + 1));
			}
		}
		else {
			// it is a roundrect
			vbuf.add(vtx(x + radius, y + radius));
			vbuf.add(vtx(x + w - radius, y + radius));
			vbuf.add(vtx(x + w - radius, y + h - radius));
			vbuf.add(vtx(x + radius, y + h - radius));

			// recheck the indice count
			_frag_cnt = int(_radius / 4);
			if (_frag_cnt < 4) _frag_cnt = 4;

			vertex_arc(x + radius, y + radius, radius,
				M_PI, M_PI_2, _frag_cnt);
			vertex_arc(x + w - radius, y + radius, radius,
				M_PI_2, 0, _frag_cnt);
			vertex_arc(x + w - radius, y + h - radius, radius,
				0, -M_PI_2, _frag_cnt);
			vertex_arc(x + radius, y + h - radius, radius,
				-M_PI_2, -M_PI, _frag_cnt);
			
			// create line indices
			for (int i = 0, c = (_frag_cnt + 1) * 4; i < c; ++i)
				ibuf.add(short(i + 4));

			// create triangle indices
			_triangle_indice_start = ibuf.getsize();
			for (int i = 0; i < _frag_cnt; ++i) 
			{
				ibuf.add(0);
				ibuf.add(short(i + 4));
				ibuf.add(short(i + 5));
			}

			// **
			// *
			ibuf.add(short(_frag_cnt * 3 + 7));
			ibuf.add(short(_frag_cnt + 4));
			ibuf.add(short(_frag_cnt + 5));
			//  *
			// **
			ibuf.add(short(_frag_cnt + 5));
			ibuf.add(short(_frag_cnt * 3 + 6));
			ibuf.add(short(_frag_cnt * 3 + 7));

			for (int i = 0; i < _frag_cnt; ++i)
			{
				ibuf.add(1);
				ibuf.add(short(i + _frag_cnt + 5));
				ibuf.add(short(i + _frag_cnt + 6));
			}
			// **
			// *
			ibuf.add(1);
			ibuf.add(short(_frag_cnt * 2 + 5));
			ibuf.add(2);
			//  *
			// **
			ibuf.add(short(_frag_cnt * 2 + 5));
			ibuf.add(short(_frag_cnt * 2 + 6));
			ibuf.add(2);

			for (int i = 0; i < _frag_cnt; ++i)
			{
				ibuf.add(2);
				ibuf.add(short(i + _frag_cnt * 2 + 6));
				ibuf.add(short(i + _frag_cnt * 2 + 7));
			}

			for (int i = 0; i < _frag_cnt; ++i)
			{
				ibuf.add(3);
				ibuf.add(short(i + _frag_cnt * 3 + 7));
				ibuf.add(short(i + _frag_cnt * 3 + 8));
			}
			//  *
			// **
			ibuf.add(0);
			ibuf.add(3);
			ibuf.add(short(_frag_cnt * 4 + 7));
			// **
			// *
			ibuf.add(short(_frag_cnt * 4 + 7));
			ibuf.add(0);
			ibuf.add(4);
		}
	}

	void vertex_arc(float x, float y, float r,
		float from, float to, int steps)
	{
		gvertexbuf& vbuf = _ctx->get_vertexbuf();
		float delta = (to - from) / float(steps);

		for (int i = 0; i < steps; ++i, from += delta)
		{
			vbuf.add(vtx(
				x + r * cos(from),
				y - r * sin(from)
			));
		}
		// add last vertex
		vbuf.add(vtx(x + r * cos(to), y - r * sin(to)));
	}

	void roundrect_stroke(void)
	{
		gindicebuf& ibuf = _ctx->get_indicebuf();
		if (_f.hbar || _f.vbar) {
			glDrawElements(GL_LINE_LOOP, (_frag_cnt + 1) * 2,
				GL_UNSIGNED_SHORT, ibuf.buffer());
		}
		else if (_f.circle) {
			glDrawElements(GL_LINE_LOOP, _frag_cnt,
				GL_UNSIGNED_SHORT, ibuf.buffer() + 1);
		}
		else {
			glDrawElements(GL_LINE_LOOP, (_frag_cnt + 1) * 4,
				GL_UNSIGNED_SHORT, ibuf.buffer());
		}
	}

	void roundrect_fill(void)
	{
		gindicebuf& ibuf = _ctx->get_indicebuf();
		if (_f.hbar || _f.vbar) {
			glDrawElements(GL_TRIANGLES,
			ibuf.getsize() - _triangle_indice_start,
			GL_UNSIGNED_SHORT,
			ibuf.buffer() + _triangle_indice_start);
		}
		else if (_f.circle) {
			glDrawElements(GL_TRIANGLE_FAN, ibuf.getsize(),
			GL_UNSIGNED_SHORT, ibuf.buffer());
		}
		else glDrawElements(GL_TRIANGLES,
			ibuf.getsize() - _triangle_indice_start,
			GL_UNSIGNED_SHORT,
			ibuf.buffer() + _triangle_indice_start);
	}

private:
	ctx2d_impl* _ctx;
	int _status;
	union {
		uint32_t _flags;
		struct {
			uint32_t roundrect : 1;
			uint32_t hbar : 1;
			uint32_t vbar : 1;
			uint32_t circle : 1;
			uint32_t data_committed : 1;
		} _f;
	};
	float _radius;
	int _frag_cnt;
	int _triangle_indice_start;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(shape_rect);
};

shape* shape_create_rect(ctx2d_impl* ctx, float x, float y,
	float w, float h, float radius)
{
	return new(ctx) shape_rect(ctx, x, y, w, h, radius);
}

}} // end of namespace zas::utils
/* EOF */
