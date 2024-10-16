/** @file imageloader.h
 * implmentation of image loader and predefined image loaders
 */

#include <malloc.h>

#include "std/list.h"
#include "utils/mutex.h"
#include "hwgrph/imageloader.h"

#include "inc/image.h"
#include "inc/bmpdef.h"

using namespace std;
using namespace zas::utils;

namespace zas {
namespace hwgrph {

struct imgldr_container
{
	listnode_t ownerlist;
	image_loader* loader;	
};

static mutex mut;
static listnode_t imgldr_list
	= LISTNODE_INITIALIZER(imgldr_list);

int register_imageloader(image_loader* ldr)
{
	if (NULL == ldr) {
		return -EBADPARM;
	}

	imgldr_container* imgldr = new imgldr_container();
	assert(NULL != imgldr);
	imgldr->loader = ldr;

	// check if we have an existed same loader
	const string name = ldr->get_name();
	auto_mutex am(mut);
	for (listnode_t* nd = imgldr_list.next;
		nd != &imgldr_list; nd = nd->next) {
		imgldr_container* c = list_entry(imgldr_container, ownerlist, nd);
		assert(c->loader != NULL);
		if (name == c->loader->get_name()) {
			delete ldr;
			return -EEXISTS;
		}
	}

	listnode_add(imgldr_list, imgldr->ownerlist);
	return 0;
}

class bmp_loader : public image_loader
{
public:
	bmp_loader()
	: _flags(0)
	, _width(0), _height(0)
	{
	}

	~bmp_loader() = delete;

	const string get_name(void) {
		return "bmp";
	}

	bool check_format(const uint8_t magic[16])
	{
		BMP_HEADER* bmphdr = (BMP_HEADER*)(magic);
		if (bmphdr->file_header.bfType == BMP_MAGIC)
		{
			_flags = 0;
			_f.format_checked = 1;
			return true;
		}
		return false;
	}

	int getinfo(absfile* file, int& width, int& height)
	{
		uint32_t bpl, bc;
	
		if (!_f.format_checked) {
			return -1;
		}

		file->rewind();
		if (sizeof(BMP_HEADER) != file->read(&_bmphdr, sizeof(BMP_HEADER)))
			return -2;

		if (_bmphdr.file_header.bfType != BMP_MAGIC)
			return -3;

		// compressed bmp is not supported
		// only BI_RGB = 0 is supported
		if (_bmphdr.info_header.biCompression)
			return -4;

		bc = _bmphdr.info_header.biBitCount;
		if (24 != bc && 16 != bc && 32 != bc)
			return -5;

		_width = width = _bmphdr.info_header.biWidth;
		_height = height = _bmphdr.info_header.biHeight;
		_f.info_gathered = 1;
		return 0;
	}

	int load_membuffer(absfile* file, void* buf,
		int width, int height)
	{
		if (!_f.info_gathered) return -ELOGIC;
		if (!file || !buf || !width || !height)
			return -EBADPARM;

		file->seek(_bmphdr.file_header.bfOffBits,
			absfile_seek_set);

		if (do_load_bmp_file(file, width, height, buf)) {
			return -1;
		}
		_f.membuf_loaded = 1;
		return 0;
	}

private:

	int do_load_bmp_file(absfile* file, int width, int hei, void* buf)
	{
		uint32_t bmpbpl = _bmphdr.info_header.biWidth
			* _bmphdr.info_header.biBitCount / 8;
		bmpbpl = (bmpbpl + 3) & ~3;

		uint32_t minbpl = _bmphdr.info_header.biWidth;
		if (minbpl > width) minbpl = width;

		uint32_t height_ = _bmphdr.info_header.biHeight;
		if (height_ > hei) height_ = hei;

		uint32_t stride = width * HWGRPH_BPP;
		uint8_t *scanline = (uint8_t*)alloca(bmpbpl);
		uint8_t *dest = ((uint8_t*)buf) + stride * height_ - stride;

		switch (_bmphdr.info_header.biBitCount)
		{
		case 16: {
			for (; height_ > 0; --height_, dest -= stride)
			{
				if (bmpbpl != file->read(scanline, bmpbpl))
					return -1;
				RGBA32_ColorMapCopy_16_32(dest, scanline, minbpl);
			}
		} break;

		case 24: {
			for (; height_ > 0; --height_, dest -= stride)
			{
				if (bmpbpl != file->read(scanline, bmpbpl))
					return -2;
				RGBA32_ColorMapCopy_24_32(dest, scanline, minbpl);
			}
		} break;

		case 32: {			
			for (; height_ > 0; --height_, dest -= stride)
			{
				if (bmpbpl != file->read(scanline, bmpbpl))
					return -3;
				RGBA32_ColorMapCopy_32_32(dest, scanline, minbpl);
			}
		} break;

		default: return -4;
		}
		return 0;
	}

	void RGBA32_ColorMapCopy_16_32(void *to, void *from, uint32_t pixels)
	{
		uint16_t* src = (uint16_t*)from;
		uint32_t* dst = (uint32_t*)to;

		for (; pixels > 0; --pixels, ++dst)
		{
			uint16_t val = *src++;
			// BGR(5,5,5)
			uint32_t b = (val & 31);
			uint32_t g = ((val >> 5) & 31);
			uint32_t r = ((val >> 10) & 31);
			*dst = r << 3 | (g << 11) | (b << 19) | 0xFF000000;
		}
	}

	void RGBA32_ColorMapCopy_24_32(void *to, void *from, uint32_t pixels)
	{
		uint8_t* src = (uint8_t*)from;
		uint32_t *dst = (uint32_t*)to;

		for (; pixels > 0; --pixels, ++dst)
		{
			// BGR888
			uint32_t b = (uint32_t) *src++;		// b
			uint32_t g = (uint32_t) *src++;		// g
			uint32_t r = (uint32_t) *src++;		// r
			*dst = r | (g << 8) | (b << 16) | 0xFF000000;
		}
	}

	void RGBA32_ColorMapCopy_32_32(void *to, void *from, uint pixels)
	{
		uint32_t *src = (uint32_t*)from;
		uint32_t *dst = (uint32_t*)to;

		for (; pixels > 0; --pixels, ++dst)
		{
			// BGRA8888
			uint32_t v = *src++;
			uint32_t r = (v & 0xFF0000) >> 16;
			uint32_t b = (v & 0xFF)  << 16;
			*dst = (v & 0xFF00FF00) | r | b;
		}
	}

private:
	BMP_HEADER _bmphdr;
	union {
		uint32_t _flags;
		struct {
			uint32_t format_checked : 1;
			uint32_t info_gathered : 1;
			uint32_t membuf_loaded : 1;
		} _f;
	};
	int _width, _height;
};

image_loader* check_image_file_format(const uint8_t magics[])
{
	auto_mutex am(mut);
	listnode_t* node = imgldr_list.next;
	for (; node != &imgldr_list; node = node->next)
	{
		imgldr_container* c = list_entry(imgldr_container, ownerlist, node);
		assert(c->loader != NULL);
		if (c->loader->check_format(magics)) {
			return c->loader;
		}
	}
	return NULL;
}

int do_loadimage(const char* filename, image& img)
{
	absfile* file = absfile_open(filename, "rb");
	if (NULL == file) return -ELOGIC;

	// read first 16 bytes to check about
	// the image file format
	uint8_t magics[16];

	file->seek(0, absfile_seek_end);
	long fsz = file->getpos();
	if (fsz < 16) return -EBADOBJ;
	file->rewind();
	if (16 != file->read(magics, 16)) {
		return -EBADOBJ;
	}

	file->rewind();
	// check the image file type
	image_loader* loader = check_image_file_format(magics);
	if (NULL == loader) {
		return -EUNRECOGNIZED;
	}
	int width, height;
	if (loader->getinfo(file, width, height)) {
		return -ENOTSUPPORT;
	}

	image_impl* imgimp;
	bool new_image = false;

	if (!img) {
		imgimp = new image_impl(width, height, filename);
		if (NULL == imgimp) return -ENOMEMORY;
		new_image = true;
	}
	else imgimp = reinterpret_cast<image_impl*>(img.get());

	// load the image in lock status
	mut.lock();
	imgimp->loadimage(loader, file, width, height);
	mut.unlock();

	if (new_image) {
		img = reinterpret_cast<image_object*>(imgimp);
	}
	return 0;
}

image loadimage(const char* filename)
{
	image ret;
	if (NULL == filename) {
		return ret;
	}

	// see if we have buffed this image
	image_impl* imgimp = image_impl::getimage(filename);
	if (imgimp) {
		ret = reinterpret_cast<image_object*>(imgimp);
		if (imgimp->texturized()) {
			// not texturied, we do it here
			do_loadimage(filename, ret);
		}
		return ret;
	}
	if (do_loadimage(filename, ret)) {
		return ret;
	}
	return ret;
}

void init_imageloader(void)
{
	register_imageloader(new bmp_loader());
}

}} // end of namespace zas::hwgrph
/* EOF */
