/**
 * @file filelist.h
 * @author James Chou (zhoujianming@civ-ip.com)
 * @brief 
 * @version 0.1
 * @date 2021-11-13
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef __CXX_OTAPACK_FILELIST_H__
#define __CXX_OTAPACK_FILELIST_H__

#include <string>
#include "std/list.h"
#include "utils/avltree.h"
#include "utils/dir.h"

using namespace std;
using namespace zas::utils;

struct filelist_listener;

enum node_type
{
	nodetype_unknown = 0,
	nodetype_dir_new_created,
	nodetype_dir_deleted,
	nodetype_dir_the_same,
	nodetype_file_deleted,
	nodetype_file_move_src,
	nodetype_file_move_dest,
	nodetype_file_the_same,
	nodetype_file_updated,
	nodetype_file_new_created,
};

class filelist
{
public:
	class node
	{
		friend class filelist;
	public:
		enum {
			flag_isdir = 1,
			flag_digested,
			flag_hardlink,
			flag_softlink,
			flag_link_master,
		};

		struct stat_info {
			size_t inode;
		};

		node();
		node(const char* root);

		const char* get_name(void) {
			return _name.c_str();
		}

		const char* get_fullname(void) {
			return _full_name.c_str();
		}
	
		bool isdir(void) {
			return (_f.isdir) ? true : false;
		}

		bool islink(void) {
			return (_f.hard_link || _f.soft_link)
				? true : false;
		}

		bool checkflag(int flag);
		int setflag(int flag, int value);

		void set_type(node_type type) {
			_f.type = (uint32_t)type;
		}

		node_type get_type(void) {
			return (node_type)_f.type;
		}

		stat_info& get_statinfo(void) {
			return _statinfo;
		}

		static node* getlist(listnode_t* item, bool detach = true) {
			auto* ret = list_entry(node, _usr_ownerlist, item);
			if (detach) listnode_del(ret->_usr_ownerlist);
			return ret;
		}
	
		void set_digest(const uint8_t* dgst) {
			assert(nullptr != dgst);
			memcpy(_digest, dgst, sizeof(_digest));
			_f.digested = 1;
		}
	
		int append_user(listnode_t& usr) {
			if (!listnode_isempty(_usr_ownerlist)) {
				listnode_del(_usr_ownerlist);
			}
			listnode_add(usr, _usr_ownerlist);
		}

		int remove_user(void) {
			if (listnode_isempty(_usr_ownerlist)) {
				return -1;
			}
			listnode_del(_usr_ownerlist);
			return 0;
		}

		bool digested(void) {
			return (_f.digested) ? true : false;
		}

		void* set_user_data(void* data) {
			void* tmp = _user_data;
			_user_data = data;
			return tmp;
		}

		void* get_user_data(void) {
			return _user_data;
		}

		long get_filesize(void) {
			return _file_size;
		}

		node* detach_child(void);
		bool check_digest_same(node* nd);
		const uint8_t* generate_digest(void);

	private:
		static int global_name_compare(avl_node_t*, avl_node_t*);
		static int global_digest_compare(avl_node_t*, avl_node_t*);

	private:
		listnode_t _ownerlist;
		listnode_t _usr_ownerlist;
		listnode_t _children;
		listnode_t _siblings;
		avl_node_t _avlnode;
		avl_node_t _digest_avlnode;
		node* _digest_next;
	
		string _full_name;
		string _name;
		long _file_size;

		// stat info
		stat_info _statinfo;

		// calculated encrypted sha256 digest of the file
		uint8_t _digest[32];

		union {
			struct {
				uint32_t isdir : 1;
				uint32_t digested : 1;
				uint32_t hard_link : 1;
				uint32_t soft_link : 1;
				uint32_t link_master : 1;
				uint32_t type : 5;
			} _f;
			uint32_t _flags;
		};
		node* _parents;

		// move_to: save the moved_fileinfo object
		// dup_file: new created but duplicated file (including
		//		soft-link), save the same file node pointer
		// hard_link: hardlink object
		void* _user_data;
	};

public:
	filelist();
	~filelist();

	int loaddir(const char*);

	node* find_node(const char* name);
	node* find_node(const uint8_t* dgst);

	int add_by_digest(node* nd);

	void add_listener(filelist_listener* lnr) {
		_listener = lnr;
	}

	void get_countinfo(size_t& dir_count, size_t& file_count) {
		dir_count = _total_dirs;
		file_count = _total_files;
	}

	node* getroot(void) {
		return _root;
	}

	const uint8_t* get_digest(void) {
		return _global_digest;
	}

	size_t get_totalsize(void) {
		return _total_size;
	}

private:
	void release_all(void);
	node* create_node(dir, node*, listnode_t&);
	int do_loaddir(node*, listnode_t&);
	bool check_entry_useless(dir d);

	int calc_digest(void);
	int file_stat(node* nd);

private:
	listnode_t _global_list;
	avl_node_t* _global_name_tree;
	avl_node_t* _global_digest_tree;
	node* _root;
	uint8_t _global_digest[32];
	filelist_listener* _listener;

	size_t _total_files;
	size_t _total_dirs;
	size_t _total_size;

	union {
		uint32_t _flags;
		struct {
			uint32_t need_global_digest : 1;
		} _f;
	};
};

enum listener_flags
{
};

#define lnr_set_objtype(f, t)	\
	do {	\
		(f) &= ~63;	\
		(f) |= (t);	\
	} while (0)

struct filelist_listener
{
	// retval < 0, an error occurred
	// retval = 0, successful
	virtual int on_directory(filelist::node* nd,
		uint32_t& ret_flags) = 0;

	virtual int on_file(filelist::node* nd,
		uint32_t& ret_flags) = 0;

	virtual int on_scanfinished(void) = 0;
	virtual int on_processing(filelist::node* nd,
		size_t index, bool finished) = 0;
};

#endif // __CXX_OTAPACK_FILELIST_H__
/* EOF */
