/** @file gbuf.h
 * declaration of graphic buffer object
 */

#ifndef __CXX_ZAS_HWGRPH_GBUF_H__
#define __CXX_ZAS_HWGRPH_GBUF_H__

#include "std/zasbsc.h"

namespace zas {
namespace hwgrph {

template <typename T> class gbuf_
{
public:
	gbuf_(const size_t blksz)
	: _buf(NULL)
	, _cursz(0)
	, _totalsz(0)
	, _blksz(blksz) {}

	~gbuf_()
	{
		if (_buf) {
			free(_buf);
			_buf = NULL;
		}
		_cursz = _totalsz = 0;
	}

	bool add(const T* objs, size_t count)
	{
		if (!count) {
			return false;
		}

		size_t newsz = _cursz + count;
		if (newsz > _totalsz)
		{
			size_t backup_totalsz = _totalsz;
			for (; newsz > _totalsz; _totalsz += _blksz);
			void* newbuf = realloc((void*)_buf, _totalsz * sizeof(T));
			if (NULL == newbuf) {
				_totalsz = backup_totalsz;
				return false;
			}
			_buf = (T*)newbuf;
		}
		// copy the data
		if (objs) memcpy(_buf + _cursz, objs, count * sizeof(T));
		_cursz = newsz;
		return true;
	}

	bool remove(size_t count) {
		if (_cursz < count) return false;
		_cursz -= count;
	}

public:
	
	void clear(void) {
		_cursz = 0;
	}

	void reset(void) {
		_cursz = _totalsz = 0;
		if (_buf) free(_buf);
	}

	size_t getsize(bool inbyte = false) {
		size_t ret = _cursz;
		return (inbyte) ? ret * sizeof(T) : ret;
	}

	bool add(const T& obj) {
		return add(&obj, 1);
	}

	T* new_objects(int count) {
		if (count <= 0) return nullptr;
		if (!add(nullptr, count) || _cursz < count) {
			return nullptr;
		}
		return &_buf[_cursz - count];
	}

	T* new_object(void) {
		return new_objects(1);
	}

	T* buffer(void) {
		return _buf;
	}

private:
	T* _buf;

	// all size is the count of T, rather
	// than the size of bytes
	size_t _cursz;
	size_t _totalsz;
	const size_t _blksz;
};

}} // end of namespace zas::hwgrph
#endif // __CXX_ZAS_HWGRPH_GBUF_H__
/* EOF */
