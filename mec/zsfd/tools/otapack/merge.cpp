/**
  filename: merge.cpp
  implementation of file merge for OTA package
 */

#include "utils/cert.h"
#include "merge.h"

merge_file_listener::merge_file_listener(
	filelist& src, filelist& dst)
	: filelist_indicator(dst), _src(src)
	, _moved_fileinfo_tree(nullptr)
	, _moved_file_count(0)
	, _moveto_file_count(0)
	, _del_object_count(0)
	, _symlink_master_tree(nullptr)
{
	listnode_init(_del_list);
	listnode_init(_dup_list);
	listnode_init(_same_list);
	listnode_init(_moved_fileinfo_list);
}

merge_file_listener::~merge_file_listener()
{
	release_all();
}

void merge_file_listener::release_all(void)
{
	while (!listnode_isempty(_moved_fileinfo_list)) {
		auto* obj = list_entry(moved_fileinfo, _ownerlist, \
		_moved_fileinfo_list.next);
		listnode_del(obj->_ownerlist);
		delete obj;
	}
	while (!listnode_isempty(_del_list)) {
		filelist::node::getlist(_del_list.next);
	}
	while (!listnode_isempty(_dup_list)) {
		filelist::node::getlist(_dup_list.next);
	}
}
	
int merge_file_listener::on_directory(
	filelist::node* nd, uint32_t& ret_flags)
{
	// show indicator first
	filelist_indicator::on_directory(nd, ret_flags);

	const char* dirname = nd->get_name();
	auto* src_nd = _src.find_node(dirname);
	if (nullptr == src_nd) {
		nd->set_type(nodetype_dir_new_created);
		lnr_set_objtype(ret_flags, nodetype_dir_new_created);
	}
	else if (!src_nd->isdir()) {
		// previously: it is a file
		// currently: it becomes a directory
		// we mark delete of the previous file
		src_nd->set_type(nodetype_file_deleted);
		src_nd->append_user(_del_list);
		nd->set_type(nodetype_dir_new_created);
		lnr_set_objtype(ret_flags, nodetype_dir_new_created);
	}
	// otherwise, the previous object is a
	// directory, and is the same as is in
	// the new package, we do nothing
	else {
		src_nd->set_type(nodetype_dir_the_same);
		src_nd->remove_user();
		nd->set_type(nodetype_dir_the_same);
		lnr_set_objtype(ret_flags, nodetype_dir_the_same);
	}
	return 0;
}

int merge_file_listener::on_file_softlink(filelist::node* nd,
	uint32_t& ret_flags)
{
	// todo: handle soft-link
	return 1;
}

int merge_file_listener::on_file(filelist::node* nd,
		uint32_t& ret_flags)
{
	// show indicator first
	filelist_indicator::on_file(nd, ret_flags);
	if (!on_file_softlink(nd, ret_flags)) {
		return 0;
	}

	const uint8_t* dgst = nd->generate_digest();
	assert(nullptr != dgst);
	_dst.add_by_digest(nd);

	auto* src_nd = _src.find_node(nd->get_name());
	if (nullptr == src_nd) {
		// file not found in previous package
		// we check if the same file is located
		// in different directory of previous
		// package
		src_nd = _src.find_node(dgst);
		if (src_nd) {
			// we will not put it into any list, since
			// we create moved_fileinfo structure for
			// the moved objects
			src_nd->set_type(nodetype_file_move_src);
			src_nd->remove_user();

			nd->set_type(nodetype_file_move_dest);
			lnr_set_objtype(ret_flags, nodetype_file_move_dest);

			auto* info = find_info(src_nd);
			assert(nullptr != info);
			info->add_moveto_object(nd);
			nd->set_user_data((void*)info);
			++_moveto_file_count;
		}
		else {
			nd->set_type(nodetype_file_new_created);
			lnr_set_objtype(ret_flags, nodetype_file_new_created);
			check_set_file_exists(nd);
		}
	} else {
		// check if the digest is the same
		if (src_nd->check_digest_same(nd)) {
			if (src_nd->islink() != nd->islink()) {
				same_file_different_linktype(src_nd, nd, ret_flags);
				return 0;
			}
			src_nd->set_type(nodetype_file_the_same);
			src_nd->remove_user();
			nd->append_user(_same_list);

			nd->set_type(nodetype_file_the_same);
			lnr_set_objtype(ret_flags, nodetype_file_the_same);
		} else {
			// we do not need to delete the old file
			// since the created new file will overwrite
			// the old one. this help to optimize OTA
			// performance
			src_nd->remove_user();
			nd->set_type(nodetype_file_new_created);
			lnr_set_objtype(ret_flags, nodetype_file_new_created);
			check_set_file_exists(nd);
		}
	}
	return 0;
}

int merge_file_listener::same_file_different_linktype(
	filelist::node* oldnd,
	filelist::node* newnd, uint32_t& ret_flags)
{
	printf("same_file_different_linktype\n");
	return 0;
}

bool merge_file_listener::check_set_file_exists(filelist::node* nd)
{
	auto* same_nd = get_filelist().find_node(nd->generate_digest());
	if (nullptr == same_nd || same_nd == nd) {
		return false;
	}
	
	// todo: check symbolic link
	if (same_nd->get_type() != nodetype_file_new_created) {
		return false;
	}
	nd->set_user_data((void*)same_nd);
	nd->append_user(_dup_list);
}

int merge_file_listener::on_scanfinished(void)
{
	return filelist_indicator::on_scanfinished();
}

int merge_file_listener::on_processing(
	filelist::node* nd,
	size_t index, bool finished)
{
	filelist_indicator::on_processing(nd, index, finished);

	if (nd->checkflag(filelist::node::flag_hardlink)) {
		process_hardlink(nd);
	}
}

merge_file_listener::symlink_info::symlink_info(
	filelist::node* n, size_t i)
: nd(n), linkto_nd(nullptr), ino(i)
{
}

int merge_file_listener::symlink_info::avl_compare(
	avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(merge_file_listener::symlink_info, avlnode, a);
	auto* bb = AVLNODE_ENTRY(merge_file_listener::symlink_info, avlnode, b);
	if (aa->ino < bb->ino) {
		return -1;
	}
	else if (aa->ino > bb->ino) {
		return 1;
	}
	else return 0;
}

void merge_file_listener::process_hardlink(filelist::node* nd)
{
	// a hard link must not be a directory
	assert(!nd->isdir());

	auto* hdlnkinfo = new symlink_info(nd, nd->get_statinfo().inode);
	assert(nullptr != hdlnkinfo);
	nd->set_user_data(hdlnkinfo);

	// check if we are the master by search the master tree
	auto* ret = avl_find(_symlink_master_tree,
		&hdlnkinfo->avlnode,
		symlink_info::avl_compare);
	if (!ret) {
		nd->setflag(filelist::node::flag_link_master, 1);
		nd->append_user(_file_list);

		// insert it to the hardlink master tree
		int tmp = avl_insert(&_symlink_master_tree,
			&hdlnkinfo->avlnode,
			symlink_info::avl_compare);
		assert(tmp == 0);
	}
	else {
		// this is not a hardlink master
		nd->append_user(_dup_list);
		// link to master
		auto* mstlnkinfo = AVLNODE_ENTRY( \
			merge_file_listener::symlink_info, \
			avlnode, ret);
		hdlnkinfo->linkto_nd = mstlnkinfo->nd;
	}
}

merge_file_listener::moved_fileinfo*
merge_file_listener::find_info(filelist::node* nd)
{
	assert(nullptr != nd);
	auto* avl = avl_find(_moved_fileinfo_tree,
		MAKE_FIND_OBJECT(nd, moved_fileinfo, _from, _avlnode),
		moved_fileinfo::node_address_compare);
	if (avl) {
		return AVLNODE_ENTRY(moved_fileinfo, _avlnode, avl);
	}
	// create a moved_fileinfo object
	auto* obj = new moved_fileinfo(nd, _moved_file_count);
	assert(nullptr != obj);
	int ret = avl_insert(&_moved_fileinfo_tree,
		&obj->_avlnode, moved_fileinfo::node_address_compare);
	assert(ret == 0);
	listnode_add(_moved_fileinfo_list, obj->_ownerlist);
	++_moved_file_count;
	return obj;
}

int merge_file_listener::moved_fileinfo::node_address_compare(
	avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(merge_file_listener::moved_fileinfo, \
		_avlnode, a);
	auto* bb = AVLNODE_ENTRY(merge_file_listener::moved_fileinfo, \
		_avlnode, b);
	if (((size_t)aa->_from) == ((size_t)bb->_from)) {
		return 0;
	} else if (((size_t)aa->_from) < ((size_t)bb->_from)) {
		return -1;
	} else return 1;
}

merge_file_listener::moved_fileinfo::moved_fileinfo(
	filelist::node* nd, size_t index)
	: _from(nd), _index(index) {
	listnode_init(_to_list);
	assert(nullptr != nd);
}

size_t merge_file_listener::deleted_object_counting(void)
{
	if (_del_object_count) {
		return _del_object_count;
	}

	auto* item = _del_list.next;
	for (; item != &_del_list; item = item->next,
		_del_object_count++);

	return _del_object_count;
}

merge_file_listener::moved_fileinfo*
merge_file_listener::get_first_moved_fileinfo(void)
{
	auto* first = avl_first(_moved_fileinfo_tree);
	if (nullptr == first) return nullptr;
	return AVLNODE_ENTRY(merge_file_listener::moved_fileinfo, \
		_avlnode, first);
}

merge_file_listener::moved_fileinfo*
merge_file_listener::get_next_moved_fileinfo(moved_fileinfo* info)
{
	if (nullptr == info) {
		return nullptr;
	}
	auto* next = avl_next(&info->_avlnode);
	if (nullptr == next) {
		return nullptr;
	}
	return AVLNODE_ENTRY(merge_file_listener::moved_fileinfo, \
		_avlnode, next);
}

int merge_file_listener::moved_fileinfo::
	get_count_of_moving_targets(void)
{
	int ret = 0;
	auto* item = _to_list.next;
	for (; item != &_to_list; ++ret, item = item->next);
	return ret;
}