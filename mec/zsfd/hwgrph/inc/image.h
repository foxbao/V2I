/** @file image.h
 * Definition of image rendering features
 */

#ifndef __CXX_ZAS_HWGRPH_IMAGE_H__
#define __CXX_ZAS_HWGRPH_IMAGE_H__

#include <string>
#include <GLES2/gl2.h>

#include "std/list.h"
#include "utils/avltree.h"
#include "utils/absfile.h"

namespace zas {
namespace hwgrph {

class ctx2d_impl;

#define IMAGE_LRU_CNT_ACCESS	(16)

zas_interface image_loader;

class image_impl
{
	friend class image_object;
public:
	image_impl(int width, int height, const char* uri);
	image_impl(GLuint texture, int width, int height);
	~image_impl();

	int addref(void);
	int release(void);

	static image_impl* getimage(const zas::utils::uri& u);
	
	int texturize(void);
	
	// load image by image_loader
	int loadimage(image_loader* loader, zas::utils::absfile* file,
		int width, int height);

	// render to eglcontext
	int render(ctx2d_impl* ctx, float left, float top, int width, int height);

public:
	void* get_membuf(void) {
		if (!_f.membuf_allocated)
			return NULL;
		return _membuf;
	}

	int texturized(void) {
		if (_f.external_texture) {
			// must be an error
			return -1;
		}
		return (_f.texturized) ? 0 : 1;
	}

private:
	static int uri_compare(zas::utils::avl_node_t*,
		zas::utils::avl_node_t*);

	// release the texture and update statistics
	void release_texture(void);

private:
	listnode_t _ownerlist;
	zas::utils::avl_node_t _avlnode;

	union {
		uint32_t _flags;
		struct {
			uint32_t inactive : 1;
			uint32_t texturized : 1;
			uint32_t membuf_allocated : 1;
			
			// means this bitmap holds an external
			// generated texture that shll not be
			// collected by image garbage collection
			uint32_t external_texture : 1;
		} _f;
	};
	union {
		GLuint _texture;
		void* _membuf;
	};

	uint8_t _uri_digest[32];
	uint32_t _lrucnt;
	int _width, _height;
	int _refcnt;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(image_impl);
};

}} // end of namespace zas::hwgrph
#endif // __CXX_ZAS_HWGRPH_IMAGE_H__
/* EOF */
