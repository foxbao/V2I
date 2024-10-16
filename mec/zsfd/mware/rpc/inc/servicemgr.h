
#ifndef __CXX_ZAS_RPC_SERVICE_MGR_H__
#define __CXX_ZAS_RPC_SERVICE_MGR_H__

#include <string>
#include "std/list.h"
#include "utils/absfile.h"
#include "utils/avltree.h"
#include "utils/cert.h"
#include "utils/dir.h"
#include "utils/mutex.h"
#include "utils/uri.h"
#include "utils/json.h"

#include "serviceinfo.h"

namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;

/* note: all the field in this header shall not changed */
struct service_config_header
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
	uint32_t instance_count;			/* 72 */
	uint32_t instance_start;			/* 76 */
	uint32_t start_of_string_section;	/* 80 */
	uint32_t string_size;				/* 84 */
} PACKED;

struct service_entry
{
	uint32_t pkgname;
	uint32_t servicename;
	uint32_t executive;
	uint32_t ipaddr;
	uint32_t port;
	uint32_t version;
	uint32_t flag;
	uint32_t instcnt;
	uint32_t instoffset;
} PACKED;

class string_section
{
	const int min_size = 1024;
	
public:
	string_section()
	: _buf(NULL), _total_sz(0)
	, _curr_sz(0) {
	}

	~string_section()
	{
		if (_buf) {
			free(_buf);
			_buf = NULL;
		}
		_total_sz = _curr_sz = 0;
	}

	int reset(void) {
		_curr_sz = 0;
		return 0;
	}

	size_t getsize(void) {
		return _curr_sz;
	}

	void* getbuf(void) {
		return _buf;
	}

	char* alloc(int sz, uint32_t* delta_pos)
	{
		if (sz <= 0) {
			return NULL;
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

	const char* alloc_copy_string(const char* str, uint32_t& pos)
	{
		assert(NULL != str);
		int len = strlen(str) + 1;
		char* ret = alloc(len, &pos);
		if (NULL == ret) return NULL;
		memcpy(ret, str, len);
		return (const char*)ret;
	}

	const char* to_string(uint32_t pos) {
		if (pos >= _curr_sz) return NULL;
		return (const char*)&_buf[pos];
	}

private:
	char* _buf;
	int _total_sz, _curr_sz;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(string_section);
};

class service_impl : public service_info
{
	friend class service_mgr_impl;

public:
	service_impl(const char* pkgname,
	const char* servicename,
	const char* instname,
	const char* executive,
	const char* ipaddr, uint32_t port,
	uint32_t version, uint32_t attributes);
	virtual ~service_impl();

	int addref(void);
	int release(void);
	int add_instance(const char* instname);
	int remove_instance(const char* instname);

private:
	uint32_t _instcnt;
	int _refcnt;
	mutex _mut;
};

class service_mgr_impl : public service_mgr_base
{
public:
	service_mgr_impl();
	virtual ~service_mgr_impl();
	// servicemgr is only for launch used
	// inst_name is no used in add service
	service_impl* add_service(const char* pkgname, const char* servicename,
		const char* inst_name, const char* executive,
		const char* ipaddr, uint32_t port,
		uint32_t version, uint32_t attribute);
	int remove_service(const char* pkgname, const char* servicename);
	service_impl* get_service(const char* pkgname, const char* servicename);
	int load_config_file(const uri& binfile);
	int load_manifest_file(const uri& binfile);
	int save_config_file(const uri& binfile);

private:
	int save_service_bin(const uri& binfile);
	int load_service_config(const uri& binfile);
	int load_manifest_service(jsonobject& node);
	int parse_service_attributes(const jsonobject &node, service_impl *impl);
	static int service_info_compare(avl_node_t* aa, avl_node_t *bb);
	int release_all_service(void);

	int write_services_header(absfile* file, service_config_header &hdr);
	int write_services_info(absfile* file, service_config_header &hdr);
	int write_instance_info(absfile* file, uint32_t offset, listnode_t &list);
	int write_service_string_section(absfile* file, service_config_header& hdr);
	int rewrite_header(absfile* file, service_config_header& hdr);

private:
	string_section _str_sect;
	mutex _mut;
	int 	_service_cnt;
};


template <typename T> 
int load_rpc_service_config(const uri& binfile, T* manager)
{
	if (!manager) return -ENOTAVAIL;
	service_config_header hdr;
	service_entry *entry;
	int ret = 0;	

	absfile* file = absfile_open(binfile, "rb");
	if (NULL == file) return -ENOTEXISTS;

	// header size check
	if (sizeof(hdr) != file->read(&hdr, sizeof(hdr))
		|| hdr.size < sizeof(hdr.magic) + sizeof(hdr.digest)) {
		file->release();
		deletefile(binfile.get_fullpath().c_str());
		return -1;
	}

	//load service & instance & string
	uint32_t servicesize = hdr.service_count * sizeof(service_entry);
	uint8_t *entrystart = new uint8_t[servicesize];
	uint32_t instssize = hdr.instance_count * sizeof(uint32_t);
	uint8_t *inststart = new uint8_t[instssize];
	uint8_t *stringstart = new uint8_t[hdr.string_size];
	file->read(entrystart, servicesize);
	file->read(inststart, instssize);
	file->read(stringstart, hdr.string_size);

	// digest check
	digest dgst("sha256");
	dgst.append((void*)&hdr.size,
		sizeof(hdr) - sizeof(hdr.magic) - sizeof(hdr.digest));
	dgst.append((void*)entrystart, servicesize);
	dgst.append((void*)inststart, instssize);
	dgst.append((void*)stringstart, hdr.string_size);

	// digest result check
	size_t sz;
	const uint8_t* result = dgst.getresult(&sz);
	assert(result && sz == sizeof(hdr.digest));
	
	// if (memcmp((svchdr + sizeof(hdr.magic)), result, sz)) {
	if (memcmp(&hdr.digest, result, sz)) {
		delete []entrystart; delete []inststart; delete []stringstart;
		return -2;
	}

	service_info* impl = NULL;
	for (int i = 0; i < hdr.service_count; i++) {
		entry = (service_entry*)(entrystart + i * sizeof(service_entry));
		const char* ipaddr = nullptr;
		if (entry->ipaddr) {
			ipaddr = (const char*)(stringstart + entry->ipaddr);
		}
		// add service
		impl = manager->add_service(
			(const char*)(stringstart + entry->pkgname),
			(const char*)(stringstart + entry->servicename),
			NULL,
			(const char*)(stringstart + entry->executive),
			ipaddr, entry->port,
			entry->version, entry->flag);
		assert(NULL != impl);

		// add service instance
		uint8_t* instoffset = inststart + entry->instoffset;
		for (int instid = 0; instid < entry->instcnt; instid++) {
			uint32_t* offset = (uint32_t*)(instoffset
				+ instid * sizeof(uint32_t));
			impl->add_instance((const char*)(stringstart + *offset));
		}
	}
	// load finished, release memory
	delete []entrystart; delete []inststart; delete []stringstart;
	return 0;
}

}}} // namespace  zas::mware::rpc

#endif /*__CXX_ZAS_ZRPC_SERVICE_MGR_H__*/