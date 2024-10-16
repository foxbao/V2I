/** @file format.h
 * definition of formatted text object
 */

#ifndef __CXX_ZAS_HWGRPH_FORMAT_H__
#define __CXX_ZAS_HWGRPH_FORMAT_H__

#include "font.h"
#include "std/smtptr.h"

namespace zas {
namespace utils {
class mutex;
}}

namespace zas {
namespace hwgrph {

class font_object;

class HWGRPH_EXPORT textline_object
{
public:
	textline_object() = delete;
	~textline_object() = delete;
	int addref(void);
	int release(void);

	/**
	  Set the font for this textline
	  @param fo the font object
	  @return 0 for success
	 */
	int setfont(font fo);

	/**
	  Set the current font cacher for the textline
	  @param fc the font cacher
	  @return 0 for success
	 */
	int setcacher(fontcacher fc);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(textline_object);
};

typedef zas::smtptr<textline_object> textline;

/**
  Create a single line text
  @param utf8 text content in utf8 format
  @param width the max width for the text line which will
	be used to truncate the text. the truncate will be
	disabled if width <= 0
  @param mut a optional mutex for thread safe
  @return textline being created
 */
textline HWGRPH_EXPORT create_textline(const char* utf8, int width = -1,
	zas::utils::mutex* mut = NULL);

}} // end of namespace zas::hwgrph
#endif // __CXX_ZAS_HWGRPH_FORMAT_H__
/* EOF */
