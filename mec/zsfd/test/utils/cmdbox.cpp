#include "cmdbox.h"

namespace sigw {

#define get_meta()  \
	((nullptr == _buf) ? nullptr : &(((meta*)_buf)[-1]))

cmdbox_str::cmdbox_str()
: _buf(nullptr) {
}

cmdbox_str::~cmdbox_str()
{
	release();
}

cmdbox_str::cmdbox_str(const char* str)
: _buf(nullptr) {
	assign(str);
}

bool cmdbox_str::assign(const char* str)
{
	if (nullptr == str) {
		return false;
	}

    meta* m = get_meta();
	auto sz = strlen(str) + 1;
	assert(sz < INT16_MAX);

    // check if we are the only referee
    if (m && m->refcnt == 1)
    {
        // check if we have enough room or we
        // re-allocate buffer
        if (m->total_size < sz) {
            free(m);
            m = (meta*)alloc(sz);
        }
    } else {
        release();
        m = (meta*)alloc(sz);
    }

	// assign the string
	m->data_size = sz - 1;
	memcpy(_buf, str, sz);

	return true;
}

void* cmdbox_str::alloc(int sz)
{
	meta* m = get_meta();
    int total = (sizeof(meta) + sz + 3) & ~3;

    m = (meta*)malloc(total);
    assert(nullptr != m);

    // set buffer meta data
    m->total_size = total - sizeof(meta);
    m->data_size = 0, m->refcnt = 1;
    _buf = (uint8_t*)&m[1];

    return m;
}

cmdbox_str::cmdbox_str(const cmdbox_str& str)
{
	str.addref();
	_buf = str._buf;
}

cmdbox_str& cmdbox_str::operator=(const cmdbox_str& str)
{
	if (this == &str) {
		return *this;
	}
	// release myself
	release();

	// copy from the "str"
	str.addref();
	_buf = str._buf;
	return *this;
}

int cmdbox_str::addref(void) const
{
	meta* m = get_meta();
	if (nullptr ==  m) {
		return 0;
	}
	assert(m->refcnt < INT16_MAX);
	int ret = ++m->refcnt;
	return ret;
}

int cmdbox_str::release(void)
{
	meta* m = get_meta();
	if (nullptr == m) {
		return 0;
	}
	int ret = --m->refcnt;
	if (!ret) {
		sigw_safe_free(m);
        _buf = nullptr;
	}
	return ret;
}

cmdbox::cmdbox()
{
}

cmdbox::~cmdbox()
{
}

class cmdbox_test {
public:
    cmdbox_test() {
        cmdbox_str str("abc");
        auto str1(str);
        cmdbox_str str2;
        str2 = str1;
        printf("str: %s\n", str1.c_str());
        str1.assign("defghijklmn");
        printf("str1: %s\n", str1.c_str());
        printf("str: %s\n", str.c_str());
        printf("str2: %s\n", str2.c_str());
    }
} _t;

} // end of namespace sigw
/* EOF */