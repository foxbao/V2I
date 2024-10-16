/** @file buffer.cpp
 * implementation of the fifo buffer
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_BUFFER

#include "utils/mutex.h"
#include "inc/fifobuffer.h"

namespace zas {
namespace utils {

size_t nonblk_read(int fd, void *vptr, size_t n);

void fifobuffer_impl::drain(void)
{
	while (!listnode_isempty(_chunklist)) {
		chunk* chk = list_entry(chunk, hdr.ownerlist, _chunklist.next);
		listnode_del(chk->hdr.ownerlist);
		free(chk);
	}
	_start = 0;
	_end = 0;
	_chunkcnt = 0;

	// clear the offset
	_chkoff = NULL;
	_offset = 0;
}

bool fifobuffer_impl::empty(void)
{
	if (!_chunkcnt) return true;
	if (_chunkcnt == 1 && _start == _end)
		return true;
	return false;
}

size_t fifobuffer_impl::getsize(void)
{
	if (!_chunkcnt)
		return 0;
	
	size_t ret = chunk_size * _chunkcnt;
	ret -= _start;
	ret -= chunk_size - _end;
	ret -= sizeof(chunk::header) * (_chunkcnt - 1);
	return ret;
}

size_t fifobuffer_impl::get_availsize(void)
{
	size_t ret = (max_chunkcnt - _chunkcnt) * chunk_size;
	if (_chunkcnt) ret += chunk_size - _end;
	return ret;
}

size_t fifobuffer_impl::remove(void* buf, size_t sz)
{
	assert(NULL != buf);
	return remove_discard(buf, sz);
}

size_t fifobuffer_impl::remove_discard(void* buf, size_t sz)
{
	if (!sz) return 0;
	if (!_chunkcnt) return 0;
	char* dst = reinterpret_cast<char*>(buf);

	while (_chunkcnt)
	{
		size_t content_size;
		chunk* chk = list_entry(chunk, hdr.ownerlist, _chunklist.next);
		if (_chunkcnt == 1) {
			assert(_start <= _end);
			content_size = _end - _start;
		}
		else content_size = chunk_size - _start;

		if (content_size <= sz) {
			if (buf) memcpy(dst, &chk->buffer[_start], content_size);
			// current chunk is drained, we release it
			release_chunk();
			dst += content_size;
			sz -= content_size;
		}
		else {
			if (buf) memcpy(dst, &chk->buffer[_start], sz);
			_start += sz;
			dst += sz;
			break;
		}
	}
	// rewind offset
	_chkoff = NULL;
	_offset = 0;
	return (size_t)dst - (size_t)buf;
}

size_t fifobuffer_impl::append_getsize(void)
{
	if (!_chunkcnt) {
		return chunk_size - sizeof(chunk::header);
	}
	// see the free space at last chunk
	if (_end < chunk_size) {
		return chunk_size - _end;
	}
	// _chunkcnt + 1 <= max_chunkcnt
	if (_chunkcnt < max_chunkcnt) {
		return chunk_size - sizeof(chunk::header);
	}
	return 0;
}

size_t fifobuffer_impl::append(void* buf, size_t sz)
{
	assert(NULL != buf);
	if (!sz) return 0;

	char* src = reinterpret_cast<char*>(buf);
	while (sz)
	{
		if (_chunkcnt && _end < chunk_size)
		{
			size_t content_size = chunk_size - _end;
			chunk* chk = list_entry(chunk, hdr.ownerlist, _chunklist.prev);
	
			if (content_size < sz) {
				memcpy(&chk->buffer[_end], src, content_size);
				// and we need a new chunk
				src += content_size;
				sz -= content_size;
				_end = sizeof(chunk::header);
			}
			else {
				memcpy(&chk->buffer[_end], src, sz);
				_end += sz;
				src += sz;
				break;
			}
		}

		// we need to append a new chunk
		if (!append_chunk())
			break;
	}
	return (size_t)src - (size_t)buf;
}

size_t fifobuffer_impl::append(int fd, size_t sz)
{
	size_t ret = 0;
	if (!fd || !sz) return 0;

	while (sz)
	{
		if (_chunkcnt && _end < chunk_size)
		{
			size_t content_size = chunk_size - _end;
			chunk* chk = list_entry(chunk, hdr.ownerlist, _chunklist.prev);

			if (content_size < sz) {
				size_t rsz = nonblk_read(fd, &chk->buffer[_end], content_size);
				// and we need a new chunk
				sz -= rsz;
				ret += rsz;
				if (rsz < content_size) {
					_end += rsz; break;
				}
				_end = sizeof(chunk::header);
			}
			else {
				size_t rsz = nonblk_read(fd, &chk->buffer[_end], sz);
				_end += rsz;
				ret += rsz;
				break;
			}
		}

		// we need to append a new chunk
		if (!append_chunk())
			break;
	}
	return ret;
}

size_t fifobuffer_impl::peekdata(size_t pos, void* buf, size_t sz)
{
	chunk* chk;
	size_t offset;
	if (calc_offset(pos, chk, offset) < 0)
		return 0;

	offset += sizeof(chunk::header);
	// the chk:offset is the start point
	return read_data(chk, offset, (char*)buf, sz);
}

int fifobuffer_impl::calc_offset(size_t pos, chunk*& chkoff, size_t& off)
{
	if (pos >= getsize()) {
		return -1;
	}

	chunk* chk = NULL;
	size_t chksz, start = _start;
	listnode_t* item = _chunklist.next;

	for (; item != &_chunklist; item = item->next)
	{
		chk = list_entry(chunk, hdr.ownerlist, item);
		if (item == _chunklist.prev) {
			assert(_end >= start);
			chksz = _end - start;
		}
		else chksz = chunk_size - start;
		if (chksz > pos) break;

		pos -= chksz;
		start = sizeof(chunk::header);
	}
	chkoff = chk;
	off = pos + start - sizeof(chunk::header);
	return 0;
}

void fifobuffer_impl::getoffset(listnode_t*& item, size_t& start)
{
	if (_chkoff) {
		item = &_chkoff->hdr.ownerlist;
		start = _offset + sizeof(chunk::header);
	}
	else {
		item = _chunklist.next;
		start = _start + _offset;
	}
}

int fifobuffer_impl::calc_offset_delta(size_t delta)
{
	if (!delta) return 0;

	listnode_t* item;
	chunk* chk = NULL;
	size_t start, chksz;
	
	getoffset(item, start);
	for (; item != &_chunklist; item = item->next)
	{
		chk = list_entry(chunk, hdr.ownerlist, item);
		if (item == _chunklist.prev) {
			assert(_end >= start);
			chksz = _end - start;
		}
		else chksz = chunk_size - start;

		if (chksz > delta) {
			_chkoff = chk;
			_offset = delta + start;
			_offset -= sizeof(chunk::header);
			return 0;
		}
		delta -= chksz;
		start = sizeof(chunk::header);
	}
	return -EEOF;
}

int fifobuffer_impl::seek(size_t pos, fb_seektype st)
{
	if (st == seek_set) {
		return calc_offset(pos, _chkoff, _offset);
	}
	else if (st == seek_cur) {
		return calc_offset_delta(pos);
	}
	else return -EBADPARM;
}

size_t fifobuffer_impl::read(void* buf, size_t sz)
{
	if (!sz) return 0;

	size_t start;
	listnode_t* item;

	getoffset(item, start);
	chunk* chk = list_entry(chunk, hdr.ownerlist, item);
	return read_data(chk, start, (char*)buf, sz, true);
}

// release the first chunk (fifo behavior)
int fifobuffer_impl::release_chunk(void)
{
	if (!_chunkcnt) return -1;
	chunk* chk = list_entry(chunk, hdr.ownerlist, _chunklist.next);
	listnode_del(chk->hdr.ownerlist);
	if (--_chunkcnt) {
		_start = sizeof(chunk::header);
	}
	else _start = _end = 0;

	// release the chunk object
	free(chk);
	return 0;
}

// append as the last chunk (fifo behavior)
fifobuffer_impl::chunk* fifobuffer_impl::append_chunk(void)
{
	// see if we exceed the limit
	if (_chunkcnt == max_chunkcnt) {
		return NULL;
	}

	chunk* chk = (chunk*)malloc(chunk_size);
	assert(NULL != chk);
		
	_end = sizeof(chunk::header);
	if (!_chunkcnt) _start = _end;
	listnode_add(_chunklist, chk->hdr.ownerlist);
	++_chunkcnt;
	return chk;
}

size_t fifobuffer_impl::read_data(chunk* chk, size_t pos, char* buf,
	size_t sz, bool moveoffset)
{
	chunk* chkoff;
	size_t off, remain, read = 0;
	while (sz && &chk->hdr.ownerlist != &_chunklist)
	{
		chkoff = chk;
		if (&chk->hdr.ownerlist == _chunklist.prev) {
			// this is the last chunk
			assert(pos <= _end);
			remain = _end - pos;
		}
		else remain = chunk_size - pos;

		if (remain >= sz) {
			memcpy(buf, chk->buffer + pos, sz);
			off = pos + sz;
			read += sz;
			break;
		}
		memcpy(buf, chk->buffer + pos, remain);
		off = pos + remain;
		read += remain;
		buf += remain;
		sz -= remain;
		pos = sizeof(chunk::header);

		// goto next chunk
		listnode_t* nd = chk->hdr.ownerlist.next;
		chk = list_entry(chunk, hdr.ownerlist, nd);
	}

	if (moveoffset) {
		_offset = off - sizeof(chunk::header);
		_chkoff = chkoff;
	}
	return read;
}

fifobuffer* create_fifobuffer(void)
{
	fifobuffer_impl* impl = new fifobuffer_impl();
	if (NULL == impl) return NULL;
	return reinterpret_cast<fifobuffer*>(impl);
}

bool fifobuffer::enter(void)
{
	fifobuffer_impl* impl = reinterpret_cast<fifobuffer_impl*>(this);
	auto_mutex am(impl->_mut);
	return impl->enter();
}

void fifobuffer::exit(void)
{
	fifobuffer_impl* impl = reinterpret_cast<fifobuffer_impl*>(this);
	auto_mutex am(impl->_mut);
	impl->exit();
}

void fifobuffer::drain(void)
{
	fifobuffer_impl* impl = reinterpret_cast<fifobuffer_impl*>(this);
	auto_mutex am(impl->_mut);
	impl->drain();
}

bool fifobuffer::empty(void)
{
	fifobuffer_impl* impl = reinterpret_cast<fifobuffer_impl*>(this);
	auto_mutex am(impl->_mut);
	return impl->empty();
}

size_t fifobuffer::getsize(void)
{
	fifobuffer_impl* impl = reinterpret_cast<fifobuffer_impl*>(this);
	auto_mutex am(impl->_mut);
	return impl->getsize();
}

size_t fifobuffer::get_availsize(void)
{
	fifobuffer_impl* impl = reinterpret_cast<fifobuffer_impl*>(this);
	auto_mutex am(impl->_mut);
	return impl->get_availsize();
}

size_t fifobuffer::remove(void* buf, size_t sz)
{
	if (!buf || !sz) return 0;
	fifobuffer_impl* impl = reinterpret_cast<fifobuffer_impl*>(this);
	auto_mutex am(impl->_mut);
	return impl->remove(buf, sz);
}

size_t fifobuffer::discard(size_t sz)
{
	if (!sz) return 0;
	fifobuffer_impl* impl = reinterpret_cast<fifobuffer_impl*>(this);
	auto_mutex am(impl->_mut);
	return impl->discard(sz);
}

size_t fifobuffer::append(void* buf, size_t sz)
{
	if (!buf || !sz) return 0;
	fifobuffer_impl* impl = reinterpret_cast<fifobuffer_impl*>(this);
	auto_mutex am(impl->_mut);
	return impl->append(buf, sz);
}

size_t fifobuffer::append(int fd, size_t sz)
{
	if (!fd || !sz) return 0;
	fifobuffer_impl* impl = reinterpret_cast<fifobuffer_impl*>(this);
	auto_mutex am(impl->_mut);
	return impl->append(fd, sz);
}

size_t fifobuffer::peekdata(size_t pos, void* buf, size_t sz)
{
	if (!buf || !sz) return 0;
	fifobuffer_impl* impl = reinterpret_cast<fifobuffer_impl*>(this);
	auto_mutex am(impl->_mut);
	return impl->peekdata(pos, buf, sz);
}

// seek to specific position
int fifobuffer::seek(size_t pos, fb_seektype st)
{
	fifobuffer_impl* impl = reinterpret_cast<fifobuffer_impl*>(this);
	auto_mutex am(impl->_mut);
	return impl->seek(pos, st);
}

// read data
size_t fifobuffer::read(void* buf, size_t sz)
{
	if (!buf || !sz) return 0;
	fifobuffer_impl* impl = reinterpret_cast<fifobuffer_impl*>(this);
	auto_mutex am(impl->_mut);
	return impl->read(buf, sz);
}

size_t readonlybuffer::getsize(void)
{
	fifobuffer_impl* impl = reinterpret_cast<fifobuffer_impl*>(this);
	auto_mutex am(impl->_mut);
	return impl->getsize();
}

size_t readonlybuffer::peekdata(size_t pos, void* buf, size_t sz)
{
	if (!buf || !sz) return 0;
	fifobuffer_impl* impl = reinterpret_cast<fifobuffer_impl*>(this);
	auto_mutex am(impl->_mut);
	return impl->peekdata(pos, buf, sz);
}

// seek to specific position
int readonlybuffer::seek(size_t pos, fb_seektype st)
{
	fifobuffer_impl* impl = reinterpret_cast<fifobuffer_impl*>(this);
	auto_mutex am(impl->_mut);
	return impl->seek(pos, st);
}

// read data
size_t readonlybuffer::read(void* buf, size_t sz)
{
	if (!buf || !sz) return 0;
	fifobuffer_impl* impl = reinterpret_cast<fifobuffer_impl*>(this);
	auto_mutex am(impl->_mut);
	return impl->read(buf, sz);
}

}} // end of namespace zas::utils
#endif // UTILS_ENABLE_FBLOCK_BUFFER
/* EOF */
