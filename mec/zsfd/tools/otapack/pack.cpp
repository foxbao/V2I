#include "zlib.h"
#include "pack.h"

pack_source_listener::pack_source_listener(filelist& srclist)
: filelist_indicator(srclist) {
}

void pack_source_listener::rescan_delete_objects(listnode_t& dellist)
{
	// handle useless file first
	while (!listnode_isempty(_file_list)) {
		auto* nd = filelist::node::getlist(_file_list.next, true);
		nd->append_user(dellist);
	}

	// handle useless directory
	handle_useless_dir(dellist);
}

void pack_source_listener::handle_useless_dir(listnode_t& dellist)
{
	while (!listnode_isempty(_dir_list)) {
		auto* nd = filelist::node::getlist(_dir_list.next, true);
		// is this original useless items in the list
		if (nodetype_unknown == nd->get_type()) {
			nd->append_user(dellist);
		}
		// remove all the children
		auto* child = nd->detach_child();
		for (; child; child = nd->detach_child()) {
			child->set_type(nodetype_dir_deleted);
			if (child->isdir()) {
				child->append_user(_dir_list);
			}
			else child->remove_user();
		}
	}
}

string_section::~string_section()
{
	if (_buf) {
		free(_buf);
		_buf = nullptr;
	}
	_total_sz = _curr_sz = 0;
}

char* string_section::alloc(int sz, size_t* delta_pos)
{
	if (sz <= 0) {
		return nullptr;
	}
	int new_sz = _curr_sz + sz;
	int new_total_sz = _total_sz;
	for (; new_sz > new_total_sz; new_total_sz += min_size);
	if (new_total_sz != _total_sz) {
		_buf = (char*)realloc(_buf, new_total_sz);
		assert(NULL != _buf);
		_total_sz = new_total_sz;
	}
	char* ret = &_buf[_curr_sz];
	_curr_sz = new_sz;
		
	if (delta_pos) {
		*delta_pos = uint32_t(ret - _buf);
	}
	return ret;
}

const char* string_section::alloc_copy_string(const char* str, size_t& pos)
{
	assert(nullptr != str);
	int len = strlen(str) + 1;
	char* ret = alloc(len, &pos);
	if (nullptr == ret) return nullptr;
	memcpy(ret, str, len);
	return (const char*)ret;
}

const char* pack_generator::_errstr[] = {
	"no error",
	"invalid OTA package version",							/* packgen_errcode_bad_version */
	"OTA package version mis-match",						/* packgen_errcode_version_mismatch */
	"error in generating moved file info table",			/* packgen_errcode_moved_file_info */
	"error in generating deleted file info table",			/* packgen_errcode_delete_object_info */
	"error in generating new directory table",				/* packgen_errcode_new_dir_object_info */
	"error in generating new file table",					/* packgen_errcode_new_file_object_info */
	"error in generating duplicated file table",			/* packgen_errcode_dup_file_object_info */
	"fail in write file, check disk free space",			/* packgen_errcode_write_file */
	"error in compressing OTA package",						/* packgen_errcode_compress_package */
	"error in creating file, check permission",				/* packgen_errcode_creating_file */
	"error in file operation, check permission",			/* packgen_errcode_file_operation */
	"bad file size",										/* packgen_errcode_bad_file_size */
	"not enough memory",									/* packgen_not_enough_memory */
	"an error occurred in compressing (zlib)",				/* packgen_error_in_compression */
};

const char* pack_generator::err_string(void)
{
	if (_errcode >= packgen_errcode_max) {
		return "error string table overflow";
	}
	return _errstr[_errcode];
}

pack_generator::pack_generator(pack_source_listener& src,
	merge_file_listener& dst, otapack_config& cfg)
	: _src(src), _dst(dst), _cfg(cfg), _packfp(nullptr)
	, _errcode(0), _total_size(0), _gen_size(0), _lnr(nullptr)
{
}

pack_generator::~pack_generator()
{
	if (_packfp) {
		fclose(_packfp);
		_packfp = nullptr;
	}
}

int pack_generator::generate(const char* dstfile)
{
	FILE *fp = fopen(dstfile, "wb");
	if (nullptr == fp) {
		return -1;
	}

	_packfp = fp;
	_destfile = dstfile;

	if (init_otapack_header()) {
		return -2;
	}

	// generate the moved file info table
	if (write_moved_object_info()) {
		seterror(packgen_errcode_moved_file_info);
		return -3;
	}
	// generate the delete file info table
	if (write_deleted_object_info()) {
		seterror(packgen_errcode_delete_object_info);
		return -4;
	}

	// generate the new directory and file table
	if (write_new_dir_object_info()) {
		seterror(packgen_errcode_new_dir_object_info);
		return -5;
	}
	if (write_new_file_object_info()) {
		seterror(packgen_errcode_new_file_object_info);
		return -6;
	}
	if (write_dup_file_object_info()) {
		seterror(packgen_errcode_dup_file_object_info);
		return -7;
	}

	// write the string table
	write_string_section();

	// write all file data
	write_file_section();

	// rewrite the final version header
	fseek(_packfp, 0, SEEK_SET);
	fwrite(&_header, sizeof(_header), 1, _packfp);

	// finished
	fclose(_packfp);
	_packfp = nullptr;
	return 0;
}

int pack_generator::init_otapack_header(void)
{
	size_t tmp;
	if (!_str_sect.alloc_copy_string("otapack.string_section", tmp)) {
		return -1;
	}

	strcpy(_header.magic, OTAPACK_MAGIC);
	_header.version = OTAPACK_VERSION;
	_header.flags = 0;

	assert(!_cfg.full_package);
	_header.f.full_package = _cfg.full_package;

	_header.cblock_size = 0;
	_header.f.compressed = _cfg.need_full_compress;
	if (_header.f.compressed) {
		// set the compress level
		int level = _cfg.compress_level;
		if (level < 1) level = 1;
		if (level > 9) level = 9;
		_header.f.clevel = level;

		_header.cblock_size = _cfg.compression_block_size;
		if (_header.cblock_size < OTA_MIN_BLKSZ) {
			_header.cblock_size = OTA_MIN_BLKSZ;
		}
		if (_header.cblock_size > OTA_MAX_BLKSZ) {
			_header.cblock_size = OTA_MAX_BLKSZ;
		}
	}

	// check package version
	_header.prev_version = _cfg.prev_version;
	_header.curr_version = _cfg.curr_version;
	if (!_header.prev_version || !_header.curr_version) {
		seterror(packgen_errcode_bad_version);
		return -2;
	}
	if (_header.prev_version >= _header.curr_version) {
		seterror(packgen_errcode_version_mismatch);
		return -3;
	}

	// set digest and related config
	memcpy(_header.prevpkg_digest, _src.get_filelist().get_digest(),
		sizeof(_header.prevpkg_digest));
	memcpy(_header.currpkg_digest, _dst.get_filelist().get_digest(),
		sizeof(_header.currpkg_digest));

	// full size
	_header.currpkg_fullsize = _dst.get_filelist().get_totalsize();

	// start of sections
	_header.start_of_string_section = 0;
	_header.start_of_file_section = 0;

	// write a temporary version to file
	fwrite(&_header, sizeof(_header), 1, _packfp);
	
	return 0;
}

int pack_generator::write_moved_object_info(void)
{
	otapack_moved_object_v1 object;
	_header.moved_file_count = _dst.moved_file_count();

	size_t count = 0;
	auto* info = _dst.get_first_moved_fileinfo();
	for (; info; info  = _dst.get_next_moved_fileinfo(info)) {
		filelist::node* nd = info->get_object_to_be_moved();

		// allocate fullpath string
		size_t pos;
		const char* ret = _str_sect.alloc_copy_string(
			nd->get_name(), pos);
		if (nullptr == ret) return -1;
		object.fullpath = pos;
		object.type = get_node_type(nd);
		object.refcnt = info->get_count_of_moving_targets();
		assert(object.refcnt);

		// write the moved file info
		fwrite(&object, sizeof(object), 1, _packfp);
		++count;
	}
	assert(count == _header.moved_file_count);
	return 0;
}

ota_object_type pack_generator::get_node_type(filelist::node* nd)
{
	assert(nullptr != nd);
	if (nd->isdir()) {
		return otaobj_type_dir;
	}
	return otaobj_type_unknown;
}

int pack_generator::write_deleted_object_info(void)
{
	otapack_deleted_object_v1 object;
	_header.deleted_object_count = _dst.deleted_object_counting();
	listnode_t& dellist = _dst.get_deleted_list();

	size_t count = 0;
	auto* item = dellist.next;
	for (; item != &dellist; item = item->next) {
		auto* nd = filelist::node::getlist(item, false);

		// allocate fullpath string
		size_t pos;
		const char* ret = _str_sect.alloc_copy_string(
			nd->get_name(), pos);
		if (nullptr == ret) return -1;

		object.fullpath = pos;
		object.type = get_node_type(nd);

		// write the moved file info
		fwrite(&object, sizeof(object), 1, _packfp);
		++count;
	}
	assert(count == _header.deleted_object_count);
	return 0;
}

int pack_generator::write_new_dir_object_info(void)
{
	size_t count = 0;
	otapack_new_dir_object_v1 object;
	auto& dirlist = _dst.get_user_dirlist();
	auto* item = dirlist.next;
	for (; item != &dirlist; item = item->next) {
		auto* nd = filelist::node::getlist(item, false);
		if (!nd->isdir()) {
			return -1;
		}
		
		node_type ntype = nd->get_type();
		if (ntype != nodetype_dir_new_created
			&& ntype != nodetype_dir_the_same) {
			return -2;
		}
		if (ntype == nodetype_dir_the_same) {
			continue;
		}

		// allocate fullpath string
		size_t pos;
		const char* ret = _str_sect.alloc_copy_string(
			nd->get_name(), pos);
		if (nullptr == ret) return -3;

		object.fullpath = pos;

		// write the moved file info
		fwrite(&object, sizeof(object), 1, _packfp);
		++count;
	}
	_header.new_created_dir_count = count;
	return 0;
}

int pack_generator::write_new_file_object_info(void)
{
	size_t count = 0, filepos = 0;
	otapack_new_file_object_v1 object;
	auto& flist = _dst.get_user_filelist();

	auto* item = flist.next;
	for (; item != &flist; item = item->next) {
		auto* nd = filelist::node::getlist(item, false);
		if (nd->isdir()) {
			return -1;
		}
		
		node_type ntype = nd->get_type();
		if (ntype != nodetype_file_new_created) {
			return -2;
		}

		// allocate fullpath string
		size_t pos;
		const char* ret = _str_sect.alloc_copy_string(
			nd->get_name(), pos);
		if (nullptr == ret) return -3;
		object.fullpath = pos;
	
		// set the file size
		long length = nd->get_filesize();
		assert(length >= 0);
		object.file_size = (size_t)length;
		object.file_pos = filepos;
		filepos += (size_t)length;

		// write the new file info
		fwrite(&object, sizeof(object), 1, _packfp);
		++count;
	}
	_header.new_created_file_count = count;
	_total_size = filepos;
	return 0;
}

int pack_generator::write_moveto_file_object_info(size_t& cnt)
{
	size_t count = 0;
	otapack_dup_file_object_v1 object;

	auto* info = _dst.get_first_moved_fileinfo();
	for (; info; info  = _dst.get_next_moved_fileinfo(info)) {
		listnode_t& moveto_list = info->get_moveto_list();
		assert(!listnode_isempty(moveto_list));

		while (!listnode_isempty(moveto_list)) {
			auto* nd = filelist::node::getlist(moveto_list.next);
			assert(info == (merge_file_listener::\
				moved_fileinfo*)nd->get_user_data());

			// parameter check
			node_type ntype = nd->get_type();
			if (ntype != nodetype_file_move_dest) {
				return -1;
			}

			// allocate fullpath string
			size_t pos;
			const char* ret = _str_sect.alloc_copy_string(
				nd->get_name(), pos);
			if (nullptr == ret) return -2;
			object.fullpath = pos;

			// todo: other attributes
			object.f.dup_from_previous_package = 1;
			
			// set the copy from file fullpath
			auto* copyfrom_nd = info->get_object_to_be_moved();
			assert(nullptr != copyfrom_nd);

			ret = _str_sect.alloc_copy_string(copyfrom_nd->get_name(), pos);
			if (nullptr == ret) return -3;
			object.copyfrom_fullpath = pos;

			// write the moved file info
			fwrite(&object, sizeof(object), 1, _packfp);
			++count;
		}
	}
	assert(_dst.moveto_file_count() == count);
	cnt = count;
	return 0;
}

int pack_generator::write_dup_file_object_info(void)
{
	size_t count = 0;
	otapack_dup_file_object_v1 object;
	auto& flist = _dst.get_duplicate_list();

	auto* item = flist.next;
	for (; item != &flist; item = item->next) {
		auto* nd = filelist::node::getlist(item, false);
		if (nd->isdir()) {
			return -1;
		}
		
		node_type ntype = nd->get_type();
		if (ntype != nodetype_file_new_created) {
			return -2;
		}

		// allocate fullpath string
		size_t pos;
		const char* ret = _str_sect.alloc_copy_string(
			nd->get_name(), pos);
		if (nullptr == ret) return -3;
		object.fullpath = pos;

		// todo: other attributes
		object.flags = 0;

		// set the copy from/link file fullpath
		filelist::node* copyfrom_nd;
		if (nd->islink()) {
			assert(!nd->checkflag(filelist::node::flag_link_master));
			auto* info = (merge_file_listener::symlink_info*)nd->get_user_data();
			assert(nullptr != info);
			copyfrom_nd = info->linkto_nd;
		} else {
			copyfrom_nd = (filelist::node*)nd->get_user_data();
		}
		assert(nullptr != copyfrom_nd);
		ret = _str_sect.alloc_copy_string(copyfrom_nd->get_name(), pos);
		if (nullptr == ret) return -4;
		object.copyfrom_fullpath = pos;

		// write the duplicate file info
		fwrite(&object, sizeof(object), 1, _packfp);
		++count;
	}
	_header.dup_file_count = count;
	if (write_moveto_file_object_info(count)) {
		return -5;
	}
	_header.dup_file_count += count;
	return 0;
}

void pack_generator::update_generate_progress(int size)
{
	if (nullptr == _lnr) {
		return;
	}
	_gen_size += size;
	_lnr->on_generating_progress(_gen_size * 100 / _total_size);
}

int pack_generator::write_file_section(void)
{
	// save the start_of_file_section
	assert(nullptr != _packfp);
	_header.start_of_file_section = ftell(_packfp);

	compression_worker cworker(
		_header.cblock_size,
		_header.f.clevel, _packfp, this);

	auto& flist = _dst.get_user_filelist();
	auto* item = flist.next;
	for (; item != &flist; item = item->next) {
		auto* nd = filelist::node::getlist(item, false);
		assert(!nd->isdir());

		// the file exists, omitted
		if (nd->get_user_data()) {
			_total_size -= filesize(nd->get_fullname());
			continue;
		}

		// add the file
		if (cworker.append(nd->get_fullname())) {
			return -1;
		}
	}
	update_generate_progress(0);
	return 0;
}

int pack_generator::write_string_section(void)
{
	_header.start_of_string_section = ftell(_packfp);
	fwrite(_str_sect.getbuf(), _str_sect.getsize(), 1, _packfp);
	return 0;
}

int pack_generator::compress_package(void)
{
	if (!_cfg.need_compress) {
		return 0;
	}

	if (_destfile.empty()) {
		seterror(packgen_errcode_compress_package);
		return -1;
	}
	string new_name = _destfile;
	new_name += ".tmp";
	string tmp = new_name;

	// get an unique temporary file name
	for (int i = 1; fileexists(tmp.c_str()); ++i) {
		char strbuf[32];
		sprintf(strbuf, "%u", i);
		tmp = new_name + strbuf;
	}
	
	if (!zas::utils::rename(_destfile.c_str(), tmp.c_str())) {
		seterror(packgen_errcode_file_operation);
		return -2;
	}
	FILE* rfp = fopen(tmp.c_str(), "rb");
	if (nullptr == rfp) {
		seterror(packgen_errcode_file_operation);
		return -3;
	}
	FILE* wfp = fopen(_destfile.c_str(), "wb");
	if (nullptr == wfp) {
		seterror(packgen_errcode_creating_file);
		return -4;
	}

	// do compression
	int ret = do_compress_package(rfp, wfp);
	fclose(rfp);
	fclose(wfp);

	// delete the temporary file
	deletefile(tmp.c_str());
	return ret;
}

int pack_generator::init_compress_fileheader(FILE *rfp, FILE* wfp,
	compressed_bundle_fileheader_v1& header)
{
	// write the header
	memcpy(&header, COMPRESSED_BUNDLE_MAGIC, sizeof(header));
	header.version = COMPRESSED_BUNDLE_VERSION;

	header.block_size = _cfg.compression_block_size;
	if (header.block_size < OTA_MIN_BLKSZ) {
		header.block_size = OTA_MIN_BLKSZ;
	}
	if (header.block_size > OTA_MAX_BLKSZ) {
		header.block_size = OTA_MAX_BLKSZ;
	}

	fseek(rfp, 0, SEEK_END);
	header.file_size = (size_t)ftell(rfp);
	if (!header.file_size) {
		seterror(packgen_errcode_bad_file_size);
		return -1;
	}
	fseek(rfp, 0, SEEK_SET);
	
	// calculate block count
	header.block_count = header.file_size / header.block_size;
	if (header.file_size % header.block_size) {
		++header.block_count;
	}
	return 0;
}

int pack_generator::do_compress_package(FILE* rfp, FILE* wfp)
{
	compressed_bundle_fileheader_v1 header;
	compressed_bundle_blockinfo_v1 blockinfo;

	if (init_compress_fileheader(rfp, wfp, header)) {
		return -1;
	}

	// allocate data buffer and compress buffer
	uint8_t* data_buf = new uint8_t [header.block_size];
	size_t comp_buf_size = compressBound(header.block_size);
	uint8_t* comp_buf = new uint8_t [comp_buf_size];
	if (!data_buf || !comp_buf) {
		if (data_buf) delete [] data_buf;
		if (comp_buf) delete [] comp_buf;
		seterror(packgen_not_enough_memory);
		return -2;
	}

	int ret = 0;
	int level = _cfg.compress_level;
	if (level < 1) level = 1;
	if (level > 9) level = 9;

	for (uint32_t i = 0; i < header.block_count; ++i) {
		// update the progress first
		update_compress_progress(i * 100 / header.block_count);
		size_t len = fread(data_buf, 1, header.block_size, rfp);
		if (len != header.block_size) {
			if (!feof(rfp)) {
				seterror(packgen_errcode_file_operation);
				ret = -3; goto exitfunc;
			}
			assert(i + 1 == header.block_count);
		}
		// compress
		size_t sz = comp_buf_size;
		if (Z_OK != compress2(comp_buf, &sz, data_buf, len, level)) {
			seterror(packgen_error_in_compression);
			ret = -4; goto exitfunc;
		}
		assert(sz <= comp_buf_size);

		// save to file
		blockinfo.original_size = (uint32_t)len;
		blockinfo.compressed_size = (uint32_t)sz;
		if (sizeof(blockinfo) != fwrite(&blockinfo, 1, sizeof(blockinfo), wfp)) {
			seterror(packgen_errcode_file_operation);
			ret = -5; goto exitfunc;
		}
		if (sz != fwrite(comp_buf, 1, sz, wfp)) {
			seterror(packgen_errcode_file_operation);
			ret = -6; goto exitfunc;
		}
	}
	assert(feof(rfp));
	update_compress_progress(100);

exitfunc:
	delete [] data_buf;
	delete [] comp_buf;
	return ret;
}

void pack_generator::update_compress_progress(int progress)
{
	if (!_lnr) return;
	if (progress < 0) progress = 0;
	if (progress > 100) progress = 100;
	_lnr->on_compressing_progress(progress);
}

compression_worker::compression_worker(int blksz, int clevel,
	FILE* wfp, pack_generator* pkgen)
: _block_size(blksz), _output_size(0), _pos(0), _clevel(clevel)
, _write_fp(wfp), _input_buffer(nullptr), _output_buffer(nullptr)
, _pkgen(pkgen)
{
	assert(nullptr != _write_fp);

	if (_block_size < OTA_MIN_BLKSZ) {
		_block_size = OTA_MIN_BLKSZ;
	}
	else if (_block_size > OTA_MAX_BLKSZ) {
		_block_size = OTA_MAX_BLKSZ;
	}
	if (_clevel < 0) _clevel = 0;
	else if (clevel > 9) _clevel = 9;

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

	if (_pkgen) _pkgen->update_generate_progress(ret);
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

/* EOF */
