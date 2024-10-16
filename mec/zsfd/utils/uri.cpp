/** @file uri.cpp
 * implementation of the uri parser
 */

#include "utils/utils.h"
#if (defined(UTILS_ENABLE_FBLOCK_URI) && defined(UTILS_ENABLE_FBLOCK_CMDLINE) && defined(UTILS_ENABLE_FBLOCK_CERT))

#include <string.h>

#include "utils/uri.h"
#include "utils/cert.h"
#include "utils/mutex.h"

namespace zas {
namespace utils {

using namespace std;

static mutex _mut;

class uri_impl
{
public:

	uri_impl(const char* uristr)
	: _refcnt(1), _flags(0), _port(-1)
	, _nr_queries(0), _total_queries(0)
	, _queries(NULL), _port_start_colon(0) {
		load(uristr);
	}

	~uri_impl()
	{
		for (int count = 0; count < _nr_queries; count++) {
			delete _queries[count];
		}
		free(_queries);
		_queries = nullptr;
		_nr_queries = 0;
		_total_queries = 0;
	}

	int addref(void) {
		return __sync_add_and_fetch(&_refcnt, 1);
	}

	int release(void) {
		int cnt = __sync_sub_and_fetch(&_refcnt, 1);
		if (cnt <= 0) delete this;
		return cnt;
	}

	bool loaded(void) {
		return _f.loaded ? true : false;
	}

	inline const string& get_scheme(void) const {
		return _scheme;
	}

	inline const string& get_fullpath(void) const {
		return _fullpath;
	}
	
	int get_user_passwd(string& user, string& passwd)
	{
		if (!loaded()) return -2;
		if (!_f.has_user_passwd) return 0;

		user = _username;
		passwd = _password;
		return 1;
	}

	int has_user_passwd(void)
	{
		if (!loaded()) return -3;
		return _f.has_user_passwd ? 1 : 0;
	}

	int get_port(void) {
		return _port;
	}

	const char* get_filename(void) {
		return _filename;
	}

	string get_host(void) {
		string ret;
		if (_port >= 0) {
			assert(_port_start_colon != 0);
			ret.assign(_fullpath, 0, _port_start_colon);
		}
		else if (_filename) {
			ret.assign(_fullpath, 0, _filename
				- _fullpath.c_str());
		}
		else {
			// there is no filename
			ret.assign(_fullpath);
		}
		return ret;
	}

	uri_impl(const uri_impl& u)
	: _refcnt(1)
	{
		_flags = u._flags;
		_scheme = u._scheme;
		_fullpath = u._fullpath;
		_username = u._username;
		_password = u._password;
		_filename = u._filename;
		_port = u._port;

		_nr_queries = u._nr_queries;
		_total_queries = (_nr_queries + 7) & ~7;
		_queries = (query_item**)malloc(sizeof(void*) * _total_queries);

		for (int i = 0; i < _nr_queries; ++i)
		{
			query_item* qi = u._queries[i];
			assert(NULL != qi);
			query_item* curr = new query_item();
			assert(NULL != curr);
			curr->key = qi->key;
			curr->value = qi->value;
			curr->utf8 = qi->utf8;
			_queries[i] = curr;
		}
	}

	void digest(uint8_t d[32])
	{
		if (_f.digested) {
			memcpy(d, _digest, 32);
			return;
		}

		string input(_scheme);
		input += "://";
		if (_f.has_user_passwd) {
			input += _username;
			input += ":";
			input += _password;
			input += "@";
		}
		input += _fullpath;

		if (_nr_queries > 0) {
			input += "?";
			for (int i = 0; i < _nr_queries; ++i) {
				input += _queries[i]->key;
				input += "=";
				input += _queries[i]->value;
			}
		}
		zas::utils::digest dgst("sha256");
		dgst.append((void*)input.c_str(), input.length());

		size_t sz;
		const uint8_t* result = dgst.getresult(&sz);
		assert(result && sz == 32);
		memcpy(d, result, sz);
		memcpy(_digest, result, sz);
	}

	string tostring(void)
	{
		string input(_scheme);
		input += "://";
		if (_f.has_user_passwd) {
			input += _username;
			input += ":";
			input += _password;
			input += "@";
		}
		input += _fullpath;

		if (_nr_queries > 0) {
			input += "?";
			for (int i = 0; i < _nr_queries; ++i) {
				query_item* item = _queries[i];
				if (i > 0) {
					input += "&";
				}
				input += item->key;
				input += "=";
				if (item->utf8) {
					char hex[8];
					const uint8_t* buf = (uint8_t*)item->value.c_str();
					for (uint32_t ch; (ch = *buf) != 0; ++buf) {
						sprintf(hex, "%%%02X", ch & 0xFF);
						input += hex;
					}
				}
				else input += item->value;
			}
		}
		return input;
	}


	// be sure that a duplicated instance of uri
	// has been created before this call
	// otherwise we will face with racing problems
	int set_fullpath(const char* newpath)
	{
		int port;
		const char* filename;
		string fullpath(newpath);

		// parse the "port" if there is one
		if (!load_port(fullpath.c_str(), port)) {
			port = -1; return -EBADPARM;
		}

		// check full path validity
		// this check must be later than parsing port
		if (!check_fullpath_validty(fullpath.c_str(),
			fullpath.c_str() + fullpath.length(), port))
			return -EBADPARM;

		// parse the filename
		if (!load_filename(fullpath.c_str(), filename)) {
			filename = NULL; return -EBADPARM;
		}

		_fullpath = fullpath;
		_filename = _fullpath.c_str() + (filename - fullpath.c_str());
		_port = port;
		return 0;
	}

	struct query_item
	{
		string key;
		string value;
		bool utf8;
	};

	struct query_list_item
	{
		query_list_item* next;
		query_item* payload;
	};

	int get_query_count(void) {
		return _nr_queries;
	}

	int get_query_key(int index, string& key)
	{
		if (index >= _nr_queries)
			return -1;
		if (NULL == _queries) return -2;
		key = _queries[index]->key;
		return 0;
	}

	int get_query_value(int index, string& value)
	{
		if (index >= _nr_queries)
			return -1;
		if (NULL == _queries) return -2;
		value = _queries[index]->value;
		return 0;
	}

	int get_query_value(const char* key, string& value)
	{
		if (!_nr_queries) return -1;
		for (int i = 0; i < _nr_queries; ++i) {
			query_item* item = _queries[i];
			if (!strcmp(item->key.c_str(), key)) {
				value = item->value;
				return 0;
			}
		}
		return -1;
	}

	int change_query(const char* key, const char* value)
	{
		if (!_nr_queries) return -1;
		for (int i = 0; i < _nr_queries; ++i) {
			query_item* item = _queries[i];
			if (!strcmp(item->key.c_str(), key)) {
				item->value = value;
				return 0;
			}
		}
		return -1;
	}

	void release_queries(void)
	{
		for (int i = 0; i < _nr_queries; ++i)
		{
			if (_queries[i]) {
				delete _queries[i];
				_queries[i] = NULL;
			}
		}
		_nr_queries = 0;
	}

	int add_query(const char* key, const char* value)
	{
		if (!key || !*key || !value || !*value) {
			return -EBADPARM;
		}
		bool utf8 = false;
		std::string tmp;
		tmp.clear();
		for (uint32_t c = 0, sid = 0; *value ; ++value)
		{
			if (*value > 127) utf8 = true;
			if (*value == '%') {
				if (sid) {
					return -EBADPARM;
				}
				utf8 = true;
				sid = 2;
				continue;
			}
	
			if (sid) {
				c *= 16;
				if (*value >= '0' && *value <= '9') {
					c += *value - '0';
				}
				else if (*value >= 'a' && *value <= 'f') {
					c += 10 + *value - 'a';
				}
				else if (*value >= 'A' && *value <= 'F') {
					c += 10 + *value - 'A';
				}
				else {
					return -EBADPARM;
				}
				if (!--sid) tmp += (char)c, c = 0;
			}
			else tmp += *value;
		}
		
		// allocate space if necessary
		if (_nr_queries + 1 > _total_queries)
		{
			int32_t total = (_nr_queries + 1 + 7) & ~7;
			query_item** querytmp = _queries;
			_queries = (query_item**)malloc(sizeof(void*) * total);
			for (int count = 0; count < _nr_queries; count++) {
				_queries[count] = querytmp[count];
			}
			free(querytmp);
			assert(NULL != _queries);
			_total_queries = total;
		}

		//create a new query_item
		auto* item = new query_item();
		item->key = key;
		item->value = tmp;
		_queries[_nr_queries] = item;
		_nr_queries++;

		qsort(_queries, _nr_queries, sizeof(void*), query_compare);
		return 0;
	}

private:
	bool load(const char* uristr)
	{
		int port;
		string scheme;
		string username;
		string password;
		string fullpath;

		const char *filename, *str;
		_f.loaded = false;
		if (nullptr == uristr) {
			return false;
		}

		// we load the scheme
		str = load_scheme(uristr);
		if (NULL == str) return false;
		scheme.assign(uristr, str - uristr);

		uristr = str + 3;	// omit "://"

		// we idendity the "username:password@fullpath"
		str = strchr(uristr, '?');
		if (NULL == str) str = uristr + strlen(uristr);
		if (str == uristr) return false;

		// parse "username" and "password"
		int ret = parse_user_passwd(uristr, str, username, password);
		if (ret < 0) return false;
		if (ret > 0) _f.has_user_passwd = 1;		
		fullpath.assign(uristr, str - uristr);

		// parse the "port" if there is one
		if (!load_port(fullpath.c_str(), port)) {
			port = -1; return false;
		}

		// check full path validity
		// this check must be later than parsing port
		if (!check_fullpath_validty(uristr, str, port))
			return false;

		// parse the filename
		if (!load_filename(fullpath.c_str(), filename)) {
			filename = NULL; return false;
		}

		if (*str && !load_kv_pairs(++str))
			return false;

		// save all information
		_port = port;
		_scheme.assign(scheme);
		_username.assign(username);
		_password.assign(password);
		_fullpath.assign(fullpath);
		_filename = (!filename) ? nullptr
			: (_fullpath.c_str() + (filename - fullpath.c_str()));

		// successful flag
		_f.loaded = 1;
		return true;
	}

	inline bool is_digit(const char c) {
		return (c >= '0' && c <= '9')
			? true : false;
	}

	inline bool is_alaphbet(const char c) {
		return ((c >= 'a' && c <= 'z')
			|| (c >= 'A' && c <= 'Z'))
			? true : false;
	}

	bool check_scheme_validty(const char* start, const char* end)
	{
		if (start == end) {
			return false;	// no scheme
		}

		const char* s;
		for (s = start; s != end; ++s)
		{
			if (is_digit(*s)) continue;
			if (is_alaphbet(*s)) continue;
			if (*s == '_' || *s == '-')
				continue;
			return false;
		}
		return true;
	}

	const char* load_scheme(const char* uristr)
	{
		const char* s = uristr;
		const char* e = strstr(s, "://");
		if (NULL == e) return NULL;

		// check the scheme validity
		if (!check_scheme_validty(s, e))
			return NULL;
		return e;
	}

	int parse_user_passwd(const char*& start, const char* end,
		string& username, string& password)
	{
		assert(start != end);
		const char* e;
		const char* s = start;

		// strnchr(s, end - start, '@');
		for (e = s; e != end && *e != '@'; ++e);
		if (e == end) return 0;

		// parse username
		const char* sep;
		// sep = strnchr(s, e - start, ':');
		for (sep = s; sep != e && *sep != ':'; ++sep);
		if (e == sep) return -1;
		if (s == sep) return -2;	// no username
		if (sep + 1 == end) return -3; // no password
		username.assign(s, sep - s);

		// parse password
		++sep;	// omit ":"
		password.assign(sep, e - sep);

		start = e + 1;
		return 1;
	}

	bool check_fullpath_validty(const char* start, const char* end, int port)
	{
		if (start == end) {
			return false;	// empty fullpath
		}

		const char* s = strchr(start, ':');
		if (NULL != s) {	// there is ':', check if it leads a port
			if (port < 0) return false;
			// make sure we only have 1 ':'
			if (strchr(++s, ':')) return false;
		}

		// '?' is already excluded since the end of fullpath is '?'
		// ':' is already excluded since we handle it early
		const char* vtbl = "\\*\"<>|";
		for (s = start; s != end; ++s)
		{
			if (is_digit(*s)) continue;
			if (is_alaphbet(*s)) continue;
			if (strchr(vtbl, *s)) return false;
		}
		return true;
	}

	bool load_port(const char* fullpath, int& port)
	{
		// make sure there is only 1 ":" or there is no ":"
		const char* colon = NULL;
		for (const char* tmp = fullpath;;) {
			tmp = strchr(tmp, ':');
			if (!tmp) break;
			if (colon) return false;
			colon = tmp++;
		}
		_port_start_colon = (colon) ? (colon - fullpath) : 0;
		if (NULL == colon) {
			port = -1; return true;
		}

		// find '/' after ':', between which is the port
		const char* port_end = strchr(++colon, '/');
		if (NULL == port_end) {
			port_end = colon + strlen(colon);
		}

		// check validty
		if (port_end - colon > 5) return false;

		// must have a valid host before the port, check it
		if (fullpath == colon) {
			port = -2; return false;
		}

		// convert to number
		const char* t;
		for (port = 0, t = colon; t < port_end; ++t) {
			if (!is_digit(*t)) return false;
			port = port * 10 + (*t - '0');
			if (port > 0xFFFF) {
				// exceeds the port maximum
				return false;
			}
		}
		return true;
	}

	bool load_filename(const char* fullpath, const char*& filename)
	{
		// find the last "/"
		const char* last_slash = strrchr(fullpath, '/');

		if (!last_slash) {
			// if there is no '/', there will be following cases:
			// 1. [file://]filename.txt				-> filename.txt be mis-regarded as host
			// [http://user:pass@]host:port
			// 3. [http://user:pass@]filename.txt	-> filename.txt be mis-regarded as host
			// 4. [http://]host
			// 5. [http://]host:port
			// so there is no filename in this case
			filename = nullptr; return true;
		} else if (last_slash == fullpath) {
			// host:port takes some charactors, check this
			return false;
		}

		// omit the '/'
		if (*++last_slash == '\0') {
			filename = NULL;
		}
		else filename = last_slash;
		return true;
	}

	void free_querylist(query_list_item* list)
	{
		for (; list; list = list->next) {
			if (list->payload) delete list->payload;
		}
	}

	static int query_compare(const void *a, const void *b)
	{
		query_item* aa = *((query_item**)a);
		query_item* bb = *((query_item**)b);
		if (aa->key > bb->key) return 1;
		else if (aa->key < bb->key) return -1;
		else return 0;
	}

	void commit_querylist(query_list_item* list)
	{
		uint32_t count = 0;
		query_list_item* tmp;

		// get query count
		for (tmp = list; tmp; tmp = tmp->next, ++count);

		// release the previous
		release_queries();

		// allocate space if necessary
		if (count > _total_queries)
		{
			int32_t total = (count + 7) & ~7;
			free(_queries);
			_queries = (query_item**)malloc(sizeof(void*) * total);
			assert(NULL != _queries);
			_total_queries = total;
		}

		// commit all queries
		_nr_queries = count;
		for (count = 0, tmp = list; tmp; tmp = tmp->next) {
			_queries[count++] = tmp->payload;
		}

		qsort(_queries, _nr_queries, sizeof(void*), query_compare);
	}

	// parse something like:
	// "query=%E8%BF%99123%E6%98%AF%E4%B8%80%E5%8F%A5abc%E8%AF%9D"
	bool load_kv_pairs(const char* str)
	{
		string tmp, key;
		query_list_item* list = NULL;

		while (*str)
		{
			uint32_t i;
			const char* key_start = str;
			
			// find a key
			for (; *str && *str != '='; ++str);
			if (!str) {
				free_querylist(list);
				return -1;
			}
			// 'key' length shall not be 0
			uint32_t len = (uint)(str - key_start);
			if (!len) {
				free_querylist(list);
				return -2;
			}
			key.assign(key_start, len);

			++str;	// omit the '='
			tmp.clear();
			bool utf8 = false;
			for (uint32_t c = 0, sid = 0; *str && *str != '&'; ++str)
			{
				if (*str > 127) utf8 = true;
				if (*str == '%') {
					if (sid) {
						free_querylist(list);
						return -3;
					}
					utf8 = true;
					sid = 2;
					continue;
				}
		
				if (sid) {
					c *= 16;
					if (*str >= '0' && *str <= '9') {
						c += *str - '0';
					}
					else if (*str >= 'a' && *str <= 'f') {
						c += 10 + *str - 'a';
					}
					else if (*str >= 'A' && *str <= 'F') {
						c += 10 + *str - 'A';
					}
					else {
						free_querylist(list);
						return -4;
					}
					if (!--sid) tmp += (char)c, c = 0;
				}
				else tmp += *str;
			}
			if (*str == '&') ++str;

			// create a new pair
			query_item* pair = new query_item();
			assert(NULL != pair);
			pair->utf8 = utf8;
			pair->key = key;
			pair->value = tmp;

			query_list_item* item = (query_list_item*)alloca(sizeof(*item));
			item->payload = pair;
			item->next = list;
			list = item;
		}

		// save all queries (replacement of old ones)
		commit_querylist(list);
		return true;
	}

private:
	union {
		uint32_t _flags;
		struct {
			uint32_t loaded : 1;
			uint32_t digested : 1;
			uint32_t has_user_passwd : 1;
		} _f;
	};
	int _refcnt;
	string _scheme;
	string _fullpath;
	string _username;
	string _password;
	const char* _filename;

	int _port_start_colon;
	int _port;
	uint8_t _digest[32];

	uint32_t _nr_queries, _total_queries;
	query_item** _queries;
};

uri::uri()
: _data(NULL) {
}

uri::uri(const uri& u)
: _data(NULL)
{
	if (this == &u)
		return;
	
	uri_impl* peer = reinterpret_cast<uri_impl*>(u._data);
	if (peer) {
		peer->addref();
		_data = reinterpret_cast<void*>(peer);
	}
}

uri& uri::operator=(const uri& u)
{
	if (this == &u)
		return *this;
	
	uri_impl* self = reinterpret_cast<uri_impl*>(_data);
	uri_impl* peer = reinterpret_cast<uri_impl*>(u._data);
	if (self) self->release();
	if (peer) {
		peer->addref();
		_data = reinterpret_cast<void*>(peer);
	}
	else _data = NULL;
	return *this;
}

uri::~uri()
{
	uri_impl* self = reinterpret_cast<uri_impl*>(_data);
	if (self) self->release();
}

uri::uri(const char* uristr)
: _data(NULL)
{
	uri_impl* urii = new uri_impl(uristr);
	assert(NULL != urii);
	if (urii->loaded()) {
		_data = reinterpret_cast<void*>(urii);
	}
	else delete urii;
}

uri::uri(const std::string& uristr)
: _data(NULL)
{
	uri_impl* urii = new uri_impl(uristr.c_str());
	assert(NULL != urii);
	if (urii->loaded()) {
		_data = reinterpret_cast<void*>(urii);
	}
	else delete urii;
}

const string uri::get_scheme(void) const
{
	string ret;
	if (!_data) return ret;
	uri_impl* urii = reinterpret_cast<uri_impl*>(_data);
	if (!urii->loaded()) return ret;
	return urii->get_scheme();
}

int uri::get_user_passwd(std::string& username, std::string& passwd) const
{
	if (!_data) return -1;
	uri_impl* urii = reinterpret_cast<uri_impl*>(_data);
	return urii->get_user_passwd(username, passwd);
}

const std::string uri::get_fullpath(void) const
{
	string ret;
	if (!_data) return ret;
	uri_impl* urii = reinterpret_cast<uri_impl*>(_data);
	if (!urii->loaded()) return ret;
	return urii->get_fullpath();
}

const char* uri::get_filename(void) const
{
	if (!_data) return nullptr;
	uri_impl* urii = reinterpret_cast<uri_impl*>(_data);
	if (!urii->loaded()) return nullptr;
	return urii->get_filename();
}

const std::string uri::get_host(void) const
{
	string ret;
	if (!_data) return ret;
	uri_impl* urii = reinterpret_cast<uri_impl*>(_data);
	if (!urii->loaded()) return ret;
	return urii->get_host();
}

int uri::get_port(void) const
{
	int ret = -1;
	if (!_data) return ret;
	uri_impl* urii = reinterpret_cast<uri_impl*>(_data);
	if (!urii->loaded()) return ret;
	return urii->get_port();
}

bool uri::change(const char* uristr)
{
	uri_impl* urii;
	if (_data) {
		urii = reinterpret_cast<uri_impl*>(_data);
		urii->release();
	}

	urii = new uri_impl(uristr);
	if (!urii->loaded()) {
		delete urii; _data = NULL;
		return false;
	}
	_data = reinterpret_cast<uri*>(urii);
	return true;
}

bool uri::change(const std::string& uristr) {
	return change(uristr.c_str());
}

bool uri::valid(void) const
{
	if (!_data) return false;
	uri_impl* urii = reinterpret_cast<uri_impl*>(_data);
	if (!urii->loaded()) return false;
	return true;
}

int uri::has_user_passwd(void) const
{
	if (!_data) return -1;
	uri_impl* urii = reinterpret_cast<uri_impl*>(_data);
	if (!urii->loaded()) return -2;
	return urii->has_user_passwd();
}

int uri::has_port(void) const
{
	if (!_data) return -1;
	uri_impl* urii = reinterpret_cast<uri_impl*>(_data);
	if (!urii->loaded()) return -2;
	return (urii->get_port() >= 0) ? 1 : 0;
}

int uri::has_filename(void) const
{
	if (!_data) return -1;
	uri_impl* urii = reinterpret_cast<uri_impl*>(_data);
	if (!urii->loaded()) return -2;
	return (urii->get_filename()) ? 1 : 0;
}

int uri::set_fullpath(const char* newpath)
{
	if (!newpath || !*newpath) {
		return -EBADPARM;
	}

	// need lock
	auto_mutex am(_mut);

	if (!_data) return -EINVALID;
	uri_impl* old = reinterpret_cast<uri_impl*>(_data);
	if (!old->loaded()) return -EINVALID;

	// duplicate a new uri object
	uri_impl* new_uri = new uri_impl(*old);
	if (NULL == new_uri) return -ENOMEMORY;

	int ret = new_uri->set_fullpath(newpath);
	if (ret) {
		delete new_uri;
		return ret;
	}

	// update to new uri object
	old->release();
	_data = reinterpret_cast<void*>(new_uri);
	return 0;
}

int uri::digest(uint8_t output[32]) const
{
	if (NULL == output) {
		return -EBADPARM;
	}
	if (!_data) return -1;
	uri_impl* urii = reinterpret_cast<uri_impl*>(_data);
	if (!urii->loaded()) return -2;
	urii->digest(output);
	return 0;
}

std::string uri::tostring(void) const
{
	string ret;
	if (!_data) return ret;
	uri_impl* urii = reinterpret_cast<uri_impl*>(_data);
	if (!urii->loaded()) return ret;
	ret = urii->tostring();
	return ret;
}

int uri::query_count(void)
{
	auto* urii = reinterpret_cast<uri_impl*>(_data);
	if (!urii->loaded()) return 0;
	return urii->get_query_count();
}

string uri::query_key(int index)
{
	string ret;
	auto* urii = reinterpret_cast<uri_impl*>(_data);
	if (!urii->loaded()) return ret;
	urii->get_query_key(index, ret);
	return ret;
}

string uri::query_value(int index)
{
	string ret;
	auto* urii = reinterpret_cast<uri_impl*>(_data);
	if (!urii->loaded()) return ret;
	urii->get_query_value(index, ret);
	return ret;
}

string uri::query_value(const char* key)
{
	string ret;
	auto* urii = reinterpret_cast<uri_impl*>(_data);
	if (!urii->loaded()) return ret;
	urii->get_query_value(key, ret);
	return ret;
}

string uri::query_value(const string& key)
{
	string ret;
	auto* urii = reinterpret_cast<uri_impl*>(_data);
	if (!urii->loaded()) return ret;
	urii->get_query_value(key.c_str(), ret);
	return ret;
}

int uri::clear_query(void)
{
	string ret;
	auto* urii = reinterpret_cast<uri_impl*>(_data);
	if (!urii->loaded()) return -ENOTAVAIL;
	urii->release_queries();
	return 0;
}

int uri::add_query(const char* key, const char* value)
{
	string ret;
	auto* urii = reinterpret_cast<uri_impl*>(_data);
	if (!urii->loaded()) return -ENOTAVAIL;
	return urii->add_query(key, value);
}

int uri::change_query(const char* key, const char* value)
{
	string ret;
	auto* urii = reinterpret_cast<uri_impl*>(_data);
	if (!urii->loaded()) return -ENOTAVAIL;
	return urii->change_query(key, value);
}

}} // end of namespace zas::utils
#endif // (defined(UTILS_ENABLE_FBLOCK_URI) && defined(UTILS_ENABLE_FBLOCK_CMDLINE) && defined(UTILS_ENABLE_FBLOCK_CERT))
/* EOF */
