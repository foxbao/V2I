/** @file exprvm_expr.cpp
 * implementation of expression analysis
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_EXPRVM

#include "inc/exprvm.h"
#include "inc/exprvm_expr.h"

namespace zas {
namespace utils {
namespace exprvm {

void exprval_node::set_negative(void)
{
	switch (int(f.type))
	{
	case valtype_uint:
		val_uint = -val_uint; break;

	case valtype_int:
		val_int = -val_int; break;

	case valtype_float:
		val_float = -val_float; break;

	case valtype_variable:
		f.negative = 1; break;

	default: assert(0);
	}
}

expr::expr(parser& psr, tokenparser& parser, errorinfo& errinfo)
: _token_parser(parser)
, _errinfo(errinfo)
, _parser(psr)
{
}

expr::~expr()
{
}

bool expr::is_allowed(expr_tokentype type, expr_tokentype* list)
{
	if (NULL == list) return false;
	for (; *list != expr_tokentype_unknown; list++) {
		if (*list == type) return true;
	}
	return false;
}

bool expr::is_in_allow_list(exprval_node& value, expr_tokentype& type,
	string& tokenstr, expr_tokentype *list)
{
	if (is_allowed(type, list)) {
		return true;
	}
	// handle 1 [+ -3]
	if (expr_tokentype_sub == type)
	{
		if (!is_allowed(expr_tokentype_number, list)) {
			return false;
		}

		if (expr_result_success != get_nexttoken(value, type, tokenstr)
			|| expr_tokentype_number != type) {
			return false;
		}
		value.set_negative();
		return true;
	}
	return false;
}

expr_tokentype* expr::get_next_allow_type_list(expr_tokentype curr)
{
	static expr_tokentype _for_unknown[] = {
		expr_tokentype_number, expr_tokentype_left_bracket, expr_tokentype_unknown
	};
	static expr_tokentype _for_add[] = {
		expr_tokentype_number, expr_tokentype_left_bracket, expr_tokentype_unknown
	};
	static expr_tokentype _for_sub[] = {
		expr_tokentype_number, expr_tokentype_left_bracket, expr_tokentype_unknown
	};
	static expr_tokentype _for_mul[] = {
		expr_tokentype_number, expr_tokentype_left_bracket, expr_tokentype_unknown
	};
	static expr_tokentype _for_div[] = {
		expr_tokentype_number, expr_tokentype_left_bracket, expr_tokentype_unknown
	};
	static expr_tokentype _for_pow[] = {
		expr_tokentype_number, expr_tokentype_left_bracket, expr_tokentype_unknown
	};
	static expr_tokentype _for_number[] = {
		expr_tokentype_add, expr_tokentype_sub,
		expr_tokentype_mul, expr_tokentype_div,
		expr_tokentype_pow,
		expr_tokentype_right_bracket, expr_tokentype_unknown
	};
	static expr_tokentype _for_left_bracket[] = {
		expr_tokentype_number, expr_tokentype_left_bracket, expr_tokentype_unknown
	};
	static expr_tokentype _for_right_bracket[] = {
		expr_tokentype_add, expr_tokentype_sub,
		expr_tokentype_mul, expr_tokentype_div,
		expr_tokentype_pow,
		expr_tokentype_right_bracket, expr_tokentype_unknown
	};

	static expr_tokentype* list[] = {
		_for_unknown,		// expr_tokentype_unknown
		_for_add,			// expr_tokentype_add
		_for_sub,			// expr_tokentype_sub
		_for_mul,			// expr_tokentype_mul
		_for_div,			// expr_tokentype_div
		_for_pow,			// epxr_tokentype_pow
		_for_number,		// expr_tokentype_number
		_for_left_bracket,	// expr_tokentype_left_bracket
		_for_right_bracket,	// expr_tokentype_right_bracket
	};
	return list[int(curr)];
}

void expr::reset(void)
{
	_valstack.reset();
}

scope_object* expr::parse_identifier(string& str)
{
	scope_object* ret = find_identifier(str);
	if (!ret) return ret;

	if (ret->gettype() != sot_variable) {
		_errinfo.printerror(errid_not_a_variable, ret->getname());
		return NULL;
	}

	token tkn = _token_parser.gettoken();
	if (tkn.getid() == tokenid_double_colon) {
		_errinfo.printerror(errid_unrecgnized_statement,
			tkn.getname().c_str());
		return NULL;
	}
	_token_parser.token_pushback();
	return ret;
}

scope_object* expr::find_identifier(string& str)
{
	scope *s, *ns;
	scope_object* ret = NULL;
	token tkn = _token_parser.gettoken();

	str = tkn;
	if (tkn.getid() == tokenid_double_colon) {
		s = _parser.getglobal();
		assert(NULL != s);
		tkn = _token_parser.gettoken();
		if (tkn.gettype() != tokentype_identifier)
			return NULL;

		// find it in global scope
		ret = s->findobject(tkn.getname().c_str());
		if (!ret) {
			_errinfo.printerror(errid_not_a_variable, tkn.getname().c_str());
			return NULL;
		}
		return ret;
	}
	else if (tkn.gettype() == tokentype_identifier) {
		s = _parser.getscope();
		assert(NULL != s);
	}
	else {
		_errinfo.printerror(errid_not_a_variable, tkn.getname().c_str());
		return NULL;
	}

	// check if the identifier exists in the scope
	for (; s; s = s->parents()) {
		ret = s->findobject(tkn.getname().c_str());
		if (ret) return ret;
	}

	// namespace1::namespace2::variable
	// move to global scope and start matching
	string idn = tkn;
	for (s = _parser.getglobal();; s = ns)
	{
		scope* ns = s->findscope(idn.c_str());
		tkn = _token_parser.gettoken();

		if (tkn.getid() == tokenid_double_colon) {
			if (!ns || ns->gettype() != st_namespace) {
				_errinfo.printerror(errid_unrecgnized_statement, idn.c_str());
				return NULL;
			}
			str += tkn;
		}
		else {
			if (ns) {
				_errinfo.printerror(errid_unrecgnized_statement, idn.c_str());
				return NULL;
			}
			_token_parser.token_pushback();
			ret = s->recurse_findobject(idn.c_str());
			if (!ret) {
				_errinfo.printerror(errid_not_a_variable, idn.c_str());
				return NULL;
			}
			return ret;
		}
		tkn = _token_parser.gettoken();
		if (tkn.gettype() != tokentype_identifier) {
			_errinfo.printerror(errid_unrecgnized_statement, tkn.getname().c_str());
			return NULL;
		}
		idn = tkn, str += idn;
	}
	return NULL;
}

int expr::exprval_setvariable(scope_object* object, exprval_node& val)
{
	if (object->gettype() != sot_variable) {
		return -1;
	}
	variable* var = scope_object_downcast(variable, object);
	if (var->f.is_const) {
		switch (var->type)
		{
		case nt_uint:
			val.val_uint = var->value.as_uint;
			val.f.type = exprval_node::valtype_uint;
			break;
		case nt_int:
			val.val_int = var->value.as_int;
			val.f.type = exprval_node::valtype_int;
			break;
		case nt_float:
			val.val_float = var->value.as_float;
			val.f.type = exprval_node::valtype_float;
			break;
		default: assert(0);
		}
	} else {
		val.f.type = exprval_node::valtype_variable;
		val.var = var;
	}
	return 0;
}

expr_result expr::get_nexttoken(exprval_node& val,
	expr_tokentype& type, string& tokenstr)
{
	double value;
	numtype ntype;

	token tkn = _token_parser.gettoken();
	if (tokentype_number == tkn) {
		tokenstr = tkn;
		type = expr_tokentype_number;
		tkn.getnum(ntype, value);

		switch (ntype)
		{
		case nt_uint:
			if (value > __UINT64_MAX__) {
				_errinfo.printerror(errid_number_exceed_limitation);
			}
			val.f.type = exprval_node::valtype_uint;
			val.val_uint = (unsigned long)value;
			break;
		
		case nt_int:
			if (value > __INT64_MAX__) {
				_errinfo.printerror(errid_number_exceed_limitation);
			}
			val.f.type = exprval_node::valtype_int;
			val.val_int = (long)value;
			break;

		case nt_float:
			val.f.type = exprval_node::valtype_float;
			val.val_float = value;
			break;

		default: assert(0);
		}
		tokenstr = tkn;
		return expr_result_success;
	}

	tokenstr = tkn;
	switch (tkn.getid())
	{
	case tokenid_add:
		type = expr_tokentype_add;
		break;

	case tokenid_sub:
		type = expr_tokentype_sub;
		break;

	case tokenid_mul:
		type = expr_tokentype_mul;
		break;

	case tokenid_div:
		type = expr_tokentype_div;
		break;

	case tokenid_power:
		type = expr_tokentype_pow;
		break;

	case tokenid_left_bracket:
		type = expr_tokentype_left_bracket;
		break;

	case tokenid_right_bracket:
		type = expr_tokentype_right_bracket;
		break;

	default: {
		_token_parser.token_pushback();
		scope_object* obj = parse_identifier(tokenstr);
		if (!obj) return expr_result_error;
		exprval_setvariable(obj, val);
		type = expr_tokentype_variable;
		} break;
	}
	return expr_result_success;
}

int expr::handle_token(exprval_node& value, expr_tokentype& type,
	const string& tokenstr)
{
	if (expr_tokentype_number == type) {
		_valstack.push(value);
	}
	return 0;
}


int expr::analyze(uint32_t flags)
{
	string tokenstr;
	exprval_node value;
	expr_tokentype type;
	expr_result res = expr_result_unknown;
	expr_tokentype* next_allow_list =
		get_next_allow_type_list(expr_tokentype_unknown);

	while (expr_result_endofexpr != (res = get_nexttoken(value, type, tokenstr)))
	{
		if (expr_result_error == res) return -1;
		if (!is_in_allow_list(value, type, tokenstr, next_allow_list)) {
			_errinfo.printerror(errid_unexpected_expression_token,
				tokenstr.c_str());
			return -2;
		}
		printf("token: %s\n", tokenstr.c_str());
		next_allow_list = get_next_allow_type_list(type);
		if (handle_token(value, type, tokenstr)) {
			return false;
		}
	}
	return 0;
}

#if 0

bool expression::HandleToken(double& value, EExprTokenType& eType, const string& token)
{
	if (eExprTokenType_Number == eType)
		PushNumber(value);
	else if (IsOperator(eType))
	{
		if (IsOptStackEmpty())
		{
			if (!PushOperator(eType)) return false;
			return true;
		}

		EExprTokenType ePrevType = GetOpTop();
		assert(eExprTokenType_Unknown != ePrevType);

		if (GetPriority(ePrevType) >= GetPriority(eType))
		{
			// conduct a calculation
			double v1, v2, r;
			EExprTokenType op;
			if (!PopNumber(v2)) return false;
			if (!PopNumber(v1)) return false;
			if (!PopOperator(op)) return false;
			assert(IsOperator(op));
			if (!BasicCalc(v1, v2, op, r)) return false;
			if (!PushNumber(r)) return false;
		}
		if (!PushOperator(eType)) return false;
	}
	else if (eExprTokenType_LeftBracket == eType)
	{
		if (!PushOperator(eType))
			return false;
	}
	else if (eExprTokenType_RightBracket == eType)
	{
		EExprTokenType ePrevType;
		while (PopOperator(ePrevType))
		{
			if (eExprTokenType_LeftBracket == ePrevType)
				return true;
			assert(IsOperator(ePrevType));
			double v1, v2, r;
			if (!PopNumber(v2)) return false;
			if (!PopNumber(v1)) return false;
			if (!BasicCalc(v1, v2, ePrevType, r)) return false;
			if (!PushNumber(r)) return false;
		}
		m_cErrInfo.PrintError(eErrID_ExpressionMismatchWithBracket);
		return false;
	}
	else return false;
	return true;
}


bool expr::analyze(void)
{

	while (!IsOptStackEmpty())
	{
		double v1, v2, r;
		expr_tokentype op;
		if (!PopOperator(op)) return false;
		if (expr_tokentype_LeftBracket == op || expr_tokentype_RightBracket == op)
		{
			m_cErrInfo.PrintError(eErrID_ExpressionMismatchWithBracket);
			return false;
		}
		else if (!IsOperator(op))
		{
			m_cErrInfo.PrintError(eErrID_ExpressionInternalError);
			return false;
		}
		if (!PopNumber(v2)) return false;
		if (!PopNumber(v1)) return false;
		if (!BasicCalc(v1, v2, op, r)) return false;
		if (!PushNumber(r)) return false;
	}

	// finally get the result
	if (!PopNumber(m_Result)) return false;

	// the operator stack comes empty means
	// the number stack also become empty, make sure this happened
	assert(IsNumStackEmpty());
	return true;
}
#endif

}}} // end of namespace zas::utils::exprvm
#endif // UTILS_ENABLE_FBLOCK_EXPRVM
/* EOF */
