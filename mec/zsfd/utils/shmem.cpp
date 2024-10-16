/** @file shmem.cpp
 * implementation of the shared memory operations
 */

#include "utils/utils.h"
#if (defined(UTILS_ENABLE_FBLOCK_SHRMEM) && defined(UTILS_ENABLE_FBLOCK_CMDLINE))

#include <string>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>		/* For mode constants */
#include <fcntl.h>
#include <unistd.h>

#include "utils/dir.h"
#include "utils/shmem.h"
#include "utils/cmdline.h"

using namespace std;

namespace zas {
namespace utils {

#define SYSTEM_APP_PACKAGE_SHM "zas.system.pkgcfg.area"

class shared_memory_impl
{
public:
	shared_memory_impl(const char* name, size_t sz,
	uint32_t flags, void* addr)
	: _fd(-1), _ptr(NULL), _sz(sz), _name(NULL), _created(false)
	{
		if (!name || !*name || !sz) return;
		if (check_name(name)) return;

		int oflags = 0, mflags = 0;
		if ((flags & shmem_rdwr) == shmem_rdwr) {
			oflags |= O_RDWR;
			mflags |= PROT_READ | PROT_WRITE;
		}
		else if (flags & shmem_read) {
			oflags |= O_RDONLY;
			mflags |= PROT_READ;
		}
		else if (flags & shmem_write) {
			oflags |= O_WRONLY;
			mflags |= PROT_WRITE;
		}
		else return;

		_name = new char [strlen(name) + 8];
		_name[0] = '/';
		strcpy(&_name[1], name);
		_fd = shm_open(_name, O_CREAT | oflags | O_EXCL, 0777);
		if (_fd < 0)
		{
			// the shared memory object exists, open it
			_fd = shm_open(_name, oflags, 0777);
			if (_fd < 0) return; // error
		}
		else {
			// the object is newly created
			ftruncate(_fd, sz);
			_created = true;
		}
		
		// map the shared memory
		_ptr = mmap(addr, sz, mflags, (flags & sheme_private)
			? MAP_PRIVATE : MAP_SHARED, _fd, 0);
		assert(NULL != _ptr);
	}

	~shared_memory_impl()
	{
		munmap(_ptr, _sz);
		close(_fd);
		if (_created) shm_unlink(_name);
		if (_name) delete [] _name;
	}

	void* addr(void) {
		return _ptr;
	}

	size_t size(void) {
		return _sz;
	}

	bool is_creator(void) {
		return _created;
	}

private:

	int check_name(const char* name)
	{
		for (; *name; ++name) {
			if (*name == '/') return -1;
		}
		return 0;
	}

private:
	int _fd;
	void* _ptr;
	size_t _sz;
	char* _name;
	bool _created;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(shared_memory_impl);
};

shared_memory* shared_memory::create(const char* name,
	size_t sz, uint32_t flags, void* addr)
{
	auto* shm = new shared_memory_impl(name, sz, flags, addr);
	if (NULL == shm || NULL == shm->addr()) {
		return NULL;
	}
	return reinterpret_cast<shared_memory*>(shm);
}

void* shared_memory::getaddr(void)
{
	shared_memory_impl* shm = reinterpret_cast<shared_memory_impl*>(this);
	return shm->addr();
}

size_t shared_memory::getsize(void)
{
	shared_memory_impl* shm = reinterpret_cast<shared_memory_impl*>(this);
	return shm->size();
}

void shared_memory::release(void)
{
	shared_memory_impl* shm = reinterpret_cast<shared_memory_impl*>(this);
	delete shm;
}

bool shared_memory::is_creator(void)
{
	shared_memory_impl* shm = reinterpret_cast<shared_memory_impl*>(this);
	return shm->is_creator();
}

int shared_memory::reset(const char* name)
{
	if (!name || !*name) {
		return -EBADPARM;
	}

	char* buf = (char*)alloca(strlen(name) + 8);
	buf[0] = '/';
	strcpy(&buf[1], name);
	return shm_unlink(buf);
}
void reset_shared_memory(void)
{
	cmdline_option opts[] = {
		{ "reset", no_argument, NULL, 'r' },
		{ 0, 0, NULL, 0},
	};

	int i, c;
	auto* cmd = cmdline::inst();
	cmd->reset();
	while ((c = cmd->getopt_long(NULL, opts, &i)) != -1)
	{
		if (c == 'r') {
			shared_memory::reset(SYSTEM_APP_PACKAGE_SHM);
			return;
		}
	}
}
}} // end of namespace zas::utils
#endif // (defined(UTILS_ENABLE_FBLOCK_SHRMEM) && defined(UTILS_ENABLE_FBLOCK_CMDLINE))
/* EOF */
