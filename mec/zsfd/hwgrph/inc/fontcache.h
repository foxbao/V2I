/** @file fontcache.h
 * declaration of the font cache related structures
 */

#ifndef __CXX_ZAS_HWGRPH_FONTCACHE_H__
#define __CXX_ZAS_HWGRPH_FONTCACHE_H__

#include <GLES2/gl2.h>

#include "std/list.h"
#include "utils/avltree.h"
#include "utils/mutex.h"

#include "gbuf.h"
#include "listree.h"

namespace zas {
namespace hwgrph {

struct textchar;
struct glyph_bitmap;
struct glyphinfo_freetype;

struct fontcache
{
	listnode_t ownerlist;
	int width;
	int height;
	zas::utils::avl_node_t* tree;
};

typedef li_listnode_<uint16_t> gblheaplist;
typedef li_avlnode_<uint16_t> gblheapavltree;

struct gblfontcache
{
	gblheaplist ownerlist;
	uint16_t width;
	uint16_t height;
	uint16_t tree;
} PACKED;

// glyph basic information (global heap version)
// these info could be used to calculate
// the width of a string line, etc
struct gblglyphcache
{
	gblheapavltree avlnode;
	uint16_t glyphcode;
	uint16_t font_ptr;

	float width;
	float height;
	float advance_x;
	float advance_y;
} PACKED;

// glyph basic information (local version)
// these info could be used to calculate
// the width of a string line, etc
struct glyphcache
{
	zas::utils::avl_node_t avlnode;
	uint16_t glyphcode;
	void* font;

	float width;
	float height;
	float advance_x;
	float advance_y;

	void* operator new(size_t sz);
	void operator delete(void* p);
};

union glyphcache_object
{
	gblglyphcache gbl;
	glyphcache lcl;
};

struct gblfontcache_header
{
	gblheaplist fcachelist;
};

struct gblfontcache_freenode
{
	uint16_t next;
};

union gblfontcache_object
{
	gblfontcache_header header;
	gblfontcache_freenode freenode;
	gblfontcache fcache;
	gblglyphcache gcache;
};

class fcache_localtexture_data
{
};

class fontcacher_impl
{
	friend class textline_impl;
public:
	fontcacher_impl();
	~fontcacher_impl();

public:

	struct glyphinfo
	{
		zas::utils::avl_node_t avlnode;
		glyphinfo* next;

		// glyph basic information
		glyphcache_object* gcache;

		struct avl_comparible {
			uint16_t glyphcode;
			uint16_t width;
			uint16_t height;
			void load(glyphcache_object* gc, bool fromgbl);
		} PACKED;

		avl_comparible comparible;

		union {
			struct {
				uint16_t texid;
				uint16_t x, y;
			} texinfo;
			glyph_bitmap* bitmap;
		};

		union {
			uint16_t flags;
			struct {
				uint16_t ready : 1;
				uint16_t global : 1;
				uint16_t gbmp_attached : 1;
			} f;
		};

		int refcnt;

		glyphinfo() = default;
		glyphinfo(glyphcache_object* gc, bool fromgbl);
		glyphinfo(glyphcache_object* gc, avl_comparible& c, bool fromgbl);

		int addref(void) {
			int ret = __sync_add_and_fetch(&refcnt, 1);
			assert(ret > 0);
			return ret;
		}

		int release(void) {
			int ret = __sync_sub_and_fetch(&refcnt, 1);
			assert(ret >= 0);
			return ret;
		}

		int add_as_pending_unlocked(fontcacher_impl* cacher, bool gbl)
		{
			int ret = avl_insert(&cacher->_glyphtree, &avlnode,
				avl_glyphinfo_compare);
			if (!ret) {
				if (gbl) {
					next = cacher->_gbl_pending;
					cacher->_gbl_pending = this;
				} else {
					next = cacher->_lcl_pending;
					cacher->_lcl_pending = this;
				}
			}
			return ret;
		}

		static int avl_glyphinfo_compare(zas::utils::avl_node_t* a,
			zas::utils::avl_node_t* b)
		{
			glyphinfo* aa = AVLNODE_ENTRY(glyphinfo, avlnode, a);
			glyphinfo* bb = AVLNODE_ENTRY(glyphinfo, avlnode, b);

			if (aa->f.global < bb->f.global) return -1;
			else if (aa->f.global > bb->f.global) return 1;

			// currently f.global is the same
			if (aa->f.global) {
				uint16_t aptr = aa->gcache->gbl.font_ptr;
				uint16_t bptr = bb->gcache->gbl.font_ptr;
				if (aptr < bptr) return -1;
				else if (aptr > bptr) return 1;
			} else {
				size_t afont = (size_t)aa->gcache->lcl.font;
				size_t bfont = (size_t)bb->gcache->lcl.font;
				if (afont < bfont) return -1;
				else if (afont > bfont) return 1;
			}

			// currently the font is the same
			// finially check the comparible object
			int ret = memcmp(&aa->comparible, &bb->comparible,
				sizeof(aa->comparible));
			if (!ret) return 0;
			else if (ret < 0) return -1;
			else return 1;
		}

		void* operator new(size_t sz);
		void operator delete(void* p);
	};

	struct texture_node
	{
		glyphinfo* glyphlist;
		GLuint texid;
		fcache_localtexture_data* data; 
	};

	typedef gbuf_<texture_node> texnodebuf;

	int addref(void) {
		return __sync_add_and_fetch(&_refcnt, 1);
	}
	int release(void);

	// this function will check if the relative glyphinfo
	// exists or not and create the glyphinfo it not
	glyphinfo* create_glyphinfo_via_glyphcache(
		glyphcache_object* gcache, bool gbl);

	// if all codes has been loaded, returns -1
	// otherwise, return the index of first unload code
	int load_glyphs(uint16_t* code_array, textchar* txlst,
		int unldlst, void* f);

	// add glyph
	// the function will not check if the glyph
	// already exists or not, caller shall make
	// sure the glyph not exists in the cacher
	int add_glyph(int code, glyphcache_object* gcache,
		glyphinfo_freetype* ginfo, void* font);

	void lock(void) { _mut.lock(); }
	void unlock(void) { _mut.unlock(); }

private:
	int create_texture_node_unlocked(void);

private:
	int _refcnt;
	int _active_texnode;
	texnodebuf _texbuf;
	zas::utils::mutex _mut;
	zas::utils::avl_node_t* _glyphtree;

	// the new glyphinfo will be added to the
	// following list respectively
	// different loading strategy will be put for
	// differnt list
	glyphinfo* _gbl_pending;
	glyphinfo* _lcl_pending;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(fontcacher_impl);
};

}} // end of namespace zas::hwgrph
#endif // __CXX_ZAS_HWGRPH_FONTCACHE_H__
/* EOF */
