/** @file filelist.h
 * defintion of filelist classes
 */

#ifndef __CXX_ZAS_MWARE_FILELIST_H__
#define __CXX_ZAS_MWARE_FILELIST_H__

#include <string>
#include "std/list.h"
#include "utils/dir.h"
#include "utils/mutex.h"
#include "utils/avltree.h"
#include "mware/mware.h"

namespace zas {
namespace utils {
	class digest;
}}

using namespace std;
using namespace zas::utils;
namespace zas {
namespace mware {

struct filestat_info
{
	size_t inode;
	size_t file_size;
};

struct fileinfo;

struct softlink_info
{
	string link_name;
	string target_name;
	fileinfo* target_info;
};

struct fileinfo
{
	fileinfo();
	~fileinfo();
	void append_user(listnode_t& userlist);
	static fileinfo* get_user(listnode_t& userlist,
		bool removed = true);
	static int global_name_compare(avl_node_t*, avl_node_t*);
	static int global_inode_compare(avl_node_t*, avl_node_t*);
	
	avl_node_t avlnode;
	listnode_t ownerlist;
	listnode_t user_ownerlist;
	string fullpath;
	const char* name;

	// for hard link or soft link
	union {
		fileinfo* linkto;
		softlink_info* linkto_info;
	};

	union {
		uint32_t flags;
		struct {
			uint32_t isdir : 1;
			uint32_t digested : 1;
			uint32_t soft_link : 1;
			uint32_t hard_link : 1;
			uint32_t anonymous : 1;
		} f;
	};
	filestat_info info;
	size_t index;
};

class filelist;
struct filelist_listener
{
	virtual int on_handle_dir(filelist*, fileinfo*) = 0;
	virtual int on_handle_file(filelist*, fileinfo*) = 0;
	virtual int on_scanfinished(filelist*) = 0;
	virtual int on_processing(filelist*, fileinfo*,
		size_t index, bool finished) = 0;
};

class filelist
{
public:
	filelist();
	~filelist();

	int loaddir(const char* dir);
	bool loaded(void);
	uint8_t* generate_digest(void);

	bool digested(void);
	uint8_t* get_digest(void);
	
	fileinfo* ordered_getfirst(void);
	fileinfo* ordered_getnext(fileinfo* prev);

	fileinfo* get_firstdir(void);
	fileinfo* get_nextdir(fileinfo* prev);

	fileinfo* get_firstfile(void);
	fileinfo* get_nextfile(fileinfo* prev);

	bool set_need_digest(bool nd);

	fileinfo* find(const char* name);
	
private:
	void release_all(void);
	void release_list_unlocked(listnode_t& list);
	fileinfo* get_pending_dir_unlocked(listnode_t& list);
	int do_loaddir(fileinfo* parents, listnode_t& pending);
	bool check_entry_useless(dir d);

	fileinfo* create_fileinfo_object(dir d,
		fileinfo* parents, listnode_t& pending);
	int file_stat_unlocked(fileinfo* info);
	int check_create_link_master_unlocked(fileinfo* info);
	int finalize_softlink_objects(void);
	int prepare_softlink_unlocked(fileinfo* info);
	int single_file_digest(fileinfo* info, digest* dgst_all);

public:
	// note: we only support one listener
	// set NULL means clean the listener
	void set_listener(filelist_listener* lnr) {
		_listener = lnr;
	}

	size_t get_totalsize(void) {
		return _total_size;
	}

	mutex& getmutex(void) {
		return _mut;
	}

	void get_count(size_t& dirs, size_t& files) {
		dirs = _total_dirs;
		files = _total_files;
	}

private:
	listnode_t _global_dirlist;
	listnode_t _global_filelist;
	listnode_t _softlink_temp_list;
	avl_node_t* _global_name_tree;
	avl_node_t* _global_anonymous_tree;

	string _rootdir_fullpath;
	size_t _total_size;
	size_t _total_files;
	size_t _total_dirs;
	filelist_listener* _listener;

	uint8_t _global_digest[32];
	union {
		uint32_t _flags;
		struct {
			uint32_t digested : 1;
			uint32_t need_global_digest : 1;
		} _f;
	};

	mutex _mut;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(filelist);
};

}} // end of namespace zas::mware
#endif	// __CXX_ZAS_MWARE_FILELIST_H__
/* EOF */
