/** @file font_freetype.h
 * declaration of the font engine implmented via freetype
 */

#ifndef __CXX_ZAS_HWGRPH_FONT_FREETYPE_H__
#define __CXX_ZAS_HWGRPH_FONT_FREETYPE_H__

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "utils/uri.h"
#include "hwgrph/font.h"

namespace zas {
namespace hwgrph {

class font_engine_freetype;

struct glyph_bitmap
{
	int left, top;
	int width, height;
	uint8_t buffer[4];
};

struct glyphinfo_freetype
{
	float width;
	float height;
	float advance_x;
	float advance_y;

	// the glyph bitmap that is located
	// in the font global heap
	glyph_bitmap* gbmp;
};

class faceinfo_freetype
{
	friend class font_engine_freetype;
public:
	faceinfo_freetype()
	: _face(NULL)
	, _width(0), _height(0) {}

	~faceinfo_freetype();

	bool loaded(void) {
		return (_face) ? true : false;
	}

	bool has_kerning(void) {
		return FT_HAS_KERNING(_face) ? true : false;
	}
	
private:
	bool is_global(void);

private:
	FT_Face _face;
	int _width;
	int _height;
	float _linespacing;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(faceinfo_freetype);
};

class font_engine_freetype
{
public:
	font_engine_freetype();
	~font_engine_freetype();

	int get_fontinfo(const zas::utils::uri& file_name,
		int faceid, fontstyle& style,
		char family_name[64]);

	// load the memory face
	int load_face(void* filemap, long sz,
		int faceid, faceinfo_freetype& f);
	
	int face_size(faceinfo_freetype& f, int width, int height);

	uint16_t get_defaultchar(faceinfo_freetype& f);
	
	// load a glyph
	int load_glyph(faceinfo_freetype& f, int code,
		glyphinfo_freetype& ginfo);

private:

	uint32_t char_index_unlocked(faceinfo_freetype&f, int code);

private:
	FT_Library _library;
	int _last_error;		// last error of freetype
	ZAS_DISABLE_EVIL_CONSTRUCTOR(font_engine_freetype);
};

}} // end of namespace zas::hwgrph
#endif // __CXX_ZAS_HWGRPH_FONT_FREETYPE_H__
/* EOF */
