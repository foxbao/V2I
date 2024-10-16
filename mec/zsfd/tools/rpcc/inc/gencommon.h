/** @file inc/gencommon.h
 * definition of common head file generation for zrpc
 */

#ifndef __RPC_RPCC_CODEGENCOMMON_H__
#define __RPC_RPCC_CODEGENCOMMON_H__

#include "grammar.h"

struct module_userdef_array_info
{
	list_item ownerlist;
	string userdef_class_name;
};

struct included_module_node
{
	list_item ownerlist;
	module* mod;
};

extern const char* protobuf_types[];

// common functions
void method_retval_protobuf_type_name(
	module*, TInterfaceNode*, string&);
void variable_protobuf_type_name(
	module*, TVariableObject*, string&);

#endif /* __RPC_RPCC_CODEGENCOMMON_H__ */
/* EOF */