/** @file exprvm_token.cpp
 * implementation of token objects for expression virtual machine
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_EXPRVM

#include <string.h>
#include "inc/exprvm_token.h"
#include "inc/exprvm_error.h"

namespace zas {
namespace utils {
namespace exprvm {

srcfilestack::srcfilenode::srcfilenode()
: _strbuf(NULL)
, _curstart(NULL)
, _curpos(NULL)
, _line_num(0)
, _line_pos(0)
{
}

srcfilestack::srcfilenode::~srcfilenode()
{
	if (NULL != _fp) {
		fclose(_fp);
		_fp = NULL;
	}
	if (NULL != _strbuf) {
		delete [] _strbuf;
		_strbuf = NULL;
	}
}

const string& srcfilestack::get_filename(void)
{
	static string _null = "<null>";
	srcfilenode *node = gettop();
	if (NULL == node) return _null;
	return node->_filename;
}

unsigned int srcfilestack::get_linenum(void)
{
	srcfilenode *node = gettop();
	if (NULL == node) return 0;
	return node->_line_num;
}

unsigned int srcfilestack::get_linepos(void)
{
	srcfilenode *node = gettop();
	if (NULL == node) return 0;
	return node->_line_pos;
}

static const char _space_identifier[] = " \t\r\n";
static const char _basic_identifier[] = "{}()[];,#+-*/^=:";

void srcfilestack::srcfilenode::killspace(void)
{	
	while (*_curpos && strchr(_space_identifier, *_curpos))
		_curpos++;
}

inline void srcfilestack::srcfilenode::omit_currline(void)
{
	_curpos = NULL;
}

bool srcfilestack::srcfilenode::omit_comments(void)
{
	if (*_curpos == '/')
	{
		char *tmp = _curpos + 1;
		if (!*tmp) return false;

		// handle the C++ style // comments
		if (*tmp == '/')
		{
			_curpos = tmp + 1;

			for (;;)
			{
				if (!*_curpos)	// an empty line
					break;

				// move to the end of the line to see if there is a '\\'
				size_t len = strlen(_curpos);
				if (_curpos[len - 1] == '\\')
				{
					// this means we need to load a new line, which is still a comment line
					if (!loadline())
						return false;
				}
				else break;
			}
			// just omit the current line
			omit_currline();
			return true;
		}
		else if (*tmp == '*')
		{
			_curpos = tmp + 1;

			// seeking for '*/'
			char* eoc;
			while (NULL == (eoc = strstr(_curpos, "*/")))
			{
				if (!loadline())
					return false;
			}
			_curpos = eoc + 2;
			return true;
		}
	}
	return false;
}

const char* srcfilestack::srcfilenode::check_double_colon(size_t& sz)
{
	sz = 2;
	char *ret = _curpos;
	if (_curpos[0] == ':' && _curpos[1] == ':')
	{
		_curpos += 2;
		return ret;
	}
	return NULL;
}

const char* srcfilestack::srcfilenode::check_number(size_t& sz)
{
	sz = 0;
	char *ret = _curpos, *start = ret;
	if (*start == '+' || *start == '-') { sz++; start++; }

	if ((*start < '0' || *start > '9') && *start != '.')
		return NULL;

	// at a high possibility of a number (here regard as a number)
	for (; (*start >= '0' && *start <= '9') || *start == '.'; ++start, sz++);

	// see if there is a 'e' or 'E'
	if (*start == 'e' || *start == 'E') ++start, ++sz;
	else { _curpos = start; return ret; }

	// see power of a number (for float)
	if (*start == '+' || *start == '-') ++start, ++sz;
	for (; *start >= '0' && *start <= '9'; ++start, sz++);

	_curpos = start;
	return ret;
}

const char *srcfilestack::srcfilenode::get_symbol(size_t& sz)
{
	do
	{
		if (NULL == _curpos)
			return NULL;

		if (*_curpos == '\0')
			return NULL;

		killspace();
		if (*_curpos == '\0')
			return NULL;
	}
	while (omit_comments());
	
	// this check must be prior to basic identifier check
	// this is to see if it is a double-colon ('::')
	const char *double_colon = check_double_colon(sz);
	if (double_colon) return double_colon;

	// check if it is a basic identifier
	if (strchr(_basic_identifier, *_curpos))
	{
		sz = 1;
		_line_pos = _curpos - _currline;
		return _curpos++;
	}

	// this is to see if it is a number
	const char *num_start = check_number(sz);
	if (num_start) return num_start;

	// as a normal case, find the end of an identifier
	char *end = _curpos;
	while (*end && !strchr(_space_identifier, *end)
		&& !strchr(_basic_identifier, *end))
		end++;
	
	sz = end - _curpos;
	const char *ret = _curpos;
	_line_pos = _curpos - _currline;
	_curpos = end;

	return ret;
}

srcfilestack::srcfilestack()
{
	listnode_init(_filelist);
}

srcfilestack::~srcfilestack()
{
	while (pop());
}

bool srcfilestack::pop(void)
{
	srcfilenode *node = gettop();
	if (!node) return false;

	listnode_del(node->_ownerlist);
	delete node;
	return true;
}

srcfilestack::srcfilenode* srcfilestack::gettop(void)
{
	if (listnode_isempty(_filelist))
		return NULL;

	listnode_t* item = _filelist.prev;
	return list_entry(srcfilenode, _ownerlist, item);
}

bool srcfilestack::is_fullname_exists(string fullname)
{
	listnode_t* item = _filelist.next;
	for (; item != &_filelist; item = item->next) {
		srcfilenode* node = list_entry(srcfilenode, _ownerlist, item);
		if (node->_strbuf) continue;
		if (node->_full_filename == fullname)
			return true;
	}
	return false;
}

bool srcfilestack::push(string fullname)
{
	FILE *fp = fopen(fullname.c_str(), "r");
	if (NULL == fp)
		return false;

	srcfilenode *node = new srcfilenode;
	node->_fp = fp;
	node->_strbuf = NULL;
	node->_full_filename = fullname;

	// extract the file name from full name
	const char *start = fullname.c_str();
	const char *end = fullname.c_str() + fullname.length();
	for (; end > start && *end != '\\' && *end != '/'; --end);
	if (*end == '\\' || *end == '/') ++end;

	node->_filename = fullname.assign(end, fullname.length() - (end - start));
	listnode_add(_filelist, node->_ownerlist);
	return true;
}

bool srcfilestack::push_from_string(const char* str, size_t sz)
{
	if (NULL == str) return false;
	if (!sz) {
		sz = strlen(str);
		if (!sz) return false;
	}

	srcfilenode* node = new srcfilenode;
	node->_fp = NULL;

	// copy the string
	node->_strbuf = new char [sz + 1];
	memcpy(node->_strbuf, str, sz);
	node->_strbuf[sz] = '\0';
	node->_curstart = node->_strbuf;
	node->_filename = "[from string]";
	node->_full_filename = "[from string]";

	listnode_add(_filelist, node->_ownerlist);
	return true;
}

bool srcfilestack::push_from_string(string str) {
	return push_from_string(str.c_str(), str.length());
}

bool srcfilestack::srcfilenode::loadline(void)
{
	if (_fp)
	{
		if (feof(_fp) || !fgets(_currline, LN_MAX_CHAR, _fp))
		{
			_curpos = NULL;
			return false;
		}

		size_t sz = strlen(_currline);
		if (_currline[sz - 1] != '\n')
			; // todo, error
		else _currline[sz - 1] = '\0';
	}
	else
	{
		if (NULL == _strbuf) return false;

		// check if it is the EOF
		if (_curstart > _strbuf) {
			if (_curstart[-1] == '\0') {
				_curpos = NULL;
				return false;
			}
		
		}

		char* end = _curstart;
		for (int i = 0; i < LN_MAX_CHAR - 1; ++i) {
			if (!*end || *end == '\n') break;
			++end;
		}
		// check if the line is too long
		if (*end && *end != '\n') return false;

		int len = end - _curstart;
		if (len) memcpy(_currline, _curstart, len);
		_currline[len] = '\0';
		_curstart = end + 1;
	}
	_curpos = _currline;
	_line_num++;
	return true;
}

bool srcfilestack::goto_nextline(void)
{
	srcfilenode *node = gettop();
	if (NULL == node) return false;

	size_t sz;
	while (node->get_symbol(sz));
	return true;
}

const char* srcfilestack::get_symbol(size_t& sz)
{
	srcfilenode *node;
	while (NULL != (node = gettop()))
	{
		const char *ret = node->get_symbol(sz);
		if (NULL == ret)
		{
			// try to load a new line
			if (!node->loadline())
			{
				// try to load a new file
				pop();
				node = gettop();
			}
		}
		else return ret;
	}
	return NULL;
}

tokenparser::reserved_identifier tokenparser::_rsvidlist[] =
{
	{ ",", tokentype_basic, tokenid_comma },
	{ ";", tokentype_basic, tokenid_semicolon },
	{ "{", tokentype_basic, tokenid_left_curly_bracket },
	{ "}", tokentype_basic, tokenid_right_curly_bracket },
	{ "[", tokentype_basic, tokenid_left_square_bracket },
	{ "]", tokentype_basic, tokenid_right_square_bracket },
	{ "(", tokentype_basic, tokenid_left_bracket },
	{ ")", tokentype_basic, tokenid_right_bracket },
	{ ":", tokentype_basic, tokenid_colon },
	{ "::", tokentype_basic, tokenid_double_colon },
	{ "+", tokentype_operator, tokenid_add },
	{ "-", tokentype_operator, tokenid_sub },
	{ "*", tokentype_operator, tokenid_mul },
	{ "/", tokentype_operator, tokenid_div },
	{ "^", tokentype_operator, tokenid_power },
	{ "=", tokentype_basic, tokenid_equal },
	{ "import", tokentype_reserved, tokenid_import },
	{ "const", tokentype_reserved, tokenid_const },
	{ "unsigned", tokentype_reserved, tokenid_unsigned },
	{ "int", tokentype_reserved, tokenid_int },
	{ "float", tokentype_reserved, tokenid_float },
	{ "void", tokentype_reserved, tokenid_void },
	{ "bool", tokentype_reserved, tokenid_boolean },
	{ "true", tokentype_reserved, tokenid_true },
	{ "false", tokentype_reserved, tokenid_false },
	{ "++", tokentype_operator, tokenid_selfinc },
	{ "--", tokentype_operator, tokenid_selfdec },
};

tokenparser::tokenparser(srcfilestack& filestack)
: _filestack(filestack)
, _flags(0)
{
}

tokenparser::~tokenparser()
{
}

const token& tokenparser::gettoken(void)
{
	if (_f.token_pushedback) {
		_f.token_pushedback = 0;
		return _currtoken;
	}

	size_t sz;
	const char *tkn = _filestack.get_symbol(sz);

	if (NULL == tkn)
	{
		_currtoken = token();
		return _currtoken;
	}

	// make a bsearch to identify the type of token
	string tmp(tkn, sz);
	reserved_identifier *tbl = NULL;
	int mid, start = 0;
	int end = sizeof(_rsvidlist) / sizeof(reserved_identifier) - 1;

	while (start <= end)
	{
		mid = (start + end) / 2;
		int ret = strcmp(tmp.c_str(), _rsvidlist[mid].idname);
		
		if (!ret) { tbl = _rsvidlist + mid; break; }
		else if (ret > 0) start = mid + 1;
		else end = mid - 1;
	}
	if (NULL == tbl) analyze_user_token(tkn, sz);
	else _currtoken = token(tbl->idname, tbl->type, tbl->id);
	return _currtoken;
}

inline bool tokenparser::isnum(char c) {
	return (c >= '0' && c <= '9') ? true : false;
}

bool tokenparser::setnum(const char *token, size_t sz, double &num, bool minus, bool is_float)
{
	if (minus)
	{
		_currtoken._data.num.value = -num;
		_currtoken._data.num.type = (is_float) ? nt_float : nt_int;
		_currtoken._id = (is_float) ? tokenid_number_float : tokenid_num_int;
	}
	else
	{
		_currtoken._data.num.value = num;
		_currtoken._data.num.type = (is_float) ? nt_float : nt_uint;
		_currtoken._id = (is_float) ? tokenid_number_float : tokenid_number_uint;
	}
	_currtoken._name.assign(token, sz);
	_currtoken._type = tokentype_number;
	return true;
}

bool tokenparser::errnum(const char *token, size_t sz)
{
	string tmp;
	tmp.assign(token, sz);
	errorinfo errinfo(*this);
	errinfo.printerror(errid_invalid_number, tmp.c_str());
	return false;
}

bool tokenparser::check_as_identifier(const char *tkn, size_t sz)
{
	if (isnum(*tkn))
		return false;

	size_t size = 0;
	const char *start = tkn;

	for (;sz && ((*tkn >= 'a' && *tkn <= 'z') ||
		(*tkn >= 'A' && *tkn <= 'Z') || isnum(*tkn)
		|| *tkn == '_'); tkn++, sz--, ++size);

	if (sz) return false;

	string name;
	name.assign(start, size);
	_currtoken = token(name, tokentype_identifier, tokenid_user);
	return true;
}

bool tokenparser::check_as_num(const char *token, size_t sz)
{
	double num = 0;
	bool minus = false;
	bool is_float = false;

	size_t org_sz = sz;
	const char *org_token = token;

	if (*token == '+') { minus = false; ++token; --sz; }
	else if (*token == '-') { minus = true; ++token; --sz; }
	if (!sz) return false;	// not a number

	// analyze the [100].32e+12 part
	const char *part1 = token;
	for (; sz && isnum(*part1); ++part1, --sz)
		num = num * 10 + (*part1 - '0');
	if (!sz) return setnum(org_token, org_sz, num, minus, is_float);

	if (part1 == token)
	{
		// see if there is a ".", like -.2, or there is an error
		if (*part1 != '.') return false; // not a number
	}

	token = part1;
	// like -10.2
	if (*token == '.')
	{
		// handle 100.[32]e+12
		is_float = true;
		double tmp = 10;
		for (++token, --sz; sz && isnum(*token); ++token, --sz, tmp *= 10)
			num += double(*token - '0') / tmp;
	}
	if (!sz) return setnum(org_token, org_sz, num, minus, is_float);

	// handle 100.32[e]+12
	if (*token != 'e' && *token != 'E')
		return errnum(org_token, org_sz);

	// must be a float number
	is_float = true;
	token++; sz--;
	if (!sz) return errnum(org_token, org_sz);

	// handle 100.32e[+]12
	bool minus_power = false;
	if (*token == '+') { token++; sz--; }
	else if (*token == '-') { minus_power = true; token++; sz--; }
	if (!sz) return errnum(org_token, org_sz);

	// handle 100.32e+[12]
	unsigned int power = 0;
	for (; sz && isnum(*token); ++token, --sz)
		power = power * 10 + (*token - '0');
	if (sz) return errnum(org_token, org_sz);
	if (power > 100) return errnum(org_token, org_sz);

	for (; power; --power)
		num = (minus_power) ? (num / 10) : (num * 10);

	return setnum(org_token, org_sz, num, minus, is_float);
}

void tokenparser::analyze_user_token(const char *tkn, size_t sz)
{
	if (check_as_num(tkn, sz))
		return;
	else if (check_as_identifier(tkn, sz))
		return;
	else {
		string name;
		name.assign(tkn, sz);
		_currtoken = token(name, tokentype_unknown, tokenid_unknown);
	}
}

int tokenparser::compare(const void* a, const void* b)
{
	auto *pa = (tokenparser::reserved_identifier*)a;
	auto *pb = (tokenparser::reserved_identifier*)b;
	return strcmp(pa->idname, pb->idname);
}

void tokenparser::initialize(void)
{
	qsort(_rsvidlist, sizeof(_rsvidlist) / sizeof(tokenparser::reserved_identifier),
		sizeof(tokenparser::reserved_identifier), tokenparser::compare);
}

void parser_test(void);

static class tokenparser_initializer
{
public:
	tokenparser_initializer() {
		tokenparser::initialize();
		parser_test();
	}
} ___initializer;

}}} // end of namespace zas::utils::exprvm
#endif // UTILS_ENABLE_FBLOCK_EXPRVM
/* EOF */
