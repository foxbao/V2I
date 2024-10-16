/** @file ota-installer-impl.h
 * defintion of OTA installer
 */

#ifndef __CXX_ZAS_MWARE_OTA_INSTALLER_IMPL_H__
#define __CXX_ZAS_MWARE_OTA_INSTALLER_IMPL_H__

#include <string>
#include "utils/mutex.h"
#include "mware/ota-installer.h"

#include "package-def.h"
#include "filelist.h"

using namespace std;

namespace zas {
namespace mware {

class ota_bundle;
class ota_compressed_package;
class ota_package;

class ota_installer_impl
{
public:
	ota_installer_impl(const char* srcdir,
		const char* dstdir,
		const char* otapack);
	~ota_installer_impl();

	bool valid(void);
	int backup_partition(ota_installer::partition_id id,
		const char* filename);

	void reset(void);
	int verify(void);
	int update(void);

	void set_listener(ota_installer_listener* lnr);

private:
	bool check_file_in_dir(ota_installer::partition_id id,
		const char* filename);
	filelist* load_dir(ota_installer::partition_id id);
	int get_backup_fullpath(const char* filename, string& ret);
	int generate_backupfile(filelist& flist, string& fullpath);

	// ready
	bool ready_set_for_backup(void);
	bool ready_set_for_verify(void);
	bool ready_set_for_update(void);

private:
	string _srcdir;
	string _dstdir;
	string _otapack;

	union {
		uint32_t _flags;
		struct {
			uint32_t valid : 1;
			uint32_t state : 4;
			uint32_t pkgtype : 4;
		} _f;
	};

	ota_installer_listener* _lnr;
	ota_extractor* _extractor;

	filelist* _srcdir_flist;
	filelist* _dstdir_flist;
	zas::utils::mutex _mut;
};

}} // end of namespace zas::mware
#endif /* __CXX_ZAS_MWARE_OTA_INSTALLER_IMPL_H__ */
/* EOF */
