/**
 * @file filelist.cpp
 * @author zhoujianming@civ-ip.com
 * @brief 
 * @version 0.1
 * @date 2021-11-13
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <sys/stat.h>
#include "utils/cert.h"
#include "filelist.h"

using namespace zas::utils;

filelist::filelist()
: _global_name_tree(nullptr), _global_digest_tree(nullptr)
, _root(nullptr), _total_dirs(0),_total_files(0)
, _listener(nullptr), _flags(0), _total_size(0)
{
	listnode_init(_global_list);
	memset(_global_digest, 0, sizeof(_global_digest));
}

filelist::~filelist()
{
	release_all();
}

void filelist::release_all(void)
{
	while (!listnode_isempty(_global_list)) {
		auto* nd = list_entry(node, _ownerlist, _global_list.next);
		listnode_del(nd->_ownerlist);
		if (!listnode_isempty(nd->_usr_ownerlist)) {
			listnode_del(nd->_usr_ownerlist);
		}
		delete nd;
	}
}

int filelist::loaddir(const char* cdir)
{
	if (!cdir || !*cdir) {
		return -EBADPARM;
	}

	// create the root
	_root = new filelist::node(cdir);
	assert(nullptr != _root);
	avl_insert(&_global_name_tree, &_root->_avlnode,
		node::global_name_compare);

	listnode_t pending = LISTNODE_INITIALIZER(pending);
	int ret = do_loaddir(_root, pending);

	while (!listnode_isempty(pending)) {
		auto* nd = list_entry(filelist::node, \
			_ownerlist, pending.next);
		listnode_del(nd->_ownerlist);

		// add the directory to global list
		listnode_add(_global_list, nd->_ownerlist);
		if (do_loaddir(nd, pending) < 0) {
			return -ELOGIC;
		}
	}

	if (_listener) {
		_listener->on_scanfinished();
	}

	// calculate digests
	calc_digest();
	return 0;
}

int filelist::calc_digest(void)
{
	size_t sz;
	string name;
	const int bufsz = 2048;
	uint8_t buffer[bufsz];
	digest dgst_all("sha256");

	size_t idx = 0;
	avl_node_t* itr = avl_first(_global_name_tree);
	for (size_t idx = 0; itr; ++idx, itr = avl_next(itr))
	{
		auto* nd = AVLNODE_ENTRY(filelist::node, _avlnode, itr);
		const char* filename = nd->get_fullname();
		assert(filename && *filename);

		// append the file size
		assert(nd->_file_size >= 0);
		_total_size += nd->_file_size;

		if (!nd->isdir() && (_f.need_global_digest || !nd->digested()))
		{
			FILE *fp = fopen(filename, "rb");
			if (nullptr == fp) {
				return -2;
			}

			// generate the digest for the single file
			// on the other time, file content will also
			// be used to generate global digest
			digest dgst("sha256");
			for (;;) {
				sz = fread(buffer, 1, bufsz, fp);
				if (!sz) break;
				if (!nd->digested()) {
					dgst.append((void*)buffer, sz);
				}
				if (_f.need_global_digest) {
					dgst_all.append((void*)buffer, sz);
				}
			}
			fclose(fp);

			if (!nd->digested()) {
				const uint8_t* dgst_result = dgst.getresult(&sz);
				assert(dgst_result && sz == 32);
				nd->set_digest(dgst_result);
				add_by_digest(nd);
			}
		}
		if (_f.need_global_digest) {
			const char* info = nd->get_name();
			dgst_all.append((void*)info, strlen(info) + 1);
		}
		if (_listener) {
			_listener->on_processing(nd, idx,
			(idx >= _total_files + _total_dirs)
			? true : false);
		}
	}

	// save the global digest result, if necessary
	if (_f.need_global_digest) {
		const uint8_t* dgst_result = dgst_all.getresult(&sz);
		assert(dgst_result && sz == 32);
		memcpy(_global_digest, dgst_result,
			sizeof(_global_digest));
	}
	return 0;
}

int filelist::do_loaddir(filelist::node* parents,
	listnode_t& pending)
{
	uint32_t lnr_flags = 0;
	assert(parents->isdir());

	// handle listener
	if (_listener) {
		int ret = _listener->on_directory(parents, lnr_flags);
		if (ret < 0) return -1;
	}

	dir d = opendir(parents->get_fullname());
	if (d == nullptr) {
		return 1;
	}

	while (-EEOF != d->nextdir_entry()) {
		const char* dirname = d->get_dirname();
		if (!strcmp(dirname, ".") || !strcmp(dirname, "..")
			|| check_entry_useless(d)) {
			continue;
		}

		auto* nd = create_node(d, parents, pending);
		if (nd->isdir()) ++_total_dirs;
		else {
			++_total_files;

			// handle listener
			if (_listener) {
				uint32_t lnr_flags = 0;
				int ret = _listener->on_file(nd, lnr_flags);
				if (ret < 0) return -2;
			}
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

int filelist::file_stat(filelist::node* nd)
{
	struct stat statbuf;
	if (-1 == lstat(nd->get_fullname(), &statbuf)) {
		return 1;
	}

	// save the stat info
	nd->_statinfo.inode = statbuf.st_ino;
	nd->_file_size = statbuf.st_size;

	switch (statbuf.st_mode & S_IFMT) {
		case S_IFBLK: case S_IFCHR:
		case S_IFIFO: case S_IFSOCK:
			assert(0); break;

		case S_IFDIR: assert(nd->isdir()); break;
		case S_IFLNK:
			nd->_f.soft_link = 1;
			break;

		default: break;		
	}

	// check hard link
	if (statbuf.st_nlink != 1 && !nd->_f.isdir) {
		// this file has a hard link
		assert(nd->_f.soft_link == 0);
		nd->_f.hard_link = 1;
	}
	return 0;
}

filelist::node* filelist::create_node(dir d,
	filelist::node* parents, listnode_t& pending)
{
	auto* ret = new filelist::node();
	if (nullptr == ret) {
		return ret;
	}

	const char* fullpath = d->get_fullpath();
	ret->_full_name = fullpath;
	ret->_full_name += d->get_dirname();

	const char *prefix = _root->get_fullname();
	size_t prefix_len = strlen(prefix);
	assert(!strncmp(fullpath, prefix, prefix_len));
	ret->_name = d->get_fullpath() + prefix_len;
	ret->_name += d->get_dirname();

	if (d->entry_isdir()) {
		ret->_f.isdir = 1;
		listnode_add(pending, ret->_ownerlist);
	}
	else listnode_add(_global_list, ret->_ownerlist);
	file_stat(ret);
	
	// add to global avltree
	int retval = avl_insert(&_global_name_tree, &ret->_avlnode,
		filelist::node::global_name_compare);
	assert(retval == 0);

	// add to directory avltree
	if (nullptr != parents) {
		listnode_add(parents->_children, ret->_siblings);
	}
	return ret;
}

filelist::node::node()
: _flags(0), _parents(nullptr)
, _digest_next(nullptr)
, _user_data(nullptr)
, _file_size(-1)
{
	listnode_init(_ownerlist);
	listnode_init(_children);
	listnode_init(_usr_ownerlist);
	memset(_digest, 0, sizeof(_digest));
	memset(&_statinfo, 0, sizeof(_statinfo));
}

filelist::node::node(const char* root)
: _flags(0), _parents(nullptr)
, _digest_next(nullptr)
, _user_data(nullptr)
, _file_size(0)	// size of root shall be 0
{
	_full_name = root;
	if (_full_name[_full_name.length() - 1] != '/') {
		_name = "/";
	}
	_f.isdir = 1;

	listnode_init(_ownerlist);
	listnode_init(_children);
	listnode_init(_usr_ownerlist);
	memset(_digest, 0, sizeof(_digest));
	memset(&_statinfo, 0, sizeof(_statinfo));
}

bool filelist::node::checkflag(int flag)
{
	switch (flag) {
	case flag_isdir:
		return (_f.isdir) ? true : false;

	case flag_digested:
		return (_f.digested) ? true : false;

	case flag_hardlink:
		return (_f.hard_link) ? true : false;

	case flag_softlink:
		return (_f.soft_link) ? true : false;

	case flag_link_master:
		return (_f.link_master) ? true : false;

	default: return false;
	}
	// shall never come here
}

int filelist::node::setflag(int flag, int value)
{
	int ret = 0;
	switch (flag) {
	case flag_isdir:
		ret = _f.isdir;
		_f.isdir = (uint32_t)value;
		break;

	case flag_digested:
		ret = _f.digested;
		_f.digested = (uint32_t)value;
		break;

	case flag_hardlink:
		ret = _f.hard_link;
		_f.hard_link = (uint32_t)value;
		break;

	case flag_softlink:
		ret = _f.soft_link;
		_f.soft_link = (uint32_t)value;
		break;

	case flag_link_master:
		ret = _f.link_master;
		_f.link_master = (uint32_t)value;
		break;
		
	default: break;
	}
	return ret;
}

int filelist::node::global_name_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(filelist::node, _avlnode, a);
	auto* bb = AVLNODE_ENTRY(filelist::node, _avlnode, b);
	int ret = strcmp(aa->_name.c_str(), bb->_name.c_str());
	if (!ret) return 0;
	else if (ret < 0) return -1;
	else return 1;
}

int filelist::node::global_digest_compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(filelist::node, _digest_avlnode, a);
	auto* bb = AVLNODE_ENTRY(filelist::node, _digest_avlnode, b);
	int ret = memcmp(aa->_digest, bb->_digest, sizeof(aa->_digest));
	if (!ret) return 0;
	else if (ret < 0) return -1;
	else return 1;
}

filelist::node* filelist::find_node(const char* name)
{
	if (!name || !*name) {
		return nullptr;
	}

	string tmp(name);
	auto* nd = avl_find(_global_name_tree,
		MAKE_FIND_OBJECT(tmp, node, _name, _avlnode),
		node::global_name_compare);
	if (nullptr == nd) return nullptr;

	return AVLNODE_ENTRY(node, _avlnode, nd);
}

filelist::node* filelist::find_node(const uint8_t* dgst)
{
	assert(nullptr != dgst);
	auto* nd = avl_find(_global_digest_tree,
		MAKE_FIND_OBJECT(*dgst, node, _digest, _digest_avlnode),
		node::global_digest_compare);
	if (nullptr == nd) return nullptr;

	return AVLNODE_ENTRY(node, _digest_avlnode, nd);
}

int filelist::add_by_digest(node* nd)
{
	if (!nd || nd->isdir() || !nd->digested()) {
		return -EBADPARM;
	}

	avl_node_t* obj = avl_find(_global_digest_tree,
		&nd->_digest_avlnode, node::global_digest_compare);
	if (nullptr != obj) {
		auto* oldnd = AVLNODE_ENTRY(filelist::node, _digest_avlnode, obj);
		// add as a same digested node
		if (oldnd == nd) {
			// already in the avltree
			return 0;
		}
		// check if it is in the link list
		for (filelist::node* tmp = nd->_digest_next;
			tmp; tmp = tmp->_digest_next) {
			if (tmp == nd) return 0;	
		}
		// not in the link list
		// add it without any problem
		nd->_digest_next = oldnd->_digest_next;
		oldnd->_digest_next = nd;
		return 0;
	}

	// insert as a new node
	int ret = avl_insert(&_global_digest_tree,
		&nd->_digest_avlnode, node::global_digest_compare);
	assert(ret == 0);
	return 0;
}

bool filelist::node::check_digest_same(node* nd)
{
	assert(nullptr != nd);
	if (digested() != nd->digested()) {
		return false;
	}
	return memcmp(_digest, nd->_digest,
		sizeof(_digest)) ? false : true;
}

const uint8_t* filelist::node::generate_digest(void)
{
	if (digested()) {
		return (const uint8_t*)_digest;
	}

	size_t sz;
	const int bufsz = 2048;
	uint8_t tmpbuf[bufsz];

	assert(_file_size >= 0);
	FILE *fp = fopen(get_fullname(), "rb");
	if (nullptr == fp) {
		return nullptr;
	}

	digest dgst("sha256");
	for (;;) {
		sz = fread(tmpbuf, 1, bufsz, fp);
		if (!sz) break;
		dgst.append((void*)tmpbuf, sz);
	}
	fclose(fp);

	const uint8_t* dgst_result = dgst.getresult(&sz);
	assert(dgst_result && sz == 32);
	set_digest(dgst_result);
	return (const uint8_t*)_digest;
}

filelist::node* filelist::node::detach_child(void)
{
	if (listnode_isempty(_children)) {
		return nullptr;
	}

	auto* item = _children.next;
	auto* nd = list_entry(filelist::node, _siblings, item);
	listnode_del(nd->_siblings);
	return nd;
}
