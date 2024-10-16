/** @file exprvm_error.cpp
 * implmentation of error handling for expression virtual machine
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_EXPRVM

#include <stdarg.h>
#include "inc/exprvm_error.h"

namespace zas {
namespace utils {
namespace exprvm {

static struct {
	const char *errstr;
	bool is_error;
} _errstr[] =
{
	/* eErrID_ImportFileNameWithoutLeftQuotation */
	{ "missing left quotation mark (\") for imported filename", true },

	/* eErrID_ImportFileNameWithoutRightQuotation */
	{ "missing right quotation mark (\") for imported filename", true },

	/* errid_invalid_number */
	{ "invalid number '%s'", true },

	/* errid_unrecgnized_statement */
	{ "'%s': the statement is invalid or not allowed here", true },

	/* errid_expect_left_bracket */
	{ "expect a '(' in the statement", true },

	/* errid_unsigned_not_a_decorator_of_float */
	{ "'unsigned' could not be used as a decorator of 'float'", false },

	/* errid_expect_variable_name */
	{ "expect a variable name in statement of %s", true },

	/* errid_number_exceed_limitation */
	{ "number exceeds its limitation result in loss of precision", false },

	/* errid_expect_variable_declaration_defore */
	{ "expect a variable declaration before '%s'", true },

	/* errid_previously_defined */
	{ "'%s' has been previously defined", true },

	/* errid_not_a_variable */
	{ "'%s' is not a variable", true },

	/* errid_unexpected_expression_token */
	{ "unexpected expression token '%s'", true },
};

errorinfo::errorinfo(tokenparser &p)
: _tokenparser(p)
{
}

void errorinfo::do_printerror(errinfoid id, va_list ap)
{
	printf("%s:%u:%u: %s: ",
		_tokenparser.get_filestack().get_filename().c_str(),
		_tokenparser.get_filestack().get_linenum(),
		_tokenparser.get_filestack().get_linepos(),
		(_errstr[(int)id].is_error) ? "error" : "warning"
	);
	vprintf(_errstr[(int)id].errstr, ap);
	printf("\n");
}

void errorinfo::printerror(errinfoid id, ...)
{
	va_list ap;

	va_start(ap, id);
	do_printerror(id, ap);
	va_end(ap);
}

void panic_out_of_memory(const char *f, unsigned int l)
{
	printf("Fatal error(at %s:%u): out of memory.\n", f, l);
	exit(1);
}

void panic_internal_error(const char *f, unsigned int l, const char *desc)
{
	printf("Internal error(at %s:%u): %s.\n", f, l, desc);
	exit(2);
}

}}} // end of namespace zas::utils::exprvm
#endif // UTILS_ENABLE_FBLOCK_EXPRVM
/* EOF */
