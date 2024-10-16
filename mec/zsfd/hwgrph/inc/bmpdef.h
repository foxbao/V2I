/** @file bmpdef.h
 * declaration of the bmp file structures
 */

#ifndef __CXX_ZAS_HWGRPH_BMP_DEF_H__
#define __CXX_ZAS_HWGRPH_BMP_DEF_H__

#include "hwgrph/hwgrph.h"

namespace zas {
namespace hwgrph {

struct BITMAPFILEHEADER
{
	uint16_t	bfType;
	uint32_t	bfSize;
	uint16_t	bfReserved1;
	uint16_t	bfReserved2;
	uint32_t	bfOffBits;
} PACKED;

struct RGBQUAD
{
	uint8_t    rgbBlue;
	uint8_t    rgbGreen;
	uint8_t    rgbRed;
	uint8_t    rgbReserved;
} PACKED;

struct BITMAPINFOHEADER
{
	uint32_t	biSize;
	int32_t		biWidth;
	int32_t		biHeight;
	uint16_t	biPlanes;
	uint16_t	biBitCount;
	uint32_t	biCompression;
	uint32_t	biSizeImage;
	int32_t		biXPelsPerMeter;
	int32_t		biYPelsPerMeter;
	uint32_t	biClrUsed;
	uint32_t	biClrImportant;
} PACKED;

struct BITMAPINFO
{
	BITMAPINFOHEADER	bmiHeader;
	RGBQUAD				bmiColors[1];
} PACKED;

struct BMP_HEADER
{
	BITMAPFILEHEADER file_header;
	BITMAPINFOHEADER info_header;
} PACKED;

#define BMP_MAGIC	(0x4D42)

}} // end of namespace zas::utils
#endif // __CXX_ZAS_HWGRPH_BMP_DEF_H__
/* EOF */
