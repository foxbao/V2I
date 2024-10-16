/** @file inc/utils/absfile.h
 * implementation of the regular file
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_ABSFILE

#include <fcntl.h>
#include <sys/param.h>
#include <sys/mman.h>

#include "std/list.h"
#include "utils/mutex.h"
#include "utils/absfile.h"

using namespace std;

namespace zas {
namespace utils {

class regular_file : public absfile
{
public:
	regular_file(FILE* fp) : _fp(fp) {}
	~regular_file() {
		close();
	}

	void release(void) {
		delete this;
	}

	int close(void) {
		if (!_fp) return -ENOTEXISTS;
		fclose(_fp);
		_fp = NULL;
		return 0;
	}

	size_t read(void* ptr, size_t sz)
	{
		if (NULL == _fp) {
			return 0;
		}
		return fread(ptr, 1, sz, _fp);
	}

	size_t read(void* ptr, size_t sz, size_t nmemb)
	{
		if (NULL == _fp) {
			return 0;
		}
		return fread(ptr, sz, nmemb, _fp);
	}

	size_t write(void* ptr, size_t sz)
	{
		if (NULL == _fp) {
			return 0;
		}
		return fwrite(ptr, 1, sz, _fp);
	}

	size_t write(void* ptr, size_t sz, size_t nmemb)
	{
		if (NULL == _fp) {
			return 0;
		}
		return fwrite(ptr, sz, nmemb, _fp);
	}

	int rewind(void)
	{
		if (!_fp) {
			return -ENOTEXISTS;
		}
		::rewind(_fp);
		return 0;
	}

	int seek(long offset, int whence)
	{
		if (!_fp) {
			return -ENOTEXISTS;
		}
		switch (whence) {
		case absfile_seek_set:
			whence = SEEK_SET;
			break;
		case absfile_seek_cur:
			whence = SEEK_CUR;
			break;
		case absfile_seek_end:
			whence = SEEK_END;
			break;
		default: return -EBADPARM;
		}
		return fseek(_fp, offset, whence);
	}

	long getpos(void)
	{
		if (!_fp) {
			return -ENOTEXISTS;
		}
		return ftell(_fp);
	}

	void* mmap(off_t offset, size_t length, int flags, void* addr)
	{
		if (!_fp) {
			return NULL;
		}
		int fd = fileno(_fp);
		if (fd < 0) return NULL;

		if (!length) {
			// save the current positon
			long tmp = ftell(_fp);
			seek(0, absfile_seek_end);
			length = getpos();
			if (offset >= length) return NULL;
			length -= offset;
			//restore the position
			fseek(_fp, tmp, SEEK_SET);
		}
		int prot = PROT_NONE;
		if (flags & absfile_map_prot_exec)
			prot |= PROT_EXEC;
		if (flags & absfile_map_prot_read)
			prot |= PROT_READ;
		if (flags & absfile_map_prot_write)
			prot |= PROT_WRITE;
		
		int f = 0;
		if (flags & absfile_map_shared)
			f |= MAP_SHARED;
		if (flags & absfile_map_private)
			f |= MAP_PRIVATE;

		return ::mmap(addr, length, prot, f, fd, offset);
	}

	int munmap(void* addr, size_t length)
	{
		if (!length) {
			// save the current position
			size_t tmp = getpos();
			seek(0, absfile_seek_end);
			length = getpos();
			// restore the current position
			seek((long)tmp, absfile_seek_set);
		}
		return ::munmap(addr, length);
	}

private:
	FILE* _fp;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(regular_file);
};

class regularfile_category : public absfile_category
{
public:
	regularfile_category() {
		get_home();
	}

	void release(void) {
		delete this;
	}

	int normalize_uri(uri& uri)
	{
		string str;
		if (check_uri_validity(uri))
			return -EBADPARM;
		str = uri.get_fullpath();
		if (str.empty()) return -EBADPARM;

		string dst;
		const char* src = str.c_str();
		assert(nullptr != src);

		for(; *src && (*src == ' ' || *src == '\t'); ++src);
		if (!_home.empty() && !strncmp(src, "~/", 2)) {
			src += 2;
			dst = _home;
		}
		dst += src;

		char buf[PATH_MAX];
		realpath(dst.c_str(), buf);
		uri.set_fullpath((const char*)buf);
		return 0;
	}

	absfile* open(const uri& u, const char* cfg)
	{
		int ret = check_uri_validity(u);
		if (ret || NULL == cfg) {
			return NULL;
		}

		FILE* fp = fopen(u.get_fullpath().c_str(), cfg);
		if (NULL == fp) return NULL;
		
		return new regular_file(fp);
	}

private:

	void get_home(void)
	{
		const char* home = getenv("HOME");
		if (nullptr != home) {
			_home = home;
			if (_home[_home.length() - 1] != '/') {
				_home += "/";
			}
		}
	}

	int check_uri_validity(const uri& uri)
	{
		if (!uri.valid()) return -1;
		if (uri.get_scheme() != "file")
			return -2;

		// we do not need a port
		if (uri.has_port()) return -3;
		return 0;
	}
	
private:
	string _home;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(regularfile_category);
};

struct absfile_category_node
{
	listnode_t ownerlist;
	const char* type;
	absfile_category* category;
};

static mutex mut;
static listnode_t category_list = LISTNODE_INITIALIZER(category_list);

int absfile_category_register(const char* type_,
	absfile_category* category_)
{
	if (NULL == type_ || NULL == category_) {
		return -EBADPARM;
	}
	absfile_category_node* node = new absfile_category_node;
	if (NULL == node) return -ENOMEMORY;

	int len = strlen(type_);
	if (!len) return -EBADPARM;

	node->type = new char [len + 1];
	assert(NULL != node->type);
	memcpy((void*)node->type, type_, len + 1);
	node->category = category_;

	auto_mutex am(mut);
	listnode_add(category_list, node->ownerlist);
	return 0;
}

// this function is not locked
static absfile_category* find_category_unlocked(const char* type)
{
	listnode_t* node = category_list.next;
	for (; node != &category_list; node = node->next)
	{
		absfile_category_node* cat = list_entry(\
			absfile_category_node, ownerlist, node);
		if (!strcmp(cat->type, type)) {
			return cat->category;
		}
	}
	return NULL;
}

static absfile_category* find_category(const uri& uri)
{
	if (!uri.valid()) return NULL;
	string scheme = uri.get_scheme();
	
	auto_mutex am(mut);
	return find_category_unlocked(scheme.c_str());
}

absfile* absfile_open(const uri& u, const char* cfg)
{
	if (!cfg) return NULL;

	absfile_category* cat = find_category(u);
	if (!cat) return NULL;
	return cat->open(u, cfg);
}

int absfile_normalize_uri(uri& u)
{
	absfile_category* cat = find_category(u);
	if (!cat) return -ENOTSUPPORT;
	return cat->normalize_uri(u);
}

void absfile_init(void)
{
	absfile_category_register("file",
		new regularfile_category());
}

}} // end of namespace zas::utils
#endif // UTILS_ENABLE_FBLOCK_ABSFILE
/* EOF */

