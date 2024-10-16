/**
  Indicator showing the progress
 */

#ifndef __CXX_OTAPACK_INDICATOR_H__
#define __CXX_OTAPACK_INDICATOR_H__

#include "filelist.h"

#define MAX_INTERVAL	(100)

class filelist_indicator : public filelist_listener
{
public:
	filelist_indicator(filelist& flist);
	
	int on_directory(filelist::node*, uint32_t&);
	int on_file(filelist::node*, uint32_t&);
	int on_scanfinished(void);

	int on_processing(filelist::node* nd,
		size_t index, bool finished);

	filelist& get_filelist(void);
	listnode_t& get_user_dirlist(void);
	listnode_t& get_user_filelist(void);

private:
	void show_scan_info(void);
	void show_process_info(size_t index);
	void regen_interval(void);

protected:
	filelist& _dst;
	listnode_t _dir_list;
	listnode_t _file_list;

private:
	long _interval;
	long _dirfile_count;
	long _process_count;
};

#endif // __CXX_OTAPACK_INDICATOR_H__
/* EOF */
