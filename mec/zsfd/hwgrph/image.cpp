/** @file image.cpp
 * implementation of image rendering features
 */

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "utils/mutex.h"

#include "hwgrph/imageloader.h"
#include "inc/shader.h"
#include "inc/ctx2d-impl.h"
#include "inc/image.h"

using namespace std;
using namespace zas::utils;

namespace zas {
namespace hwgrph {

static mutex mut;
static avl_node_t* image_tree = NULL;
static listnode_t active_list = LISTNODE_INITIALIZER(active_list);
static listnode_t inactive_list = LISTNODE_INITIALIZER(inactive_list);

// statistic data
static uint32_t managed_image_count = 0;
static uint32_t non_managed_image_count = 0;
static uint32_t managed_image_total_size = 0;

image_impl::image_impl(int width, int height,
	const char* uri)
: _width(width), _height(height)
, _membuf(NULL)
, _flags(0)
, _lrucnt(0)
, _refcnt(0)
{
	memset(_uri_digest, 0, 32);
	uint32_t bufsz = width * height * HWGRPH_BPP;
	_membuf = new uint8_t [bufsz];
	assert(NULL != _membuf);
	_f.membuf_allocated = 1;

	// add the the list and tree
	mut.lock();
	listnode_add(active_list, _ownerlist);
	int ret = avl_insert(&image_tree, &_avlnode, uri_compare);
	assert(ret == 0);

	++managed_image_count;
	mut.unlock();
}

image_impl::image_impl(GLuint texture, int width, int height)
: _width(width), _height(height)
, _texture(texture)
, _flags(0)
, _lrucnt(0)
, _refcnt(0)
{
	_f.texturized = 1;
	_f.external_texture = 1;
	__sync_add_and_fetch(&non_managed_image_count, 1);	
}

image_impl::~image_impl()
{
	if (_f.membuf_allocated)
	{
		if (_f.texturized || _f.external_texture) {
			printf("image_impl: fatal error of flag combination.\n");
			exit(9960);
		}
		if (_membuf) delete [] (uint8_t*)_membuf;
		_membuf = NULL;
	}

	if (_f.texturized && !_f.external_texture)
		release_texture();
	_texture = 0;

	// update statistic data for managed image count
	if (_f.external_texture)
		__sync_sub_and_fetch(&non_managed_image_count, 1);
	else __sync_sub_and_fetch(&managed_image_count, 1);
}

void image_impl::release_texture(void)
{
	glDeleteTextures(1, &_texture);
	_f.texturized = 0;

	// update statistic data (memory size)
	uint32_t bufsz = _width * _height * HWGRPH_BPP;
	__sync_sub_and_fetch(&managed_image_total_size, bufsz);
}

int image_impl::texturize(void)
{
	if (_f.texturized) return 0;
	if (_f.external_texture) return 0;
	if (!_f.membuf_allocated) return -1;

	void* buf = _membuf;
	_texture = ctx2d_impl::create_texture(_width, _height,
		GL_RGBA, buf);
	__sync_add_and_fetch(&managed_image_total_size,
		_width * _height * HWGRPH_BPP);
	
	delete [] (uint8_t*)buf;
	_f.membuf_allocated = 0;
	_f.texturized = 1;
	return 0;
}

int image_impl::uri_compare(avl_node_t* a, avl_node_t* b)
{
	image_impl* ba = AVLNODE_ENTRY(image_impl, _avlnode, a);
	image_impl* bb = AVLNODE_ENTRY(image_impl, _avlnode, b);
	int ret = memcmp(ba->_uri_digest, bb->_uri_digest,
		sizeof(ba->_uri_digest));
	if (ret < 0) return -1;
	else if (ret > 0) return 1;
	else return 0;
}

int image_impl::addref(void)
{
	int ret = __sync_add_and_fetch(&_refcnt, 1);
	if (_f.external_texture) {
		return ret;
	}

	if (_f.inactive) {
		auto_mutex am(mut);
		if (!_f.inactive) return ret;
		listnode_del(_ownerlist);
		listnode_add(active_list, _ownerlist);
		_f.inactive = 0;
	}
	return ret;
}

int image_impl::release(void)
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt > 0) return cnt;

	// if this is external texture, we directly
	// release the object
	if (_f.external_texture) {
		delete this;
		return cnt;
	}

	// we move this to inactive list
	auto_mutex am(mut);
	if (_f.inactive) return cnt;
	listnode_del(_ownerlist);
	listnode_add(inactive_list, _ownerlist);
	_f.inactive = 1;

	return cnt;
}

int image_object::addref(void)
{
	image_impl* ii = reinterpret_cast<image_impl*>(this);
	return ii->addref();
}

int image_object::release(void)
{
	image_impl* ii = reinterpret_cast<image_impl*>(this);
	return ii->release();
}

image_impl* image_impl::getimage(const uri& u)
{
	uri uri(u);
	int ret = absfile_normalize_uri(uri);
	if (ret) return NULL;

	uint8_t dgst[32];
	uri.digest(dgst);

	// search if we have this uri
	mut.lock();
	avl_node_t* node = avl_find(image_tree,
		MAKE_FIND_OBJECT(dgst[0], image_impl, _uri_digest, _avlnode),
		uri_compare);
	mut.unlock();
	if (NULL == node) return NULL;

	image_impl* imgimp = AVLNODE_ENTRY(image_impl, _avlnode, node);
	return imgimp;
}

int image_impl::loadimage(image_loader* loader, absfile* file, int width, int height)
{
	if (_f.external_texture)
		return -1;

	int ret = texturized();
	if (ret < 0) {
		printf("image_impl::loadimage: wrong texture flags.\n");
		exit(9950);
	}

	if (!ret) release_texture();

	if (!_f.membuf_allocated) {
		_membuf = new uint8_t [width * height * HWGRPH_BPP];
		if (NULL == _membuf) return -2;
		_f.membuf_allocated = 1;
	}
	else if (width != _width || height != _height) {
		// release the previous memory
		delete [] (uint8_t*)_membuf;
		// allocate in new size
		_membuf = new uint8_t [width * height * HWGRPH_BPP];
		if (NULL == _membuf) return -3;
	}

	if (loader->load_membuffer(file, get_membuf(), width, height))
		return -ELOGIC;
	if (texturize()) return -ELOGIC;

	return 0;
}

int image_impl::render(ctx2d_impl* ctx, float left, float top, int width, int height)
{
	if (!width && !height) {
		// indicate using default size
		width = _width; height = _height;
	}
	if (!width || !height) {
		return -EBADPARM;
	}

	int vx, vy, vw, vh;
	ctx2d_target_impl* tar = reinterpret_cast
		<ctx2d_target_impl*>(ctx->_target.get());
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
	shader* s = sdrmgr->getshader(NULL, "default-texshader");
	s->activate();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, _texture);

	GLint utex = s->get_varible(glslvar_u_srctex);
	assert(utex >= 0);
	glUniform1i(utex, 0);

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
	glDrawElements(GL_TRIANGLE_FAN, 5, GL_UNSIGNED_SHORT, (GLshort*)indices);

	_lrucnt += IMAGE_LRU_CNT_ACCESS;
	return 0;
}

}} // end of namespace zas::hwgrph
/* EOF */
