/** @file filelist.cpp
 * implementation of filelist classes
 */

#include <sys/stat.h>
#include <unistd.h>
#include "utils/cert.h"
#include "inc/filelist.h"

namespace zas {
namespace mware {

static inline void listnode_moveto(
	listnode_t& to, listnode_t& ownerlist)
{
	if (!listnode_isempty(ownerlist)) {
		listnode_del(ownerlist);
	}
	listnode_add(to, ownerlist);
}

fileinfo::fileinfo()
: name(nullptr)
, linkto(nullptr), flags(0)
, index(0)
{
	listnode_init(ownerlist);
	listnode_init(user_ownerlist);
	memset(&info, 0, sizeof(info));
}

fileinfo::~fileinfo()
{
	if (!listnode_isempty(ownerlist)) {
		listnode_del(ownerlist);
	}
	if (!listnode_isempty(user_ownerlist)) {
		listnode_del(user_ownerlist);
	}
	if (f.soft_link) {
		delete linkto_info;
		linkto_info = nullptr;
	}
}

void fileinfo::append_user(listnode_t& userlist)
{
	if (!listnode_isempty(user_ownerlist)) {
		listnode_del(user_ownerlist);
	}
	listnode_add(userlist, user_ownerlist);
}

fileinfo* fileinfo::get_user(listnode_t& userlist,
	bool removed)
{
	if (listnode_isempty(userlist)) {
		return nullptr;
	}
	listnode_t* node = userlist.next;
	auto* ret = list_entry(fileinfo, user_ownerlist, node);
	if (removed) listnode_del(*node);

	return ret;
}

int fileinfo::global_name_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(fileinfo, avlnode, a);
	auto* bb = AVLNODE_ENTRY(fileinfo, avlnode, b);
	int ret = strcmp(aa->name, bb->name);
	if (!ret) return 0;
	else if (ret < 0) return -1;
	else return 1;
}

int fileinfo::global_inode_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(fileinfo, avlnode, a);
	auto* bb = AVLNODE_ENTRY(fileinfo, avlnode, b);
	if (aa->info.inode == bb->info.inode) {
		return 0;
	} else if (aa->info.inode < bb->info.inode) {
		return -1;
	}
	else return 1;
}

filelist::filelist()
: _global_name_tree(nullptr)
, _global_anonymous_tree(nullptr)
, _total_files(0)
, _total_dirs(0)
, _total_size(0)
, _listener(nullptr)
, _flags(0)
{
	listnode_init(_global_dirlist);
	listnode_init(_global_filelist);
	listnode_init(_softlink_temp_list);
	memset(&_global_digest, 0, sizeof(_global_digest));

	// we need digest be default
	_f.need_global_digest = 1;
}

filelist::~filelist()
{
	release_all();
}

void filelist::release_list_unlocked(listnode_t& list)
{
	while (!listnode_isempty(list)) {
		auto* nd = list_entry(fileinfo, ownerlist, \
			list.next);
		delete nd;
	}
}

void filelist::release_all(void)
{
	auto_mutex am(_mut);
	release_list_unlocked(_global_dirlist);
	release_list_unlocked(_global_filelist);
	release_list_unlocked(_softlink_temp_list);
}

int filelist::loaddir(const char* dir)
{
	if (!dir || !*dir || !isdir(dir)) {
		return -EBADPARM;
	}
	_rootdir_fullpath = dir;

	int ret;
	fileinfo root, *node = &root;
	root.name = nullptr;
	root.fullpath = dir;
	listnode_t pending = LISTNODE_INITIALIZER(pending);

	for (;;) {
		ret = do_loaddir(node, pending);
		if (ret < 0) return ret;

		_mut.lock();
		// check if there is any pending directory
		if (listnode_isempty(pending)) {
			_mut.unlock();
			break;
		}
		node = get_pending_dir_unlocked(pending);		
		_mut.unlock();
	}
	return finalize_softlink_objects();
}

fileinfo* filelist::get_pending_dir_unlocked(listnode_t& list)
{
	auto* node = list_entry(fileinfo, ownerlist, list.next);
	listnode_del(node->ownerlist);
	assert(node->f.isdir);
	listnode_add(_global_dirlist, node->ownerlist);
	return node;
}

int filelist::do_loaddir(fileinfo* parents, listnode_t& pending)
{
	// sanity check
	assert(isdir(parents->fullpath.c_str()));

	// handle dir listener
	if (_listener) {
		int ret = _listener->on_handle_dir(this, parents);
		if (ret < 0) return -EINVALID;
	}

	dir d = opendir(parents->fullpath.c_str());
	if (d == nullptr) {
		return -EINVALID;
	}

	while (-EEOF != d->nextdir_entry()) {
		const char* dirname = d->get_dirname();
		if (!strcmp(dirname, ".") || !strcmp(dirname, "..")
			|| check_entry_useless(d)) {
			continue;
		}

		auto* nd = create_fileinfo_object(d, parents, pending);
		if (nullptr == nd) return -EINVALID;
		if (!nd->f.isdir && _listener) {
			// handle file listener
			int ret = _listener->on_handle_file(this, nd);
			if (ret < 0) return -EINVALID;
		}

		if (nullptr == nd) return -3;
	}
	return 0;
}

bool filelist::check_entry_useless(dir d)
{
	devfile_type type;
	if (d->entry_is_devfile(type)) {
		return true;
	}
	if (d->entry_is_domain_socket()
		|| d->entry_is_named_pipe()) {
		return true;
	}
	return false;
}

fileinfo* filelist::create_fileinfo_object(
	dir d, fileinfo* parents, listnode_t& pending)
{
	auto* ret = new fileinfo();
	if (nullptr == ret) {
		return ret;
	}

	const char* fullpath = d->get_fullpath();
	ret->fullpath = fullpath;
	ret->fullpath += d->get_dirname();

	assert(!strncmp(fullpath, _rootdir_fullpath.c_str(), \
		_rootdir_fullpath.length()));

	ret->name = ret->fullpath.c_str() + _rootdir_fullpath.length();

	// need lock
	auto_mutex am(_mut);

	if (d->entry_isdir()) {
		ret->f.isdir = 1;
		++_total_dirs;
		listnode_add(pending, ret->ownerlist);
	}
	else {
		++_total_files;
		listnode_add(_global_filelist, ret->ownerlist);
	}

	// update the file status
	if (file_stat_unlocked(ret)) {
		delete ret;
		return nullptr;
	}

	// add to global avltree
	int retval = avl_insert(&_global_name_tree, &ret->avlnode,
		fileinfo::global_name_compare);
	assert(retval == 0);

	return ret;
}

int filelist::file_stat_unlocked(fileinfo* info)
{
	int ret;
	struct stat statbuf;
	if (-1 == lstat(info->fullpath.c_str(), &statbuf)) {
		return 1;
	}

	// save the stat info
	info->info.inode = statbuf.st_ino;
	info->info.file_size = statbuf.st_size;
	
	// update total size
	_total_size += statbuf.st_size;

	switch (statbuf.st_mode & S_IFMT) {
		case S_IFBLK: case S_IFCHR:
		case S_IFIFO: case S_IFSOCK:
			assert(0); break;

		case S_IFDIR: assert(info->f.isdir); break;
		case S_IFLNK:
			ret = prepare_softlink_unlocked(info);
			if (ret) return ret;
			break;

		default: break;		
	}

	// check hard link
	if (statbuf.st_nlink != 1 && !info->f.isdir) {
		// this file has a hard link
		assert(info->f.soft_link == 0);
		info->f.hard_link = 1;

		// check if we have this link_master
		ret = check_create_link_master_unlocked(info);
		if (ret) return ret;
	}
	return 0;
}

int filelist::prepare_softlink_unlocked(fileinfo* info)
{
	const ssize_t bufsz = 512;
	char buf[bufsz];
	ssize_t ret = readlink(info->fullpath.c_str(), buf, bufsz);
	if (ret < 0 || ret >= bufsz) {
		return -ETOOLONG;
	}
	buf[ret] = '\0';

	string fullname = canonicalize_filename(info->fullpath.c_str());
	if (fullname.empty()) {
		return -EINVALID;
	}

	if (strncmp(fullname.c_str(), _rootdir_fullpath.c_str(),
		_rootdir_fullpath.length())) {
		return -ENOTALLOWED;
	}

	softlink_info* linkto = new softlink_info();
	if (nullptr == linkto) {
		return -ENOMEMORY;
	}

	linkto->link_name = buf;
	linkto->target_info = nullptr;
	linkto->target_name = fullname.c_str() + _rootdir_fullpath.length();
	info->linkto_info = linkto;
	info->f.soft_link = 1;

	listnode_moveto(_softlink_temp_list, info->ownerlist);
	return 0;
}

int filelist::check_create_link_master_unlocked(fileinfo* info)
{
	avl_node_t* nd = avl_find(_global_anonymous_tree,
		&info->avlnode,
		fileinfo::global_inode_compare);

	fileinfo* obj;
	if (!nd) {
		// note that the anonymous fileinfo items will
		// not increase the _total_files count, meaning
		// that the file will not be packed to otapack,
		// not only for full package but also for
		// differentiate package
		obj = new fileinfo();
		if (nullptr == obj) return -ENOMEMORY;
		obj->f.anonymous = 1;
		obj->info = info->info;
		obj->fullpath = info->fullpath;
		obj->name = obj->fullpath.c_str() + _rootdir_fullpath.length();
		
		// add to avltree
		int ret = avl_insert(&_global_anonymous_tree,
			&obj->avlnode,
			fileinfo::global_inode_compare);
		assert(ret == 0);
	}
	else {
		obj = AVLNODE_ENTRY(fileinfo, avlnode, nd);
		if (info->fullpath < obj->fullpath) {
			obj->fullpath = info->fullpath;
			obj->name = obj->fullpath.c_str() + _rootdir_fullpath.length();
		}
	}
	info->linkto = obj;
	return 0;
}

int filelist::finalize_softlink_objects(void)
{
	string name;
	auto_mutex am(_mut);
	while (!listnode_isempty(_softlink_temp_list)) {
		auto* obj = list_entry(fileinfo, ownerlist, \
			_softlink_temp_list.next);
		assert(obj->f.soft_link);

		auto* slinfo = obj->linkto_info;
		assert(nullptr != slinfo);

		const char* dummy_name = slinfo->target_name.c_str();
		avl_node_t* node = avl_find(_global_name_tree,
			MAKE_FIND_OBJECT(dummy_name, fileinfo, name, avlnode),
			fileinfo::global_name_compare);
		if (nullptr == node) {
			return -ENOTFOUND;
		}
		slinfo->target_info = AVLNODE_ENTRY(fileinfo, avlnode, node);		

		// move to real list
		listnode_t& list = (obj->f.isdir) ? _global_dirlist : _global_filelist;
		listnode_moveto(list, obj->ownerlist);
	}
	return 0;
}

bool filelist::loaded(void)
{
	if (listnode_isempty(_global_dirlist)
		&& listnode_isempty(_global_filelist)) {
		return false;
	}
	if (!listnode_isempty(_softlink_temp_list)) {
		return false;
	}
	return true;
}

fileinfo* filelist::ordered_getfirst(void)
{
	auto_mutex am(_mut);
	avl_node_t* node = avl_first(_global_name_tree);
	if (nullptr == node) return nullptr;
	return AVLNODE_ENTRY(fileinfo, avlnode, node);
}

fileinfo* filelist::ordered_getnext(fileinfo* prev)
{
	auto_mutex am(_mut);
	if (nullptr == prev) return nullptr;
	avl_node_t* node = avl_next(&prev->avlnode);
	if (nullptr == node) return nullptr;
	return AVLNODE_ENTRY(fileinfo, avlnode, node);
}

fileinfo* filelist::get_firstdir(void)
{
	auto_mutex am(_mut);
	if (listnode_isempty(_global_dirlist)) {
		return nullptr;
	}
	return list_entry(fileinfo, ownerlist, \
		_global_dirlist.next);
}

fileinfo* filelist::get_nextdir(fileinfo* prev)
{
	if (!prev->f.isdir) {
		return nullptr;
	}
	auto_mutex am(_mut);
	listnode_t* nd = prev->ownerlist.next;
	if (nd == &_global_dirlist) {
		return nullptr;
	}
	return list_entry(fileinfo, ownerlist, nd);
}

fileinfo* filelist::get_firstfile(void)
{
	auto_mutex am(_mut);
	if (listnode_isempty(_global_filelist)) {
		return nullptr;
	}
	return list_entry(fileinfo, ownerlist, \
		_global_filelist.next);
}

fileinfo* filelist::get_nextfile(fileinfo* prev)
{
	if (prev->f.isdir) {
		return nullptr;
	}
	auto_mutex am(_mut);
	listnode_t* nd = prev->ownerlist.next;
	if (nd == &_global_filelist) {
		return nullptr;
	}
	return list_entry(fileinfo, ownerlist, nd);
}

int filelist::single_file_digest(fileinfo* info, digest* dgst_all)
{
	size_t sz;
	fileinfo* nd = info;
	const int bufsz = 2048;
	uint8_t buffer[bufsz];
	string* fullpath = nullptr;

	if (info->f.hard_link) {
		auto* anonymous = info->linkto;
		assert(nullptr != anonymous);
		if (anonymous->fullpath == info->fullpath) {
			// this is the hardlink master
			nd = anonymous;
			fullpath = &anonymous->fullpath;
		}
	}
	// if it is a soft link, we omit generating the digest
	// since the "linkto" file will be generated
	else if (!info->f.soft_link) {
		fullpath = &info->fullpath;
	}

	if (nullptr != fullpath) {
		const char* filename = fullpath->c_str();
		assert(filename && *filename);

		FILE *fp = fopen(filename, "rb");
		if (nullptr == fp) {
			return -1;
		}

		for (;;) {
			sz = fread(buffer, 1, bufsz, fp);
			if (!sz) break;
			if (_f.need_global_digest) {
				dgst_all->append((void*)buffer, sz);
			}
		}
		fclose(fp);
	}
		dgst_all->append((void*)nd->name, strlen(nd->name) + 1);
	return 0;
}

uint8_t* filelist::generate_digest(void)
{
	if (!_f.need_global_digest) {
		return nullptr;
	} else if (_f.digested) {
		return _global_digest;
	}

	size_t sz;
	digest dgst_all("sha256");
	
	// we lock in the whole process
	auto_mutex am(_mut);

	size_t idx = 0;
	avl_node_t* itr = avl_first(_global_name_tree);
	for (size_t idx = 0; itr; ++idx, itr = avl_next(itr))
	{
		auto* nd = AVLNODE_ENTRY(fileinfo, avlnode, itr);
		if (nd->f.anonymous) continue;

		if (!nd->f.isdir &&
			(_f.need_global_digest || !nd->f.digested)) {
			if (single_file_digest(nd, &dgst_all))
				return nullptr;
		}

		if (_listener) {
			_listener->on_processing(this, nd, idx,
			(idx >= _total_files + _total_dirs)
			? true : false);
		}
	}

	// save the global digest result, if necessary
	assert(_f.need_global_digest);
	const uint8_t* dgst_result = dgst_all.getresult(&sz);
	assert(dgst_result && sz == 32);
	memcpy(_global_digest, dgst_result,
		sizeof(_global_digest));

	_f.digested = 1;
	return _global_digest;
}

bool filelist::digested(void)
{
	return (_f.need_global_digest
		&& _f.digested) ? true : false;
}

uint8_t* filelist::get_digest(void)
{
	if (!digested()) {
		return nullptr;
	}
	return _global_digest;
}

bool filelist::set_need_digest(bool nd)
{
	bool ret = (_f.need_global_digest)
		? true : false;
	_f.need_global_digest = (nd) ? 1 : 0;
	return ret;
}

fileinfo* filelist::find(const char* dummy_name)
{
	auto* node = avl_find(_global_name_tree,
		MAKE_FIND_OBJECT(dummy_name, fileinfo, name, avlnode),
		fileinfo::global_name_compare);

	if (nullptr == node) return nullptr;
	return AVLNODE_ENTRY(fileinfo, avlnode, node);
}

}} // end of namespace zas::mware
/* EOF */
