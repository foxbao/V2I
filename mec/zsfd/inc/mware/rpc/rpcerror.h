
#ifndef __CXX_ZAS_RPC_ERROR_H__
#define __CXX_ZAS_RPC_ERROR_H__

#include "mware/mware.h"

#include <string>

#include "std/smtptr.h"
#include "rpcerrordef.h"

namespace zas {
namespace mware {
namespace rpc {

class MWARE_EXPORT rpc_error
{
public:
	rpc_error(const char *filename, unsigned int line,
		err_category catid, err_scenario sid, errid eid)
		: _filename(filename), _line(line)
		, _err_catid(catid), _err_sid(sid), _err_id(eid)
	{
	}

	rpc_error(const char *filename, unsigned int line,
		err_category catid, err_scenario sid, errid eid, std::string desc)
		: _filename(filename), _line(line)
		, _err_catid(catid), _err_sid(sid), _err_id(eid)
		, _description(desc)
	{
	}

	rpc_error(const rpc_error& c)
		: _filename(c._filename)
		, _line(c._line)
		, _err_catid(c._err_catid)
		, _err_sid(c._err_sid)
		, _err_id(c._err_id)
		, _description(c._description)
	{
	}

	virtual ~rpc_error() {}

	std::string get_source_filename(void) const {
		return _filename;
	}

	unsigned int get_error_line(void) const {
		return _line;
	}

	err_category get_category_id(void) const {
		return _err_catid;
	}

	err_scenario get_scenario_id(void) const {
		return _err_sid;
	}

	errid get_id(void) const {
		return _err_id;
	}

	std::string get_description(void) const {
		return _description;
	}

private:
	std::string _filename;
	unsigned int _line;
	err_category _err_catid;
	err_scenario _err_sid;
	errid _err_id;
	std::string _description;
};

class MWARE_EXPORT rpc_invalid_argument : public rpc_error
{
public:

	rpc_invalid_argument(const char *filename, unsigned int line,
		err_scenario sid, errid eid)
		: rpc_error(filename, line, errcat_invalidargument, sid, eid)
	{
	}

	rpc_invalid_argument(const char *filename, unsigned int line,
		err_scenario sid, errid eid, std::string desc)
		: rpc_error(filename, line, errcat_invalidargument, sid, eid, desc)
	{
	}

	rpc_invalid_argument(const rpc_invalid_argument& c)
		: rpc_error(c)
	{
	}

	~rpc_invalid_argument() {}
};

class MWARE_EXPORT rpc_logic_error : public rpc_error
{
public:

	rpc_logic_error(const char *filename, unsigned int line,
		err_scenario sid, errid eid)
		: rpc_error(filename, line, errcat_logicerror, sid, eid)
	{
	}

	rpc_logic_error(const char *filename, unsigned int line,
		err_scenario sid, errid eid, std::string desc)
		: rpc_error(filename, line, errcat_logicerror, sid, eid, desc)
	{
	}

	rpc_logic_error(const rpc_logic_error& c)
		: rpc_error(c)
	{
	}

	~rpc_logic_error() {}
};

}}}

#endif  /*__CXX_ZAS_ZRPC_ERROR_H__*/