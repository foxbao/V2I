/** @file dir.cpp
 * implementation of the directory operations
 */

#include <unistd.h>
#include <alloca.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "utils/dir.h"

namespace zas {
namespace utils {

using namespace std;

int createdir(const char* dir)
{
	char buffer[64], *buf, *cur;
	if (NULL == dir || *dir == '\0') {
		return -EBADPARM;
	}

	size_t len = strlen(dir);
	buf = (len < 64) ? buffer : (char*)alloca(len + 1);
	strcpy(buf, dir);
	cur = buf + 1;

	for (; *cur; ++cur)
	{
		if (*cur == '/')
		{
			*cur = '\0';
			if (access(buf, F_OK) != 0 && mkdir(buf, 0755) == -1)
				return -ELOGIC;
			*cur = '/';
		}
	}
	if (access(buf, F_OK) != 0 && mkdir(buf, 0755) == -1)
		return -ELOGIC;
	return 0;
}

bool fileexists(const char* filename)
{
	int ret = access(filename, F_OK);
	if(ret == -1)
		return false;
	return true;
}

bool rename(const char* oldname, const char* newname)
{
	int ret = ::rename(oldname, newname);
	return (!ret) ? true : false;
}

bool deletefile(const char *filename)
{
	int iret = unlink(filename);
	if (-1 == iret)
		return false;
	return true;
}

long filesize(const char *path)
{
	long sz = -1;	
	struct stat statbuff;
	if(stat(path, &statbuff) < 0){
		return sz;
	}else{
		sz = (long)statbuff.st_size;
	}
	return sz;
}

bool isdir(const char* path)
{
	struct stat statbuff;
	if(stat(path, &statbuff) < 0){
		return false;
	}
	return (S_ISDIR(statbuff.st_mode))
		? true : false;
}

#ifndef QNX_PLATEFORM
int removedir(const char* dir)
{
	string tmp;
	dirent* ent;

	if (NULL == dir || *dir == '\0') {
		return -EBADPARM;
	}
	DIR* d = ::opendir(dir);
	if (NULL == d) {
		return (errno == ENOENT) ? 0 : -ELOGIC;
	}
	while (NULL != (ent = ::readdir(d)))
	{
		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
			continue;
		tmp = dir;
		tmp += "/";
		tmp += ent->d_name;
		if (ent->d_type == DT_DIR) {
			tmp += '/';
			if (removedir(tmp.c_str())) {
				::closedir(d);
				return -ELOGIC;
			}
		}
		else if (-1 == ::remove(tmp.c_str())) {
			::closedir(d);
			return -ELOGIC;
		}
	}
	::closedir(d);
	if (-1 == ::remove(dir))
		return -ELOGIC;
	return 0;
}

string canonicalize_filename(const char* filename,
	bool remove_slash)
{
	string tmp;
	char *ret = canonicalize_file_name(filename);
	if (nullptr == ret) {
		return tmp;
	}
	int sz;
	if (remove_slash && (sz = strlen(ret)) > 0
		&& ret[sz - 1] == '/') {
		ret[sz - 1] = '\0';
	}
	tmp = ret;
	free(ret);
	return tmp;
}

class dir_object_impl
{
public:
	enum dir_object_status {
		dos_unknown = 0,
		dos_ready,
		dos_eor,
	};

	dir_object_impl(DIR* ddir, const char* dirname)
	: _refcnt(0), _inode(0), _status(dos_unknown)
	, _type(0), _dir(ddir), _path(dirname) {
		assert(nullptr != ddir);
		if (_path[_path.length() - 1] != '/') {
			_path += "/";
		}
	}

	~dir_object_impl() {
		if (_dir) {
			::closedir(_dir);
			_dir = nullptr;
		}	
	}

	int addref(void) {
		return __sync_add_and_fetch(&_refcnt, 1);
	}

	int release(void)
	{
		int cnt = __sync_sub_and_fetch(&_refcnt, 1);
		if (cnt <= 0) {
			delete this;
		}
		return cnt;
	}

	int nextdir(void)
	{
		if (nullptr == _dir) {
			return -ELOGIC;
		}
		if (_status == dos_eor) {
			return -EEOF;
		}

		errno = 0;
		dirent* ent = readdir(_dir);
		if (nullptr == ent) {
			clear_dirinfo();
			if (errno) return -ELOGIC;
			_status = dos_eor;
			return -EEOF;
		}

		set_dirinfo(ent);
		_status = dos_ready;
		return 0;
	}

	const char* get_dirname(void) {
		return _dir_name.c_str();
	}

	const char* get_fullpath(void) {
		return _path.c_str();
	}

	size_t get_inode(void) {
		return _inode;
	}

	bool entry_isdir(void) {
		return (_type == DT_DIR) ? true : false;
	}

	bool entry_is_regular_file(void) {
		return (_type == DT_REG) ? true : false;
	}

	bool entry_is_devfile(devfile_type& type) {
		if (_type == DT_BLK) {
			type = devfile_type_block;
			return true;
		}
		else if (_type == DT_CHR) {
			type = devfile_type_char;
			return true;
		}
		return false;
	}

	bool entry_is_symbolic_link(void) {
		return (_type == DT_LNK) ? true : false;
	}

	bool entry_is_named_pipe(void) {
		return (_type == DT_FIFO) ? true : false;
	}

	bool entry_is_domain_socket(void) {
		return (_type == DT_SOCK) ? true : false;
	}

private:
	void set_dirinfo(dirent* ent)
	{
		_dir_name = ent->d_name;
		_type = ent->d_type;
		_inode = ent->d_ino;
	}

	void clear_dirinfo(void)
	{
		_dir_name.clear();
		_type = 0;
	}

private:
	DIR* _dir;
	int _status;
	size_t _inode;
	uint32_t _type;
	string _dir_name;
	string _path;
	int _refcnt;
};

int dir_object::addref(void)
{
	auto* di = reinterpret_cast<dir_object_impl*>(this);
	return di->addref();
}

int dir_object::release(void)
{
	auto* di = reinterpret_cast<dir_object_impl*>(this);
	return di->release();
}

const char* dir_object::get_dirname(void)
{
	auto* di = reinterpret_cast<dir_object_impl*>(this);
	return di->get_dirname();
}

size_t dir_object::get_inode(void)
{
	auto* di = reinterpret_cast<dir_object_impl*>(this);
	return di->get_inode();
}

bool dir_object::entry_isdir(void)
{
	auto* di = reinterpret_cast<dir_object_impl*>(this);
	return di->entry_isdir();
}

bool dir_object::entry_is_regular_file(void)
{
	auto* di = reinterpret_cast<dir_object_impl*>(this);
	return di->entry_is_regular_file();
}

bool dir_object::entry_is_devfile(devfile_type& type)
{
	auto* di = reinterpret_cast<dir_object_impl*>(this);
	return di->entry_is_devfile(type);
}

bool dir_object::entry_is_symbolic_link(void)
{
	auto* di = reinterpret_cast<dir_object_impl*>(this);
	return di->entry_is_symbolic_link();
}

bool dir_object::entry_is_named_pipe(void)
{
	auto* di = reinterpret_cast<dir_object_impl*>(this);
	return di->entry_is_named_pipe();
}

bool dir_object::entry_is_domain_socket(void)
{
	auto* di = reinterpret_cast<dir_object_impl*>(this);
	return di->entry_is_domain_socket();
}

int dir_object::nextdir_entry(void)
{
	auto* di = reinterpret_cast<dir_object_impl*>(this);
	return di->nextdir();
}

const char* dir_object::get_fullpath(void)
{
	auto* di = reinterpret_cast<dir_object_impl*>(this);
	return di->get_fullpath();
}

dir opendir(const char* dirname)
{
	dir d;
	if (!dirname || !*dirname) {
		return d;
	}

	DIR* ddir = ::opendir(dirname);
	if (nullptr == ddir) {
		return d;
	}
	dir_object_impl* di = new dir_object_impl(ddir, dirname);
	if (nullptr == di) return d;

	d = reinterpret_cast<dir_object*>(di);
	return d;
}

string get_hostdir(void)
{
	string ret;
	char dir[PATH_MAX];
	int n = readlink("/proc/self/exe", dir, PATH_MAX);
	if (n == PATH_MAX) {
		return ret;
	}

	for (; n && dir[n] != '/'; --n);
	if (!n) ++n;
	dir[n] = '\0';
	ret.assign(dir);
	return ret;
}
#endif
}} // end of namespace zas::utils
/* EOF */
