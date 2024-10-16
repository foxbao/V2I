/** @file sysconfig.cpp
 * implementation of system config object
 */

#include <string>
#include "utils/json.h"
#include "mware/sysconfig.h"

using namespace std;
using namespace zas::utils;

namespace zas {
namespace mware {

class sysconfig_impl
{
public:
	sysconfig_impl()
	: _config_root(nullptr) {
	}

	~sysconfig_impl() {
		if (_config_root) {
			_config_root->release();
			_config_root = nullptr;
		}
	}

	bool loaded(void) {
		return _config_root ? true : false;
	}

	int load(const uri& filename)
	{
		if (nullptr != _config_root) {
			_config_root->release();
		}
		_config_root = &json::loadfromfile(filename);
		if (_config_root->is_null()) {
			_config_root->release();
			_config_root = nullptr;
			return -1;
		}
		return 0;
	}

	const jsonobject& path_walk(const char* key)
	{
		string name;
		const char* end;
		jsonobject* ret = _config_root;
		if (nullptr == ret) {
			return json::get_nullobject();
		}

		do {
			end = strchr(key, '.');
			int idx = get_index(key, end);
			if (idx >= 0) {
				if (ret->get_type() != json_array) {
					return json::get_nullobject();
				}
				ret = &ret->get(idx);
			}
			else {
				if (end) {
					name.assign(key, end - key);
				} else {
					name.assign(key);
				}
				if (ret->get_type() != json_object) {
					return json::get_nullobject();
				}
				ret = &ret->get(name.c_str());
			}
			key = end + 1;
		} while (end);
		return *ret;
	}

private:
	int get_index(const char* key, const char* end)
	{
		ssize_t val = 0;
		for (; key != end && *key; ++key)
		{
			if (*key < '0' || *key > '9') {
				// not a digit
				return -1;
			}
			val = val * 10 + *key;
		}
		// check if we exceed the limition
		if (val < 0 || val > INT32_MAX) return -2;
		return int(val);
	}
private:
	jsonobject* _config_root;	
};

// global configure object
static sysconfig_impl* _syscfg = nullptr;

void load_sysconfig(void)
{
	if (_syscfg) {
		assert(_syscfg->loaded());
		return;
	}

	// todo: TBD
	_syscfg = new sysconfig_impl();
	assert(nullptr != _syscfg);
	_syscfg->load("file:///zassys/etc/syssvrconfig/sysconfig.json");
	assert(_syscfg->loaded());
}

ssize_t get_sysconfig(const char* key, ssize_t defval, int* ret)
{
	load_sysconfig();

	if (!key || !*key) {
		if (ret) *ret = -EBADPARM;
		return 0;
	}

	if (ret) *ret = 0;
	const jsonobject& retval = _syscfg->path_walk(key);
	int type = retval.get_type();

	if (type == json_number) {
		return retval.to_number();
	}
	else if (type == json_bool) {
		return retval.to_bool();
	}

	// if any error occurred, use the
	// default value
	return defval;
}

const char* get_sysconfig(const char* key,
	const char* defval, int* ret)
{
	load_sysconfig();

	if (!key || !*key) {
		if (ret) *ret = -EBADPARM;
		return nullptr;
	}

	if (ret) *ret = 0;
	const jsonobject& retval = _syscfg->path_walk(key);
	int type = retval.get_type();

	if (type == json_string) {
		return retval.to_string();
	}

	// if any error occurred, use the
	// default value
	return defval;
}

}} // end of namespace zas::mware
/* EOF */
