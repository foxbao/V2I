/** @file fmttext.cpp
 * implementation of the formatted text object
 */

#include "hwgrph/format.h"
#include "inc/grphgbl.h"
#include "inc/fmttext.h"
#include "inc/fontmgr.h"

using namespace zas::utils;

namespace zas {
namespace hwgrph {

extern semaphore font_sem;

inline static const char* utf8_countchar(const char* str)
{
	const char* ret = str;
	char tmp = *ret++;
	char data = tmp;

	// calculate how many 1's
	int count = 0;
	for (; tmp & 0x80; tmp <<= 1, ++count);
	
	// handle the continuous count bytes except for the first one
	for (; count > 1; --count, ret++) {
		if (!*ret) return NULL;
	}
	return ret;
}

static int utf8_strlen(const char* str)
{
	int ret = 0;
	for (; *str; ++ret) {
		str = utf8_countchar(str);
		if (NULL == str) return -1;
	}
	return ret;	
}

static const char* utf8_getchar(const char* str, uint32_t& c)
{
	if (!str) {
		c = 0;
		return NULL;
	}

	const char* ret = str;
	char tmp = *ret++;
	char data = tmp;

	// calculate how many 1's
	int count = 0;
	for (; tmp & 0x80; tmp <<= 1, ++count);
	c = ((uint32_t)data) & ((1 << (8 - count - 1)) - 1);
	
	// handle the continuous count bytes except for the first one
	for (; count > 1; --count)
	{
		if (!*ret) {
			// error
			c = 0;
			return NULL;
		}

		data = *ret++;
		data &= 0x3F;	// 00111111
		c = (c << 6) | data;
	}
	return ret;
}

textline_impl::textline_impl(const char* utf8str, int width, mutex* mut)
: _refcnt(0), _origstr(NULL), _str(NULL), _count(0), _width(width)
, _fontcacher(NULL), _mut(mut)
{
	for (int i = 0; i < fcs_count; ++i) {
		_fonts[i] = NULL;
	}
	parse_str_unlocked(utf8str);
}

textline_impl::~textline_impl()
{
	if (_origstr) delete [] _origstr;
	if (_str) {
		if (_mut) _mut->lock();
		release_textchar_unlocked();
		if (_mut) _mut->unlock();
	}

	for (int i = 0; i < fcs_count; ++i) {
		release_font_unlocked(i);
	}
	release_fcacher();
}

int textline_impl::release(void)
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0) delete this;
	return cnt;
}

int textline_impl::loadstr(void)
{
	if (!_fontcacher) return -1;
	if (!_origstr ||!_count) return -2;

	uint16_t* origstr_curr = _origstr;
	textchar* curr = _str, *end = _str + _count;
	
	// we try to load all unloaded glyph from the
	// cacher, global heap, and finally the font file

	// we will try loading in the following order:
	// 1) selected western font
	// 2) selected eastern font
	// 3) default font
	bool usedef[3] = { false };
	fontmgr::font_impl* fonts[3] = { NULL };
	if (_fonts[fcs_western]) fonts[0] = _fonts[fcs_western];
	if (_fonts[fcs_eastern]) fonts[1] = _fonts[fcs_eastern];
	// todo: default
	int usedef_pos = -1;
	for (int i = 0; i < 3; ++i) {
		if (fonts[i]) usedef_pos = i;
	}
	if (usedef_pos >= 0) usedef[usedef_pos] = true;

	// load from cacher
	int unload_list = 0;
	for (int i = 0; i < 3; ++i) {
		if (!fonts[i]) continue;
		unload_list = _fontcacher->load_glyphs(_origstr,
			_str, unload_list, fonts[i]);
		if (unload_list < 0) return 0;
	}
	
	// load from global heap and font file
	for (int i = 0; i < 3; ++i) {
		if (!fonts[i]) continue;
		unload_list = fonts[i]->load_glyphs(_origstr, _str,
			unload_list, _fontcacher, usedef[i]);
		if (unload_list < 0) return 0;
	}
	return (unload_list < 0) ? 0 : -3;
}

void textline_impl::release_textchar_unlocked(void)
{
	if (!_str) return;
			
	textchar* s = _str, *end = s + _count;
	for (; s != end; ++s) {
		// todo: release refcnt
	}
	delete [] _str;
	_str = NULL;
}

int textline_impl::parse_str_unlocked(const char* str)
{
	if (!str || !*str) {
		return -1;
	}

	// release the old one
	if (_count) {
		if (_origstr) {
			delete [] _origstr;
			_origstr = NULL;
		}
		if (_str) release_textchar_unlocked();
	}
	
	_count = utf8_strlen(str);
	if (!_count) return 0;

	// allocate array
	_origstr = new uint16_t [_count];
	if (NULL == _origstr) {
		_origstr = NULL; _count = 0;
		return -2;
	}
	// load the string
	uint16_t* s = _origstr, *e = _origstr + _count;
	for (; s != e; ++s) {
		uint32_t c;
		str = utf8_getchar(str, c);
		if (NULL == str) {
			delete [] _origstr;
			_origstr = NULL; return -3;
		}
		*s = uint16_t(c);
	}
	return 0;
}

static void init_textchar(textchar* str, int count)
{
	for (int i = 0; i < count; ++i, ++str) {
		str->next_unloaded = i + 1;
	}
	str[-1].next_unloaded = -1;
}

int textline_impl::set_fontcacher(fontcacher_impl* fci)
{
	if (_fontcacher == fci) {
		return 0;
	}

	auto_mutex am(_mut);
	if (!fci) {
		// since the cache become NULL, we just
		// release all textchars
		release_textchar_unlocked();
	}
	else {
		// check if we need to allocate textchars
		if (!_str && _count) {
			_str = new textchar [_count];
			assert(NULL != _str);
			init_textchar(_str, _count);
		}
	}

	// switch the cacher
	release_fcacher();
	_fontcacher = fci;

	// load the string from new font and fcacher
	if (_str) {
		// reload all charactors using new cacher
		if (loadstr()) return -1;
	}
	return 0;
}

int textline_impl::set_font(fontmgr::font_impl* fnt)
{
	int charset = -1;
	if (fnt) {
		// check if we already set the font
		charset = fnt->get_charset();
		assert(charset < fcs_max);
		if (_fonts[charset] == fnt) return 0;

		// grant this font
		if (fnt->grant()) return -1;
	}

	auto_mutex am(_mut);
	if (!fnt) {
		// since the font will be NULL, we just
		// release all textchars
		release_textchar_unlocked();
	}
	else {
		// check if we need to allocate textchars
		if (!_str && _fontcacher && _count) {
			_str = new textchar [_count];
			assert(NULL != _str);
		}
	}

	// switch the font
	release_font_unlocked(charset);
	_fonts[charset] = fnt;

	// load the string from new font and fcacher
	if (_str) {
		assert(NULL != _fontcacher);
		// reload all charactors using new cacher
		if (loadstr()) return -2;
	}
	return 0;
}

void textline_impl::release_font_unlocked(int charset)
{
	// todo:
}

void textline_impl::release_fcacher(void)
{
	if (!_fontcacher) return;
}

textline create_textline(const char* utf8, int width, zas::utils::mutex* mut)
{
	textline ret;
	if (!utf8 || !*utf8) {
		return ret;
	}
	textline_impl* ti = new textline_impl(utf8, width, mut);
	if (NULL == ti) return ret;

	ret = reinterpret_cast<textline_object*>(ti);
	return ret;
}

int textline_object::addref(void)
{
	textline_impl* ti = reinterpret_cast<textline_impl*>(this);
	return ti->addref();
}

int textline_object::release(void)
{
	textline_impl* ti = reinterpret_cast<textline_impl*>(this);
	return ti->release();
}

int textline_object::setfont(font fo)
{
	textline_impl* tl = reinterpret_cast<textline_impl*>(this);
	fontmgr::font_impl* fi = reinterpret_cast<fontmgr::font_impl*>(fo.get());
	return tl->set_font(fi);
}

int textline_object::setcacher(fontcacher fc)
{
	textline_impl* tl = reinterpret_cast<textline_impl*>(this);
	fontcacher_impl* fi = reinterpret_cast<fontcacher_impl*>(fc.get());
	return tl->set_fontcacher(fi);
}

}} // end of namespace zas::hwgrph
/* EOF */
