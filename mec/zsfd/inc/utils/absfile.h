/** @file inc/utils/absfile.h
 * definition of an abstract file
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_ABSFILE

#ifndef __CXX_UTILS_ABS_FILE_H__
#define __CXX_UTILS_ABS_FILE_H__

#include "uri.h"

namespace zas {
namespace utils {

enum absfile_seek_type
{
	absfile_seek_set = 0,
	absfile_seek_cur,
	absfile_seek_end,
};

enum mmap_flags
{
	absfile_map_prot_exec = 1,
	absfile_map_prot_read = 2,
	absfile_map_prot_write = 4,
	absfile_map_shared = 8,
	absfile_map_private = 16,
};

zas_interface absfile
{
	/**
	  Close the abstract file and release the object
	 */
	virtual void release(void) = 0;

	/**
	  Close the file
	  @return 0 for success
	 */
	virtual int close(void) = 0;

	/**
	  Read data from file
	  @param ptr the buffer for reading data
	  @param sz the size to read
	  @return the size that has been read
	 */
	virtual size_t read(void* ptr, size_t sz) = 0;

	/**
	  Read data from file
	  @param ptr the buffer for reading data
	  @param sz the item size to read
	  @param nmemb the item count to be read
	  @return items that has been read
	 */
	virtual size_t read(void* ptr, size_t sz, size_t nmemb) = 0;

	/**
	  Write data to file
	  @param ptr the buffer to writing data
	  @param sz the size to write
	  @return the size that has been written
	 */
	virtual size_t write(void* ptr, size_t sz) = 0;

	/**
	  Write data to file
	  @param ptr the buffer to writing data
	  @param sz the size to write
	  @param nmemb the item count to be written
	  @return items that has been written
	 */
	virtual size_t write(void* ptr, size_t sz, size_t nmemb) = 0;

	/**
	  Move the file position pointer to the start
	  @return 0 for success
	 */
	virtual int rewind(void) = 0;

	/**
	  Seek to the specified position
	  @param offset the offset to be set
	  @param whence set for absolute or relative seek
	  @return 0 for success
	 */
	virtual int seek(long offset, int whence) = 0;

	/**
	  Get the current position
	  @return the position
	 */
	virtual long getpos(void) = 0;

	/**
	  map the file
	  @param offset the start offset to be mapped
	  @param length the length to be mapped
	  @param flags the flags
	  @param addr a hint for identifying the address
	  @return the mapped address
	 */
	virtual void* mmap(off_t offset, size_t length, int flags, void* addr = NULL) = 0;

	/**
	  unmap the file
	  @param addr the address goint to be unmapped
	  @param length the length to be unmapped
	 */
	virtual int munmap(void* addr, size_t length) = 0;
};

zas_interface absfile_category
{
	/**
	  release the absfile_category object
	 */
	virtual void release(void) = 0;

	/**
	  normalize the uri for this absfile category
	  @param uri the inputted uri
	  @return the normalized uri
	 */
	virtual int normalize_uri(uri& u) = 0;
	/**
	  Open a file
	  @param the uri of the file
	  @param cfg the fopen like configure string
	  @return absfile object
	 */
	virtual absfile* open(const uri& uri, const char* cfg) = 0;
};

/**
  register a new category to the registry
  @param type the type of the absfile category
  @param category the category handler
  @return 0 for success
 */
UTILS_EXPORT int absfile_category_register(const char* type,
	absfile_category* category);

/**
  Open an abs file
  @param the uri of the file
  @param cfg the fopen like configure string
  @return 0 for success
 */
UTILS_EXPORT absfile* absfile_open(const uri& u, const char* cfg);

/**
  Normalize the uri for the recognized absfile category
  @param uri the abs file uri to be normalized
  @return 0 for success
 */
UTILS_EXPORT int absfile_normalize_uri(uri& u);

}} // end of namespace zas::utils
#endif // __CXX_UTILS_ABS_FILE_H__
#endif // UTILS_ENABLE_FBLOCK_ABSFILE
/* EOF */
