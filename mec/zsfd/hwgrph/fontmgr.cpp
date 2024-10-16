/** @file fontmgr.cpp
 * implementation of the font manager
 */

#include "inc/fontmgr.h"
#include "inc/fmttext.h"

using namespace std;
using namespace zas::utils;

namespace zas {
namespace hwgrph {

#define FONTDATA_GLOBAL_NAME	"grph-fontdata"

semaphore font_sem("gblfontheap");

fontmgr::fontmgr()
: _shm(NULL)
{
	listnode_init(_fontfilelist);
	listnode_init(_facelist);
	listnode_init(_shadowfacelist);
	listnode_init(_fontlist);

	_shm = shared_memory::create(FONTDATA_GLOBAL_NAME,
		FONT_SHARED_DATA_AREA_SIZE);
	assert(NULL != _shm);
	init();
}

fontmgr::~fontmgr()
{
	release_facelist();
	release_fontfilelist();
	if (_shm) {
		_shm->release();
	}
}

fontmgr* fontmgr::inst(void)
{
	static fontmgr sfntmgr;
	return &sfntmgr;
}

void fontmgr::reset(void)
{
	shared_memory::reset(FONTDATA_GLOBAL_NAME);
}

int fontmgr::init(void)
{
	int ret;
	if (!_shm->is_creator()) {
		return 0;
	}

	ret = -20 + init_outline();
	if (ret != -20) goto error;

	return 0;

error:
	printf("fontmgr::init(): fatal error %u\n", ret);
	exit(9930);
}

int fontmgr::init_outline(void)
{
	fontdata_outline* ol = fdoutline();
	memset(ol, 0, sizeof(fontdata_outline));
	memcpy(ol->magic, FONT_SHARED_DATA_AREA_MAGIC,
		sizeof(ol->magic));
	ol->maintainer = getpid();

	// init the fontcache freelist
	ol->fontcache_freelist = 0xFFFF;
	return 0;
}

fontmgr::fontfile* fontmgr::getfontfile(const uri& fontname)
{
	fontfile* ffile;
	MUTEX_ENTER(_mut);
	listnode_t* node = _fontfilelist.next;
	for (; node != &_fontfilelist; node = node->next)
	{
		ffile = list_entry(fontfile, _ownerlist, node);
		if (ffile->isfile(fontname)) return ffile;
	}
	MUTEX_EXIT();

	// we need to create a new fontfile
	ffile = new fontfile(fontname);
	_mut.lock();
	listnode_add(_fontfilelist, ffile->_ownerlist);
	_mut.unlock();
	return ffile;
}

fontmgr::fontface_impl::fontface_impl(const char* facename)
: _facename(facename) {
	memset(_faces, 0, sizeof(_faces));
}

fontmgr::fontface_impl::~fontface_impl()
{
	for (int i = 0; i < fts_count; ++i) {
		fontface_node* node = _faces[i];
		if (!node) continue;
		delete node;
	}
}

fontmgr::fontface_impl* fontmgr::findface_unlocked(
	const char* facename)
{
	assert(NULL != facename);
	listnode_t* node = _facelist.next;
	for (; node != &_facelist; node = node->next) {
		fontface_impl* ff = list_entry(fontface_impl, _ownerlist, node);
		if (ff->_facename == facename) return ff;
	}
	return NULL;
}

gblfontface_node* fontmgr::gblfindface(const char* facename,
	fontstyle style, int* index)
{
	assert (style >= 0 || style < fts_count);

	assert(NULL != facename);
	fontdata_outline* ol = fdoutline();
	if (NULL == ol) return NULL;
	gblfontface* first = getface(0);

	// global lock
	gblfontface *face, *end = first + ol->face_cnt;

	SEM_ENTER(font_sem);
	if (!ol->face_cnt) return NULL;
	for (face = first; face != end; ++face) {
		const char* name = TYPE_OFFSET(face->facename, \
			first, const char);
		if (!strcmp(name, facename)) break;
	}
	SEM_EXIT();

	// not found
	if (face == end) return NULL;

	int idx = face->faces[style];
	if (index) *index = idx;
	return GBLFONTFACE_EXISTS(idx) ? getfacenode(idx) : NULL;
}

fontmgr::fontface_node* fontmgr::fontface_impl::addface(
	const uri& filename,
	unsigned int faceid, font_charset charset,
	fontstyle style)
{
	// we must have a modifiable object for normalization
	uri nor_filename(filename);

	if (charset < 0 || charset >= fcs_max)
		return NULL;
	if (style < 0 || style >= fts_max)
		return NULL;

	// already exists?
	if (_faces[style]) return NULL;

	if (absfile_normalize_uri(nor_filename))
		return NULL;
	fontfile* fl = fontmgr::inst()->getfontfile(
		nor_filename);
	if (NULL == fl) return NULL;

	fontface_node* ret = new fontface_node;
	assert(NULL != ret);

	ret->flags = 0;
	ret->faceid = faceid;
	ret->file = fl;
	ret->charset = charset;
	listnode_init(ret->fcache_list);

	_faces[style] = ret;
	return ret;
}

fontmgr::fontface_node* fontmgr::createface(const char* facename,
	const uri& filename, unsigned int faceid,
	font_charset type, fontstyle ftype)
{
	char buf[64];
	if (!facename || !*facename)
	{
		if (_feng.get_fontinfo(filename, (int)faceid, ftype, buf))
			return NULL;
		facename = (char*)buf;
	}

	// find if the face exists in global font heap
	if (gblfindface(facename, ftype)) {
		return NULL;
	}

	_mut.lock();
	fontface_impl* ffi = findface_unlocked(facename);
	_mut.unlock();

	if (NULL == ffi) {
		ffi = new fontface_impl(facename);
		_mut.lock();
		listnode_add(_facelist, ffi->_ownerlist);
		_mut.unlock();
	}

	return ffi->addface(filename, faceid, type, ftype);
}

fontface* fontface::register_face(const char* facename,
	const uri& filename, unsigned int faceid,
	font_charset type, fontstyle ftype)
{
	if (!filename.has_filename()) {
		return NULL;
	}
	if (ftype < 0 || ftype >= fts_count) {
		return NULL;
	}
	if (type < 0 || type >= fcs_count) {
		return NULL;
	}

	return reinterpret_cast<fontface*>(
		fontmgr::inst()->createface(facename, filename,
		faceid, type, ftype));
}

fontmgr::fontfile::fontfile(const uri& uri)
: _file(NULL), _ptr(NULL), _uri(uri)
{
}

long fontmgr::fontfile::getsize(void)
{
	_file->seek(0, absfile_seek_end);
	return _file->getpos();
}

void* fontmgr::fontfile::getmemmap(void)
{
	if (_ptr) return _ptr;

	assert(NULL == _file);
	_file = absfile_open(_uri, "rb");
	if (NULL == _file) return NULL;

	_ptr = _file->mmap(0, 0, absfile_map_prot_read
		| absfile_map_private);
	return _ptr;
}

fontmgr::fontfile::~fontfile()
{
	if (_file) {
		if (_ptr) {
			_file->seek(0, absfile_seek_end);
			_file->munmap(_ptr, _file->getpos());
		}
		_file->release();
	}
}

void fontmgr::release_facelist(void)
{
	_mut.lock();
	while (!listnode_isempty(_facelist))
	{
		listnode_t* node = _facelist.next;
		listnode_del(*node);
		_mut.unlock();

		fontface_impl* ffi = list_entry(fontface_impl, _ownerlist, node);
		delete ffi;
		_mut.lock();		
	}
	_mut.unlock();
}

void fontmgr::release_fontfilelist(void)
{
	_mut.lock();
	while (!listnode_isempty(_fontfilelist))
	{
		listnode_t* node = _fontfilelist.next;
		fontfile* ffile = list_entry(fontfile, _ownerlist, node);
		listnode_del(*node);
		_mut.unlock();
		delete ffile;
		_mut.lock();
	}
	_mut.unlock();
}

int fontmgr::get_facecount(int& facecnt, int& facenode_cnt)
{
	facecnt = 0;
	facenode_cnt = 0;

	auto_mutex am(_mut);
	listnode_t* node = _facelist.next;
	for (; node != &_facelist; node = node->next, ++facecnt)
	{
		fontface_impl* ffi = list_entry(fontface_impl, _ownerlist, node);
		for (int j = 0; j < fts_count; ++j) {
			if (ffi->_faces[j]) ++facenode_cnt;
		}
	}
	return 0;
}

fontmgr::fontface_object* fontmgr::findface(
	const char* facename, fontstyle style)
{
	// first, find the face in global font heap
	int index;
	if (gblfindface(facename, style, &index)) {
		_mut.lock();
		gblfontface_shadow* shadow = findfaceshadow_unlocked(index);
		_mut.unlock();
		if (!shadow)
		{
			shadow = createfaceshadow(index);
			if (!shadow) return NULL;

			_mut.lock();
			listnode_add(_shadowfacelist, shadow->ownerlist);
			_mut.unlock();
		}
		return reinterpret_cast<fontface_object*>(shadow);
	}

	// then, find it in private font face list
	_mut.lock();
	fontface_impl* ffi = findface_unlocked(facename);
	_mut.unlock();

	if (NULL == ffi) return NULL;
	return reinterpret_cast<fontface_object*>(ffi->_faces[style]);
}

fontmgr::gblfontface_shadow* fontmgr::findfaceshadow_unlocked(uint32_t idx)
{
	listnode_t* node = _shadowfacelist.next;
	for (; node != &_shadowfacelist; node = node->next)
	{
		gblfontface_shadow* gffs = list_entry(gblfontface_shadow, ownerlist, node);
		if (gffs->gblface == idx) return gffs;
	}
	return NULL;
}

fontmgr::gblfontface_shadow* fontmgr::createfaceshadow(uint32_t idx)
{
	gblfontface* firstface = getface(0);
	if (NULL == firstface) return NULL;

	gblfontface_node* facenode = getfacenode(idx);
	if (NULL == facenode) return NULL;

	const char* filename = TYPE_OFFSET(facenode->filename_ptr, \
		firstface, const char);

	fontfile* fl = getfontfile(filename);
	if (NULL == fl) return NULL;

	gblfontface_shadow* faceshadow = new gblfontface_shadow;
	faceshadow->flags = FONTFACE_SHADOW;
	faceshadow->file = fl; // todo: addref
	faceshadow->gblface = idx;
	return faceshadow;
}

int fontmgr::fontface_node::grant_face(void)
{
	if (face.loaded()) return 0;

	assert(NULL != file);
	void* ptr = file->getmemmap();

	// this must be later than the calling of
	// getmemmap() since the getmemmap() will
	// load the file object which is needed by
	// file->getsize()
	long sz = file->getsize();
	return fontmgr::inst()->_feng.load_face(ptr, sz, faceid, face);
}

int fontmgr::gblfontface_shadow::grant_face(void)
{
	if (face.loaded()) return 0;

	assert(NULL != file);
	fontmgr* fmgr = fontmgr::inst();
	gblfontface_node* node = fmgr->getfacenode(gblface);

	void* ptr = file->getmemmap();

	// this must be later than the calling of
	// getmemmap() since the getmemmap() will
	// load the file object which is needed by
	// file->getsize()
	long sz = file->getsize();
	return fmgr->_feng.load_face(ptr, sz, node->faceid, face);
}

int fontmgr::publish(void)
{
	if (!_shm || !_shm->is_creator()) {
		return -ENOTALLOWED;
	}
	fontdata_outline* ol = fdoutline();
	if (NULL == ol) return -ENOTAVAIL;

	if (getpid() != ol->maintainer) {
		return -ENOTALLOWED;
	}

	if (ol->face_cnt || ol->face_node_cnt) {
		return -ENOTALLOWED;
	}

	int facecnt, facenode_cnt;
	get_facecount(facecnt, facenode_cnt);
	if (facecnt > UINT16_MAX || facenode_cnt > UINT16_MAX) {
		return -ETOOMANYITEMS;
	}

	// global lock
	auto_semaphore as(font_sem);
	ol->face_cnt = uint16_t(facecnt);
	ol->face_node_cnt = uint16_t(facenode_cnt);

	int ret = do_publish_unlocked();
	if (!ret) {
		release_facelist();
		release_fontfilelist();
		ol->f.ready = 1;
	}
	return ret;
}

int fontmgr::do_publish_unlocked(void)
{
	fontdata_outline* ol = fdoutline();
	gblfontface* firstface = getface(0), *face = firstface;
	gblfontface_node* firstfacenode = getfacenode(0),
		*facenode = firstfacenode;
	char* str_area = reinterpret_cast<char*>(firstfacenode + ol->face_node_cnt);

	if (!ol->face_cnt && !ol->face_node_cnt) {
		str_area = reinterpret_cast<char*>(ol + 1);
	}

	listnode_t* node = _facelist.next;
	for (; node != &_facelist; node = node->next, ++face)
	{
		fontface_impl* ffi = list_entry(fontface_impl, _ownerlist, node);
		strcpy(str_area, ffi->_facename.c_str());

		face->facename = UINT16_DELTA(str_area, firstface);
		str_area += ffi->_facename.length() + 1;

		face->reset_faces();
		for (int i = 0; i < fts_count; ++i)
		{
			fontface_node* ffn = ffi->_faces[i];
			if (!ffn) continue;
			
			facenode->faceid = ffn->faceid;
			string path = ffn->file->get_fileuri().tostring();
			size_t len = path.length() + 1;
			memcpy(str_area, path.c_str(), len);
			facenode->filename_ptr = UINT16_DELTA(str_area, firstface);
			str_area += len;
			facenode->charset = uint16_t(ffn->charset);
			facenode->fcachehdr_ptr = 0xFFFF;
			face->faces[i] = uint16_t(facenode - firstfacenode);
			++face;
		}
	}

	// calculate the start of fontcache
	size_t delta = (size_t)str_area;
	delta -= (size_t)_shm->getaddr();
	delta = (delta + 15) & ~15;
	assert(delta < 0xFFFF);
	ol->startof_fcache = uint16_t(delta);

	// calculate the start of font data area
	delta += sizeof(gblfontcache_object) * 0xFFFF;
	delta = (delta + 15) & ~15;
	assert(delta < FONT_SHARED_DATA_AREA_SIZE);
	ol->startof_glyphdata = uint32_t(delta);
	ol->glyphdata_curpos = uint32_t(delta);
	return 0;
}

static void normalize_size(int& width, int& height)
{
	if (!width) {
		if (!height) return;
		width = height;
	}
	else if (!height) {
		if (!width) return;
		height = width;
	}
}

fontmgr::font_impl* fontmgr::findfont(fontface_object* face,
	int width, int height)
{
	auto_mutex am(_mut);
	listnode_t* node = _fontlist.next;
	for (; node != &_fontlist; node = node->next) {
		font_impl* fi = list_entry(font_impl, _ownerlist, node);
		if (fi->_face == face) {
			int fi_width, fi_height;
			fi->getsize(fi_width, fi_height);
			if (width == fi_width && height == fi_height)
				return fi;
		}
	}
	return NULL;
}

fontmgr::font_impl* fontmgr::createfont(const char* facename,
	fontstyle style, int width, int height)
{
	int flags = 0;
	enum { bold = 1, italic = 2 };
	fontface_object* face = findface(facename, style);
	if (NULL == face)
	{
		if (style == fts_bold) {
			flags |= bold;
		} else if (style == fts_italic) {
			flags |= italic;
		} else if (style == fts_bold_italic) {
			flags |= bold | italic;
		}
		else return NULL;

		face = findface(facename, fts_regular);
		if (NULL == face) return NULL;
	}

	normalize_size(width, height);
	font_impl* ret = findfont(face, width, height);
	if (ret) {
		bool b = (flags & bold) ? true : false;
		bool i = (flags & italic) ? true : false;
		if (b == ret->isbold() || i == ret->isitalic())
			return ret;
	}
	
	ret = new font_impl(face);
	assert(NULL != ret);

	if (flags & bold)
		ret->bold_effect(true);
	if (flags & italic)
		ret->italic_effect(true);
	
	// load the font cache for the specific size
	if (ret->load_fontcache(width, height)) {
		delete ret; return NULL;
	}
	
	// add to the font list
	_mut.lock();
	listnode_add(_fontlist, ret->_ownerlist);
	_mut.unlock();
	return ret;
}

void* fontmgr::globalheap_alloc(size_t sz)
{
	fontdata_outline* fout = fdoutline();
	if (NULL == fout) return NULL;

	// todo:
	// we need to retry for several times
	// by waiting for the font consumer to release
	// some buffer space
	font_sem.lock();
	void* ret = fout->glyphdata_allocate_unlocked(sz);
	font_sem.unlock();
	return ret;
}

fontmgr::font_impl::font_impl(fontmgr::fontface_object* face)
: _refcnt(0)
, _face(face)
, _flags(0)
, _lclfcache(NULL)
, _defchar_cache_lcl(NULL)
{
	assert(NULL != _face);
}

fontmgr::font_impl::~font_impl()
{
}

int fontmgr::font_impl::release(void)
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0) delete this;
	return cnt;
}

void fontmgr::font_impl::lock(void)
{
	if (is_globalface()) {
		font_sem.lock();
	}
	else _face->node.mut.lock();
}

void fontmgr::font_impl::unlock(void)
{
	if (is_globalface()) {
		font_sem.unlock();
	}
	else _face->node.mut.unlock();
}

int fontmgr::font_impl::filltext(int x, int y, int width, textline_impl* txt)
{
	if (NULL == txt) {
		return -EBADPARM;
	}

	txt->set_font(this);
	return 0;
}

int font_object::addref(void)
{
	fontmgr::font_impl* fi = reinterpret_cast<fontmgr::font_impl*>(this);
	return fi->addref();
}

int font_object::release(void)
{
	fontmgr::font_impl* fi = reinterpret_cast<fontmgr::font_impl*>(this);
	return fi->release();
}

font createfont(const char* facename, fontstyle style,
	int width, int height)
{
	font fnt;
	if (!facename || !*facename) {
		return fnt;
	}
	if (style < 0 || style > fts_count) {
		return fnt;
	}

	fontmgr::font_impl* fi = fontmgr::inst()->createfont(
		facename, style, width, height);
	fnt = reinterpret_cast<font_object*>(fi);
	return fnt;
}

int publish_fonts(void)
{
	return fontmgr::inst()->publish();
}

void set_as_font_manager(void)
{
	fontmgr::reset();
}

}} // end of namespace zas::hwgrph
/* EOF */

