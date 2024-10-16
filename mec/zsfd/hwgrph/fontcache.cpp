/** @file fontcache.cpp
 * implementation of the font cache manager
 */

#include "utils/mutex.h"
#include "utils/mcache.h"

#include "inc/fontcache.h"
#include "inc/fontmgr.h"
#include "inc/grphgbl.h"
#include "inc/fmttext.h"

using namespace zas::utils;

namespace zas {
namespace hwgrph {

extern semaphore font_sem;

// global memcaches
static memcache _gcache_mcache("fnt-glyphcache", sizeof(glyphcache),
	0, mcacheflg_cache_compact);
static memcache _ginfo_mcache("fnt-glyphinfo", sizeof(fontcacher_impl::glyphinfo),
	0, mcacheflg_cache_compact);

void* glyphcache::operator new(size_t sz)
{
	assert(sz == sizeof(glyphcache));
	return _gcache_mcache.alloc();
}

void glyphcache::operator delete(void* p)
{
	_gcache_mcache.free(p);
}

void* fontcacher_impl::glyphinfo::operator new(size_t sz)
{
	assert(sz == sizeof(fontcacher_impl::glyphinfo));
	return _ginfo_mcache.alloc();
}

void fontcacher_impl::glyphinfo::operator delete(void* p)
{
	_ginfo_mcache.free(p);
}

void fontmgr::font_impl::getsize(int& width, int& height)
{
	if (is_globalface()) {
		width = _gblfcache->width;
		height = _gblfcache->height;
	}
	else {
		width = _lclfcache->width;
		height = _lclfcache->height;
	}
}

gblfontcache_object* fontmgr::fcache_alloc_unlock(void)
{
	fontdata_outline* ol = fontmgr::inst()->fdoutline();
	assert(NULL != ol);
	gblfontcache_object* obj = (gblfontcache_object*)((size_t)ol + ol->startof_fcache);

	int index = ol->fontcache_alloc_tag;
	if (index >= 0xFFFF) {
		index = ol->fontcache_freelist;
		if (index == 0xFFFF) return NULL;

		// use the first node in freelist
		ol->fontcache_freelist = obj[index].freenode.next;
	}
	else index = ol->fontcache_alloc_tag++;
	return &obj[index];
}

int fontmgr::font_impl::update_facesize(void)
{
	int ret, width, height;
	getsize(width, height);

	return fontmgr::inst()->_feng.face_size(
		is_globalface()
		? _face->shadow.face
		: _face->node.face,
		width, height
	);
}

int fontmgr::font_impl::load_glyphs(uint16_t* str, textchar* txlst,
	int unldlst, fontcacher_impl* cacher, bool usedef)
{
	int ret = -1, last = -1;

	if (unldlst < 0) return 0;
	assert(str && txlst && cacher);

	// update the face size
	if (update_facesize()) return -1;

	// check readiness
	bool gbl = is_globalface();
	if (gbl && !fontmgr::inst()->fdoutline()->ready()) {
		return -1;
	}

	// we need lock
	lock();

	for (int next; unldlst >= 0; unldlst = next)
	{
		uint16_t chr = str[unldlst];
		textchar& txtchr = txlst[unldlst];
		next = txtchr.next_unloaded;
	
		// load + create
		glyphcache_object* gc;
		bool loaded = load_glyph_unlocked(chr, txtchr, cacher, gc);
		if (!loaded) {
			gc = (gbl) ? reinterpret_cast<glyphcache_object*>
				(gblcreate_glyph_unlocked(chr, txtchr, cacher, gc, usedef)) 
			: reinterpret_cast<glyphcache_object*>
				(lclcreate_glyph_unlocked(chr, txtchr, cacher, gc, usedef));
		}

		if (NULL == gc) {
			if (ret < 0) ret = unldlst;
			if (last >= 0) {
				txlst[last].next_unloaded = unldlst;
			}
			txlst[unldlst].next_unloaded = -1;
			last = unldlst;
		}
	}
	unlock();
	return ret;
}

bool fontmgr::font_impl::load_glyph_unlocked(uint16_t chr,
	textchar& txtchr, fontcacher_impl* cacher, glyphcache_object*& gc)
{
	bool ret = false;
	gc = find_glyphcache_unlock(chr);
	if (NULL != gc) {
		fontcacher_impl::glyphinfo* ginfo =
			cacher->create_glyphinfo_via_glyphcache(
			gc, is_globalface());

		if (ginfo) {
			txtchr.glyph = ginfo;
			ret = true;
		}
	} 
	return gc;
}

glyphcache* fontmgr::font_impl::lclcreate_glyph_unlocked(uint16_t chr,
	textchar& txtchr, fontcacher_impl* cacher,glyphcache_object* gco,
	bool usedef)
{
	glyphinfo_freetype ginfo;
	font_engine_freetype& feng = fontmgr::inst()->_feng;

	glyphcache* gcache = &gco->lcl;
	if (NULL == gcache) {
		// not found, we load it from file
		int ret = feng.load_glyph(_face->node.face, chr, ginfo);
		if (ret == ENOTFOUND) {
			// check if we need to use default char
			if (!usedef) return NULL;

			// the specific charcode not found, we use
			// default char as replacement
			if (!_defchar_cache_lcl) {
				// default char never used, load it now
				uint16_t defchar_code = feng.get_defaultchar(_face->node.face);

				// make sure it is never used
				gcache = lcl_getglyph_unlocked(defchar_code);
				if (NULL == gcache) {
					ret = feng.load_glyph(_face->node.face,
						defchar_code, ginfo);
					assert(!ret);
					gcache = lclcreate_glyphcache_unlocked(chr, &ginfo);
					assert(NULL != gcache);
				}
				_defchar_cache_lcl = gcache;
			} else gcache = _defchar_cache_lcl;
		} else {
			if (ret) return NULL;
			gcache = lclcreate_glyphcache_unlocked(chr, &ginfo);
			assert(NULL != gcache);
		}
	}
	if (cacher->add_glyph(chr, (glyphcache_object*)gcache, &ginfo, this)) {
		if (gcache != &gco->lcl) {
			// todo: release the gcache
		}
	}
	return gcache;
}

gblglyphcache* fontmgr::font_impl::gblcreate_glyph_unlocked(uint16_t chr,
	textchar& txtchr, fontcacher_impl* cacher,glyphcache_object* gco,
	bool usedef)
{
	glyphinfo_freetype ginfo;
	font_engine_freetype& feng = fontmgr::inst()->_feng;

	gblglyphcache* gcache = &gco->gbl;
	if (NULL == gcache) {
		// not found, we load it from file
		int ret = feng.load_glyph(_face->shadow.face, chr, ginfo);
		if (ret == ENOTFOUND) {
			// check if we need to use default char
			if (!usedef) return NULL;

			// the specific charcode not found, we use
			// default char as replacement
			if (!_defchar_cache_gbl) {
				// default char never used, load it now
				uint16_t defchar_code = feng.get_defaultchar(_face->shadow.face);

				// make sure it is never used
				gcache = gbl_getglyph_unlocked(defchar_code);
				if (NULL == gcache) {
					ret = feng.load_glyph(_face->shadow.face,
						defchar_code, ginfo);
					assert(!ret);
					gcache = gblcreate_glyphcache_unlocked(chr, &ginfo);
					assert(NULL != gcache);
				}
				_defchar_cache_gbl = gcache;
			} else gcache = _defchar_cache_gbl;
		} else {
			if (ret) return NULL;
			gcache = gblcreate_glyphcache_unlocked(chr, &ginfo);
			assert(NULL != gcache);
		}
	}
	if (cacher->add_glyph(chr, (glyphcache_object*)gcache, &ginfo, this)) {
		if (gcache != &gco->gbl) {
			// todo: release the gcache
		}
	}
	return gcache;
}

static int font_gbl_getglyph_compare(gblheapavltree* a, gblheapavltree* b)
{
	gblglyphcache* aa = li_avlnode_entry(gblglyphcache, avlnode, a);
	gblglyphcache* bb = li_avlnode_entry(gblglyphcache, avlnode, b);
	if (aa->glyphcode < bb->glyphcode) {
		return -1;
	} else if (aa->glyphcode > bb->glyphcode) {
		return 1;
	} else return 0;
}

static int font_lcl_getglyph_compare(avl_node_t* a, avl_node_t* b)
{
	glyphcache *aa = AVLNODE_ENTRY(glyphcache, avlnode, a);
	glyphcache *bb = AVLNODE_ENTRY(glyphcache, avlnode, b);
	if (aa->glyphcode < bb->glyphcode) {
		return -1;
	} else if (aa->glyphcode > bb->glyphcode) {
		return 1;
	} else return 0;
}

glyphcache* fontmgr::font_impl::lclcreate_glyphcache_unlocked(
	int gcode, glyphinfo_freetype* ginfo)
{
	glyphcache* gcache = new glyphcache();
	if (NULL == gcache) return NULL;

	gcache->glyphcode = (uint16_t)gcode;
	gcache->font = (void*)_lclfcache;

	if (ginfo) {
		gcache->width = ginfo->width;
		gcache->height = ginfo->height;
		gcache->advance_x = ginfo->advance_x;
		gcache->advance_y = ginfo->advance_y;
	}

	// todo: set flags for glyphcache
	
	// insert the new node to avltree
	if (avl_insert(&_lclfcache->tree, &gcache->avlnode,
		font_lcl_getglyph_compare)) {
		delete gcache;
		return NULL;
	}
	return gcache;
}

font_charset fontmgr::font_impl::get_charset(void)
{
	if (is_globalface()) {
		auto* node = fontmgr::inst()->getfacenode(_face->shadow.gblface);
		if (!node) return fcs_unknown;
		return (font_charset)node->charset;
	}
	else return _face->node.charset;
}

uint16_t fontmgr::font_impl::get_global_fontcache_ptr(void)
{
	if (!is_globalface()) return 0xFFFF;

	fontdata_outline* ol = fontmgr::inst()->fdoutline();
	assert(NULL != ol);
	
	void* base = (void*)(ol->startof_fcache + (size_t)ol);
	return (uint16_t)TYPE_PTR(_gblfcache, base, gblfontcache_object);
}

gblglyphcache* fontmgr::font_impl::gblcreate_glyphcache_unlocked(
	int gcode, glyphinfo_freetype* ginfo)
{
	fontmgr* fmgr = fontmgr::inst();
	fontdata_outline* ol = fmgr->fdoutline();
	assert(NULL != ol);
	
	void* base = (void*)(ol->startof_fcache + (size_t)ol);
	const size_t elemsz = sizeof(gblfontcache_object);
	
	gblfontcache_object* obj = fmgr->fcache_alloc_unlock();
	assert(NULL != obj);

	memset(obj, 0, sizeof(*obj));
	gblglyphcache& gcache = obj->gcache;
	gcache.glyphcode = (uint16_t)gcode;
	gcache.font_ptr = TYPE_PTR(_gblfcache, base, gblfontcache_object);
	
	if (ginfo) {
		gcache.width = ginfo->width;
		gcache.height = ginfo->height;
		gcache.advance_x = ginfo->advance_x;
		gcache.advance_y = ginfo->advance_y;
	}
	
	// insert the new node to avltree
	if (gcache.avlnode.avl_insert(&_gblfcache->tree,
		font_gbl_getglyph_compare, base, elemsz)) {
		// todo: release obj
		return NULL;
	}
	return &gcache;
}

int fontmgr::font_impl::grant(void)
{
	int ret;
	if (is_globalface()) {
		ret = _face->shadow.grant_face();
	} else {
		ret = _face->node.grant_face();
	}
	return ret;
}

int fontmgr::font_impl::load_fontcache(int width, int height)
{
	int ret;
	assert(_face != NULL);
	if (is_globalface()) {
		ret = load_global_fontcache(width, height);
		// load font cache parameter for it specific size
		if (!ret) finalize_global_fontcache();
	}
	else ret = load_local_fontcache(width, height);
	return ret;
}

int fontmgr::font_impl::load_local_fontcache(int width, int height)
{
	fontcache* fcache = new fontcache();
	assert(NULL != fcache);

	fcache->width = width;
	fcache->height = height;
	fcache->tree = NULL;
	_lclfcache = fcache;

	// add to fcache_list
	fontface_node& ffnode = _face->node;
	ffnode.mut.lock();
	listnode_add(ffnode.fcache_list, fcache->ownerlist);
	ffnode.mut.unlock();
	return 0;
}

gblfontcache* fontmgr::font_impl::find_fcache_unlock(gblfontcache_header* hdr)
{
	return NULL;
}

glyphcache* fontmgr::font_impl::lcl_getglyph_unlocked(uint16_t gcode)
{
	avl_node_t* node = avl_find(_lclfcache->tree,
		MAKE_FIND_OBJECT(gcode, glyphcache, glyphcode, avlnode),
		font_lcl_getglyph_compare);
	
	if (NULL == node) return NULL;
	return AVLNODE_ENTRY(glyphcache, avlnode, node);
}

gblglyphcache* fontmgr::font_impl::gbl_getglyph_unlocked(uint16_t gcode)
{
	fontmgr* fmgr = fontmgr::inst();
	fontdata_outline* ol = fmgr->fdoutline();
	assert(NULL != ol);

	void* base = (void*)(ol->startof_fcache + (size_t)ol);
	const size_t elemsz = sizeof(gblfontcache_object);

	gblheapavltree* node = li_avl_find(
		_gblfcache->tree,
		li_make_find_object(gcode, gblglyphcache, \
			glyphcode, avlnode, gblheapavltree),
		font_gbl_getglyph_compare, base, elemsz
	);
	if (NULL == node) return NULL;
	return li_avlnode_entry(gblglyphcache, avlnode, node);
}

fontcacher_impl::glyphinfo::glyphinfo(glyphcache_object* gc, bool fromgbl)
: gcache(gc), bitmap(NULL), next(NULL)
, flags(0), refcnt(0)
{
	f.global = fromgbl ? 1 : 0;
	comparible.load(gc, fromgbl);
}

fontcacher_impl::glyphinfo::glyphinfo(glyphcache_object* gc,
	avl_comparible& c, bool fromgbl)
: gcache(gc), bitmap(NULL), next(NULL)
, flags(0), refcnt(0)
{
	f.global = fromgbl;
	memcpy(&comparible, &c, sizeof(c));
}

void fontcacher_impl::glyphinfo::avl_comparible::load(
	glyphcache_object* gc, bool fromgbl)
{
	if (fromgbl) {
		gblfontcache* font = fontmgr::inst()->get_gblfontcache(&gc->gbl);
		glyphcode = gc->gbl.glyphcode;
		width = font->width;
		height = font->height;
	}
	else {
		fontcache* font = reinterpret_cast<fontcache*>(gc->lcl.font);
		assert(NULL != font);
		
		glyphcode = gc->lcl.glyphcode;
		width = (uint16_t)font->width;
		height = (uint16_t)font->height;
	}
}

gblfontcache* fontmgr::get_gblfontcache(gblglyphcache* gcache)
{
	fontmgr* fmgr = fontmgr::inst();
	fontdata_outline* ol = fmgr->fdoutline();
	assert(NULL != ol);

	void* base = (void*)(ol->startof_fcache + (size_t)ol);
	const size_t elemsz = sizeof(gblfontcache_object);
	gblfontcache* font = TYPE_OFFSET(gcache->font_ptr * elemsz, base, gblfontcache);
	return font;
}

int fontmgr::font_impl::load_global_fontcache(int width, int height)
{
	// we need lock
	auto_semaphore as(font_sem);
	fontmgr* fmgr = fontmgr::inst();
	fontdata_outline* ol = fmgr->fdoutline();
	assert(NULL != ol);
	
	void* base = (void*)(ol->startof_fcache + (size_t)ol);
	const size_t elemsz = sizeof(gblfontcache_object);

	gblfontface_node* ffnode = fmgr->getfacenode(_face->shadow.gblface);
	if (NULL == ffnode) return -1;
	
	gblfontcache_object* obj;
	gblfontcache_header* hdr;

	if (INVALID_PTR(ffnode->fcachehdr_ptr))
	{
		obj = fmgr->fcache_alloc_unlock();
		if (NULL == obj) return -2;
		ffnode->fcachehdr_ptr = TYPE_PTR(obj, base, gblfontcache_object);

		// init the font cache header
		hdr = &obj->header;
		hdr->fcachelist.init(base, elemsz);
	}
	else hdr = TYPE_OFFSET(ffnode->fcachehdr_ptr * elemsz, base, gblfontcache_header);

	// allocate the font cache for current font
	gblfontcache* fc = find_fcache_unlock(hdr);
	if (NULL == fc) {
		obj = fmgr->fcache_alloc_unlock();
		if (NULL == obj) return -3;
		fc = &obj->fcache;
	}
	fc->width = width;
	fc->height = height;
	fc->tree = gblheapavltree::_nullptr;
	fc->ownerlist.add(hdr->fcachelist, base, elemsz);

	_gblfcache = fc;
	return 0;
}

int fontmgr::font_impl::finalize_global_fontcache(void)
{
}

glyphcache_object* fontmgr::font_impl::find_glyphcache_unlock(uint16_t gcode)
{
	if (is_globalface()) {
		gblglyphcache* gcache = gbl_getglyph_unlocked(gcode);
		if (NULL == gcache) return NULL;
		return reinterpret_cast<glyphcache_object*>(gcache);
	}
	else {
		glyphcache* gcache = lcl_getglyph_unlocked(gcode);
		if (NULL == gcache) return NULL;
		return reinterpret_cast<glyphcache_object*>(gcache);
	}
	// shall never come here
}

gblglyphcache* fontmgr::font_impl::create_glyphcache_unlock(
	gblfontcache* fcache, uint16_t gcode)
{
	return NULL;
}

fontcacher_impl::fontcacher_impl()
: _refcnt(0)
, _active_texnode(-1)
, _texbuf(16)
, _glyphtree(NULL)
, _gbl_pending(NULL)
, _lcl_pending(NULL)
{
}

fontcacher_impl::~fontcacher_impl()
{
	// todo: release the glyphtree
	// todo: release gbl pending and lcl pending
}

int fontcacher_impl::release(void)
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0) delete this;
	return cnt;
}

fontcacher_impl::glyphinfo* fontcacher_impl::create_glyphinfo_via_glyphcache(
	glyphcache_object* gcache, bool gbl)
{
	glyphinfo dummy_ginfo;
	dummy_ginfo.comparible.load(gcache, gbl);
	dummy_ginfo.gcache = gcache;
	
	// need lock
	auto_mutex am(_mut);
	avl_node_t* node = avl_find(_glyphtree, &dummy_ginfo.avlnode,
		glyphinfo::avl_glyphinfo_compare);

	if (NULL != node) {
		return AVLNODE_ENTRY(glyphinfo, avlnode, node);
	}
	if (!gbl) return NULL;

	// for global glyph, we leave the glyph loading to
	// the font daemon to do so
	glyphinfo* ginfo = new glyphinfo(gcache, dummy_ginfo.comparible, gbl);
	assert(NULL != ginfo);

	// add to the font cacher
	int ret = ginfo->add_as_pending_unlocked(this, gbl);
	assert(ret == 0);
	return 0;
}

int fontcacher_impl::load_glyphs(uint16_t* code_array,
	textchar* txlst, int unldlst, void* f)
{
	if (unldlst < 0) return 0;
	assert(code_array && txlst);
	
	glyphinfo dummy;
	int width, height;
	fontmgr::font_impl* font = reinterpret_cast<fontmgr::font_impl*>(f);
	font->getsize(width, height);
	dummy.comparible.width = (uint16_t)width;
	dummy.comparible.height = (uint16_t)height;

	glyphcache_object dummy_gcache;
	dummy.gcache = &dummy_gcache;
	if (font->is_globalface()) {
		dummy.f.global = 1;	
		dummy_gcache.gbl.font_ptr = font->get_global_fontcache_ptr();
	} else {
		dummy.f.global = 0;
		dummy_gcache.lcl.font = f;
	}

	int ret = -1, last = -1;

	// we need lock to start the loop
	auto_mutex am(_mut);
	for (int next; unldlst >= 0; unldlst = next)
	{
		dummy.comparible.glyphcode = code_array[unldlst];
		next = txlst[unldlst].next_unloaded;

		avl_node_t* node = avl_find(_glyphtree, &dummy.avlnode,
			glyphinfo::avl_glyphinfo_compare);

		if (NULL == node) {
			if (ret < 0) ret = unldlst;
			if (last >= 0) {
				txlst[last].next_unloaded = unldlst;
			}
			txlst[unldlst].next_unloaded = -1;
			last = unldlst;
		}
		else txlst[unldlst].glyph = AVLNODE_ENTRY(glyphinfo, avlnode, node);
	}
	return ret;
}

int fontcacher_impl::add_glyph(int code, glyphcache_object* gcache,
	glyphinfo_freetype* ginfo, void* f)
{
	fontmgr::font_impl* font = reinterpret_cast<fontmgr::font_impl*>(f);
	glyphinfo* info = new glyphinfo(gcache, font->is_globalface());
	if (NULL == info) return -1;

	// fill information to glyphinfo
	assert(NULL != ginfo->gbmp);
	info->bitmap = ginfo->gbmp;
	info->f.gbmp_attached = 1;

	// we need lock
	auto_mutex am(_mut);
	font->addref();			// todo: replace to glyphcache->addref()

	// add to pending list
	info->add_as_pending_unlocked(this, font->is_globalface());

	// create the necessary active texture node
	if (_active_texnode < 0) {
		_active_texnode = create_texture_node_unlocked();
		assert(_active_texnode >= 0);
	}
	info->texinfo.texid = uint16_t(_active_texnode);
	texture_node& texnode = _texbuf.buffer()[_active_texnode];

	// add to the glyph list
	info->next = texnode.glyphlist;
	texnode.glyphlist = info;

	// todo: add to pending buffer
	return 0;
}

int fontcacher_impl::create_texture_node_unlocked(void)
{
	texture_node node;
	node.data = NULL;
	node.texid = -1;
	node.glyphlist = NULL;
	_texbuf.add(node);
	return _texbuf.getsize() - 1;
}

int fontcacher_object::addref(void)
{
	fontcacher_impl* fi = reinterpret_cast<fontcacher_impl*>(this);
	return fi->addref();
}

int fontcacher_object::release(void)
{
	fontcacher_impl* fi = reinterpret_cast<fontcacher_impl*>(this);
	return fi->release();
}

fontcacher get_global_fontcacher(void)
{
	fontcacher_impl* fi = gblconfig::inst()->get_global_fontcacher();
	fontcacher fc(reinterpret_cast<fontcacher_object*>(fi));
	return fc;
}

}} // end of namespace zas::hwgrph
/* EOF */
