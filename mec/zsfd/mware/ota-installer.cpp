/** @file ota-installer.cpp
 * implementation of OTA installer
 */

#include "utils/dir.h"
#include "mware/sysconfig.h"
#include "inc/otabundle.h"
#include "inc/ota-installer-impl.h"

using namespace zas::utils;

namespace zas {
namespace mware {

#define set_state_unlocked(st)	_f.state = (uint32_t)(st)
#define set_state(st)	\
	do {	\
		_mut.lock();	\
		set_state_unlocked(st);	\
		if (_lnr) _lnr->on_state_changed(	\
			(ota_state)_f.state);	\
		_mut.unlock();	\
	} while (0)

ota_installer_impl::ota_installer_impl(const char* srcdir,
	const char* dstdir, const char* otapack)
: _otapack(otapack), _extractor(nullptr), _flags(0)
, _srcdir_flist(nullptr), _dstdir_flist(nullptr)
, _lnr(nullptr)
{
	if (!isdir(srcdir) || !isdir(dstdir)
		|| !fileexists(otapack)) {
		return;
	}

	_srcdir = canonicalize_filename(srcdir, true);
	_dstdir = canonicalize_filename(dstdir, true);
	_f.valid = 1;
}

ota_installer_impl::~ota_installer_impl()
{
	if (_extractor) {
		delete _extractor;
		_extractor = nullptr;
	}
}

void ota_installer_impl::reset(void)
{
	set_state(ota_state_unknown);
}

inline bool ota_installer_impl::valid(void) {
	return (_f.valid) ? true : false;
}

bool ota_installer_impl::ready_set_for_backup(void)
{
	auto_mutex am(_mut);
	if (!valid()) {
		return false;
	}
	if (_f.state != ota_state_unknown) {
		return false;
	}
	set_state(ota_state_backup_started);
	return true;
}

int ota_installer_impl::backup_partition(
	ota_installer::partition_id id,
	const char* filename)
{
	int ret;
	if (!ready_set_for_backup()) {
		return -ENOTREADY;
	}

	if (ota_installer::partition_unknown == id
		|| !filename || !*filename) {
		reset(); return -EBADPARM;
	}
	string& dir = (id == ota_installer::partition_src)
		? _srcdir : _dstdir;

	string fullpath; // (fullpath)
	if (get_backup_fullpath(filename, fullpath)) {
		reset(); return -EINVALID;
	}
	if (check_file_in_dir(id, fullpath.c_str())) {
		reset(); return -EINVALID;
	}

	// generate the filelist
	filelist* dstflist = load_dir(ota_installer::partition_dst);
	if (nullptr == dstflist) {
		reset(); return -EINVALID;
	}

	ret = generate_backupfile(*dstflist, fullpath);
	set_state(ota_state_backup_finished);
	return ret;
}

int parse_verson(const char* verstr, uint32_t& ver);

int ota_installer_impl::generate_backupfile(filelist& flist, string& fullpath)
{
	// get parameters (compress level)
	int clevel = get_sysconfig(
		"services.fota.backupcfg.compress_level", 9);

	// (block size)
	int block_size = get_sysconfig(
		"services.fota.backupcfg.blocksize", 1024 * 1024);

	// (version)
	const char* verstr = get_sysconfig(
		"services.fota.backupcfg.version", nullptr);
	uint32_t version = 0x1000000;
	if (verstr) {
		parse_verson(verstr, version);
	}

	ota_fullpackage_generator pkgen(flist);
	
	// set parameters
	pkgen.set_compression(clevel);
	pkgen.set_version(version);
	pkgen.set_compression_block_size(block_size);

	return pkgen.generate(fullpath.c_str());
}

int ota_installer_impl::get_backup_fullpath(const char* filename, string& fullpath)
{
	int ret;
	assert(nullptr != filename);

	// check if the filename is path or pure filename
	// if it contains path, we will use it
	if (strchr(filename, '/')) {
		fullpath.assign(filename);
		return 0;
	}
	const char* path = get_sysconfig(
		"services.fota.backupcfg.dir", nullptr, &ret);
	if (ret) return ret;

	// if it is a pure filename, we use path in sysconfig
	if (path) {
		fullpath.assign(path);
		if (fullpath[fullpath.length() - 1] != '/')
			fullpath += "/";
		fullpath += filename;
	} else fullpath.assign(filename);
	return 0;
}

filelist* ota_installer_impl::load_dir(ota_installer::partition_id id)
{
	assert(ota_installer::partition_unknown != id);

	int ret;
	string* dir;
	filelist** flist;
	if (ota_installer::partition_src == id) {
		dir = &_srcdir;
		flist = nullptr;	// todo:
	}
	else {
		dir = &_dstdir;
		flist = &_dstdir_flist;
	}

	if (nullptr == *flist) {
		*flist = new filelist();
		if (nullptr == *flist) return nullptr;
		ret = _dstdir_flist->loaddir(dir->c_str());
		if (ret) {
			delete *flist;
			*flist = nullptr;
			return nullptr;
		}
	}
	return *flist;
}

bool ota_installer_impl::check_file_in_dir(
	ota_installer::partition_id id,
	const char* filename)
{
	string& dir = (id == ota_installer::partition_src)
		? _srcdir : _dstdir;

	if (nullptr == filename) {
		return false;
	}
	const char* end = filename + strlen(filename);
	for (; end != filename && *end != '/'; --end);

	string tmp(filename, end);
	if (tmp.empty()) tmp = ".";
	tmp = canonicalize_filename(tmp.c_str());
	assert(tmp.length());

	return strncmp(tmp.c_str(), dir.c_str(), dir.length())
		? false : true;
}

bool ota_installer_impl::ready_set_for_verify(void)
{
	auto_mutex am(_mut);
	if (!valid()) {
		return false;
	}
	ota_state os = (ota_state)_f.state;
	if (os != ota_state_unknown &&
		os != ota_state_backup_finished) {
		return false;
	}
	set_state(ota_state_verify_started);
	return true;
}

void ota_installer_impl::set_listener(ota_installer_listener* lnr)
{
	auto_mutex am(_mut);
	_lnr = lnr;
	if (lnr) lnr->on_state_changed((ota_state)_f.state);
}

int ota_installer_impl::verify(void)
{
	if (!ready_set_for_verify()) {
		return -ENOTREADY;
	}
	auto type =  otapack_gettype(_otapack.c_str());
	if (type == otaft_unknown) {
		return -EINVALID;
	}
	_f.pkgtype = (uint32_t)type;
	assert(nullptr == _extractor);

	switch (type) {
	case otaft_bundle_package:
		_extractor = new ota_bundle(_otapack.c_str());
		break;

	case otaft_compressed_package:
		_extractor = new ota_compressed_package
		(_otapack.c_str());
		break;

	case otaft_regular_package:
		_extractor = new ota_package(_otapack.c_str());
		break;

	default: break;
	}
	if (nullptr == _extractor) {
		return -ENOMEMORY;
	}

	// do the real verify
	int ret = _extractor->verify();
	if (ret) {
		reset();
		return ret;
	}
	set_state(ota_state_verify_finished);
	return 0;
}

bool ota_installer_impl::ready_set_for_update(void)
{
	auto_mutex am(_mut);
	if (!valid()) {
		return false;
	}
	ota_state os = (ota_state)_f.state;
	if (os != ota_state_verify_finished) {
		return false;
	}
	set_state(ota_state_update_started);
	return true;
}

int ota_installer_impl::update(void)
{
	if (!ready_set_for_update()) {
		return -ENOTREADY;
	}
	if (nullptr == _extractor) {
		return -EINVALID;
	}
	int ret = _extractor->update(
		_srcdir.c_str(),
		_dstdir.c_str());
	if (ret) {
		reset();
		return ret;
	}
	set_state(ota_state_update_finished);
	return 0;
}

ota_installer::ota_installer(const char* srcdir,
	const char* dstdir, const char* otapack)
: _data(nullptr)
{
	auto* impl = new ota_installer_impl(srcdir,
		dstdir, otapack);
	if (nullptr == impl) return;
	if (!impl->valid()) {
		delete impl;
		return;
	}
	_data = reinterpret_cast<void*>(impl);
}

ota_installer::~ota_installer()
{
	auto* impl = reinterpret_cast<ota_installer_impl*>(_data);
	if (impl) {
		delete impl;
		_data = nullptr;
	}
}

int ota_installer::backup_partition(ota_installer::partition_id id,
	const char* filename)
{
	auto* impl = reinterpret_cast<ota_installer_impl*>(_data);
	if (nullptr == impl) {
		return -EINVALID;
	}
	return impl->backup_partition(id, filename);
}

int ota_installer::verify(void)
{
	auto* impl = reinterpret_cast<ota_installer_impl*>(_data);
	if (nullptr == impl) {
		return -EINVALID;
	}
	return impl->verify();
}

int ota_installer::set_listener(ota_installer_listener* lnr)
{
	auto* impl = reinterpret_cast<ota_installer_impl*>(_data);
	if (nullptr == impl) {
		return -EINVALID;
	}
	impl->set_listener(lnr);
	return 0;	
}

int ota_installer::update(void)
{
	auto* impl = reinterpret_cast<ota_installer_impl*>(_data);
	if (nullptr == impl) {
		return -EINVALID;
	}
	return impl->update();
}

}} // end of namespace zas::mware
/* EOF */
