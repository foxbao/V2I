/**
  filename: merge.h
  definition of details for OTA package file comparison and merging
 */

#ifndef __CXX_OTAPACK_MERGE_H__
#define __CXX_OTAPACK_MERGE_H__

#include "filelist.h"
#include "indicator.h"

class merge_file_listener : public filelist_indicator
{
public:
	class moved_fileinfo
	{
	public:
		friend class merge_file_listener;
		moved_fileinfo(filelist::node* nd, size_t index);
		int get_count_of_moving_targets(void);

		void add_moveto_object(filelist::node* nd) {
			nd->append_user(_to_list);
		}
		
		filelist::node* get_object_to_be_moved(void) {
			return _from;
		}

		listnode_t& get_moveto_list(void) {
			return _to_list;
		}

	private:
		static int node_address_compare(avl_node_t*, avl_node_t*);

	private:
		size_t _index;
		listnode_t _ownerlist;
		avl_node_t _avlnode;
		filelist::node* _from;
		listnode_t _to_list;
	};

	struct symlink_info
	{
		symlink_info(filelist::node*, size_t);
		static int avl_compare(avl_node_t*, avl_node_t*);

		filelist::node* nd;
		filelist::node* linkto_nd;
		avl_node_t avlnode;
		size_t ino;
	};

public:
	merge_file_listener(filelist&, filelist&);
	~merge_file_listener();

	int on_directory(filelist::node* nd,
		uint32_t& ret_flags);

	int on_file(filelist::node* nd,
		uint32_t& ret_flags);

	int on_scanfinished(void);
	int on_processing(filelist::node* nd,
		size_t index, bool finished);

public:
	size_t deleted_object_counting(void);
	moved_fileinfo* get_first_moved_fileinfo(void);
	moved_fileinfo* get_next_moved_fileinfo(moved_fileinfo*);
	void process_hardlink(filelist::node*);

public:
	listnode_t& get_deleted_list(void) {
		return _del_list;
	}

	listnode_t& get_duplicate_list(void) {
		return _dup_list;
	}

	size_t moved_file_count(void) {
		return _moved_file_count;
	}

	size_t moveto_file_count(void) {
		return _moveto_file_count;
	}

private:
	void release_all(void);
	int calc_file_digest(filelist::node* nd);
	moved_fileinfo* find_info(filelist::node* nd);
	bool check_set_file_exists(filelist::node* nd);
	int on_file_softlink(filelist::node* nd,
		uint32_t& ret_flags);
	int same_file_different_linktype(filelist::node* oldnd,
		filelist::node* newnd, uint32_t& ret_flags);

private:
	filelist& _src;
	listnode_t _del_list;
	listnode_t _same_list;
	listnode_t _dup_list;
	size_t _moved_file_count;
	size_t _moveto_file_count;
	size_t _del_object_count;
	avl_node_t* _moved_fileinfo_tree;
	listnode_t _moved_fileinfo_list;
	avl_node_t* _symlink_master_tree;
};

#endif // __CXX_OTAPACK_MERGE_H__
/* EOF */
