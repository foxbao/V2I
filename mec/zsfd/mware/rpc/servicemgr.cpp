
#include "rpc/rpcservice.h"
#include "inc/servicemgr.h"

#include "std/zasbsc.h"
#include "std/list.h"
#include "utils/avltree.h"
#include "utils/dir.h"
#include "utils/mutex.h"
#include "utils/cert.h"
#include "utils/json.h"

namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;

static const char* service_magic = "zas-service-config-file";

service_impl::service_impl(const char* pkgname,
	const char* servicename,
	const char* instname,
	const char* executive,
	const char* ipaddr, uint32_t port,
	uint32_t version, uint32_t attributes)
: service_info(pkgname, servicename, instname, executive,
	ipaddr, port, version, attributes)
{
	_instcnt = 0;
	_refcnt = 1;
}

int service_impl::addref(void)
{
	return __sync_add_and_fetch(&_refcnt, 1);
}

int service_impl::release(void)
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt < 1)
		delete this;
	return cnt;
}

service_impl::~service_impl()
{
}

int service_impl::add_instance(const char* instname)
{
	auto_mutex am(_mut);
	if (!instname || !*instname) return -EBADPARM;

	if (!is_default_service())
		return -ENOTAVAIL;

	if (!check_multi_instance())
		return -ENOTALLOWED;

	if (find_inst_unlock(instname)) return -EEXISTS;
	service_impl* inst = new service_impl(_pkg_name.c_str(),
		_service_name.c_str(), instname, _executive.c_str(),
		_ipaddr.c_str(), _port,
		_version, _flags);

	if (add_instance_unlock(inst)) {
		delete inst;
		return -ELOGIC;
	}
	_instcnt++;
	return 0;
}

int service_impl::remove_instance(const char* instname)
{
	auto_mutex am(_mut);
	int ret = remove_instance_unlock(instname);
	if (ret) return ret;
	_instcnt--;
	return 0;
}


service_mgr_impl::service_mgr_impl()
: _service_cnt(0)
{

}

service_mgr_impl::~service_mgr_impl()
{
	release_all_service();
}

// servicemgr is only for launch used
// inst_name is no used in add service
service_impl* service_mgr_impl::add_service(const char* pkgname,
	const char* servicename, const char* inst_name,
	const char* executive,
	const char* ipaddr, uint32_t port,
	uint32_t version, uint32_t attributes)
{
	auto_mutex am(_mut);

	if (find_service_unlock(pkgname, servicename))
		return NULL;

	auto* impl = new service_impl(pkgname,
		servicename, NULL, executive, ipaddr, port, version, attributes);

	if (add_service_unlock(impl)) {
		delete impl; return NULL;
	}
	_service_cnt++;
	return impl;
}

int service_mgr_impl::remove_service(const char* pkgname,
	const char* servicename)
{
	auto_mutex am(_mut);

	if (remove_service_unlock(pkgname, servicename))
		return -ENOTEXISTS;
	_service_cnt--;
	return 0;
}

service_impl* service_mgr_impl::get_service(const char* pkgname,
	const char* servicename)
{
	auto_mutex am(_mut);
	auto* info = find_service_unlock(pkgname, servicename);
	if (!info) return NULL;
	return zas_downcast(service_info, service_impl, info);
}

int service_mgr_impl::save_config_file(const uri& binfile)
{
	if (fileexists(binfile.get_fullpath().c_str())) {
		deletefile(binfile.get_fullpath().c_str());
	}
	return save_service_bin(binfile);
}

int service_mgr_impl::save_service_bin(const uri& binfile)
{
	auto_mutex am(_mut);
	service_config_header header;
	int ret = 0;
	_str_sect.reset();

	absfile* file = absfile_open(binfile, "wb+");
	if (NULL == file) return -ENOTALLOWED;

	if (write_services_header(file, header)) {
		ret = 1; goto error;
	}

	if (write_services_info(file, header)) {
		ret = 2; goto error;
	}

	if (write_service_string_section(file, header)) {
		ret = 3; goto error;
	}

	if (rewrite_header(file, header)) {
		ret = 4; goto error;
	}
	file->release();
	return ret;
error:
	file->release();
	deletefile(binfile.get_fullpath().c_str());
	return ret;
}

int service_mgr_impl::write_services_header(absfile* file,
	service_config_header &hdr)
{
	assert(NULL != file);
	memset(&hdr, 0, sizeof(hdr));
	memcpy(hdr.magic, service_magic, sizeof(hdr.magic));
	hdr.service_count = _service_cnt;
	file->seek(0, absfile_seek_set);
	file->write(&hdr, sizeof(hdr));
	return 0;
}

int service_mgr_impl::write_services_info(absfile* file,
	service_config_header &hdr)
{
	assert(NULL != file);
	listnode_t *nd = _service_info_list.next;
	service_impl* impl = NULL;
	service_entry entry;
	uint32_t delta;
	hdr.instance_count = 0;
	hdr.instance_start = sizeof(hdr) + hdr.service_count * sizeof(entry);
	int hcnt = 0;
	for(; nd != &_service_info_list; nd = nd->next)
	{
		impl = LIST_ENTRY(service_impl, ownerlist, nd);
		//write pkgname & host name
		if (!_str_sect.alloc_copy_string(impl->_pkg_name.c_str(), delta)) {
			return -1;
		}
		entry.pkgname = delta;
		if (!_str_sect.alloc_copy_string(impl->_service_name.c_str(), delta)) {
			return -2;
		}
		entry.servicename = delta;
		if (!_str_sect.alloc_copy_string(impl->_executive.c_str(), delta)) {
			return -3;
		}
		entry.executive = delta;
		entry.ipaddr = 0;
		if (impl->_ipaddr.length() > 0) {
			if (!_str_sect.alloc_copy_string(impl->_ipaddr.c_str(), delta)) {
				return -3;
			}
			entry.ipaddr = delta;
		}
		entry.port = impl->_port;
		// wirte version & instance count
		entry.version = impl->_version;
		entry.flag = impl->_flags;
		entry.instcnt = impl->_instcnt;
		if (entry.instcnt < 1) {
			entry.instoffset = 0;
		} else {			
			entry.instoffset = hdr.instance_count * sizeof(uint32_t);
		}
		file->seek(sizeof(hdr) + hcnt * sizeof(entry), absfile_seek_set);
		file->write(&entry, sizeof(entry));
		hcnt++;
		if (entry.instcnt < 1) continue;

		// write instance info
		int inscnt = write_instance_info(file, 
			hdr.instance_start + entry.instoffset,
			impl->_inst_list);
		if (inscnt < 0 || inscnt != impl->_instcnt)
			return -3;
		hdr.instance_count += inscnt;
	}
	return 0;
}

int service_mgr_impl::write_instance_info(absfile* file,
	uint32_t offset, listnode_t &list)
{
	assert(NULL != file);
	listnode_t *instnd = list.next;
	service_impl* instimpl = NULL;
	uint32_t delta;
	int incnt = 0;
	for(;instnd != &list; instnd = instnd->next)
	{
		instimpl = LIST_ENTRY(service_impl, ownerlist, instnd);
		if (!_str_sect.alloc_copy_string(
				instimpl->_inst_name.c_str(), delta)) {
			return -1;
		}
		file->seek(offset + incnt * sizeof(uint32_t),
			absfile_seek_set);
		file->write(&delta, sizeof(delta));
		incnt++;
	}
	return incnt;
}

int service_mgr_impl::write_service_string_section(absfile* file,
	service_config_header& hdr)
{
	assert(NULL != file);
	size_t sz = _str_sect.getsize();
	if (sz) {
		void* buf = _str_sect.getbuf();
		assert(NULL != buf);
		file->seek(0, absfile_seek_end);
		hdr.start_of_string_section = (uint32_t)file->getpos();
		hdr.string_size = sz;
		file->write(buf, sz);
		// udate the file header
		file->rewind();
		file->write(&hdr, sizeof(hdr));
	}
	return 0;
}

int service_mgr_impl::rewrite_header(absfile* file,
	service_config_header& hdr)
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
	file->seek(zas_offsetof(service_config_header, size)
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

int service_mgr_impl::load_config_file(const uri& binfile)
{
	if (!fileexists(binfile.get_fullpath().c_str()))
		return -ENOTEXISTS;
	return load_service_config(binfile);
}

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

int service_mgr_impl::load_manifest_file(const uri& binfile)
{
	auto_mutex am(_mut);

	jsonobject& config = json::loadfromfile(binfile);
	load_manifest_service(config);
	config.release();
	return 0;
}

int service_mgr_impl::load_manifest_service(jsonobject& config)
{
	jsonobject& jservices = config["services"];
	const jsonobject& jpackage = config["package"];
	if (!jpackage.is_string()) {
		return 0;
	}
	const char* pkgname = jpackage.to_string();
	// if there is any service description
	if (jservices.is_null()) {
		return 0;
	}
	// must be an array
	if (!jservices.is_array()) {
		return -1;
	}
	int cnt = jservices.count();
	if (!cnt) {
		return 0;
	}

	for (int i = 0; i < cnt; ++i)
	{
		size_t value;
		jsonobject& node = jservices[i];
		if (!node.is_object()) continue;

		// name - mandatory
		const jsonobject& jname = node["name"];
		if (!jname.is_string()) continue;
		// check if the name exists
		const char* name = jname.to_string();

		// version - mandatory
		uint32_t version_val;
		const jsonobject& jversion = node["version"];
		if (!jversion.is_string()) continue;
		if (parse_verson(jversion.to_string(), version_val))
			continue;
		if (find_service_unlock(pkgname, name))
			return -EEXIST;

		auto* impl = new service_impl(pkgname, name, nullptr, nullptr,
			nullptr, 0, version_val, 0);

		if (parse_service_attributes(node, impl))
			continue;

		const jsonobject& jexecutive = node["executive"];
		if (!jexecutive.is_string()) {
			if (impl->_f.global_accessible || !jexecutive.is_null())
				continue;
		}
		impl->_executive = jexecutive.to_string();

		const jsonobject& jipaddr = node["ipaddr"];
		if ((!jipaddr.is_null()) && jipaddr.is_string()) {
			impl->_ipaddr = jipaddr.to_string();
		}

		jsonobject& jipport = node["port"];
		if ((!jipport.is_null()) && jipport.is_number()) {
			impl->_port = jipport.to_number();
		}

		if (add_service_unlock(impl)) {
			delete impl; return -ELOGIC;
		}
		_service_cnt++;
		// executive
		// if "global_accessible", then executive is mandatory
		// otherwise, executive is optional
		const jsonobject& jinstname = node["naming"];
		if (jinstname.is_null()) 
			continue;
		
		if (!jinstname.is_array())
			continue;

		int instcnt = jinstname.count();
		for (int j = 0; j < instcnt; j++) {
			const jsonobject& instnode = jinstname[j];
			if (!instnode.is_string())
				continue;
			impl->add_instance(instnode.to_string());
		}
	}
	return 0;
}

int service_mgr_impl::parse_service_attributes(const jsonobject& node, service_impl *impl)
{
	const jsonobject& jattr = node["attributes"];
	impl->_flags = 0;
	if (!jattr.is_object()) {
		if (!jattr.is_null()) return -1;
		// the "attributes" object not exists
		// use default values
		impl->_f.global_accessible = 0;
		impl->_f.shared = 1;
		impl->_f.singleton = singleton_type_device_only;
		return 0;
	}

	// global_accessible: optional
	const jsonobject& jgblacs = jattr["global_accessible"];
	if (!jgblacs.is_bool()) {
		if (!jgblacs.is_null()) return -2;
		// use default: false
		impl->_f.global_accessible = 0;
	}
	else {
		int val = jgblacs.to_bool() ? 1 : 0;
		impl->_f.global_accessible = val;
	}

	// apartment: optional
	const jsonobject& japartment = jattr["apartment"];
	if (!japartment.is_string()) {
		if (!japartment.is_null()) return -3;
		// use default: shared
		impl->_f.shared = 1;
	}
	else {
		const char* val = japartment.to_string();
		if (!strcmp(val, "shared"))
			impl->_f.shared = 1;
		else if (!strcmp(val, "unshared"))
			impl->_f.shared = 0;
		else return -4;
	}

	// singleton: optional
	const jsonobject& jsingleton = jattr["singleton"];
	if (!jsingleton.is_string()) {
		if (!jsingleton.is_null()) return -5;
		// use default: device-only
		impl->_f.singleton = singleton_type_device_only;
	}
	else {
		const char* val = jsingleton.to_string();
		if (!strcmp(val, "none"))
			impl->_f.singleton = singleton_type_none;
		else if (!strcmp(val, "globally"))
			impl->_f.singleton = singleton_type_globally;
		else if (!strcmp(val, "device-only"))
			impl->_f.singleton = singleton_type_device_only;
		else return -6;
	}
	return 0;
}

int service_mgr_impl::load_service_config(const uri& binfile)
{
	if (_service_cnt > 0) return -EEXISTS;
	return load_rpc_service_config(binfile, this);
}

int service_mgr_impl::release_all_service()
{
	auto_mutex am(_mut);
	release_all_service_unlock();
	_service_cnt = 0;
	return 0;
}

int service_object::addref(void)
{
	service_impl* impl = reinterpret_cast<service_impl*>(this);
	if (!impl) return -ENOTAVAIL;
	return impl->addref();
}

int service_object::release(void)
{
	service_impl* impl = reinterpret_cast<service_impl*>(this);
	if (!impl) return -ENOTAVAIL;
	return impl->release();
}

int service_object::add_instance(const char* instname)
{
	service_impl* impl = reinterpret_cast<service_impl*>(this);
	if (!impl) return -ENOTAVAIL;
	return impl->add_instance(instname);
}

int service_object::remove_instance(const char* instname)
{
	service_impl* impl = reinterpret_cast<service_impl*>(this);
	if (!impl) return -ENOTAVAIL;
	return impl->remove_instance(instname);
}

service_mgr::service_mgr()
: _data(NULL)
{
}

service_mgr::~service_mgr()
{
	service_mgr_impl* smgr = reinterpret_cast<service_mgr_impl*>(_data);
	if (smgr) delete smgr;
}

int service_mgr::load_config_file(uri &file)
{
	if (NULL == _data) {
		service_mgr_impl* mgrimpl = new service_mgr_impl();
		_data = (void*) mgrimpl;
	}
	service_mgr_impl* smgr = reinterpret_cast<service_mgr_impl*>(_data);
	return smgr->load_config_file(file);
}

int service_mgr::load_manifest_file(uri &file)
{
	if (NULL == _data) {
		service_mgr_impl* mgrimpl = new service_mgr_impl();
		_data = (void*) mgrimpl;
	}
	service_mgr_impl* smgr = reinterpret_cast<service_mgr_impl*>(_data);
	return smgr->load_manifest_file(file);
}

int service_mgr::save_config_file(uri &file)
{
	if (NULL == _data) return -EINVALID;
	service_mgr_impl* smgr = reinterpret_cast<service_mgr_impl*>(_data);
	return smgr->save_config_file(file);
}

service service_mgr::add_service(const char* pkgname, const char* servicename,
	const char* executive, 
	const char* ipaddr, uint32_t port,
	uint32_t version, uint32_t attributes)
{
	service svc;
	if (NULL == _data) return svc;
	service_mgr_impl* smgr = reinterpret_cast<service_mgr_impl*>(_data);
	service_impl* impl = smgr->add_service(pkgname, servicename, NULL,
		executive, ipaddr, port, version, attributes);
	impl->addref();
	svc.set(reinterpret_cast<service_object*>(impl));
	return svc;
}

int service_mgr::remove_service(const char* pkgname, const char* servicename)
{
	if (NULL == _data) return -EINVALID;
	service_mgr_impl* smgr = reinterpret_cast<service_mgr_impl*>(_data);
	return smgr->remove_service(pkgname, servicename);
}

service service_mgr::get_service(const char* pkgname, const char* servicename)
{
	service svc;
	if (NULL == _data) return svc;
	service_mgr_impl* smgr = reinterpret_cast<service_mgr_impl*>(_data);
	service_impl* impl = smgr->get_service(pkgname, servicename);
	impl->addref();
	svc.set(reinterpret_cast<service_object*>(impl));
	return svc;
}

}}}
