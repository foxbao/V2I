/** @file exprvm_token.h
 * definition of token objects for expression virtual machine
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_EXPRVM

#ifndef __CXX_ZAS_UTILS_EXPRVM_TOKEN_H__
#define __CXX_ZAS_UTILS_EXPRVM_TOKEN_H__

#include <string>
#include "std/zasbsc.h"
#include "std/list.h"

namespace zas {
namespace utils {
namespace exprvm {

using namespace std;
using namespace zas::utils;

class srcfilestack
{
public:
	srcfilestack();
	~srcfilestack();

	struct srcfilenode
	{
		static const int LN_MAX_CHAR = 1024;

		srcfilenode();
		~srcfilenode();

		bool loadline(void);
		void killspace(void);
		const char* get_symbol(size_t& sz);
		const char* check_number(size_t& sz);
		const char* check_double_colon(size_t& sz);
		bool omit_comments(void);
		void omit_currline(void);

		listnode_t	_ownerlist;
		FILE*		_fp;
		char*		_strbuf;
		char*		_curstart;
		char		_currline[LN_MAX_CHAR];
		char*		_curpos;
		uint32_t	 _line_num;
		uint32_t	 _line_pos;

		string		_filename;
		string		_full_filename;
	};

	bool push(string fullname);
	bool push_from_string(const char* str, size_t sz = 0);
	bool push_from_string(string str);

	bool pop(void);
	const char *get_symbol(size_t& sz);

	const string& get_filename(void);
	uint32_t get_linenum(void);
	uint32_t get_linepos(void);

	bool is_fullname_exists(string fullname);
	bool goto_nextline(void);

private:
	srcfilenode* gettop(void);

private:
	listnode_t	_filelist;

};

// actually float is "double"
enum numtype { nt_unknown, nt_uint, nt_int, nt_float };

enum tokentype
{
	tokentype_unknown = 0,
	tokentype_basic,
	tokentype_operator,
	tokentype_reserved,
	tokentype_identifier,
	tokentype_number,
};

enum tokenid
{
	tokenid_unknown = 0,
	tokenid_left_bracket,			// (
	tokenid_right_bracket,			// )
	tokenid_left_curly_bracket,		// {
	tokenid_right_curly_bracket,	// }
	tokenid_comma,					// ,
	tokenid_semicolon,				// ;
	tokenid_colon,					// :
	tokenid_add,					// +
	tokenid_sub,					// -
	tokenid_mul,					// *
	tokenid_div,					// /
	tokenid_power,					// ^
	tokenid_double_colon,			// ::
	tokenid_equal,					// =
	tokenid_left_square_bracket,	// [
	tokenid_right_square_bracket,	// ]
	tokenid_import,					// import
	tokenid_const,					// const
	tokenid_unsigned,				// unsigned
	tokenid_int,					// int
	tokenid_float,					// float
	tokenid_void,					// void
	tokenid_boolean,				// bool
	tokenid_true,					// true
	tokenid_false,					// false
	tokenid_selfinc,				// ++
	tokenid_selfdec,				// --
	tokenid_num_int,
	tokenid_num_short,
	tokenid_number_long,
	tokenid_number_uint,
	tokenid_number_float,
	tokenid_user,					// user defined
};

class token
{
	friend class tokenparser;
public:

	token(const string& name, tokentype type, tokenid id)
		: _name(name), _type(type), _id(id) {
	}

	token(const token& token) {
		if (this == &token)
			return;
		_name = token._name;
		_type = token._type;
		_id = token._id;
		_data = token._data;
	}

	void operator=(const token& token) {
		if (this == &token)
			return;
		_name = token._name;
		_type = token._type;
		_id = token._id;
		_data = token._data;
	}

	token() : _type(tokentype_unknown), _id(tokenid_unknown) {}
	~token() {}

	char operator[](int pos) {
		if (pos < 0) {
			if (-pos > (int)_name.length())
				return '\0';
			return _name.c_str()[_name.length() + pos];
		}
		if (pos > (int)_name.length())
			return '\0';
		return _name.c_str()[pos];
	}

	const string& getname(void) const {
		return _name;
	}

	operator const string&() const {
		return _name;
	}

	tokentype gettype(void) const {
		return _type;
	}

	tokenid getid(void) const {
		return _id;
	}

	bool operator==(const string& s) const {
		return (_name == s) ? true : false;
	}

	bool operator!=(const string& s) const {
		return (_name != s) ? true : false;
	}

	bool operator==(tokentype type) const {
		return (type == _type) ? true : false;
	}

	bool operator!=(tokentype type) const {
		return (type != _type) ? true : false;
	}

	bool operator==(tokenid id) const {
		return (id == _id) ? true : false;
	}

	bool operator!=(tokenid id) const {
		return (id != _id) ? true : false;
	}


	void setnum(numtype type, double& val) {
		_data.num.type = type;
		_data.num.value = val;
	}

	bool getnum(numtype& type, double& val) {
		if (tokentype_number != _type)
			return false;
		type = _data.num.type;
		val = _data.num.value;
		return true;
	}

private:
	string _name;
	tokentype _type;
	tokenid _id;

	union {
		struct {
			numtype	type;
			double value;
		} num;
	} _data;
};

inline static bool operator==(const string& s, const token& token)
{
	return (token == s);
}

inline static bool operator!=(const string& s, const token& token)
{
	return (token != s);
}

inline static bool operator==(tokentype type, const token& token)
{
	return (token == type);
}

inline static bool operator!=(tokentype type, const token& token)
{
	return (token != type);
}

inline static bool operator==(tokenid eId, const token& token)
{
	return (token == eId);
}

inline static bool operator!=(tokenid eId, const token& token)
{
	return (token != eId);
}

class tokenparser
{
	friend class tokenparser_initializer;
public:
	tokenparser(srcfilestack& filestack);
	~tokenparser();

	srcfilestack& get_filestack(void) {
		return _filestack;
	}

	const token& gettoken(void);

	void token_pushback(void) {
		_f.token_pushedback = 1;
	}

	bool omit_currline(void) {
		_f.token_pushedback = 0;
		return _filestack.goto_nextline();
	}

private:
	bool isnum(char c);
	bool check_as_num(const char *token, size_t sz);
	bool errnum(const char *token, size_t sz);
	bool setnum(const char *token, size_t sz, double &num, bool minus, bool is_float);

	bool check_as_identifier(const char *token, size_t sz);
	void analyze_user_token(const char *token, size_t sz);

private:
	srcfilestack&	_filestack;
	token			_currtoken;

	union {
		uint32_t _flags;
		struct {
			uint32_t token_pushedback : 1;
		} _f;
	};

	struct reserved_identifier
	{
		const char *idname;
		tokentype type;
		tokenid   id;
	};

	static void initialize(void);
	static int compare(const void*, const void*);
	static reserved_identifier _rsvidlist[];

private:
	tokenparser(const tokenparser&);
	void operator=(const tokenparser&);
};

}}} // end of namespace zas::utils
#endif // __CXX_ZAS_UTILS_EXPRVM_TOKEN_H__
#endif // UTILS_ENABLE_FBLOCK_EXPRVM
/* EOF */
