#include "utils/evloop.h"
#include "utils/log.h"
#include <stdio.h>

using namespace zas::utils;


#define utils_safe_free(x) do { \
	if (nullptr != (x)) { \
		free(x); (x) = nullptr; \
	} \
} while (0)







#define easy_str_min_sz		(8)
#define easy_str_getsize(s)	(((s) + (easy_str_min_sz - 1)) & ~(easy_str_min_sz - 1))

class easy_string
{
public:
	easy_string();
	easy_string(const char* str);
	~easy_string();

	easy_string(const easy_string&);
	easy_string& operator=(const easy_string&);

public:
	bool assign(const char* str);
	bool append(const char* str);
	bool append(const void* buf, size_t sz);

	size_t length(void) const;

public:
	const char* c_str(void) const {
		return reinterpret_cast<const char*>(_buf);
	}

	easy_string& operator=(const char* str) {
		assign(str); return *this;
	}

private:
	int addref(void) const;
	int release(void);
	void* alloc(int sz);
	void expand(size_t sz, bool copy);

private:
	struct meta {
		uint16_t total_size;
		uint16_t data_size;	 // size w/o '\0'
		int16_t refcnt;
	} PACKED;
	uint8_t* _buf;
};







#define get_meta()  \
	((nullptr == _buf) ? nullptr : &(((meta*)_buf)[-1]))

easy_string::easy_string()
: _buf(nullptr) {
}

easy_string::~easy_string()
{
	release();
}

easy_string::easy_string(const char* str)
: _buf(nullptr) {
	assign(str);
}

void easy_string::expand(size_t sz, bool copy)
{
	meta* m = get_meta(), *tmp = m;
	// check if we are the only referee
	if (m && m->refcnt == 1)
	{
		// check if we have enough room or we
		// re-allocate buffer
		if (m->total_size < sz) {
			m = (meta*)alloc(sz);
			assert(nullptr != m);
			if (copy && tmp) {
				memcpy(&m[1], &tmp[1], tmp->data_size);
				m->data_size = tmp->data_size;
			}
			free(tmp);
		}
	} else {
		release();
		m = (meta*)alloc(sz);
		assert(nullptr != m);
		if (copy && tmp) {
			memcpy(&m[1], &tmp[1], tmp->data_size);
			m->data_size = tmp->data_size;			
		}
	}
}

bool easy_string::append(const char* str)
{
	if (nullptr == str) {
		return false;
	}
	if (!*str) return true;

	auto m = get_meta();
	auto sz = strlen(str) + 1;
	auto data_size = (m) ? m->data_size : 0;
	expand(easy_str_getsize(data_size + sz), true);
	
	// append the string
	m = get_meta();
	memcpy(&_buf[m->data_size], str, sz);
	m->data_size += sz - 1;

	return true;
}

bool easy_string::append(const void* buf, size_t sz)
{
	if (nullptr == buf) {
		return false;
	}

	auto m = get_meta();
	auto data_size = (m) ? m->data_size : 0;
	expand(easy_str_getsize(data_size + sz), true);

	// append the data
	m = get_meta();
	memcpy(&_buf[m->data_size], buf, sz);
	m->data_size += sz;
	
	return true;
}

bool easy_string::assign(const char* str)
{
	if (nullptr == str) {
		return false;
	}

	auto sz = strlen(str) + 1;
	assert(sz < INT16_MAX - easy_str_min_sz);
	expand(easy_str_getsize(sz), false);

	// assign the string
	get_meta()->data_size = sz - 1;
	memcpy(_buf, str, sz);

	return true;
}

size_t easy_string::length(void) const
{
	auto m = get_meta();
	return (m) ? m->data_size : 0;
}


void* easy_string::alloc(int sz)
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

easy_string::easy_string(const easy_string& str)
{
	str.addref();
	_buf = str._buf;
}

easy_string& easy_string::operator=(const easy_string& str)
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

int easy_string::addref(void) const
{
	meta* m = get_meta();
	if (nullptr ==  m) {
		return 0;
	}
	assert(m->refcnt < INT16_MAX);
	int ret = ++m->refcnt;
	return ret;
}

int easy_string::release(void)
{
	meta* m = get_meta();
	if (nullptr == m) {
		return 0;
	}
	int ret = --m->refcnt;
	if (!ret) {
		utils_safe_free(m);
		_buf = nullptr;
	}
	return ret;
}


easy_string readline(void)
{
	char buf[32];
	easy_string str;

	for (;;) {
		const char* s = fgets(buf, 32, stdin);
		assert(nullptr != s);

		auto len = strlen(s);
		if (len > 0 && s[len - 1] == '\n') {
			str.append(s, len - 1);
			break;
		}
		else str.append(s, len);
	}

	printf("str(%lu): %s\n", str.length(), str.c_str());
	return str;
}

int main()
{
	for (;;) readline();

	evloop *evl = evloop::inst();
	evl->setrole(evloop_role_server);

	evl->updateinfo(evlcli_info_client_name, "zas.system")
		->updateinfo(evlcli_info_instance_name, "sysd")
		->updateinfo(evlcli_info_listen_port, 5556)
		->updateinfo(evlcli_info_commit);

	if (log_collector::inst()->bindto(evl)) {
		printf("log_collector(): fail to bind to evloop\n");
		return 1;
	}

	// start the system server
	return evl->start(false);
    return 0;
}