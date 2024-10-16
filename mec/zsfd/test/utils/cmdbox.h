#ifndef __CXX_SIGW_CMDBOX_H__
#define __CXX_SIGW_CMDBOX_H__

#include "utils/utils.h"

#define sigw_safe_free(x) do { \
	if (nullptr != (x)) { \
		free(x); (x) = nullptr; \
	} \
} while (0)

namespace sigw {

class cmdbox_str
{
public:
	cmdbox_str();
	cmdbox_str(const char* str);
	~cmdbox_str();

	cmdbox_str(const cmdbox_str&);
	cmdbox_str& operator=(const cmdbox_str&);

public:
	bool assign(const char* str);

public:
	const char* c_str(void) const {
		return reinterpret_cast<const char*>(_buf);
	}

private:
	int addref(void) const;
	int release(void);
    void* alloc(int sz);

private:
	struct meta {
		uint16_t total_size;
		uint16_t data_size;	 // size w/o '\0'
		int16_t refcnt;
	} PACKED;
	uint8_t* _buf;
};

class cmdbox
{
public:
	cmdbox();
	~cmdbox();

public:
};

} // end of namespace sigw
#endif // __CXX_SIGW_CMDBOX_H__
/* EOF */
