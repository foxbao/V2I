/** @file exprvm_codegen.cpp
 * implementation of expression analysis
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_EXPRVM

#include "inc/exprvm_codegen.h"

namespace zas {
namespace utils {
namespace exprvm {

register_object::register_object(int id)
: scope_object(sot_register)
, _id(id)
, _lru_count(0)
{
	assert(_id < register_manager::_MAX_NR_REGS);
}

register_manager::register_manager()
: _count(0)
{
	listnode_init(_inactive);
	listnode_init(_active);
	listnode_init(_free);
}

register_manager* register_manager::inst(void)
{
	static register_manager srm;
	return &srm;
}

}}} // end of namespace zas::utils::exprvm
#endif // UTILS_ENABLE_FBLOCK_EXPRVM
/* EOF */
