/** @file otapack-def.h
 * defintion of OTA package structure
 */

#ifndef __CXX_ZAS_MWARE_OTAPACK_DEF_H__
#define __CXX_ZAS_MWARE_OTAPACK_DEF_H__

#include "mware/mware.h"

namespace zas {
namespace mware {

#define OTA_MIN_BLKSZ	(32 * 1024)
#define OTA_MAX_BLKSZ	(32 * 1024 * 1024)

enum ota_filetype
{
	otaft_unknown = 0,
	otaft_bundle_package,
	otaft_compressed_package,
	otaft_regular_package,
};

#define DISIG_ENVELOPE_MAGIC	"disigenv"
#define DISIG_EVLELOPE_VERSION	0x01000000

enum digital_signature_type
{
	disig_type_unknown = 0,
	disig_type_sha256,
	disig_type_md5,
};

struct digital_signature_envelope_v1
{
	uint8_t magic[8];
	uint32_t version;
	digital_signature_type disig_type; 
	uint32_t cert_size;
	// 1) disig content
	// 2) cert content
} PACKED;

#define OTA_BUNDLE_MAGIC			"otabundlepackage"
#define OTA_BUNDLE_VERSION			0x01000000

struct otabundle_fileheader_v1
{
	char magic[16];
	size_t disig_envelope_pos;
	uint32_t version;
	uint32_t object_count;
} PACKED;

enum otabundle_object_type
{
	otabundle_objtype_unknown = 0,
	otabundle_objtype_otapack,
	otabundle_objtype_compressed_bundle,
	otabundle_objtype_file,
};

struct otabundle_object_v1
{
	int type;
	uint32_t object_name;
	uint32_t file_name;
	size_t size;
} PACKED;

#define COMPRESSED_BUNDLE_MAGIC		"compressedbundle"
#define COMPRESSED_BUNDLE_VERSION	0x01000000

struct compressed_bundle_fileheader_v1
{
	char magic[16];
	size_t disig_envelope_pos;
	uint32_t version;
	size_t file_size;
	uint32_t block_size;
	uint32_t block_count;
};

struct compressed_bundle_blockinfo_v1
{
	uint32_t original_size;
	uint32_t compressed_size;
};

#define OTAPACK_MAGIC		"OTAPACK"
// current version
#define OTAPACK_VERSION		0x01000000
// all supported version list
#define OTAPACK_VERSION_V1	0x01000000

/** OTA package file format
 *  1) ota_pack_fileheader			--- fixed size
 *  2) otapack_moved_object			--- variable size, could be empty, only for otapack
 *  3) otapack_deleted_object		--- variable size, could be emtpy, only for otapack
 *  4) otapack_new_dir_object		--- variable size, could be empty, only for otapack
 *  5) otapack_new_file_object		--- variable size, could be empty
 *  6) otapack_dup_file_object		--- variable size, could be empty
 *  7) string section				--- variable size
 *  8) file section					--- variable size
 *     all files saved sequently since the file size
 *     has already saved
 */

struct otapack_fileheader_v1
{
	char magic[8];
	size_t disig_envelope_pos;
	uint32_t version;
	
	union {
		uint32_t flags;
		struct {
			uint32_t full_package : 1;
			uint32_t compressed : 1;
			uint32_t clevel : 4;
		} f;
	};

	// compression block size
	// this field remain invalid unless the flag
	// compressed is set as true
	uint32_t cblock_size;

	// package versions
	uint32_t prev_version;
	uint32_t curr_version;

	uint8_t prevpkg_digest[32];
	uint8_t currpkg_digest[32];

	size_t currpkg_fullsize;

	// start of the string table in file
	uint32_t start_of_string_section;

	// start of the file conent section
	uint32_t start_of_file_section;

	// only file could be moved
	size_t moved_file_count;

	// both directory and file could be deleted
	// so the count contains both directory and file
	size_t deleted_object_count;

	// new created items
	size_t new_created_dir_count;
	size_t new_created_file_count;

	// there are 2 kinds of duplicated file type:
	// 1) the one duplicated from another file in a different
	//    directory in the previous package
	// 2) the one duplicated from another file in a different
	//    directory in the current package
	size_t dup_file_count;
} PACKED;

enum ota_object_type
{
	otaobj_type_unknown = 0,

	// directory
	otaobj_type_dir,

	// regular file
	otaobj_type_regular_file,

	// softlink file
	// a softlink file may pointer
	// to a directory
	otaobj_type_softlink_file,

	// hard link master file
	// a hard link could not link
	// to a directory
	otaobj_type_hardlink,
	otaobj_type_hardlink_master,
};

struct otapack_deleted_object_v1
{
	size_t fullpath;		// string
	int type;
} PACKED;

struct otapack_moved_object_v1
{
	size_t fullpath;
	int type;
	int refcnt;				// how many targets to be moved/copied
} PACKED;

struct otapack_new_dir_object_v1
{
	size_t fullpath;
} PACKED;

struct otapack_new_file_object_v1
{
	size_t fullpath;
	uint8_t type;
	uint8_t action;
	size_t file_size;
	size_t file_pos;		// relative to the start of file section
} PACKED;

struct otapack_dup_file_object_v1
{
	size_t fullpath;
	uint8_t type;
	union {
		uint8_t flags;
		struct {
			uint8_t dup_from_previous_package : 1;
		} f;
	};
	size_t copyfrom_fullpath;
} PACKED;

}} // end of namespace zas::mware
#endif /* __CXX_ZAS_MWARE_OTAPACK_DEF_H__ */
/* EOF */
