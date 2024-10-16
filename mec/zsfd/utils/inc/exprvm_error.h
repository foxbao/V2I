/** @file exprvm_error.h
 * definition of error handling for expression virtual machine
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_EXPRVM

#ifndef __CXX_ZAS_UTILS_EXPRVM_ERROR_H__
#define __CXX_ZAS_UTILS_EXPRVM_ERROR_H__

#include "exprvm_token.h"

namespace zas {
namespace utils {
namespace exprvm {

#define exprvm_fatalerror	(-999)

enum errinfoid
{
	errid_import_filename_without_left_quotation = 0,
	errid_import_filename_without_right_quotation,
	errid_invalid_number,
	errid_unrecgnized_statement,
	errid_expect_left_bracket,
	errid_unsigned_not_a_decorator_of_float,
	errid_expect_variable_name,
	errid_number_exceed_limitation,
	errid_expect_variable_declaration_defore,
	errid_previously_defined,
	errid_not_a_variable,
	errid_unexpected_expression_token,
};

class errorinfo
{
public:
	errorinfo(tokenparser &p);

	void printerror(errinfoid eid, ...);

private:
	void do_printerror(errinfoid eid, va_list ap);
	tokenparser& _tokenparser;
};

}}} // end of namespace zas::utils::exprvm
#endif // __CXX_ZAS_UTILS_EXPRVM_ERROR_H__
#endif // UTILS_ENABLE_FBLOCK_EXPRVM
/* EOF */
