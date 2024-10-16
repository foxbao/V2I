/** @file exprvm_expr.h
 * definition of expression analysis
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_EXPRVM

#ifndef __CXX_ZAS_UTILS_EXPRVM_EXPR_H__
#define __CXX_ZAS_UTILS_EXPRVM_EXPR_H__

#include "exprvm_error.h"

// todo: re-arrange the following header
#include "./../hwgrph/inc/gbuf.h"

namespace zas {
namespace utils {
namespace exprvm {

enum expr_result
{
	expr_result_unknown,
	expr_result_success,
	expr_result_error,
	expr_result_endofexpr,
};

enum expr_tokentype
{
	expr_tokentype_unknown = 0,
	expr_tokentype_add,
	expr_tokentype_sub,
	expr_tokentype_mul,
	expr_tokentype_div,
	expr_tokentype_pow,
	expr_tokentype_number,
	expr_tokentype_left_bracket,
	expr_tokentype_right_bracket,
	expr_tokentype_variable,
};

template <typename T> class stack_
{
public:
	stack_() : _buf(32) {}

	void reset(void) {
		_buf.clear();
	}

	void push(T& node) {
		_buf.add(node);
	}

	bool gettop(T& node) {
		size_t count = _buf.getsize();
		if (!count) return false;
		node = _buf.buffer()[count - 1];
		return true;
	}

	bool pop(T& node) {
		bool ret = gettop(node);
		if (!ret) return ret;
		return _buf.remove(1);
	}

	bool empty(void) {
		return _buf.getsize() ? false : true;
	}

private:
	zas::hwgrph::gbuf_<T> _buf;
};

struct variable;

struct exprval_node
{
	enum {
		valtype_unknown = 0,
		valtype_uint,
		valtype_int,
		valtype_float,
		valtype_variable,
	};

	exprval_node() : flags(0) {}
	void set_negative(void);

	union {
		struct {
			uint32_t type : 8;
			uint32_t negative : 1;
		} f;
		uint32_t flags;
	};
	union {
		unsigned long val_uint;
		long val_int;
		double val_float;
		variable* var;
	};
	
	ZAS_DISABLE_EVIL_CONSTRUCTOR(exprval_node);
};
typedef stack_<exprval_node> exprval_stack;

class parser;
class scope_object;

class expr
{
public:
	enum flagdef {
		must_be_const = 1,
	};

	expr(parser& psr, tokenparser& parser, errorinfo& errinfo);
	~expr();

	void reset(void);
	int analyze(uint32_t flags);

private:
	expr_tokentype* get_next_allow_type_list(expr_tokentype curr);
	bool is_allowed(expr_tokentype type, expr_tokentype* list);
	bool is_in_allow_list(exprval_node& value, expr_tokentype& type,
		string& tokenstr, expr_tokentype *list);

	expr_result get_nexttoken(exprval_node& val,
		expr_tokentype& type, string& tokenstr);

	scope_object* parse_identifier(string& str);
	scope_object* find_identifier(string& str);

	int exprval_setvariable(scope_object* object, exprval_node& val);
	int handle_token(exprval_node& value, expr_tokentype& type,
		const string& tokenstr);

private:
	tokenparser &_token_parser;
	errorinfo &_errinfo;
	parser& _parser;
	exprval_stack _valstack;
};

}}} // end of namespace zas::utils::exprvm
#endif // __CXX_ZAS_UTILS_EXPRVM_EXPR_H__
#endif // UTILS_ENABLE_FBLOCK_EXPRVM
/* EOF */
