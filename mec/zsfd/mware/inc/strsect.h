/** @file strsect.h
 * defintion and implementation of string section
 */

#ifndef __CXX_ZAS_MWARE_STRING_SECTION_H__
#define __CXX_ZAS_MWARE_STRING_SECTION_H__

#include "mware/mware.h"

namespace zas {
namespace mware {

class string_section
{
	const size_t min_size = 1024;
public:
	string_section()
	: _buf(nullptr), _total_sz(0)
	, _curr_sz(0) {
	}

	~string_section()
	{
		if (_buf) {
			free(_buf);
			_buf = nullptr;
		}
		_total_sz = _curr_sz = 0;
	}

	size_t getsize(void) {
		return _curr_sz;
	}

	void* getbuf(void) {
		return _buf;
	}

	char* alloc(int sz, size_t* delta_pos)
	{
		if (sz <= 0) {
			return nullptr;
		}
		int new_sz = _curr_sz + sz;
		int new_total_sz = _total_sz;
		for (; new_sz > new_total_sz; new_total_sz += min_size);
		if (new_total_sz != _total_sz) {
			_buf = (char*)realloc(_buf, new_total_sz);
			assert(NULL != _buf);
			_total_sz = new_total_sz;
		}
		char* ret = &_buf[_curr_sz];
		_curr_sz = new_sz;
		
		if (delta_pos) {
			*delta_pos = size_t(ret - _buf);
		}
		return ret;
	}

	const char* alloc_copy_string(const char* str, size_t& pos)
	{
		assert(NULL != str);
		if (nullptr == str) {return nullptr;}
		int len = strlen(str) + 1;
		char* ret = alloc(len, &pos);
		if (NULL == ret) return NULL;
		memcpy(ret, str, len);
		return (const char*)ret;
	}

	const char* to_string(size_t pos) {
		if (pos >= _curr_sz) return NULL;
		return (const char*)&_buf[pos];
	}

private:
	char* _buf;
	size_t _total_sz, _curr_sz;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(string_section);
};

}} // end of namespace zas::mware
#endif // __CXX_ZAS_MWARE_STRING_SECTION_H__
/* EOF */
