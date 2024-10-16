/** @file buffer.h
 * definition of fifo buffer for event loop
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_BUFFER

#ifndef __CXX_ZAS_UTILS_BUFFER_H__
#define __CXX_ZAS_UTILS_BUFFER_H__

namespace zas {
namespace utils {

enum fb_seektype {
	seek_set = 0,
	seek_cur = 1
};

class UTILS_EXPORT fifobuffer
{
	ZAS_DISABLE_EVIL_CONSTRUCTOR(fifobuffer);
	fifobuffer();

public:

	/**
	  check and allow only one to enter the buffer
	  note: the enter() will fail if any one has already
	  enter the buffer and not exit
	  @return true for success
	  */
	bool enter(void);

	/**
	  Exit the buffer, means other one could enter
	  the buffer
	  @return none
	  */
	void exit(void);

	/**
	  This is to clear all data in buffer and truncate
	  the buffer size to 0
	  @return none
	  */
	void drain(void);

	/**
	  Check if the buffer is empty (size = 0)
	  @return true mean empty
	  */
	bool empty(void);

	/**
	  Get the data size of the buffer
	  @return the data size
	  */
	size_t getsize(void);

	/**
	  Get the available buffer size, which means how
	  many data we could feed to buffer to exceed the
	  maximum size of the buffer
	  @return the size of available buffer size
	  */
	size_t get_availsize(void);

	/**
	  Read and remove data from the buffer
	  the data will be saved in the provided buffer

	  @param buf buffer to fill the removed data
	  @param sz size we request to remove, note that the
	  buff must be bigger than the size or unexpected
	  behavior may happen
	  @return the actual size we remove, may smaller than
	  the sz as parameter when there is not enough data
	  in the buffer
	  */
	size_t remove(void* buf, size_t sz);

	// discard data from the buffer
	/**
	 * @brief discard data from the buffer
	 * 
	 * @param sz 	discard data size
	 * @return size_t 
	 */
	size_t discard(size_t sz);

	/**
	  Append the buf to the tail of the buffer
	  @return the size that is appended into the buffer
	  note the size may smaller than the sz as parameter if
	  the buffer exceeds the maximum size
	  */
	size_t append(void* buf, size_t sz);

	/**
	  Read data from fd and append to the buffer
	  @return the size that is appended into the buffer
	  note the size may smaller than the sz as parameter if
	  the buffer exceeds the maximum size
	  */
	size_t append(int fd, size_t sz);

	/**
	  Read data from the the buffer, the data will
	  remain in the buffer (not removed)
	  @param pos position of the start data to read
	  @param buf buffer for the data
	  @param sz size of data to be read
	  @return the actual size we remove, may smaller than
	  the sz as parameter when there is not enough data
	  in the buffer
	  */
	size_t peekdata(size_t pos, void* buf, size_t sz);

	/**
	  seek to the specific position
	  @param pos the position we need to seek
	  @param st seek type:
	    1) seek_set seek from the beginning, pos is the absulote position
		2) seek_cur seek from current position, pos is the relative position
	  @return 0 for success
	  */
	int seek(size_t pos, fb_seektype st = seek_set);

	/**
	  Read data from current pos (set by seek()) and move the offset
	  @param buf the buffer for read data
	  @param sz size of data to be read
	  @return how many data did we read
	  */
	size_t read(void* buf, size_t sz);
};

/**
  Create an instance of fifobuffer
  note: must use this function to create fifobuffer
  "buf = new fifobuffer()" is forbidden and its
  behavior is unexpected

  @return the instance of a new fifobuffer
  */
UTILS_EXPORT fifobuffer* create_fifobuffer(void);

class UTILS_EXPORT readonlybuffer
{
	ZAS_DISABLE_EVIL_CONSTRUCTOR(readonlybuffer);
public:
	readonlybuffer() = delete;
	~readonlybuffer() = delete;

	/**
	  Get the data size of the buffer
	  @return the data size
	  */
	size_t getsize(void);

	/**
	  Read data from the the buffer, the data will
	  remain in the buffer (not removed)
	  @param pos position of the start data to read
	  @param buf buffer for the data
	  @param sz size of data to be read
	  @return the actual size we remove, may smaller than
	  the sz as parameter when there is not enough data
	  in the buffer
	  */
	size_t peekdata(size_t pos, void* buf, size_t sz);

	/**
	  seek to the specific position
	  @param pos the position we need to seek
	  @param st seek type:
	    1) seek_set seek from the beginning, pos is the absulote position
		2) seek_cur seek from current position, pos is the relative position
	  @return 0 for success
	  */
	int seek(size_t pos, fb_seektype st = seek_set);

	/**
	  Read data from current pos (set by seek()) and move the offset
	  @param buf the buffer for read data
	  @param sz size of data to be read
	  @return how many data did we read
	  */
	size_t read(void* buf, size_t sz);

};

}} // end of namespace zas::utils
#endif /* __CXX_ZAS_UTILS_BUFFER_H__ */
#endif // UTILS_ENABLE_FBLOCK_BUFFER
/* EOF */

