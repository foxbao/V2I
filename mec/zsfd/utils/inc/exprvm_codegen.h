/** @file exprvm_codegen.h
 * definition of expression code generation
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_EXPRVM

#ifndef __CXX_ZAS_UTILS_EXPRVM_CODEGEN_H__
#define __CXX_ZAS_UTILS_EXPRVM_CODEGEN_H__

#include "exprvm.h"

namespace zas {
namespace utils {
namespace exprvm {

class register_object : scope_object
{
public:
	register_object(int id);
private:
	int _id;
	int _lru_count;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(register_object);
};

class register_manager
{
public:
	enum {
		_MAX_NR_REGS = 64,
	};
	register_manager();
	static register_manager* inst(void);

private:
	listnode_t _inactive;
	listnode_t _active;
	listnode_t _free;
	int _count;
};

}}} // end of namespace zas::utils::exprvm
#endif // __CXX_ZAS_UTILS_EXPRVM_CODEGEN_H__
#endif // UTILS_ENABLE_FBLOCK_EXPRVM
/* EOF */
