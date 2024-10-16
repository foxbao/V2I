/** @file pkgconfig.h
 * defintion of package configuration
 */

#ifndef __CXX_ZAS_MWARE_OTA_INSTALLER_H__
#define __CXX_ZAS_MWARE_OTA_INSTALLER_H__

#include "mware/mware.h"

namespace zas {
namespace mware {

enum ota_state
{
	ota_state_unknown = 0,
	ota_state_backup_started,
	ota_state_backup_finished,
	ota_state_verify_started,
	ota_state_verify_finished,
	ota_state_update_started,
	ota_state_update_finished,
};

struct ota_installer_listener
{
	/**
	  Notify the state change of ota process
	  @param curr_state the current state
	 */
	virtual void on_state_changed(ota_state curr_state) = 0;
};

class MWARE_EXPORT ota_installer
{
public:
	enum partition_id {
		partition_unknown = 0,
		partition_src,
		partition_dst,
	};

public:
	/**
	  @param srcdir mounted source partition root directory
	  @param dstdir mounted destination partition root directory
	  @param otapack the otapackage file
	 */
	ota_installer(const char* srcdir,
		const char* dstdir,
		const char* otapack);

	~ota_installer();

	/**
	  Set the ota instaler listener
	  only one listener could be added and the listener
	  and the ota installer shall be in the same process
	  @return 0 for success
	 */
	int set_listener(ota_installer_listener* lnr);

	/**
	  Backup the specific partition to file
	  @param id the specific partition id
	  @param filename the file to be write
	  @return 0 for success
	 */
	int backup_partition(partition_id id, const char* filename);

	/**
	  Verify if the package is valid
	  @return 0 means valid
	 */
	int verify(void);

	/**
	  Update the partition (dir) using the OTA package
	  @return 0 for success
	 */
	int update(void);

private:
	void* _data;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(ota_installer);
};

}} // end of namespae zas::mware
#endif /* __CXX_ZAS_MWARE_OTA_INSTALLER_H__ */
/* EOF */
