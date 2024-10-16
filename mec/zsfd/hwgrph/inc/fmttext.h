/** @file fmttext.h
 * declaration of the formatted text object
 */

#ifndef __CXX_ZAS_HWGRPH_FMT_TEXT_H__
#define __CXX_ZAS_HWGRPH_FMT_TEXT_H__

#include "std/list.h"
#include "fontmgr.h"
#include "fontcache.h"

namespace zas {
namespace hwgrph {

struct textchar
{
	union {
		fontcacher_impl::glyphinfo* glyph;
		int next_unloaded;
	};

	void init_via_glyphcache(glyphcache_object* gcache,
		fontcacher_impl* cacher, bool gbl);
};

class textline_impl
{
public:
	textline_impl(const char* utf8str, int width = -1,
		zas::utils::mutex* mut = NULL);
	~textline_impl();

	int set_fontcacher(fontcacher_impl* fci);
	int set_font(fontmgr::font_impl* fnt);

public:
	int addref(void) {
		return __sync_add_and_fetch(&_refcnt, 1);
	}
	int release(void);

private:
	int loadstr(void);

	int parse_str_unlocked(const char* str);
	void release_textchar_unlocked(void);

	void release_font_unlocked(int charset);
	void release_fcacher(void);

private:
	int _refcnt;
	uint16_t* _origstr;
	textchar* _str;
	int _count;
	int _width;
	zas::utils::mutex* _mut;
	fontmgr::font_impl* _fonts[fcs_count];
	fontcacher_impl* _fontcacher;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(textline_impl);
};

class fmttext_impl
{
private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(fmttext_impl);
};

}} // end of namespace zas::hwgrph
#endif // __CXX_ZAS_HWGRPH_FMT_TEXT_H__
/* EOF */
