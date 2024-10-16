#include "utils/utils.h"
#if (defined(UTILS_ENABLE_FBLOCK_EVLOOP) && defined(UTILS_ENABLE_FBLOCK_LOG))

#include <string>
#include <sqlite3.h>

#include "utils/dir.h"
#include "utils/log.h"
#include "inc/evlmsg.h"
#include "inc/evloop-impl.h"

namespace zas {
namespace utils {
using namespace std;

class log_collector_impl : public evloop_pkglistener
{
public:
	log_collector_impl()
	: _db(nullptr)
	{
		init_database();
	}

	~log_collector_impl()
	{
	}

public:
	int bindto(evloop_impl* evl)
	{
		if (evl->getrole() != evloop_role_server) {
			return -EINVALID;
		}
		if (evl->add_listener("log-collector", &_evl_lnr)) {
			return -EINVALID;
		}
		if (evl->add_package_listener(EVL_SSVC_LOG_SUBMISSION, this)) {
			return -EINVALID;
		}
		fprintf(stdout, "log-collector: start collecting log\n");
		return 0;
	}

public:
	class log_evloop_listener : public evloop_listener
	{
	public:
		void accepted(evlclient client) {
		}

		void disconnected(const char* cliname, const char* instname) {
		}

	private:
		log_collector_impl* ancestor(void) {
			return zas_ancestor(log_collector_impl, _evl_lnr);
		}
	} _evl_lnr;

protected:
	bool on_package(evlclient evl, const package_header& pkg,
		const triggered_pkgevent_queue&)
	{
		auto buf = pkg.get_readbuffer();
		assert(nullptr != buf);

		auto logc = (evl_ssvc_log_submission*)alloca(pkg.size);
		auto n = buf->peekdata(0, logc, pkg.size);
		assert(n == pkg.size);

		write_log(logc);
		return true;
	}

private:
	void write_log(evl_ssvc_log_submission* logc)
	{
		int l = logc->level;
		if (l < (int)log_level::error || l > (int)log_level::verbose) {
			l = (int)log_level::verbose;
		}

		// output the time stamp (readable)
		// in format of YYYY-MM-DD HH:MM:SS:MILLSEC
		tm t;
		localtime_r(&logc->timestamp.tv_sec, &t);
		char time_str[32];
		sprintf(time_str, "%d-%02d-%02d %02d:%02d:%02d:%06ld",
			t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
			t.tm_hour, t.tm_min, t.tm_sec,
			logc->timestamp.tv_usec);

		fprintf(stdout, "%s%s  %s: ", _level_prefix[l], time_str, logc->data);
		fprintf(stdout, "%s%s\n", &logc->data[logc->tag_length + 1],
			"\033[0m");

		// write to database
		if (nullptr == _db) {
			return;
		}
		string sql = "INSERT INTO ZASLOG (TIMESTAMP,TAG,LEVEL,CONTENT) VALUES('";
		sql += time_str;
		sql += "','";
		sql += (const char*)logc->data;	// tag
		
		// level
		sprintf(time_str, "',%u,'", (uint32_t)logc->level);
		sql += time_str;
		sql += (const char*)&logc->data[logc->tag_length + 1];	// content
		sql += "');";

		char* errmsg = nullptr;
		int ret = sqlite3_exec(_db, sql.c_str(), nullptr, 0, &errmsg);
		assert(ret == SQLITE_OK);
	}

	void init_database(void)
	{
		if (nullptr != _db) {
			return;
		}
		// todo: change directory
		string path = get_hostdir();
		path += "/clog.db";

		// open the database file
		int ret = sqlite3_open(path.c_str(), &_db);
		if (ret || nullptr == _db) {
			_db = nullptr;
			return;
		}
		db_create_table();
	}

	void db_create_table(void)
	{
		assert(nullptr != _db);
		const char* sql = "CREATE TABLE IF NOT EXISTS ZASLOG("	\
			"ID INTEGER PRIMARY KEY AUTOINCREMENT,"	\
			"TIMESTAMP TEXT NOT NULL,"	\
			"TAG TEXT NOT NULL,"	\
			"LEVEL INT NOT NULL,"	\
			"CONTENT TEXT NOT NULL);";
		
		char* errmsg = nullptr;
		int ret = sqlite3_exec(_db, sql, nullptr, 0, &errmsg);
		assert(ret == SQLITE_OK);
	}

private:
	sqlite3* _db;
	static const char* _level_prefix[];
};

const char* log_collector_impl::_level_prefix[] = {
	nullptr,					// unknown
	"\033[0;31m",				// error
	"\033[0;33m",				// warning
	"\033[0;32m",				// debug
	"\033[0;37m",				// info
	"\033[1;38;2;75;75;75m",	// verbose
};

log_collector* log_collector::inst(void)
{
	static log_collector_impl* _inst = nullptr;
	if (nullptr == _inst) {
		_inst = new log_collector_impl();
		assert(nullptr != _inst);
	}
	return reinterpret_cast<log_collector*>(_inst);
}

int log_collector::bindto(evloop* e)
{
	if (nullptr == e) {
		return -EBADPARM;
	}
	auto evl = reinterpret_cast<evloop_impl*>(e);
	auto logc = reinterpret_cast<log_collector_impl*>(this);
	return logc->bindto(evl);
}

}} // end of namespace zas::utils
#endif // (defined(UTILS_ENABLE_FBLOCK_EVLOOP) && defined(UTILS_ENABLE_FBLOCK_LOG))
/* EOF */
