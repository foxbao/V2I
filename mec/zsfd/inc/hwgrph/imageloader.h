/** @file imageloader.h
 * definition of image loader that makes it possible to let customer
 * to implement their own image loaders
 */

#ifndef __CXX_ZAS_HWGRPH_IMAGE_LOADER__H__
#define __CXX_ZAS_HWGRPH_IMAGE_LOADER__H__

#include "std/smtptr.h"
#include "utils/absfile.h"
#include "hwgrph.h"
#include <string>

namespace zas {
namespace hwgrph {

zas_interface image_loader
{
	/**
	  Get the name of the image loader
	  @return the name of the loader
	 */
	virtual const std::string get_name(void) = 0;

	/**
	  check the image format
	  the input data is the first 16 bytes
	  of the image file, the method need to check
	  if the image file format is supported or not
	  @param magic the 16 bytes buffer
	  @return true for supporting format
	 */
	virtual bool check_format(const uint8_t magic[16]) = 0;

	/**
	  Check and get the image info
	  the width and height will be used to allocate memory
	  @param file the image file
	  @param width [out] the image width
	  @param height [out] the image height
	 */
	virtual int getinfo(zas::utils::absfile* file,
		int& width, int& height) = 0;

	/**
	  load the image to the memory buffer
	  the width and height as input is given
	  for sanity check
	  @param file the image file
	  @param buf the memory buffer pre-allocated
	  @param width [in] the image width for check
	  @param height [in] the image height for check
	  @return 0 for success
	 */
	virtual int load_membuffer(zas::utils::absfile* file,
		void* buf, int width, int height) = 0;
};

class HWGRPH_EXPORT image_object
{
public:
	image_object() = delete;
	~image_object() = delete;

	int addref(void);
	int release(void);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(image_object);
};

typedef zas::smtptr<image_object> image;

/**
  Load the image by its file name
  @param filename the file name of the file
  @return the loaded image
 */
HWGRPH_EXPORT image loadimage(const char* filename);

}} // end of namespace zas::hwgrph
#endif // __CXX_ZAS_HWGRPH_IMAGE_LOADER__H__
/* EOF */
