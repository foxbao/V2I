#ifndef __CXX_OTAPACK_PACK_H__
#define __CXX_OTAPACK_PACK_H__

#include "package-def.h"
#include "config.h"
#include "merge.h"

using namespace zas::mware;

class pack_generator;

class compression_worker
{
public:
	compression_worker(int blksz, int clevel, FILE* wfp,
		pack_generator* pkgen);
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
	
	pack_generator* _pkgen;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(compression_worker);
};

class pack_source_listener : public filelist_indicator
{
public:
	pack_source_listener(filelist& srclist);
	void rescan_delete_objects(listnode_t& dellist);

private:
	void handle_useless_dir(listnode_t& dellist);
};

class string_section
{
	const size_t min_size = 1024;
public:
	string_section()
	: _buf(NULL), _total_sz(0)
	, _curr_sz(0) {
	}

	~string_section();

	size_t getsize(void) {
		return _curr_sz;
	}

	void* getbuf(void) {
		return _buf;
	}

	char* alloc(int sz, size_t* delta_pos);
	const char* alloc_copy_string(const char* str, size_t& pos);

	const char* to_string(size_t pos) {
		if (pos >= _curr_sz) return NULL;
		return (const char*)&_buf[pos];
	}

private:
	char* _buf;
	size_t _total_sz, _curr_sz;
};

enum packgen_errcode
{
	packgen_errcode_bad_version = 1,
	packgen_errcode_version_mismatch,
	packgen_errcode_moved_file_info,
	packgen_errcode_delete_object_info,
	packgen_errcode_new_dir_object_info,
	packgen_errcode_new_file_object_info,
	packgen_errcode_dup_file_object_info,
	packgen_errcode_write_file,
	packgen_errcode_compress_package,
	packgen_errcode_creating_file,
	packgen_errcode_file_operation,
	packgen_errcode_bad_file_size,
	packgen_not_enough_memory,
	packgen_error_in_compression,
	packgen_errcode_max,
};

struct pack_generator_listener
{
	virtual void on_generating_progress(int percentage) = 0;
	virtual void on_compressing_progress(int percentage) = 0;
};

class pack_generator
{
	friend class compression_worker;
public:
	pack_generator(pack_source_listener&,
		merge_file_listener&,
		otapack_config&);
	~pack_generator();

	int generate(const char* dstfile);
	int compress_package(void);
	const char* err_string(void);

public:
	size_t get_totalsize(void) {
		return _total_size;
	}

	otapack_config& getconfig(void) {
		return _cfg;
	}

	void set_listener(pack_generator_listener* lnr) {
		_lnr = lnr;
	}

private:
	int init_otapack_header(void);
	int write_moved_object_info(void);
	int write_deleted_object_info(void);
	int write_new_dir_object_info(void);
	int write_new_file_object_info(void);
	int write_dup_file_object_info(void);
	int write_string_section(void);
	int write_file_section(void);
	int write_moveto_file_object_info(size_t& cnt);

	ota_object_type get_node_type(filelist::node*);
	int do_compress_package(FILE* rfp, FILE* wfp);
	int init_compress_fileheader(FILE* rfp, FILE* wfp,
		compressed_bundle_fileheader_v1& header);

	void update_compress_progress(int progress);
	void update_generate_progress(int size);
	
private:
	int seterror(int errcode) {
		if (!_errcode) _errcode = errcode;
		return errcode;
	}

private:
	pack_source_listener& _src;
	merge_file_listener& _dst;
	otapack_config& _cfg;

	int _errcode;
	static const char* _errstr[];

	string_section _str_sect;
	otapack_fileheader_v1 _header;
	FILE* _packfp;

	string _destfile;
	size_t _total_size;
	size_t _gen_size;

	pack_generator_listener* _lnr;
};

#endif // __CXX_OTAPACK_PACK_H__
/* EOF */
