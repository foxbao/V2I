/** @file otabundle.h
 * defintion of ota bundle operations
 */

#ifndef __CXX_ZAS_MWARE_OTABUNDLE_H__
#define __CXX_ZAS_MWARE_OTABUNDLE_H__

#include <string>
#include "filelist.h"
#include "strsect.h"
#include "package-def.h"

namespace zas {
namespace mware {

typedef int (*qsort_compare_fn)(const void*, const void*, void*);

class extract_worker
{
public:
	extract_worker(int blksz, int clevel, FILE* rfp);
	~extract_worker();

private:
	int _block_size;
	int _output_size;
	int _clevel;
	uint8_t* _input_buffer;
	uint8_t* _output_buffer;
	FILE* _read_fp;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(extract_worker);
};

class compression_worker
{
public:
	compression_worker(int blksz, int clevel, FILE* wfp);
	~compression_worker();

	int append(const char* filename);

public:
	int finalize(void) {
		return compress();
	}

	int get_blocksize(void) {
		return _block_size;
	}

private:
	// return: remain
	int append_data(FILE* fp);
	int compress(void);

private:
	int _block_size;
	int _output_size;
	int _clevel;
	int _pos;
	uint8_t* _input_buffer;
	uint8_t* _output_buffer;
	FILE* _write_fp;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(compression_worker);
};

class ota_fullpackage_generator
{
public:
	ota_fullpackage_generator(filelist&);
	~ota_fullpackage_generator();

	int generate(const char* filename);
	bool set_compression(int clevel);
	bool get_compression(int* clevel = nullptr);

public:
	void set_version(uint32_t ver) {
		_version = ver;
	}

	void set_compression_block_size(int blksz) {
		_cblock_size = blksz;
	}

private:
	int init_header(void);
	int finalize_header(void);
	int write_new_created_dirlist_unlocked(void);
	int write_new_created_filelist_unlocked(void);
	int write_dup_filelist_unlocked(void);
	int write_string_section(void);
	int write_file_section(void);

	void calc_digest(uint8_t buf[32]);
	int write_disig(const uint8_t* dgst);

private:
	FILE* _packfp;

	// compression level
	// 0 means no compression
	int _clevel;
	int _cblock_size;

	union {
		uint32_t _flags;
		struct {
			uint32_t need_disig : 1;
		} _f;
	};

	string_section _str_sect;
	filelist& _flist;
	uint32_t _version;
	listnode_t _dup_list;
	otapack_fileheader_v1 _header;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(ota_fullpackage_generator);
};

struct ota_extractor
{
public:
	ota_extractor(const char* filename);
	ota_extractor(FILE* fp);
	virtual ~ota_extractor();

	virtual int verify(void);
	virtual int copy_verify(void);
	virtual int update(const char* srcdir, const char* dstdir);

protected:
	FILE* _packfp;
	std::string _pkgfile;
};

class ota_bundle : public ota_extractor
{
public:
	ota_bundle(const char* filename);
	int verify(void);
	int copy_verifiy(void);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(ota_bundle);
};

class ota_compressed_package : public ota_extractor
{
public:
	ota_compressed_package(const char* filename);
	int verify(void);
	int copy_verify(void);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(ota_compressed_package);
};

class ota_package : public ota_extractor
{
public:
	class srcflist_listener : public filelist_listener
	{
	public:
		srcflist_listener();
		void use_copy(bool copy) {
			_f.use_copy = (copy) ? 1 : 0;
		}

	private:
		int on_handle_dir(filelist*, fileinfo*);
		int on_handle_file(filelist*, fileinfo*);
		int on_scanfinished(filelist*);
		int on_processing(filelist*, fileinfo*, size_t, bool);

	private:
		ota_package* ancestor(void) {
			return zas_ancestor(ota_package, _flist_listener);
		}
		union {
			uint32_t _flags;
			struct {
				uint32_t use_copy : 1;
			} _f;
		};
	};

	ota_package(const char* filename);
	ota_package(FILE* fp, ssize_t pos = -1);
	~ota_package();

	int verify(void);
	int copy_verify(void);

	// IMPORTANT NOTICE:
	// the update method is not reentriable
	// the caller must make sure not calling this
	// method twice in the same time
	int update(const char* srcdir, const char* dstdir);

private:
	int check_load_package(void);
	void* mapfile(size_t size);

	int load_all_sections(void);

	int qsort_partition(void** arr, int low,
		int high, qsort_compare_fn fn);
	void quick_sort(void** arr, int start,
		int end, qsort_compare_fn fn);
	void* binary_search(void** arr,
		void* pseudo_item, int count,
		qsort_compare_fn fn);

	void sort_moved_object(void);
	static int movobj_compare(const void*, const void*, void*);
	void sort_deleted_object(void);
	static int delobj_sorter(const void*, const void*, void*);
	static int delobj_compare(const void*, const void*, void*);

	const char* get_string(size_t strid);

	int copy_objects(const char* srcdir, const char* dstdir);
	int copy_create_dirs(const char* dstdir);
	int copy_files(const char* srcdir, const char* dstdir);
	int copy_single_file(const char* src, const char* dst);
	int create_new_dirs(const char* dstdir);
	int create_new_files(const char* dstdir);

	int update_moved_objects_v1(void);

private:
	ssize_t _pos;
	size_t _mapping_size;
	uint8_t* _mapping;

	union {
		uint32_t _flags;
		struct {
			uint32_t loaded : 1;
		} _f;
	};

	otapack_fileheader_v1* _header;
	filelist _filelist;
	srcflist_listener _flist_listener;

	union {
		listnode_t _del_filelist;
		listnode_t _copy_filelist;
	};
	union {
		listnode_t _del_dirlist;
		listnode_t _copy_dirlist;
	};

	// sections
	otapack_moved_object_v1** _movobj_sect;
	otapack_deleted_object_v1** _delobj_sect;
	otapack_new_dir_object_v1* _newdir_sect;
	otapack_new_file_object_v1* _newfile_sect;
	otapack_dup_file_object_v1* _dupfile_sect;

	ZAS_DISABLE_EVIL_CONSTRUCTOR(ota_package);
};

ota_filetype otapack_gettype(const char* pkgname);

}} // end of namespace zas::mware
#endif // __CXX_ZAS_MWARE_OTABUNDLE_H__
/* EOF */
