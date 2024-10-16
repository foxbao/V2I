/** @file exprvm_token.h
 * implementation of all objects for express virtual machine
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_EXPRVM

#include "inc/exprvm.h"

namespace zas {
namespace utils {
namespace exprvm {

scope_object::scope_object(int type)
: _flags(0)
, _scope(NULL)
{
	_f.id = type;
}

scope_object::~scope_object()
{
}

scope::scope()
: _parents(NULL)
, _flags(0)
{
	listnode_init(_children);
	listnode_init(_objects);
}

scope::~scope()
{
}

void scope::release_tree(void)
{
	listnode_t tmp = LISTNODE_INITIALIZER(tmp);
	// todo:
}

scope* scope::create_function_scope(bool entry)
{
	scope* s = new scope();
	assert(NULL != s);

	s->_f.type = st_function;
	s->_f.entry = entry ? 1 : 0;

	listnode_init(s->_children);
	return s;
}

scope_object* scope::findobject(const char* name)
{
	scope_object* ret;
	if (!name || !*name) return NULL;

	listnode_t* item = _objects.next;
	for (; item != &_objects; item = item->next) {
		ret = list_entry(scope_object, _ownerlist, item);
		if (!strcmp(ret->getname(), name)) {
			return ret;
		}
	}
	return NULL;
}

scope* scope::findscope(const char* name)
{
	scope* ret;
	if (!name || !*name) return NULL;

	listnode_t* item = _children.next;
	for (; item != &_children; item = item->next) {
		ret = list_entry(scope, _sibling, item);
		if (_name == name) return ret;
	}
	return NULL;
}

parser::parser(tokenparser& tknparser)
: _token_parser(tknparser)
, _errinfo(_token_parser)
, _expr_parser(*this, tknparser, _errinfo)
, _global(new scope())
, _flags(0)
{
	_cur_scope = _global;
}

parser::~parser()
{
	if (_global) {
		_global->release_tree();
		_global = NULL;
	}
}

int parser::parse(void)
{
	int ret, err = 0;
	token tkn = _token_parser.gettoken();
	while (tokenid_unknown != tkn)
	{
		switch (tkn.getid())
		{
		case tokenid_import:
			break;

		case tokenid_user:
			ret = parse_token_user(tkn);
			break;
		}
		if (ret) {
			if (ret == exprvm_fatalerror)
				return exprvm_fatalerror;
			++err;
		}
		tkn = _token_parser.gettoken();
	}
	return -err;
}

int parser::parse_token_user(token& tkn)
{
	if (tkn.gettype() == tokentype_identifier) {
		// check if it is "input"
		if (tkn.getname() == "input") {
			// parse the main function like:
			// input(int a)->output(int b) {}
			return parse_entry_function();
		}
		else {
			// it may be a function without return value, like
			// add_value(int c)
			return parse_function(tkn);
		}
	} else {
		_errinfo.printerror(errid_unrecgnized_statement, tkn.getname().c_str());
		_token_parser.omit_currline();
		return 1;
	}
	return 0;
}

int parser::parse_entry_function(void)
{
	// create the scope
	scope* s = push_scope(scope::create_function_scope(true));
	assert(NULL != s);

	// input[(]int a, float b)->output(int c)
	token tkn = _token_parser.gettoken();
	if (tkn.getid() != tokenid_left_bracket) {
		_errinfo.printerror(errid_expect_left_bracket);
		_token_parser.omit_currline();
		pop_scope(true);
		return 1;
	}

	int ret, varible_count = 0;
	tkn = _token_parser.gettoken();
	for (;; tkn = _token_parser.gettoken())
	{
		tokenid id = tkn.getid();
		if (id == tokenid_right_bracket) break;
		if (id == tokenid_comma) {
			// a comma must be after a variable
			if (!varible_count) {
				_errinfo.printerror(errid_expect_variable_declaration_defore,
				tkn.getname().c_str());
			} continue;
		}

		if (id == tokenid_void) {
			if (varible_count) {
				_errinfo.printerror(errid_unrecgnized_statement,
				tkn.getname().c_str());
			} continue;
		}

		_token_parser.token_pushback();
		if (parse_entry_function_variable()) {
			ret = 2; // error but we continue parsing
			continue;
		}
		// a valid variable has been parsed
		++varible_count;
	}
	if (ret) pop_scope(true);
	return ret;
}

uint32_t parser::to_exprflags(uint32_t flags)
{
	uint32_t ret = 0;
	if (flags & setval_must_const) {
		ret |= expr::must_be_const;
	}
	return ret;
}

// the method shall return with a token loaded
variable* parser::parse_variable_declaration(uint32_t flags, const char* statname)
{
	variable* varobj = new variable();
	assert(NULL != varobj);
	token tkn = _token_parser.gettoken();

	for (varobj->type = nt_unknown; varobj->type == nt_unknown;)
		switch (tkn.getid()) {
	// [const] int unsigned a = 10
	case tokenid_const:
		varobj->f.is_const = 1;
		tkn = _token_parser.gettoken();
		break;

	// const [unsigned] int a = 10
	case tokenid_unsigned:
		varobj->f.is_unsigned = 1;
		tkn = _token_parser.gettoken();
		break;

	// const unsigned [int] a = 10
	case tokenid_int:
		varobj->type = (varobj->f.is_unsigned)
			? nt_uint : nt_int;
		break;

	case tokenid_float:
		if (varobj->f.is_unsigned) {
			_errinfo.printerror(errid_unsigned_not_a_decorator_of_float);
			varobj->f.is_unsigned = 0;
		}
		varobj->type = nt_float;
		break;

	default:
		_errinfo.printerror(errid_unrecgnized_statement,
			tkn.getname().c_str());
		tkn = _token_parser.gettoken();
		goto error;
	}

	// const unsigned int [a] = 10
	tkn = _token_parser.gettoken();
	if (tkn.gettype() != tokentype_identifier) {
		_errinfo.printerror(errid_expect_variable_name, statname);
		tkn = _token_parser.gettoken();
		goto error;
	}

	// check if the name exists
	varobj->name = tkn.getname();
	if (getscope()->findobject(varobj->name.c_str())) {
		_errinfo.printerror(errid_previously_defined,
			tkn.getname().c_str());
		goto error;
	}
	
	// const unsigned int a[[]100]= { 0 }
	tkn = _token_parser.gettoken();
	if (tkn.getid() == tokenid_left_square_bracket) {
		_expr_parser.analyze(to_exprflags(flags));
	}

	// const unsigned int a[100] [=] {0};
	tkn = _token_parser.gettoken();
	if (tkn.getid() == tokenid_equal) {
	}
	return varobj;

error:
	delete varobj;
	return NULL;
}

int parser::parse_entry_function_variable(void)
{
	variable* var = parse_variable_declaration(
		setval_allowed | setval_must_const,
		"'main function' defintion");
	if (NULL == var) return -1;

	return 0;
}

int parser::parse_function(token& tkn)
{
	return 0;
}

void parser_test(void)
{
#if 0
	const char* sample =
		"input(unsigned float a[1+a+b], int b)->output(int result) {"
		"	result = a * b;"
		"}";
	
	srcfilestack file_stack;
	tokenparser token_parser(file_stack);
	parser psr(token_parser);

	file_stack.push_from_string(sample);
	psr.parse();
#endif
}

}}} // end of namespace zas::utils::exprvm
#endif // UTILS_ENABLE_FBLOCK_EXPRVM
/* EOF */
