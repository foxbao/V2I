/** @file font_freetype.cpp
 * implmentation of the font engine via freetype
 */

#include "utils/mutex.h"
#include "utils/absfile.h"

#include "inc/fontmgr.h"
#include "inc/font_freetype.h"

using namespace zas::utils;

namespace zas {
namespace hwgrph {

font_engine_freetype::font_engine_freetype()
: _library(NULL)
, _last_error(0)
{
	_last_error = FT_Init_FreeType(&_library);
	assert(NULL != _library);
}

font_engine_freetype::~font_engine_freetype()
{
	if(_library)
	{
		FT_Done_FreeType(_library);
		_library = NULL;
	}
}

static void strupr(char *buf)
{
	for (; *buf; ++buf) {
		char c = *buf;
		if (c >= 'a' && c <= 'z')
			*buf -= ('a' - 'A');
	}
}

int font_engine_freetype::get_fontinfo(const uri& file_name,
		int faceid, fontstyle& style, char family_name[64])
{
	assert(NULL != _library);

	absfile* f = absfile_open(file_name, "rb");
	if (NULL == f) return -1;

	void* ptr = f->mmap(0, 0, absfile_map_prot_read
		| absfile_map_private);
	if (NULL == ptr) {
		f->release(); return -2;
	}

	f->seek(0, absfile_seek_end);
	FT_Long fsz = f->getpos();

	FT_Face fontface;
	if (FT_New_Memory_Face(_library, (const FT_Byte*)ptr,
		fsz, faceid, &fontface))
		return -3;

	size_t fn_sz = strlen(fontface->family_name);
	if (fn_sz > 63)
	{
		memcpy(family_name, fontface->family_name, 63);
		family_name[63] = '\0';
	}
	else strcpy(family_name, fontface->family_name);

	char style_name[32];
	strcpy(style_name, fontface->style_name);
	strupr(style_name);

	if (!strcmp(style_name, "REGULAR")) style = fts_regular;
	else if (!strcmp(style_name, "BOLD")) style = fts_bold;
	else if (!strcmp(style_name, "ITALIC")) style = fts_italic;
	else if (!strcmp(style_name, "BOLD ITALIC")) style = fts_bold_italic;
	else {
		style = fts_regular;
		if (fn_sz < 62)
		{
			family_name[fn_sz] = ' ';
			size_t sn_sz = strlen(style_name);
			if (fn_sz + sn_sz >= 62)
			{
				memcpy(&family_name[fn_sz + 1], fontface->style_name, 62 - fn_sz);
				family_name[63] = '\0';
			}
			else strcpy(&family_name[fn_sz + 1], fontface->style_name);
		}
	}   
	FT_Done_Face(fontface);
	f->munmap(ptr, 0);
	f->release();
	return 0;
}

faceinfo_freetype::~faceinfo_freetype()
{
	if (_face) {
		FT_Done_Face(_face);
		_face = NULL;
	}
}

int font_engine_freetype::load_face(void* filemap,
	long sz, int faceid, faceinfo_freetype& f)
{
	if (f._face) return 0;

	// todo: auto_mutex am(f._mut);
	if (NULL == filemap || !sz) return -1;
	_last_error = FT_New_Memory_Face(
		_library, (const FT_Byte*)filemap,
		sz, faceid, &f._face);

	if (_last_error) return _last_error;
	if (f._face->face_flags & FT_FACE_FLAG_SCALABLE)
		return 0;
	else return -ENOTSUPPORT;
}

int font_engine_freetype::face_size(
	faceinfo_freetype& f, int width, int height)
{
	if (!f._face) return -1;

	// we need lock
	// todo: f._mut.lock();
	if (f._width == width && f._height == height) {
		// todo: f._mut.unlock();
		return 0;
	}
	_last_error = FT_Set_Pixel_Sizes(f._face,
		width, height);
	// todo: f._mut.unlock();

	if (_last_error) return _last_error;

	f._linespacing = double(f._face->size->metrics.height) / 64.0;

	// update the size
	f._width = width;
	f._height = height;
	return 0;
}

bool faceinfo_freetype::is_global(void)
{
	fontmgr::fontface_node* node =
		list_entry(fontmgr::fontface_node, face, this);
	return (node->flags & FONTFACE_SHADOW)
		? true : false;
}

uint16_t font_engine_freetype::get_defaultchar(faceinfo_freetype& f)
{
	// todo: reimplment if necessary
	return '?';
}

uint32_t font_engine_freetype::char_index_unlocked(
	faceinfo_freetype&f, int code)
{
	if (!f._face) return 0;

	FT_UInt glyph_index = FT_Get_Char_Index(f._face, code);
	return glyph_index;
}

int font_engine_freetype::load_glyph(faceinfo_freetype& f,
	int code, glyphinfo_freetype& ginfo)
{
	if (!f._face) return -1;

	// todo: we need lock
	if (!f._width || !f._height) {
		return -2;
	}

	uint32_t glyph_index = char_index_unlocked(f, code);
	if (!glyph_index) return -ENOTFOUND;

	_last_error = FT_Load_Glyph(f._face, glyph_index,
		FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING
		| FT_LOAD_NO_AUTOHINT | FT_LOAD_RENDER);
	if (_last_error) return _last_error;

	FT_GlyphSlot slot = f._face->glyph;
	ginfo.width = double(slot->metrics.width) / 64.0;
	ginfo.height = double(slot->metrics.height) / 64.0;
	ginfo.advance_x = double(slot->advance.x) / 64.0;
	ginfo.advance_y = double(slot->advance.y) / 64.0;

	FT_Bitmap* bmp = &slot->bitmap;
	size_t bufsz = bmp->width * bmp->rows + sizeof(glyph_bitmap);

	// allocate bitmap memory
	if (f.is_global()) {
		ginfo.gbmp = (glyph_bitmap*)fontmgr::inst()->globalheap_alloc(bufsz);
	} else {
		ginfo.gbmp = (glyph_bitmap*)malloc(bufsz);
	}
	if (NULL == ginfo.gbmp) return -4;
	
	// prepare for the glyph image
	ginfo.gbmp->left = slot->bitmap_left;
	ginfo.gbmp->top = slot->bitmap_top;
	ginfo.gbmp->width = bmp->width;
	ginfo.gbmp->height = bmp->rows;

	uint32_t rows = bmp->rows;
	uint8_t* src = (uint8_t*)bmp->buffer;
	uint8_t* dst = ginfo.gbmp->buffer;
	for (uint32_t i = 0; i < rows; ++i)
	{
		memcpy(dst, src, bmp->width);
		src += bmp->pitch;
		dst += bmp->width;
	}
	return 0;
}

}} // end of namespace zas::hwgrph
/* EOF */
