/** @file fontmgr.h
 * declaration of the font related structures
 */

#ifndef __CXX_ZAS_HWGRPH_FONTMGR_H__
#define __CXX_ZAS_HWGRPH_FONTMGR_H__

#include <string>
#include <sys/types.h>
#include <unistd.h>

#include "std/list.h"
#include "utils/mutex.h"
#include "utils/shmem.h"
#include "utils/absfile.h"
#include "hwgrph.h"

#include "font_freetype.h"

namespace zas {
namespace hwgrph {

class fontcache;
class gblfontcache;
class gblglyphcache;
class gblfontcache_header;
union gblfontcache_object;

struct glyphcache;
struct gblglyphcache;
union glyphcache_object;

class glyphinfo_freetype;

class fontcacher_impl;
class textline_impl;
class textchar;

#define FONT_SHARED_DATA_AREA_SIZE	(8 * 1024 * 1024)	// 8M
#define FONT_SHARED_DATA_AREA_MAGIC	"zas-grph-fntdata"

#define FONTFACE_SHADOW		(1)

struct fontdata_outline
{
	// magic: shall be FONT_SHARED_DATA_AREA_MAGIC
	uint8_t magic[16];

	// the font mantainer process pid
	pid_t maintainer;

	// flags
	union {
		uint32_t flags;
		struct {
			uint32_t ready : 1;
		} f;
	};
	bool ready(void) { return (f.ready) ? true : false; }

	// the count for face and face_node
	uint16_t face_cnt;
	uint16_t face_node_cnt;

	// the list for glyph cache
	// the glyph may have 2 structures:
	// 1) gblfontcache
	// 2) gblglyphinfo
	// as the cache object may linked to font object
	// so here we only maintain the freelist
	uint16_t fontcache_freelist;

	// the relative position to the start of fontcache
	uint16_t startof_fcache;
	uint16_t fontcache_count;
	uint16_t glyphinfo_count;

	// how many fontcache object has been alocated
	// when all objects has been allocated, we may only
	// allocate new object from freelist, or try to free
	// some object before allocation
	uint16_t fontcache_alloc_tag;

	// the info of font glyph data area
	// it is relative to the start of global memory heap
	uint32_t startof_glyphdata;
	uint32_t glyphdata_total_size;
	uint32_t glyphdata_curpos;

	uint32_t glyphdata_total(void) {
		return FONT_SHARED_DATA_AREA_SIZE - startof_glyphdata - 1;
	}
	uint32_t glyphdata_avail(void) {
		return FONT_SHARED_DATA_AREA_SIZE - glyphdata_curpos - 1;
	}

	void* glyphdata_allocate_unlocked(size_t sz) {
		if (sz > glyphdata_avail()) return NULL;
		void* ret = (void*)(((size_t)this) + glyphdata_curpos);
		glyphdata_curpos += sz;
		return ret;
	}
};

#define GBLFONTFACE_EXISTS(x)	((x) != 0xFFFF)

struct gblfontface
{
	// const char*
	// this is relative to the first gblfontface object
	uint16_t facename;
	uint16_t faces[fts_count];

	void reset_faces(void) {
		for (int i = 0; i < fts_count; ++i)
			faces[i] = 0xFFFF;
	}
} PACKED;

struct gblfontface_node
{
	unsigned int faceid;
	// const char*
	// this is relative to the first gblfontface object
	uint16_t filename_ptr;
	uint16_t charset;

	// font cache header pointer
	uint16_t fcachehdr_ptr;
} PACKED;

#define UINT16_DELTA(x, base)	(uint16_t)(((size_t)(x)) - ((size_t)(base)))
#define TYPE_OFFSET(delta, base, type)	(type*)(((size_t)(base)) + ((size_t)(delta)))
#define TYPE_PTR(ptr, base, type) ((((size_t)(ptr)) - ((size_t)(base))) / sizeof(type))

#define INVALID_PTR(ptr)	((ptr) == 0xFFFF)

class font_impl;

class fontmgr
{
public:
	class fontfile
	{
		friend class fontmgr;
	public:
		fontfile(const zas::utils::uri& uri);
		~fontfile();

		bool isfile(const zas::utils::uri& filename) {
			uint8_t myself[32], peer[32];
			_uri.digest(myself);
			filename.digest(peer);
			return memcmp(myself, peer, 32) ? false : true;
		}

		long getsize(void);
		void* getmemmap(void);
		zas::utils::uri& get_fileuri(void) {
			return _uri;
		}
	
	private:
		listnode_t _ownerlist;
		zas::utils::uri _uri;
		zas::utils::absfile* _file;
		void* _ptr;
		ZAS_DISABLE_EVIL_CONSTRUCTOR(fontfile);
	};

	struct fontface_node
	{
		uint32_t flags;			// this must be at the first

		// must be an object rather an pointer
		// must be second member (after flags)
		// will use something like LIST_ENTRY
		// must be the second member
		faceinfo_freetype face;

		unsigned int faceid;
		fontfile* file;
		font_charset charset;

		// font cache header
		listnode_t fcache_list;
		zas::utils::mutex mut;
		int grant_face(void);
	};

	struct gblfontface_shadow
	{
		uint32_t flags;			// this must be at the first

		// must be an object rather an pointer
		// must be second member (after flags)
		// will use something like LIST_ENTRY
		// must be the second member
		faceinfo_freetype face;

		listnode_t ownerlist;
		fontfile* file;
		uint32_t gblface;

		gblfontcache_header* fcache_hdr;
		int grant_face(void);
	};

	union fontface_object
	{
		uint32_t flags;
		fontface_node node;
		gblfontface_shadow shadow;
	};

	class fontface_impl
	{
		friend class fontmgr;
	public:
		fontface_impl(const char* facename);
		~fontface_impl();

		fontface_node* addface(const zas::utils::uri& u,
			unsigned int faceid, font_charset charset,
			fontstyle style);

	private:
		listnode_t _ownerlist;
		std::string _facename;
		fontface_node* _faces[fts_count];
		ZAS_DISABLE_EVIL_CONSTRUCTOR(fontface_impl);
	};

	class font_impl
	{
		friend class fontmgr;
		friend class textline_impl;
	public:
		font_impl(fontmgr::fontface_object* face);
		~font_impl();

		void lock(void);
		void unlock(void);

		// grant everything before we start
		// doing any actions
		int grant(void);

		// create the font cache for current size
		int load_fontcache(int width, int height);

		int filltext(int x, int y, int width, textline_impl* txt);

		// load glyph for textchar string array
		int load_glyphs(uint16_t* str, textchar* txlst,
			int unldlst, fontcacher_impl* cacher,
			bool usedef);

		void getsize(int& width, int& height);

		// get the charset
		font_charset get_charset(void);

		uint16_t get_global_fontcache_ptr(void);

	public:
		int addref(void) {
			return __sync_add_and_fetch(&_refcnt, 1);
		}
		int release(void);

		bool isbold(void) {
			return (_f.simulate_bold) ? true : false;
		}

		bool isitalic(void) {
			return (_f.simulate_italic) ? true : false;
		}
	
		void bold_effect(bool be) {
			_f.simulate_bold = (be) ? 1 : 0;
		}

		void italic_effect(bool ie) {
			_f.simulate_italic = (ie) ? 1 : 0;
		}
		
		bool is_globalface(void) {
			return (_face->flags & FONTFACE_SHADOW)
				? true : false;
		}

	private:
		int update_facesize(void);

		int load_local_fontcache(int width, int height);
		int load_global_fontcache(int width, int height);
		int finalize_global_fontcache(void);

		// get glyph from global heap or local memory
		bool load_glyph_unlocked(uint16_t chr, textchar& txtchr,
			fontcacher_impl* cacher, glyphcache_object*& gc);

		// create a new glyph object and save to global heap
		gblglyphcache* gblcreate_glyph_unlocked(uint16_t chr,
			textchar& txtchr, fontcacher_impl* cacher,
			glyphcache_object* gco, bool usedef);
	
		// create a new glyph object locally
		glyphcache* lclcreate_glyph_unlocked(uint16_t chr,
			textchar& txtchr, fontcacher_impl* cacher,
			glyphcache_object* gco, bool usedef);

		// create the global glyph cache object
		gblglyphcache* gblcreate_glyphcache_unlocked(int gcode,
			glyphinfo_freetype* ginfo);

		// create the local glyph cache object
		glyphcache* lclcreate_glyphcache_unlocked(int gcode,
			glyphinfo_freetype* ginfo);

		gblfontcache* find_fcache_unlock(gblfontcache_header* hdr);
		
		// find a glyph cache (support both global and local)
		glyphcache_object* find_glyphcache_unlock(uint16_t gcode);

		// create a glyph cache in global heaps
		gblglyphcache* create_glyphcache_unlock(
			gblfontcache* fcache, uint16_t gcode);

		// global version
		gblglyphcache* gbl_getglyph_unlocked(uint16_t gcode);
		// local version
		glyphcache* lcl_getglyph_unlocked(uint16_t gcode);
		
	private:
		int _refcnt;
		listnode_t _ownerlist;
		fontmgr::fontface_object* _face;
		union {
			uint32_t _flags;
			struct {
				uint32_t simulate_bold : 1;
				uint32_t simulate_italic : 1;
			} _f;
		};
		union {
			fontcache* _lclfcache;
			gblfontcache* _gblfcache;
		};
		union {
			glyphcache* _defchar_cache_lcl;
			gblglyphcache* _defchar_cache_gbl;
		};
		ZAS_DISABLE_EVIL_CONSTRUCTOR(font_impl);
	};

public:
	fontmgr();
	~fontmgr();

	static void reset(void);
	static fontmgr* inst(void);

	fontfile* getfontfile(const zas::utils::uri& fontname);

	fontface_node* createface(const char* facename,
		const zas::utils::uri& filename, unsigned int faceid,
		font_charset type, fontstyle ftype);

	int publish(void);

	fontface_object* findface(const char* facename, fontstyle style);
	font_impl* findfont(fontface_object* face, int width, int height);

	font_impl* createfont(const char* facename, fontstyle style,
		int width, int height);

	gblfontcache* get_gblfontcache(gblglyphcache* gcache);

	void* globalheap_alloc(size_t sz);

private:
	
	gblfontcache_object* fcache_alloc_unlock(void);

	zas::utils::mutex& getmutux(void) {
		return _mut;
	}

	fontdata_outline* fdoutline(void) {
		if (NULL == _shm) return NULL;
		return reinterpret_cast<fontdata_outline*>
			(_shm->getaddr());
	}

	gblfontface* getface(int index)
	{
		fontdata_outline* ol = fdoutline();
		if (NULL == ol) return NULL;
		if (index >= ol->face_cnt) return NULL;
		gblfontface* ff = (gblfontface*)(ol + 1);
		return &ff[index];
	}

	gblfontface_node* getfacenode(int index)
	{
		fontdata_outline* ol = fdoutline();
		if (NULL == ol || !ol->face_cnt) return NULL;
		if (index >= ol->face_node_cnt) return NULL;
		gblfontface_node* ffn = (gblfontface_node*)((size_t)(ol + 1)
			+ sizeof(gblfontface) * ol->face_cnt);
		return &ffn[index];
	}

private:
	int init(void);
	int init_outline(void);
	void release_facelist(void);
	void release_fontfilelist(void);

	fontface_impl* findface_unlocked(const char* facename);
	gblfontface_shadow* findfaceshadow_unlocked(uint32_t idx);
	gblfontface_shadow* createfaceshadow(uint32_t idx);
	
	gblfontface_node* gblfindface(const char* facename,
		fontstyle style, int* index = NULL);

	int get_facecount(int& facecnt, int& facenode_cnt);
	int do_publish_unlocked(void);

private:
	listnode_t _fontfilelist;
	listnode_t _facelist;
	listnode_t _shadowfacelist;
	listnode_t _fontlist;
	zas::utils::mutex _mut;
	zas::utils::shared_memory* _shm;
	font_engine_freetype _feng;
	
	ZAS_DISABLE_EVIL_CONSTRUCTOR(fontmgr);
};

}} // end of namespace zas::hwgrph
#endif // __CXX_ZAS_HWGRPH_FONTMGR_H__
/* EOF */
