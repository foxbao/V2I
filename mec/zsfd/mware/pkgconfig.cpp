/** @file pkgconfig.cpp
 * implementation of the load and access of pakcage configure file
 */

#include <string>

#include "utils/dir.h"
#include "utils/uri.h"
#include "utils/json.h"
#include "utils/cert.h"
#include "utils/shmem.h"
#include "utils/absfile.h"
#include "utils/cmdline.h"
#include "mware/pkgconfig.h"
#include "inc/strsect.h"

namespace zas {
namespace mware {

using namespace std;
using namespace zas::utils;

static const uint32_t pkgname_maxlen = 200;
static const uint32_t pkgcfg_verson = 0x01000000;
static const uint32_t pkgcfg_nullstr = UINT32_MAX;
static const char* pkgcfg_magic = "zas-package-config-file";

int parse_verson(const char* verstr, uint32_t& ver)
{
	// check validity
	const char *p, *c = verstr;
	for (; *c && (*c == '.' || (*c >= '0' && *c <= '9')); ++c);
	if (*c) return -1; // un-allow charactors encountered

	uint32_t i = 0;
	uint32_t result = 0;
	for (p = c = verstr; *p; ++i, ++c)
	{
		p = c;
		c = strchr(c, '.');
		if (!c) c = p + strlen(p);

		if (p == c) return -2;
		// convert to number
		uint32_t n = 0;
		for (; p != c; ++p) {
			n = n * 10 + (*p - '0');
			if (n > 255) return -3; // exceed maximum number
		}
		result <<= 8, result |= n;
	}

	// check if there is too many minors
	// like: 1.01.03.04.05 is invalid because
	// only a.b.c.d is allowed
	if (i > 4) return -4;
	result <<= (4 - i) * 8;
	ver = result;
	return 0;
}

/* note: all the field in this header shall not changed */
struct pkg_config_header
{
	// the length of magic is fixed and
	// shall never change
	char magic[24];

	// calculated encrypted sha256 digest of the file
	// note: must leave "digest" blank when calculating
	// the sha256 digest
	uint8_t digest[32];

	// the total file size
	uint32_t size;						/* 60 */
	uint32_t version;					/* 64 */

	// how many services are there in this config file
	uint32_t service_count;				/* 68 */
	uint32_t start_of_inst;				/* 72 */
	uint32_t start_of_string_section;	/* 76 */
} PACKED;

enum pkg_config_flags
{
	pkgcfg_flag_private = 1,
};

/* note: all the field in this header shall not changed */
struct pkg_essential_config
{
	uint32_t package_name;				/* 76 */
	uint32_t package_version;			/* 80 */
	uint32_t flags;						/* 84 */
	uint32_t api_level;					/* 88 */
} PACKED;

struct pkg_service
{
	uint32_t name;
	uint32_t version;
	uint32_t executive;
	uint32_t ipaddr;
	uint32_t port;
	uint32_t inst_index;
	uint32_t inst_count;
	union {
		uint32_t flags;
		struct {
			uint32_t global_accessible : 1;

			// if the service could share apartment
			// with other services
			uint32_t shared : 1;

			// the singleton type
			uint32_t singleton : 2;
		} f;
	};
} PACKED;

class pkgcfg_service_info_impl
{
public:
	pkgcfg_service_info_impl(pkg_service* service, 
		uint8_t* stringstart, uint32_t* inststart)
	: _service(service), _string_start(stringstart)
	, _inst_start(inststart), _refcnt(1){
	}

	int addref(void) {
		return __sync_add_and_fetch(&_refcnt, 1);
	}

	int release(void) {
		int cnt = __sync_sub_and_fetch(&_refcnt, 1);
		if (cnt < 1) delete this;
		return cnt;
	}

	const char* get_name(void) {
		assert(NULL != _service);
		assert(NULL != _string_start);
		return (const char*)(_string_start + _service->name);
	}

	uint32_t get_version(void) {
		assert(NULL != _service);
		return _service->version;
	}

	const char* get_executive(void) {
		assert(NULL != _service);
		assert(NULL != _string_start);
		return (const char*)(_string_start + _service->executive);
	}

	const char* get_ipaddr(void) {
		assert(NULL != _service);
		assert(NULL != _string_start);
		if (!_service->ipaddr) {
			return nullptr;
		}
		return (const char*)(_string_start + _service->ipaddr);
	}

	uint32_t get_port(void) {
		assert(NULL != _service);
		return _service->port;
	}

	uint32_t get_attributes(void) {
		assert(NULL != _service);
		return _service->flags;
	}

	singleton_type get_singleton(void) {
		assert(NULL != _service);
		return (singleton_type)_service->f.singleton;
	}

	bool is_shared(void) {
		assert(NULL != _service);
		return (1 == _service->f.shared) ? true : false;
	}

	uint32_t get_instance_count(void) {
		assert(NULL != _service);
		return _service->inst_count;
	}

	const char* get_instance_by_index(uint32_t index) {
		assert(NULL != _service);
		assert(NULL != _string_start);
		assert(NULL != _inst_start);

		if (index >= _service->inst_count)
			return NULL;
			
		uint32_t* inst_index = (uint32_t*)(_inst_start
			+ _service->inst_index + index);
		return (const char*)(_string_start + *inst_index);
	}

	bool check_instance(const char* inst_name) {
		assert(NULL != _service);
		assert(NULL != _string_start);
		assert(NULL != _inst_start);

		if (_service->inst_count < 1) return false;

		uint32_t* inst_index;
		for (int i = 0; i < _service->inst_count; i++) {
			inst_index = (uint32_t*)(_inst_start
				+ _service->inst_index + i);
			if (!strcmp((const char*)(_string_start + *inst_index)
				, inst_name)) {
				return true;
			}	
		}
		return false;
	}
private:
	pkg_service* _service;
	uint8_t*	_string_start;
	uint32_t*	_inst_start;
	int	_refcnt;
};

class pkg_config_impl
{
public:
	const void* _pkgcfg_mem_addr = (void*)CONFIG_MAPPING_BASE;
	
	pkg_config_impl(const uri& cfgfile)
	: _shmem(NULL), _header(NULL) {
		load_file(cfgfile);
	}

	pkg_config_impl()
	: _shmem(NULL), _header(NULL) {
		attach_shared_memory();
	}

	~pkg_config_impl()
	{
		// release the share memory object
		// if necessary
		if (_shmem) {
			_shmem->release();
			_shmem = NULL;
		}
		else if (_header) {
			delete [] (uint8_t*)_header;
		}
		_header = NULL;
	}

	bool loaded(void) {
		return (_header) ? true : false;
	}

	const char* get_package_name(void){
		if (!_header) return NULL;
		pkg_essential_config* pkgcfg =
			reinterpret_cast<pkg_essential_config*>(_header + 1);
		return (const char*)(get_start_string() + pkgcfg->package_name);
	}

	uint32_t get_version(void) {
		if (!_header) return 0;
		pkg_essential_config* pkgcfg =
			reinterpret_cast<pkg_essential_config*>(_header + 1);
		return pkgcfg->package_version;
	}

	uint32_t get_api_level(void) {
		if (!_header) return 0;
		pkg_essential_config* pkgcfg =
			reinterpret_cast<pkg_essential_config*>(_header + 1);
		return pkgcfg->api_level;
	}

	uint32_t get_access_permission(void) {
		if (!_header) return 0;
		pkg_essential_config* pkgcfg =
			reinterpret_cast<pkg_essential_config*>(_header + 1);
		return pkgcfg->flags;
	}

	bool check_service(const char* name) {
		if (!name || !*name || !_header) return false;
		uint32_t svccnt = _header->service_count;
		pkg_essential_config* pkgcfg =
		reinterpret_cast<pkg_essential_config*>(_header + 1);
		//get services list
		pkg_service* svclist = reinterpret_cast<pkg_service*>(pkgcfg + 1);
		pkg_service* svritem = NULL;
		for (int i = 0; i < svccnt; i++) {
			svritem = svclist + i;
			const char* svcname = (const char*)
				(get_start_string() + svritem->name);
			if (!strcmp(svcname, name)) return true;
		}
		return false;
	}

	uint32_t get_service_count(void) {
		if (!_header) return 0;
		return _header->service_count;
	}

	pkg_service* get_service_by_name(const char* name) {
		if (!name || !*name || !_header) return NULL;
		uint32_t svccnt = _header->service_count;
		pkg_essential_config* pkgcfg =
		reinterpret_cast<pkg_essential_config*>(_header + 1);
		//get services list
		pkg_service* svclist = reinterpret_cast<pkg_service*>(pkgcfg + 1);
		pkg_service* svritem = NULL;
		for (int i = 0; i < svccnt; i++) {
			svritem = svclist + i;
			const char* svcname = (const char*)
				(get_start_string() + svritem->name);
			if (!strcmp(svcname, name)) return svritem;
		}
		return NULL;
	}

	pkg_service* get_service_by_index(uint32_t index) {

		if (!_header) return NULL;

		if (index >= _header->service_count)
			return NULL;

		pkg_essential_config* pkgcfg =
		reinterpret_cast<pkg_essential_config*>(_header + 1);
		//get services list
		pkg_service* svclist = reinterpret_cast<pkg_service*>(pkgcfg + 1);
		return (svclist + index);
	}

	uint8_t* get_start_string(void) {
		if (!_header) return NULL;
		return (((uint8_t*)_header) + _header->start_of_string_section);
	}

	uint32_t* get_start_index(void) {
		if (!_header) return NULL;
		return (uint32_t*)(((uint8_t*)_header) + _header->start_of_inst);
	}
private:

	int get_launch_args(uri& u)
	{
		int c;
		auto* cmd = cmdline::inst();
		cmd->reset();
		while ((c = cmd->getopt("l:")) != -1)
		{
			if (c == 'l') {
				u = cmd->get_option_arg();
				return 0;
			}
		}
		return -1;
	}

	int attach_shared_memory(void)
	{
		uri u;
		if (get_launch_args(u)) {
			return -1;
		}
		string client_name = u.get_fullpath();
		if (client_name.length() > pkgname_maxlen) {
			return -2;
		}

		// attache to the package config share memory area
		// that has been prepared by "launch"
		client_name += ".pkgcfg.area";

		// we map it as "read only" memory area
		// in a fixed address
		// we try a temporary map to get the real size
		auto* tmp = shared_memory::create(client_name.c_str(),
			ZAS_PAGESZ, shmem_read,
			(void*)_pkgcfg_mem_addr);

		if (NULL == tmp) {
			return -3;
		}
		if (!tmp->getaddr() || tmp->is_creator()) {
			tmp->release(); return -4;
		}

		// check if the size exceeds 1 page size
		// if so, we need to remap the area
		auto* hdr = reinterpret_cast<pkg_config_header*>(tmp->getaddr());

		// verify if the header is valid
		if (memcmp(hdr->magic, pkgcfg_magic, sizeof(hdr->magic))) {
			tmp->release(); return -5;
		}

		assert(hdr->size > 0);
		if (hdr->size > ZAS_PAGESZ) {
			tmp->release();
			size_t sz = hdr->size;
			sz = (sz + ZAS_PAGESZ - 1) & ~(ZAS_PAGESZ - 1);

			_shmem = shared_memory::create(client_name.c_str(),
				sz, shmem_read,
				(void*)_pkgcfg_mem_addr);
			
			assert(NULL != _shmem);
			assert(_shmem->getaddr() && !_shmem->is_creator());
		}
		else _shmem = tmp;

		// set the header
		_header = (pkg_config_header*)_shmem->getaddr();

		// verify the digest, to make sure the config
		// file has not been illegally changed
		int ret = verify();
		if (ret) {
			// verify failed
			_shmem->release();
			_shmem = NULL;
			_header = NULL;
		}
		return ret;
	}

	int load_file(const uri& file)
	{
		absfile* f = absfile_open(file, "rb");
		if (!f) return -1;

		// get the length of the file
		f->seek(0, absfile_seek_end);
		long length = f->getpos();
		if (length <= 0) { f->release(); return -2; }

		// allocate memory
		uint8_t* buf = new uint8_t [length];
		if (!buf) { f->release(); return  -ENOMEMORY; }
		
		// load file to memory
		f->rewind();
		if ((size_t)length != f->read(buf, (size_t)length)) {
			f->release(); return -3;
		}
		_header = (pkg_config_header*)buf;
		f->release();

		// quick verify the file size
		if ((long)_header->size != length) {
			delete [] buf;
			_header = NULL;
			return -4;
		}

		// verify the digest, to make sure the config
		// file has not been illegally changed
		int ret = verify();
		if (ret) {
			// verify failed
			delete [] buf;
			_header = NULL;
			return -5;
		}
		return 0;
	}

	int verify(void)
	{
		if (NULL == _header) {
			return -1;
		}

		// check magic
		if (memcmp(_header->magic, pkgcfg_magic,
			sizeof(_header->magic))) {
			return -2;
		}

		// check version, must be not greater than
		// the current system
		if (_header->version > pkgcfg_verson) {
			return -3;
		}

		// verify the digest, to make sure the config
		// file has not been illegally changed
		if (verify_digest()) return -ECORRUPTTED;
		return 0;
	}

	int verify_digest(void)
	{
		assert(NULL != _header);

		digest dgst("sha256");

		// make sure the file size is larger than the
		// magic size + digest size
		assert(_header->size > sizeof(_header->magic) \
			+ sizeof(_header->digest));

		dgst.append((void*)&_header->size,
			_header->size - sizeof(_header->magic)
			- sizeof(_header->digest));

		size_t sz;
		const uint8_t* result = dgst.getresult(&sz);
		assert(result && sz == sizeof(_header->digest));
		
		if (memcmp(&_header->digest, result, sz)) {
			return -1;
		}
		return 0;
	}

private:	
	shared_memory* _shmem;
	pkg_config_header* _header;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(pkg_config_impl);
};

pkgconfig::pkgconfig()
: _config(NULL)
{
}

pkgconfig::~pkgconfig()
{
	if (_config) {
		delete reinterpret_cast<pkg_config_impl*>(_config);
		_config = NULL;
	}
}

pkgconfig::pkgconfig(const uri& file)
{
	auto* cfg = new pkg_config_impl(file);
	assert(NULL != cfg);
	_config = (void*)cfg;
}

const pkgconfig& pkgconfig::getdefault(void)
{
	static pkgconfig proc_default;
	if (!proc_default._config) {
		proc_default._config = reinterpret_cast<void*>(
			new pkg_config_impl());
		assert(NULL != proc_default._config);
	}
	return proc_default;
}
const char* pkgconfig::get_package_name(void) const
{
	pkg_config_impl* cfg = reinterpret_cast<pkg_config_impl*>(_config);
	if (NULL == cfg) return NULL;
	return cfg->get_package_name();
}

uint32_t pkgconfig::get_version(void) const
{
	pkg_config_impl* cfg = reinterpret_cast<pkg_config_impl*>(_config);
	if (NULL == cfg) return 0;
	return cfg->get_version();	
}

uint32_t pkgconfig::get_api_level(void) const
{
	pkg_config_impl* cfg = reinterpret_cast<pkg_config_impl*>(_config);
	if (NULL == cfg) return 0;
	return cfg->get_api_level();
}

uint32_t pkgconfig::get_access_permission(void) const
{
	pkg_config_impl* cfg = reinterpret_cast<pkg_config_impl*>(_config);
	if (NULL == cfg) return 0;
	return cfg->get_access_permission();
}

bool pkgconfig::check_service(const char* name) const
{
	pkg_config_impl* cfg = reinterpret_cast<pkg_config_impl*>(_config);
	if (NULL == cfg) return false;
	return cfg->check_service(name);
}

uint32_t pkgconfig::get_service_count(void) const
{
	pkg_config_impl* cfg = reinterpret_cast<pkg_config_impl*>(_config);
	if (NULL == cfg) return 0;
	return cfg->get_service_count();
}

pkgcfg_service_info pkgconfig::get_service_by_name(const char* name) const
{
	pkg_config_impl* cfg = reinterpret_cast<pkg_config_impl*>(_config);
	pkgcfg_service_info info;
	if (NULL == cfg) return info;
	pkg_service* service = cfg->get_service_by_name(name);
	if (NULL == service) return info;
	pkgcfg_service_info_impl *impl = new pkgcfg_service_info_impl(service,
		cfg->get_start_string(), cfg->get_start_index());
	info.set(reinterpret_cast<pkgcfg_service_info_object*>(impl));
	return info;	
}

pkgcfg_service_info pkgconfig::get_service_by_index(uint32_t index) const
{
	pkg_config_impl* cfg = reinterpret_cast<pkg_config_impl*>(_config);
	pkgcfg_service_info info;
	if (NULL == cfg) return info;
	pkg_service* service = cfg->get_service_by_index(index);
	if (NULL == service) return info;
	pkgcfg_service_info_impl *impl = new pkgcfg_service_info_impl(service,
		cfg->get_start_string(), cfg->get_start_index());
	info.set(reinterpret_cast<pkgcfg_service_info_object*>(impl));
	return info;	
}

int pkgcfg_service_info_object::addref(void)
{
	pkgcfg_service_info_impl* service = 
		reinterpret_cast<pkgcfg_service_info_impl*>(this);
	if (NULL == service) return 0;
	return service->addref();
}

int pkgcfg_service_info_object::release(void)
{
	pkgcfg_service_info_impl* service = 
		reinterpret_cast<pkgcfg_service_info_impl*>(this);
	if (NULL == service) return 0;
	return service->release();
}

const char* pkgcfg_service_info_object::get_name(void)
{
	pkgcfg_service_info_impl* service = 
		reinterpret_cast<pkgcfg_service_info_impl*>(this);
	if (NULL == service) return NULL;
	return service->get_name();
}

uint32_t pkgcfg_service_info_object::get_version(void)
{
	pkgcfg_service_info_impl* service = 
		reinterpret_cast<pkgcfg_service_info_impl*>(this);
	if (NULL == service) return 0;
	return service->get_version();
}

uint32_t pkgcfg_service_info_object::get_attributes(void)
{
	pkgcfg_service_info_impl* service = 
		reinterpret_cast<pkgcfg_service_info_impl*>(this);
	if (NULL == service) return 0;
	return service->get_attributes();
}

uint32_t pkgcfg_service_info_object::get_port(void)
{
	pkgcfg_service_info_impl* service = 
		reinterpret_cast<pkgcfg_service_info_impl*>(this);
	if (NULL == service) return 0;
	return service->get_port();
}

singleton_type pkgcfg_service_info_object::get_singleton(void)
{
	pkgcfg_service_info_impl* service =
		reinterpret_cast<pkgcfg_service_info_impl*>(this);
	if (NULL == service) return singleton_type_none;
	return service->get_singleton();
}

bool pkgcfg_service_info_object::is_shared(void)
{
	pkgcfg_service_info_impl* service =
		reinterpret_cast<pkgcfg_service_info_impl*>(this);
	if (NULL == service) return false;
	return service->is_shared();
}

const char* pkgcfg_service_info_object::get_executive(void)
{
	pkgcfg_service_info_impl* service =
		reinterpret_cast<pkgcfg_service_info_impl*>(this);
	if (NULL == service) return NULL;
	return service->get_executive();
}

const char* pkgcfg_service_info_object::get_ipaddr(void)
{
	pkgcfg_service_info_impl* service =
		reinterpret_cast<pkgcfg_service_info_impl*>(this);
	if (NULL == service) return NULL;
	return service->get_ipaddr();
}

uint32_t pkgcfg_service_info_object::get_instance_count(void)
{
	pkgcfg_service_info_impl* service =
		reinterpret_cast<pkgcfg_service_info_impl*>(this);
	if (NULL == service) return 0;
	return service->get_instance_count();
}

const char* pkgcfg_service_info_object::get_instance_by_index(uint32_t index)
{
	pkgcfg_service_info_impl* service =
		reinterpret_cast<pkgcfg_service_info_impl*>(this);
	if (NULL == service) return NULL;
	return service->get_instance_by_index(index);
}

bool pkgcfg_service_info_object::check_instance(const char* inst_name)
{
	pkgcfg_service_info_impl* service =
		reinterpret_cast<pkgcfg_service_info_impl*>(this);
	if (NULL == service) return NULL;
	return service->check_instance(inst_name);
}

class pkgcfg_compiler_impl
{
public:
	pkgcfg_compiler_impl(const uri& cfgfile)
	: _cfg(json::loadfromfile(cfgfile)) {
	}

	~pkgcfg_compiler_impl()
	{
		_cfg.release();
	}

	int save(const uri& binfile)
	{
		int ret;
		pkg_config_header hdr;

		if (_cfg.is_null()) {
			return -EFAILTOLOAD;
		}
		absfile* file = absfile_open(binfile, "wb+");
		if (NULL == file) return -ENOTAVAIL;

		if (write_pkgcfg_header(file, hdr)) {
			ret = 1; goto error;
		}
		if (write_pkgcfg_essentials(file)) {
			ret = 2; goto error;
		}

		// write services list
		if (write_pkgcfg_service_list(file, hdr)) {
			ret = 3; goto error;
		}

		// write the string section
		write_pkgcfg_string_section(file, hdr);

		// rewrite the header
		rewrite_header(file, hdr);
		file->release();
		return 0;

	error:
		// delete the file
		file->release();
		deletefile(binfile.get_fullpath().c_str());
		return ret;
	}

private:

	int write_pkgcfg_string_section(absfile* file,
		pkg_config_header& hdr)
	{
		size_t sz = _str_sect.getsize();
		
		if (sz) {
			void* buf = _str_sect.getbuf();
			assert(NULL != buf);
			hdr.start_of_string_section = (uint32_t)file->getpos();
			file->write(buf, sz);

			// udate the file header
			file->rewind();
			file->write(&hdr, sizeof(hdr));
		}
		return 0;
	}

	int rewrite_header(absfile* file, pkg_config_header& hdr)
	{
		assert(NULL != file);

		// set the file size
		file->seek(0, absfile_seek_end);
		hdr.size = (uint32_t)file->getpos();

		// calculate the digest
		size_t sz;
		char buf[256];
		digest dgst("sha256");

		// using the "modified" version of size
		dgst.append(&hdr.size, sizeof(hdr.size));

		// we already append "size", start from the next field
		file->seek(zas_offsetof(pkg_config_header, size)
			+ sizeof(hdr.size), absfile_seek_set);
		for (;;) {
			sz = file->read(buf, 256);
			if (!sz) break;
			dgst.append((void*)buf, sz);
		}

		// calculate the digest
		const uint8_t* result = dgst.getresult(&sz);
		assert(result && sz == sizeof(hdr.digest));
		memcpy(&hdr.digest, result, sz);

		// write back the header
		file->seek(0, absfile_seek_set);
		file->write(&hdr, sizeof(hdr));
		return 0;
	}

	int write_pkgcfg_header(absfile* file, pkg_config_header& hdr)
	{
		assert(NULL != file);
		memset(&hdr, 0, sizeof(hdr));
		memcpy(hdr.magic, pkgcfg_magic, sizeof(hdr.magic));
		hdr.version = pkgcfg_verson;
		hdr.start_of_string_section = 0;
		file->write(&hdr, sizeof(hdr));
		return 0;
	}

	int parse_service_attributes(const jsonobject& node, pkg_service& item)
	{
		const jsonobject& jattr = node["attributes"];
		item.flags = 0;
		if (!jattr.is_object()) {
			if (!jattr.is_null()) return -1;
			// the "attributes" object not exists
			// use default values
			item.f.global_accessible = 0;
			item.f.shared = 1;
			item.f.singleton = singleton_type_device_only;
			return 0;
		}

		// global_accessible: optional
		const jsonobject& jgblacs = jattr["global_accessible"];
		if (!jgblacs.is_bool()) {
			if (!jgblacs.is_null()) return -2;
			// use default: false
			item.f.global_accessible = 0;
		}
		else {
			int val = jgblacs.to_bool() ? 1 : 0;
			item.f.global_accessible = val;
		}

		// apartment: optional
		const jsonobject& japartment = jattr["apartment"];
		if (!japartment.is_string()) {
			if (!japartment.is_null()) return -3;
			// use default: shared
			item.f.shared = 1;
		}
		else {
			const char* val = japartment.to_string();
			if (!strcmp(val, "shared"))
				item.f.shared = 1;
			else if (!strcmp(val, "unshared"))
				item.f.shared = 0;
			else return -4;
		}

		// singleton: optional
		const jsonobject& jsingleton = jattr["singleton"];
		if (!jsingleton.is_string()) {
			if (!jsingleton.is_null()) return -5;
			// use default: device-only
			item.f.singleton = singleton_type_device_only;
		}
		else {
			const char* val = jsingleton.to_string();
			if (!strcmp(val, "none"))
				item.f.singleton = singleton_type_none;
			else if (!strcmp(val, "globally"))
				item.f.singleton = singleton_type_globally;
			else if (!strcmp(val, "device-only"))
				item.f.singleton = singleton_type_device_only;
			else return -6;
		}
		return 0;
	}

	bool service_check_exists(pkg_config_header& hdr,
		pkg_service* s, const char* name)
	{
		assert(NULL != name);
		for (int i = 0; i < hdr.service_count; ++i) {
			const char* str = _str_sect.to_string(s[i].name);
			assert(NULL != str);
			if (!strcmp(str, name)) return true;
		}
		return false;
	}

	int write_pkgcfg_service_list(absfile* file, pkg_config_header& hdr)
	{
		assert(NULL != file);
		jsonobject& jservices = _cfg["services"];

		// if there is any service description
		if (jservices.is_null()) return 0;

		// must be an array
		if (!jservices.is_array()) {
			return -1;
		}
		int cnt = jservices.count();
		if (!cnt) return 0;
		hdr.service_count = 0;
		uint32_t inst_count = 0;

		// allocate the temporary space
		auto* services = (pkg_service*)malloc(sizeof(pkg_service) * cnt);
		int inst_max_count = 64;
		auto* instances = (uint32_t*)malloc(sizeof(uint32_t) * inst_max_count);
		if (NULL == services) return -2;

		for (int i = 0; i < cnt; ++i)
		{
			size_t value;
			pkg_service& item = services[i];
			jsonobject& node = jservices[i];
			if (!node.is_object()) continue;

			// name - mandatory
			const jsonobject& jname = node["name"];
			if (!jname.is_string()) continue;
			// check if the name exists
			const char* name = jname.to_string();
			if (service_check_exists(hdr, services, name))
				continue;
			if (!_str_sect.alloc_copy_string(name, value))
				continue;
			item.name = value;

			// version - mandatory
			uint32_t version_val;
			const jsonobject& jversion = node["version"];
			if (!jversion.is_string()) continue;
			if (parse_verson(jversion.to_string(), version_val))
				continue;
			item.version = version_val;

			if (parse_service_attributes(node, item))
				continue;

			// executive
			// if "global_accessible", then executive is mandatory
			// otherwise, executive is optional
			const jsonobject& jexecutive = node["executive"];
			if (!jexecutive.is_string()) {
				if (item.f.global_accessible || !jexecutive.is_null())
					continue;
			}
			if (!_str_sect.alloc_copy_string(jexecutive.to_string(), value))
				continue;

			item.executive = value;
			item.ipaddr = 0;
			item.port = 0;

			const jsonobject& jipaddr = node["ipaddr"];
			if ((!jipaddr.is_null()) && jipaddr.is_string()) {
				if (_str_sect.alloc_copy_string(jipaddr.to_string(), value)) {
					item.ipaddr = value;
				}
			}
			jsonobject& jipport = node["port"];
			if ((!jipport.is_null()) && jipport.is_number()) {
				item.port = jipport.to_number();
			}

			item.inst_count = 0;
			item.inst_index = 0;
			++hdr.service_count;

			const jsonobject& jinstname = node["naming"];
			if (jinstname.is_null()) 
				continue;
			
			if (!jinstname.is_array())
				continue;

			int instcnt = jinstname.count();
			if (inst_count + instcnt > inst_max_count) {
				inst_max_count += 64;
				instances = (uint32_t*)realloc(instances, inst_max_count);
			}
			item.inst_count = instcnt;
			item.inst_index = inst_count;
			for (int j = 0; j < instcnt; j++) {
				const jsonobject& instnode = jinstname[j];
				if (!instnode.is_string())
					continue;
				if (!_str_sect.alloc_copy_string(instnode.to_string(), value))
					continue;
				instances[inst_count + j] = value;
			}
			inst_count += instcnt;
		}
		if (hdr.service_count) {
			// write service info to file
			file->write(services, sizeof(pkg_service) * hdr.service_count);
		}
		free(services);
		if (inst_count) {
			// write service instance to file
			hdr.start_of_inst = (uint32_t)file->getpos();
			file->write(instances, sizeof(uint32_t) * inst_count);
		}
		free(instances);
		return 0;
	}

	int write_pkgcfg_essentials(absfile* file)
	{
		pkg_essential_config ess;

		if (pkg_essential_get_name(ess))
			return -1;
		if (pkg_essential_get_version(ess))
			return -2;
		if (pkg_essential_get_apilevel(ess))
			return -3;
		if (pkg_essential_get_access_permission(ess))
			return -4;

		file->write(&ess, sizeof(ess));
		return 0;
//	uint32_t package_version;
//	uint32_t flags;
//	uint32_t api_level;
	}

	inline int char_valid(const char c)
	{
		// [0-9][a-z][A-Z][_-.]
		if (c >= '0' && c <= '9') return 0;
		if (c >= 'a' && c <= 'z') return 0;
		if (c >= 'A' && c <= 'Z') return 0;

		const char rsvtbl[] = "_-.";
		if (strchr(rsvtbl, c)) return 0;
		return -1;
	}

	int name_valid(const char* name)
	{
		if (!name || !*name) return -1;

		// check all charactor's validty
		const char* s;
		for (s = name; *s; ++s) {
			if (char_valid(*s)) return -2;
		}

		// check if the package name exceed
		// the max length
		if (s - name > pkgname_maxlen) {
			return -3;
		}

		// first charactor shall not be a
		// number or [.-]
		if (*name >= '0' && *name <= '9') {
			return -4;
		}
		const char rsvtbl[] = ".-";
		return (NULL == strchr(rsvtbl, *name)) ? 0 : -5;
	}

	int pkg_essential_get_name(pkg_essential_config& ess)
	{
		const char* pkg_name = _cfg["package"].to_string();
		if (NULL == pkg_name) return -1;
		
		// check if the name is valid
		if (name_valid(pkg_name)) return -2;

		size_t delta;
		if (!_str_sect.alloc_copy_string(pkg_name, delta)) {
			return -3;
		}
		ess.package_name = delta;
		return 0;
	}

	int pkg_essential_get_version(pkg_essential_config& ess)
	{
		const char* pkg_version = _cfg["version"].to_string();
		if (NULL == pkg_version) return -1;

		uint32_t version;
		if (parse_verson(pkg_version, version))
			return -2;
		ess.package_version = version;
		return 0;
	}
	
	int pkg_essential_get_apilevel(pkg_essential_config& ess)
	{
		jsonobject& apilevel = _cfg["apilevel"];
		if (!apilevel.is_number()) return -1;
		ess.api_level = (uint32_t)apilevel.to_number();
		return 0;
	}

	int pkg_essential_get_access_permission(pkg_essential_config& ess)
	{
		const char* pkg_ap = _cfg["access_permission"].to_string();
		if (NULL == pkg_ap) return -1;

		if (!strcmp(pkg_ap, "private")) {
			ess.flags |= pkgcfg_flag_private;
		}
		else if (strcmp(pkg_ap, "public"))
			return -2;
		return 0;
	}


private:
	jsonobject& _cfg;
	string_section _str_sect;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(pkgcfg_compiler_impl);
};

pkgcfg_compiler::pkgcfg_compiler(const uri& cfgfile)
: _data(NULL) {
	pkgcfg_compiler_impl* compiler = new pkgcfg_compiler_impl(cfgfile);
	assert(NULL != compiler);
	_data = reinterpret_cast<void*>(compiler);
}

pkgcfg_compiler::~pkgcfg_compiler()
{
	pkgcfg_compiler_impl* compiler = reinterpret_cast<pkgcfg_compiler_impl*>(_data);
	if (NULL != compiler) {
		delete compiler; _data = NULL;
	}
}

int pkgcfg_compiler::compile(const uri& tarfile)
{
	pkgcfg_compiler_impl* compiler = reinterpret_cast<pkgcfg_compiler_impl*>(_data);
	if (NULL == compiler) return -ENOTAVAIL;
	return compiler->save(tarfile);
}

}} // end of namespace zas::mware
/* EOF */
