/** @file font.h
 * definition of font related structure
 */

#ifndef __CXX_ZAS_HWGRPH_FONT_H__
#define __CXX_ZAS_HWGRPH_FONT_H__

#include "hwgrph.h"
#include "std/smtptr.h"
#include "utils/uri.h"

namespace zas {
namespace hwgrph {

enum fontstyle
{
	fts_unknown = -1,
	fts_regular,
	fts_light,
	fts_bold,
	fts_italic,
	fts_bold_italic,
	fts_max,
	fts_count = fts_max,
};

enum font_charset
{
	fcs_unknown = -1,
	fcs_western,
	fcs_eastern,

	fcs_max,
	fcs_count = fcs_max,
};

class HWGRPH_EXPORT fontface
{
public:
	fontface() = delete;
	~fontface() = delete;

	/**
	  Register a face from font file to font face pool
	  @param facename face name
	  @param font file name (with path)
	  @param faceid indicate the faceid in the font file
	  @param type westen or eastern
	  @param ftype retular/bold/italic
	 */
	static fontface* register_face(const char* facename,
		const zas::utils::uri& filename, unsigned int faceid,
		font_charset type = fcs_western,
		fontstyle ftype = fts_regular);

	ZAS_DISABLE_EVIL_CONSTRUCTOR(fontface);
};

class HWGRPH_EXPORT font_object
{
public:
	font_object() = delete;
	~font_object() = delete;

	int addref(void);
	int release(void);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(font_object);
};

typedef zas::smtptr<font_object> font;

class HWGRPH_EXPORT fontcacher_object
{
public:
	fontcacher_object() = delete;
	~fontcacher_object() = delete;

	int addref(void);
	int release(void);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(fontcacher_object);
};

typedef zas::smtptr<fontcacher_object> fontcacher;

/**
  Get the global font cacher
  @return the font cacher
 */
HWGRPH_EXPORT fontcacher get_global_fontcacher(void);

/**
  Create the font object
  @param facename the face name
  @param style the font style
  @param width the width of font
  @param height the height of font
 */
HWGRPH_EXPORT font createfont(const char* facename, fontstyle style,
	int width, int height = 0);

/**
  Set the current process as the font manager
 */
HWGRPH_EXPORT void set_as_font_manager(void);

/**
  Publish all register fonts so that all app could
  access to the font
  note that only the font worker process has the right
  to do this operation
  @return 0 for success
 */
HWGRPH_EXPORT int publish_fonts(void);

}} // end of namespace zas::hwgrph
#endif // __CXX_ZAS_HWGRPH_FONT_H__
/* EOF */
