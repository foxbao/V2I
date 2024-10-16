/** @file ota-package.cpp
 * implementation of ota package operations
 */

#include <unistd.h>
#include <sys/mman.h>

#include "zlib.h"
#include "utils/cert.h"
#include "inc/otabundle.h"

namespace zas {
namespace mware {

using namespace zas::utils;

ota_fullpackage_generator::ota_fullpackage_generator(filelist& flist)
: _flist(flist), _packfp(nullptr), _clevel(9)
, _cblock_size(1024 * 1024), _version(0x10000000)
, _flags(0) {
	listnode_init(_dup_list);
}

ota_fullpackage_generator::~ota_fullpackage_generator()
{
	if (_packfp) {
		fclose(_packfp);
		_packfp = nullptr;
	}
}

bool ota_fullpackage_generator::set_compression(int clevel)
{
	bool ret = (_clevel > 0) ? true : false;
	if (clevel > 0) {
		_clevel = (clevel > 9) ? 9 : clevel;
	}
	else _clevel = 0;
	return ret;
}

bool ota_fullpackage_generator::get_compression(int* clevel)
{
	bool ret = (_clevel > 0) ? true : false;
	if (clevel) {
		if (!ret) *clevel = 0;
		else if (_clevel < 1) *clevel = 1;
		else if (_clevel > 9) *clevel = 9;
		else *clevel = _clevel;
	}
	return ret;
}

int ota_fullpackage_generator::generate(const char* filename)
{
	if (!_flist.loaded()) {
		return -ENOTREADY;
	}
	if (!filename || !*filename) {
		return -EBADPARM;
	}

	_packfp = fopen(filename, "wb+");
	if (nullptr == _packfp || init_header()) {
		return -EINVALID;
	}

	auto_mutex am(_flist.getmutex());
	if (write_new_created_dirlist_unlocked()) {
		return -EINVALID;
	}
	if (write_new_created_filelist_unlocked()) {
		return -EINVALID;
	}
	if (write_dup_filelist_unlocked()) {
		return -EINVALID;
	}

	// write the string section
	if (write_string_section()) {
		return -EINVALID;
	}
	
	// write the file section (all file data,
	// could be compressed)
	if (write_file_section()) {
		return -EINVALID;
	}

	// finally rewrite the file header
	if (finalize_header()) {
		return -EINVALID;
	}

	// successfully finished
	fclose(_packfp);
	_packfp = nullptr;
	return 0;
}

void ota_fullpackage_generator::calc_digest(uint8_t buf[32])
{
	const int bufsz = 2048;
	uint8_t buffer[bufsz];

	digest dgst("sha256");
	rewind(_packfp);

	size_t sz, cnt = 0;
	size_t total = _header.disig_envelope_pos;
	for (; cnt < total; cnt += sz) {
		sz = fread(buffer, 1, bufsz, _packfp);
		dgst.append((void*)buffer, sz);
	}

	assert(cnt == total && feof(_packfp));
	const uint8_t* ret = dgst.getresult(&sz);
	assert(sz == 32);

	memcpy((void*)buf, (void*)ret, sz);
}

int ota_fullpackage_generator::write_disig(
	const uint8_t* dgst)
{
	digital_signature_envelope_v1 header;
	memcpy(header.magic, DISIG_ENVELOPE_MAGIC,
		sizeof(header.magic));
	header.version = DISIG_EVLELOPE_VERSION;
	header.disig_type = disig_type_sha256;
	// todo: cert_size;

	// write the structure
	if (sizeof(header) != fwrite(&header, 1, sizeof(header), _packfp)) {
		return -1;
	}
	return 0;
}

int ota_fullpackage_generator::finalize_header(void)
{
	assert(nullptr != _packfp);
	if (_f.need_disig) {
		fseek(_packfp, 0, SEEK_END);
		_header.disig_envelope_pos = ftell(_packfp);
	}

	// rewrite the header
	rewind(_packfp);
	if (sizeof(_header) != fwrite(&_header, 1, sizeof(_header), _packfp)) {
		return -EINVALID;
	}

	// calculate the digest if necessary
	if (_f.need_disig)
	{
		uint8_t dgst_buf[32];
		calc_digest(dgst_buf);
		// manual request to do an fseek between read / write
		// operation. see man fopen
		fseek(_packfp, _header.disig_envelope_pos, SEEK_SET);
		if (write_disig((const uint8_t*)dgst_buf)) {
			return -EINVALID;
		}
	}
	return 0;
}

int ota_fullpackage_generator::write_string_section(void)
{
	_header.start_of_string_section = ftell(_packfp);
	fwrite(_str_sect.getbuf(), _str_sect.getsize(), 1, _packfp);
	return 0;
}

int ota_fullpackage_generator::write_file_section(void)
{
	// save the start_of_file_section
	assert(nullptr != _packfp);
	_header.start_of_file_section = ftell(_packfp);

	auto* file = _flist.get_firstfile();
	compression_worker cworker(_cblock_size, _clevel, _packfp);

	for (; file; file = _flist.get_nextfile(file)) {
		fileinfo* node = file;
		// must not be a directory
		if (file->f.isdir) {
			return -1;
		}

		if (file->f.hard_link) {
			// check if it is the hard link master
			// if it is the master, add it to new_create list
			auto* anonymous = file->linkto;
			if (anonymous->fullpath != file->fullpath) {
				continue;
			}
			node = anonymous;
		}
		else if (file->f.soft_link) {
			continue;
		}

		// add the file
		if (cworker.append(node->fullpath.c_str())) {
			return -1;
		}
	}
	// update the block size
	_header.cblock_size = uint32_t(cworker.get_blocksize());
	return 0;
}

int ota_fullpackage_generator::init_header(void)
{
	size_t tmp;
	if (!_str_sect.alloc_copy_string(
		"otapack.string_section", tmp)) {
		return -1;
	}

	strcpy(_header.magic, OTAPACK_MAGIC);
	_header.version = OTAPACK_VERSION;
	_header.disig_envelope_pos = 0;
	_header.f.full_package = 1;
	
	// set compression info
	int clevel;
	_header.f.compressed = get_compression(&clevel)
		? 1 : 0;
	_header.f.clevel = clevel;

	// check package version
	_header.prev_version = 0;	// we have no previous package
	_header.curr_version = 0;	// tbd

	// set digest and related config
	memset(_header.prevpkg_digest, 0, sizeof(_header.prevpkg_digest));

	// calcuate the digest for the directory
	_flist.set_need_digest(true);
	const uint8_t* digest = _flist.generate_digest();
	if (nullptr == digest) {
		return -2;
	}
	memcpy(_header.currpkg_digest, digest,
		sizeof(_header.currpkg_digest));

	// full size
	_header.currpkg_fullsize = _flist.get_totalsize();

	// start of sections
	_header.start_of_string_section = 0;
	_header.start_of_file_section = 0;

	// unused field
	_header.moved_file_count = 0;
	_header.deleted_object_count = 0;

	// new created items are used, leave it blank
	// because we will put correct number later
	_header.new_created_dir_count = 0;
	_header.new_created_file_count = 0;

	// dup file count is used, leave it blank
	// because we will put currect number later
	_header.dup_file_count = 0;

	// write a temporary version to file
	fwrite(&_header, sizeof(_header), 1, _packfp);
	return 0;
}

int ota_fullpackage_generator::write_new_created_dirlist_unlocked(void)
{
	size_t count = 0;
	auto* dir = _flist.get_firstdir();
	otapack_new_dir_object_v1 dirobject;

	assert(nullptr != _packfp);
	for (; dir; dir = _flist.get_nextdir(dir)) {
		// todo:
		assert(!dir->f.hard_link && !dir->f.soft_link);
		size_t pos;
		if (!_str_sect.alloc_copy_string(dir->name, pos)) {
			return -1;
		}
		dirobject.fullpath = pos;

		if (sizeof(dirobject) !=
			fwrite(&dirobject, 1, sizeof(dirobject), _packfp)) {
			return -2;
		}
		dir->index = count++;
	}
	_header.new_created_dir_count = count;
	return 0;
}

int ota_fullpackage_generator::write_new_created_filelist_unlocked(void)
{
	size_t count = 0;
	size_t filepos = 0;
	auto* file = _flist.get_firstfile();
	otapack_new_file_object_v1 fileobject;
	
	assert(nullptr != _packfp);
	for (; file; file = _flist.get_nextfile(file)) {
		// must not be a directory
		if (file->f.isdir) {
			return -1;
		}

		if (file->f.hard_link) {
			// check if it is the hard link master
			// if it is the master, add it to new_create list
			auto* anonymous = file->linkto;
			if (anonymous->fullpath != file->fullpath) {
				file->append_user(_dup_list);
				continue;
			}
			fileobject.type = otaobj_type_hardlink_master;
		}
		else if (file->f.soft_link) {
			// all softlink file is regarded as duplicate file
			file->append_user(_dup_list);
			continue;
		}
		else fileobject.type = otaobj_type_regular_file;

		size_t pos;
		if (!_str_sect.alloc_copy_string(file->name, pos)) {
			return -2;
		}

		fileobject.fullpath = pos;
		fileobject.file_size = file->info.file_size;
		fileobject.file_pos = filepos;
		filepos += file->info.file_size;

		// write file structure
		if (sizeof(fileobject) !=
			fwrite(&fileobject, 1, sizeof(fileobject), _packfp)) {
			return -3;
		}
		file->index = count++;
	}
	// update the header info
	_header.new_created_file_count = count;
	return 0;
}

int ota_fullpackage_generator::write_dup_filelist_unlocked(void)
{
	size_t count = 0;
	size_t filepos = 0;
	auto* file = _flist.get_firstfile();
	otapack_dup_file_object_v1 fileobject;
	
	assert(nullptr != _packfp);
	while (!listnode_isempty(_dup_list)) {
		auto* file = fileinfo::get_user(_dup_list);
		// must not be a directory
		if (file->f.isdir) {
			return -1;
		}

		size_t pos;
		if (!_str_sect.alloc_copy_string(file->name, pos)) {
			return -2;
		}
		fileobject.fullpath = pos;

		// set type
		if (file->f.soft_link) {
			fileobject.type = otaobj_type_softlink_file;

			// copyfrom_fullpath store the link name
			assert(nullptr != file->linkto_info);
			if (!_str_sect.alloc_copy_string(
				file->linkto_info->link_name.c_str(), pos)) {
				return -3;	
			}
			fileobject.copyfrom_fullpath = pos;
		}
		else if (file->f.hard_link) {
			fileobject.type = otaobj_type_hardlink;

			// get the copyfrom fileinfo object
			assert(nullptr != file->linkto);
			auto* ret = _flist.find(file->linkto->name);
			if (nullptr == ret) return -4;

			fileobject.copyfrom_fullpath = ret->index;
		}
		else return -5;

		fileobject.f.dup_from_previous_package = 0;

		// write file structure
		if (sizeof(fileobject) !=
			fwrite(&fileobject, 1, sizeof(fileobject), _packfp)) {
			return -6;
		}
		file->index = count++;
	}
	// update the header info
	_header.dup_file_count = count;
	return 0;
}

static inline void normalize_compression_parameter(
	int& block_size, int& clevel)
{
	if (block_size < OTA_MIN_BLKSZ) {
		block_size = OTA_MIN_BLKSZ;
	}
	else if (block_size > OTA_MAX_BLKSZ) {
		block_size = OTA_MAX_BLKSZ;
	}
	if (clevel < 0) clevel = 0;
	else if (clevel > 9) clevel = 9;
}

extract_worker::extract_worker(int blksz, int clevel, FILE* rfp)
: _block_size(blksz), _output_size(0), _clevel(clevel)
, _read_fp(rfp), _input_buffer(nullptr), _output_buffer(nullptr)
{
	assert(nullptr != _read_fp);
	normalize_compression_parameter(_block_size, _clevel);
}

compression_worker::compression_worker(int blksz, int clevel, FILE* wfp)
: _block_size(blksz), _output_size(0), _pos(0), _clevel(clevel)
, _write_fp(wfp), _input_buffer(nullptr), _output_buffer(nullptr)
{
	assert(nullptr != _write_fp);
	normalize_compression_parameter(_block_size, _clevel);

	_output_size = compressBound(_block_size);
	_input_buffer = new uint8_t [ _block_size ];
	_output_buffer = (_clevel > 0) ? new uint8_t [ _output_size ] : nullptr;
}

compression_worker::~compression_worker()
{
	// write the final data
	compress();

	if (_input_buffer) {
		delete [] _input_buffer;
		_input_buffer = nullptr;
	}
	if (_output_buffer) {
		delete [] _output_buffer;
		_output_buffer = nullptr;
	}
}

int compression_worker::append_data(FILE* fp)
{
	if (nullptr == fp) {
		return -1;
	}

	int remain = _block_size - _pos;
	size_t ret = fread(_input_buffer + _pos, 1, remain, fp);
	_pos += int(ret);

	if (ret < size_t(remain)) {
		if (!feof(fp)) {
			// an error occurred
			return -2;
		}
		return int(size_t(remain) - ret);
	}
	else return 0;
}

int compression_worker::compress(void)
{
	// nothing to write
	if (!_pos) return 0;

	size_t sz;
	if (!_clevel) {
		// we do not need compression
		sz = fwrite(_input_buffer, 1, _pos, _write_fp);
		if (sz != _pos) {
			// an error occurred
			_pos = 0; return -1;
		}
	}
	else {
		sz = _output_size;
		if (Z_OK != compress2(_output_buffer, &sz,
			_input_buffer, _pos, _clevel)) {
			_pos = 0; return -2;
		}
		assert(sz <= _output_size);

		// write the size first
		if (sizeof(sz) != fwrite(&sz, 1, sizeof(sz), _write_fp)) {
			_pos = 0; return -3;
		}
		// then write the content
		size_t ret = fwrite(_output_buffer, 1, sz, _write_fp);
		if (ret != sz) {
			_pos = 0; return -4;
		}
	}
	// reset the buffer
	_pos = 0;
	return 0;
}

int compression_worker::append(const char* filename)
{
	FILE *fp = fopen(filename, "rb");
	if (nullptr == fp) {
		return -1;
	}

	while (!feof(fp)) {
		int remain = append_data(fp);
		if (remain < 0) {
			// an error occurred
			fclose(fp);
			return -2;
		}

		if (!remain) {
			// currently the buffer is full
			// compress the buffer contant and write
			// it to file
			if (compress()) return -3;
		}
	}
	fclose(fp);
	return 0;
}

ota_package::ota_package(const char* filename)
: ota_extractor(filename), _pos(0)
, _flags(0), _mapping_size(0)
, _mapping(nullptr), _header(nullptr)
, _movobj_sect(nullptr), _delobj_sect(nullptr)
, _newdir_sect(nullptr), _newfile_sect(nullptr)
, _dupfile_sect(nullptr)
{
	listnode_init(_del_dirlist);
	listnode_init(_del_filelist);
}

ota_package::ota_package(FILE* fp, ssize_t pos)
: ota_extractor(fp), _pos(pos)
, _flags(0), _mapping_size(0)
, _mapping(nullptr), _header(nullptr)
, _movobj_sect(nullptr), _delobj_sect(nullptr)
, _newdir_sect(nullptr), _newfile_sect(nullptr)
, _dupfile_sect(nullptr)
{
	listnode_init(_del_dirlist);
	listnode_init(_del_filelist);
	assert(nullptr != fp);
	if (_pos < 0) {
		_pos = ftell(fp);
	}
}

ota_package::~ota_package()
{
	if (_mapping) {
		assert(_mapping_size != 0);
		::munmap(_mapping, _mapping_size);
		_mapping = nullptr;
	}
	if (_packfp != nullptr) {
		fclose(_packfp);
		_packfp = nullptr;
		_pos = 0;
	}
	if (_movobj_sect) {
		void** buf = (void**)_movobj_sect;
		delete [] buf;
	}
}

int ota_package::check_load_package(void)
{
	otapack_fileheader_v1 header;
	if (sizeof(header) !=
		fread(&header, 1, sizeof(header), _packfp)) {
		return -EINVALID;
	}
	if (strcmp(header.magic, OTAPACK_MAGIC)) {
		return -EBADFORMAT;
	}

	// check if we need to verify the package
	if (header.disig_envelope_pos) {
		// todo:
	}

	// check compressed
	if (header.f.compressed) {
		if (header.f.clevel < 1 && header.f.clevel > 9) {
			return -EBADFORMAT;
		}
		if (!header.cblock_size
			|| header.cblock_size < OTA_MIN_BLKSZ
			|| header.cblock_size > OTA_MAX_BLKSZ) {
			return -EBADFORMAT;
		}
	}
	
	// check version
	if (!header.curr_version) {
		return -EBADFORMAT;
	}
	if (!header.f.full_package) {
		if (!header.prev_version ||
			header.curr_version <= header.prev_version) {
			return -EBADFORMAT;
		}
	}

	if (header.start_of_file_section <= sizeof(header)
		|| header.start_of_file_section <= header.start_of_string_section) {
		return -EBADFORMAT;
	}
	_mapping = (uint8_t*)mapfile(header.start_of_file_section);
	if (nullptr == _mapping) {
		return -EINVALID;
	}
	assert(_mapping_size >= header.start_of_file_section);
	_header = (otapack_fileheader_v1*)(&_mapping[_mapping_size
		- header.start_of_file_section]);
	
	return load_all_sections();
}

void* ota_package::mapfile(size_t size)
{
	assert(nullptr != _packfp);
	int fd = fileno(_packfp);
	if (fd < 0) return nullptr;

	off_t offset = _pos;
	offset &= ~(sysconf(_SC_PAGE_SIZE) - 1);
	if (offset < _pos) {
		size += _pos - offset;
	}
	_mapping_size = size;

	void* ret = ::mmap(nullptr, size, PROT_READ,
		MAP_PRIVATE, fd, offset);
	if (ret == MAP_FAILED) {
		return nullptr;
	}
	return ret;
}

int ota_package::load_all_sections(void)
{
	size_t total_count = _header->moved_file_count
		+ _header->deleted_object_count;

	void** buf = new void* [total_count];
	if (nullptr == buf) {
		return -ENOMEMORY;
	}

	auto* movobj = (otapack_moved_object_v1*)(_header + 1);
	auto* delobj = (otapack_deleted_object_v1*)
		(&movobj[_header->moved_file_count]);

	auto* newdir = (otapack_new_dir_object_v1*)
		(&delobj[_header->deleted_object_count]);

	auto* newfile = (otapack_new_file_object_v1*)
		(&newdir[_header->new_created_dir_count]);

	auto* dupfile = (otapack_dup_file_object_v1*)
		(&newfile[_header->new_created_file_count]);

	_movobj_sect = (otapack_moved_object_v1**)(buf);
	_delobj_sect = (otapack_deleted_object_v1**)
		(_movobj_sect + _header->moved_file_count);

	_newdir_sect = newdir;
	_newfile_sect = newfile;
	_dupfile_sect = dupfile;

	sort_moved_object();
	sort_deleted_object();
	return 0;
}

int ota_package::movobj_compare(const void *a, const void *b, void* data)
{
	auto* otapack = reinterpret_cast<ota_package*>(data);
	assert(nullptr != otapack);

	auto* aa = (otapack_moved_object_v1*)a;
	auto* bb = (otapack_moved_object_v1*)b;
	const char* astr = otapack->get_string(aa->fullpath);
	const char* bstr = otapack->get_string(bb->fullpath);
	int ret = strcmp(astr, bstr);
	if (ret < 0) return -1;
	else if (ret > 0) return 1;
	else return 0;
}

int ota_package::qsort_partition(void** arr, int low,
	int high, qsort_compare_fn fn)
{
	void* key = arr[low];
	while (low < high) {
		while (low < high && fn(arr[high], key, this) >= 0) {
			high--;
		}
		if(low < high) {
			arr[low++] = arr[high];
		}
		while(low < high && fn(arr[low], key, this) <= 0) {
			low++;
		}
		if (low < high) {
			arr[high--] = arr[low];
		}
	}
	arr[low] = key;
	return low;
}

void ota_package::quick_sort(void** arr, int start,
	int end, qsort_compare_fn fn)
{
	int pos;
	if (start < end) {
		pos = qsort_partition(arr, start, end, fn);
		quick_sort(arr, start, pos - 1, fn);
		quick_sort(arr, pos + 1, end, fn);
	}
}

void ota_package::sort_moved_object(void)
{
	auto* sect = (otapack_moved_object_v1*)(_header + 1);
	for (int i = 0; i < _header->moved_file_count; ++i) {
		_movobj_sect[i] = &sect[i];
	}
	quick_sort((void**)_movobj_sect, 0,
		_header->moved_file_count - 1, movobj_compare);
}

int ota_package::delobj_sorter(const void *a, const void *b, void* data)
{
	auto* otapack = reinterpret_cast<ota_package*>(data);
	assert(nullptr != otapack);

	auto* aa = (otapack_deleted_object_v1*)a;
	auto* bb = (otapack_deleted_object_v1*)b;
	const char* astr = otapack->get_string(aa->fullpath);
	const char* bstr = otapack->get_string(bb->fullpath);
	int ret = strcmp(astr, bstr);
	if (ret < 0) return -1;
	else if (ret > 0) return 1;
	else return 0;
}

// a - the fileinfo object
int ota_package::delobj_compare(const void *a, const void *b, void* data)
{
	auto* otapack = reinterpret_cast<ota_package*>(data);
	assert(nullptr != otapack);

	auto* aa = (fileinfo*)a;
	auto* bb = (otapack_deleted_object_v1*)b;
	const char* bstr = otapack->get_string(bb->fullpath);
	int ret = strcmp(aa->fullpath.c_str(), bstr);
	if (ret < 0) return -1;
	else if (ret > 0) return 1;
	else return 0;
}

void ota_package::sort_deleted_object(void)
{
	auto* tmp = (otapack_moved_object_v1*)(_header + 1);
	auto* sect = (otapack_deleted_object_v1*)(
		&tmp[_header->moved_file_count]);
	
	for (int i = 0; i < _header->deleted_object_count; ++i) {
		_delobj_sect[i] = &sect[i];
	}
	quick_sort((void**)_delobj_sect, 0,
		_header->deleted_object_count - 1, delobj_sorter);
}

void* ota_package::binary_search(void** arr,
	void* pseudo_item, int count,
	qsort_compare_fn fn)
{
	int mid;
	int start = 0;
	int end = count - 1;
	void* ret = nullptr;

	// find the item
	while (start <= end)
	{
		mid = (start + end) / 2;
		int c = fn(pseudo_item, arr[mid], this);
		
		if (!c) {
			ret = arr[mid];
			break;
		}
		else if (c > 0) start = mid + 1;
		else end = mid - 1;
	}
	return ret;
}

int ota_package::verify(void)
{
	if (nullptr == _packfp) {
		return -EINVALID;
	}
	int ret = check_load_package();
	if (ret) return ret;

	return 0;
}

int ota_package::copy_verify(void)
{
	return 0;
}

ota_package::srcflist_listener::srcflist_listener()
: _flags(0) {
}

int ota_package::srcflist_listener::on_handle_dir(
	filelist*, fileinfo* finfo)
{
	ota_package* otapack = ancestor();
	if (_f.use_copy) {
		void* ret = otapack->binary_search(
			(void**)otapack->_delobj_sect,
			(void*)finfo,
			otapack->_header->deleted_object_count,
			ota_package::delobj_compare);

		if (nullptr == ret) {
			finfo->append_user(otapack->_copy_dirlist);
		}
	}
	else {

	}
	return 0;
}

int ota_package::srcflist_listener::on_handle_file(
	filelist*, fileinfo* finfo)
{
	ota_package* otapack = ancestor();
	if (_f.use_copy) {
		void* ret = otapack->binary_search(
			(void**)otapack->_delobj_sect,
			(void*)finfo,
			otapack->_header->deleted_object_count,
			ota_package::delobj_compare);

		if (nullptr == ret) {
			finfo->append_user(otapack->_copy_filelist);
		}
	}
	else {

	}
	return 0;
}

int ota_package::srcflist_listener::on_scanfinished(
	filelist*)
{
	return 0;
}

int ota_package::srcflist_listener::on_processing(
	filelist*, fileinfo*, size_t, bool)
{
	return 0;
}

int ota_package::update(const char* srcdir, const char* dstdir)
{
	int ret;
	if (!srcdir || !*srcdir) {
		return -EBADPARM;
	}
	if (!dstdir) {
		dstdir = srcdir;
	}
	else if (!*dstdir) {
		return -EBADPARM;
	}
	bool bcopy = strcmp(srcdir, dstdir) ? true : false;
	_flist_listener.use_copy(bcopy);

	// load the filelist of the source directory
	_filelist.set_listener(&_flist_listener);
	if (_filelist.loaddir(srcdir)) {
		return -EINVALID;
	}

	if (bcopy) {
		ret = copy_objects(srcdir, dstdir);
		if (ret) return ret;
	}

	// generate all new created directories
	ret = create_new_dirs(dstdir);
	if (ret) return ret;

	create_new_files(dstdir);
//	update_moved_objects_v1();
	return 0;
}

int ota_package::create_new_dirs(const char* dstdir)
{
	string dst;
	for (int i = 0; i < _header->new_created_dir_count; ++i) {
		dst = dstdir;
		dst += get_string(_newdir_sect[i].fullpath);
		if (createdir(dst.c_str())) {
			return -1;
		}
	}
	return 0;
}

int ota_package::create_new_files(const char* dstdir)
{
	for (int i = 0; i < _header->new_created_file_count; ++i) {
		auto& file = _newfile_sect[i];
	}
	return 0;
}

int ota_package::copy_objects(const char* srcdir, const char* dstdir)
{
	assert(srcdir && dstdir);
	if (copy_create_dirs(dstdir)) {
		return -1;
	}
	if (copy_files(srcdir, dstdir)) {
		return -2;
	}
	return 0;
}

int ota_package::copy_create_dirs(const char* dstdir)
{
	string dir;
	while (!listnode_isempty(_copy_dirlist)) {
		auto* node = fileinfo::get_user(_copy_dirlist);
		assert(node->f.isdir);

		dir = dstdir;
		dir += node->name;
		createdir(dir.c_str());
	}
	return 0;
}

int ota_package::copy_files(const char* srcdir, const char* dstdir)
{
	string src, dst;
	while (!listnode_isempty(_copy_filelist)) {
		auto* node = fileinfo::get_user(_copy_filelist);
		assert(nullptr != node);

		src = srcdir;
		src += node->name;
		dst = dstdir;
		dst += node->name;

		if (copy_single_file(src.c_str(), dst.c_str())) {
			return -1;
		}
	}
	return 0;
}

int ota_package::copy_single_file(const char* src, const char* dst)
{
	int ret = 0;
	const size_t bufsz = 2048;
	char buffer[bufsz];

	FILE* rfp = fopen(src, "rb");
	FILE* wfp = fopen(dst, "wb");
	if (nullptr == rfp || nullptr == wfp) {
		ret = -EACCESS; goto error;
	}

	for (;;) {
		size_t sz = fread(buffer, 1, bufsz, rfp);
		if (sz != fwrite(buffer, 1, sz, wfp)) {
			ret = -EACCES; goto error;
		}
		if (sz < bufsz) {
			assert(feof(rfp));
			break;
		}
	}

error:
	if (rfp) fclose(rfp);
	if (wfp) fclose(wfp);
	return ret;
}

int ota_package::update_moved_objects_v1(void)
{
	assert(nullptr != _header && nullptr != _movobj_sect);
	if (_header->version != OTAPACK_VERSION_V1) {
		return -ENOTSUPPORT;
	}
/*
	for (int i = 0; i < _header->moved_file_count; ++i) {
		printf("%s\n", get_string(_movobj_sect[i].fullpath));
	}
	for (int i = 0; i < _header->dup_file_count; ++i) {
		printf("dup: %s->%s\n",
			get_string(_dupfile_sect[i].copyfrom_fullpath),
			get_string(_dupfile_sect[i].fullpath));
	}*/
	return 0;
}

const char* ota_package::get_string(size_t strid)
{
	if (nullptr == _header) {
		return nullptr;
	}
	size_t strtbl_sz = _header->start_of_file_section
		- _header->start_of_string_section;
	if (strid >= strtbl_sz) {
		return nullptr;
	}
	return &((const char*)_header)[
		_header->start_of_string_section + strid];
}

ota_compressed_package::ota_compressed_package(
	const char* filename)
	: ota_extractor(filename)
{
}

int ota_compressed_package::verify(void)
{
	// todo: need implementation
	return 0;
}

int ota_compressed_package::copy_verify(void)
{
	// todo: need implementation
	return 0;
}

ota_filetype otapack_gettype(const char* pkgname)
{
	ota_filetype ret = otaft_unknown;
	if (!pkgname || !*pkgname) {
		return ret;
	}

	FILE *fp = fopen(pkgname, "rb");
	if (nullptr == fp) {
		return ret;
	}

	uint8_t buf[16];
	if (8 != fread(buf, 1, 8, fp)) {
		goto error;
	}

	if (!memcmp(buf, OTAPACK_MAGIC, 8)) {
		ret = otaft_regular_package;
		goto error;
	}
	// read another 8 bytes
	if (8 != fread(&buf[8], 1, 8, fp)) {
		goto error;
	}

	if (!memcmp(buf, OTA_BUNDLE_MAGIC, 16)) {
		ret = otaft_bundle_package;
		goto error;
	}
	if (!memcmp(buf, COMPRESSED_BUNDLE_MAGIC, 16)) {
		ret = otaft_compressed_package;
		goto error;
	}
error:
	fclose(fp);
	return ret;
}

}} // end of namespace zas::mware
/* EOF */
