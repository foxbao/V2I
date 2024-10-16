/** @file buffer.h
 * definition of fifo buffer for event loop
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_BUFFER

#ifndef __CXX_ZAS_UTILS_FIFOBUFFER_H__
#define __CXX_ZAS_UTILS_FIFOBUFFER_H__

#include "std/list.h"
#include "utils/mutex.h"
#include "utils/buffer.h"

namespace zas {
namespace utils {

class mutex;

class fifobuffer_impl
{
	friend class fifobuffer;
	friend class readonlybuffer;

	static const int chunk_size = 4 * 1024;
	static const int max_chunkcnt = 256;

	union chunk
	{
		struct header {
			listnode_t ownerlist;
		} hdr;
		char buffer[chunk_size];
	};

public:

	fifobuffer_impl()
	: _start(0), _end(0)
	, _chunkcnt(0), _flags(0)
	, _offset(0), _chkoff(NULL)
	, _mut(NULL) {
		listnode_init(_chunklist);
	}

	~fifobuffer_impl() {
		drain();
	}

	// enter the buffer
	bool enter(void) {
		if (_f.locked) return false;
		_f.locked = 1;
		return true;
	}

	// exit the buffer
	void exit(void) {
		_f.locked = 0;
	}

	// clear all data in buffer
	void drain(void);

	// discard data from the buffer
	size_t discard(size_t sz) {
		return remove_discard(NULL, sz);
	}

	// check if the buffer is empty
	bool empty(void);

	// get the data size in the buffer
	size_t getsize(void);

	// get the available size for write of the buffer
	size_t get_availsize(void);

	// read and remove data from the buffer
	size_t remove(void* buf, size_t sz);

	// get the suitable size for append data
	size_t append_getsize(void);

	// append data
	size_t append(void* buf, size_t sz);

	// append data from a file
	size_t append(int fd, size_t sz);

	// read data randomly in the buffer
	size_t peekdata(size_t pos, void* buf, size_t sz);

	// seek to specific position
	int seek(size_t pos, fb_seektype st = seek_set);

	// read data
	size_t read(void* buf, size_t sz);

private:

	// get the chunk and offset
	void getoffset(listnode_t*& item, size_t& start);
	
	// calc the chunk and in chunk offset according to pos
	int calc_offset(size_t pos, chunk*& chkoff, size_t& off);
	
	// calc the chunk and in chunk offset according to relative delta
	int calc_offset_delta(size_t delta);

	// release the first chunk (fifo behavior)
	int release_chunk(void);

	// append as the last chunk (fifo behavior)
	chunk* append_chunk(void);

	// read the data and try to move the offset pointer
	size_t read_data(chunk* chk, size_t pos, char* buf, size_t sz,
		bool moveoffset = false);

	// do remove or discard
	size_t remove_discard(void* buf, size_t sz);

private:
	listnode_t _chunklist;
	size_t _start;	// including sizeof(chunk::header)
	size_t _end;	// including sizeof(chunk::header)
	size_t _offset;	// not include sizeof(chunk::header)
	chunk* _chkoff;
	int _chunkcnt;
	mutex* _mut;

	union {
		uint32_t _flags;
		struct {
			uint32_t locked : 1;
		} _f;
	};
};

}} // end of namespace zas::utils
#endif /* __CXX_ZAS_UTILS_BUFFER_H__ */
#endif // UTILS_ENABLE_FBLOCK_BUFFER
/* EOF */

