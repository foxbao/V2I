/**
  implementation: Indicator showing the progress
 */

#include "indicator.h"

filelist_indicator::filelist_indicator(filelist& flist)
: _dirfile_count(0)
, _process_count(0), _dst(flist) {
	listnode_init(_dir_list);
	listnode_init(_file_list);
	regen_interval();
}
	
int filelist_indicator::on_directory(filelist::node* nd, uint32_t&)
{
	nd->append_user((nd->isdir()) ? _dir_list : _file_list);
	++_dirfile_count;
	if (_dirfile_count > _interval) {
		_dirfile_count = 0;
		show_scan_info();
		regen_interval();
	}
	return 0;
}

int filelist_indicator::on_file(filelist::node* nd, uint32_t&)
{
	nd->append_user((nd->isdir()) ? _dir_list : _file_list);
	++_dirfile_count;
	if (_dirfile_count > _interval) {
		_dirfile_count = 0;
		show_scan_info();
		regen_interval();
	}
	return 0;
}
	
int filelist_indicator::on_scanfinished(void)
{
	size_t dircnt, filecnt;
	_dst.get_countinfo(dircnt, filecnt);
	printf("\r\"%s\" scanned (dir: %lu, file: %lu)\n",
		_dst.getroot()->get_fullname(),
		dircnt, filecnt);
	fflush(nullptr);
}

int filelist_indicator::on_processing(filelist::node* nd,
	size_t index, bool finished)
{
	++_process_count;
	if (finished || _process_count > _interval) {
		_process_count = 0;
		show_process_info(index);
		regen_interval();
	}
	return 0;
}

filelist& filelist_indicator::get_filelist(void)
{
	return _dst;
}

listnode_t& filelist_indicator::get_user_dirlist(void)
{
	return _dir_list;
}

listnode_t& filelist_indicator::get_user_filelist(void)
{
	return _file_list;
}

void filelist_indicator::show_scan_info(void)
{
	size_t dircnt, filecnt;
	_dst.get_countinfo(dircnt, filecnt);
	printf("\rScanning ... (dir: %lu, file: %lu)",
		dircnt, filecnt);
	fflush(nullptr);
}

void filelist_indicator::show_process_info(size_t index)
{
	size_t dircnt, filecnt;
	_dst.get_countinfo(dircnt, filecnt);
	filecnt += dircnt;
	if (!filecnt) return;
	printf("\rProcessing ... %lu%% (%lu of %lu)%s",
		index * 100 / filecnt, index, filecnt,
		(index >= filecnt) ? "\n" : "");
	fflush(nullptr);
}

inline void filelist_indicator::regen_interval(void)
{
	_interval = random() * MAX_INTERVAL / RAND_MAX;
}
