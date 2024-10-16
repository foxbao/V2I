/** @file inc/cgjava-module.h
 * implementation of c++ code generation of module for zrpc
 */

#include <assert.h>
#include <string.h>

#include "error.h"
#include "parser.h"
#include "codegenjava.h"
#include "utilities.h"
#include "config.h"

using namespace std;
using namespace utilities;

userdeftype_object_java_code_generator::userdeftype_object_java_code_generator(
	module* mod, const string& name)
	: userdef_type_object(mod, name)
	, _protobuf_count(0)
{
}

userdeftype_object_java_code_generator::~userdeftype_object_java_code_generator()
{
}

int userdeftype_object_java_code_generator::generate_global_scope_header_file_code(void)
{
	module* mod = GetModule();
	if (nullptr == mod) return 1;

	bool namespace_created = false;
	auto& header = mod->UserDefTypeItemHeader();
	for (int i = 0; i < HASH_MAX; ++i) {
		list_item &h = header[i];
		list_item* next = h.getnext();
		for (; next != &h; next = next->getnext()) {
			auto *obj = LIST_ENTRY(TGrammarObject, m_OwnerList, next);

			// create the namespace first
			if (!namespace_created) {
				// check if it is generated
				if (obj->test_flags(grammer_object_flags_userdef_type_pb_generated)) {
					return 0;
				}
				mod->fputs(srcfile_module_proxy_header,
				"// protobuffer version userdef type declaration\n");
				mod->fprintf(srcfile_module_proxy_header,
				"namespace %s_pbf {\n", mod->getname().c_str());
				namespace_created = true;
			}

			// write the current user defined type probuffer declaration
			mod->fprintf(srcfile_module_proxy_header, "\tclass %s;\n\tclass %s_array;\n",
				obj->getname().c_str(), obj->getname().c_str());

			// mark the flag
			obj->set_flags(grammer_object_flags_userdef_type_pb_generated);
		}
	}

	if (namespace_created) {
		mod->fputs(srcfile_module_proxy_header, "};\n\n");
	}
	return 0;
}

bool userdeftype_object_java_code_generator::global_scope_generate_code(void)
{
	if (generate_global_scope_header_file_code()) {
		return false;
	}
	return true;
}

int userdeftype_object_java_code_generator::generate_class_dependency(void)
{
	module* mod = GetModule();
	if (nullptr == mod) return 1;

	auto* mod_codegen = zrpc_downcast( \
		module, module_object_java_code_generator, mod);
	
	// variable list
	list_item *item = VariableListHeader().getnext();
	for (; item != &VariableListHeader(); item = item->getnext())
	{
		auto* var = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		if (!is_array(var->m_ArrayType)) continue;

		// if the variable is an array, check if the array definition
		// has been created
		// we only care about "userdef" and "typedefed"
		if (var->_basic_type == eBasicObjType_UserDefined) {
			if (!mod_codegen->get_userdef_array_info(var->getname().c_str())) {
				// if (generate_generic_array(var)) {
				// 	return 2;
				// }
				if (mod_codegen->create_userdef_array_info(
					var->getname().c_str())) {
					return 3;
				}
			}
		}
		else if (var->_basic_type == eBasicObjType_Typedefed) {
			// todo:
		}
	}
	return 0;
}


int userdeftype_object_java_code_generator::generate_variable_class_boolean(
	TVariableObject *obj)
{
	return 0;
}

int userdeftype_object_java_code_generator::generate_variable_class(
	TVariableObject *obj)
{
	assert(nullptr != obj);

	int ret = 1;
	switch (obj->_basic_type) {
	case basic_objtype_boolean:
		ret = generate_variable_class_boolean(obj);
		break;

	case basic_objtype_uint8:
	case eBasicObjType_Int16:
	case eBasicObjType_Int32:
	case eBasicObjType_Int64:
	case eBasicObjType_UInt32:
	case eBasicObjType_Float:
		ret = generate_variable_class_int(obj, obj->_basic_type);
		break;

	case eBasicObjType_Enum:
		break;

	case eBasicObjType_String:
		ret = generate_variable_class_string(obj, obj->_basic_type);
		break;

	case eBasicObjType_Stream:
		break;

	case eBasicObjType_Typedefed:
		break;

	case eBasicObjType_UserDefined:
		ret = generate_variable_class_userdefined(obj, obj->_basic_type);
		break;

	case eBasicObjType_Interface:
		break;

	default: return 33;
	}
	return ret;
}

int userdeftype_object_java_code_generator::generate_variable_class_constructor(
	TVariableObject *obj, bool param)
{
	assert(nullptr != obj);

	int ret = 0;
	switch (obj->_basic_type) {
	case basic_objtype_boolean:
		break;

	case basic_objtype_uint8:
	case eBasicObjType_Int16:
	case eBasicObjType_Int32:
	case eBasicObjType_Int64:
	case eBasicObjType_UInt32:
	case eBasicObjType_Float:
		break;

	case eBasicObjType_Enum:
		break;

	case eBasicObjType_String:
		break;

	case eBasicObjType_Stream:
		break;

	case eBasicObjType_Typedefed:
		break;

	case eBasicObjType_UserDefined:
		ret = generate_variable_class_constructor_userdefined(obj,
			obj->_basic_type, param);
		break;

	case eBasicObjType_Interface:
		break;

	default: return 33;
	}
	return ret;
}

int userdeftype_object_java_code_generator::generate_protobuf_variable_class(
	TVariableObject *obj)
{
	assert(nullptr != obj);
	module* mod = GetModule();
	if (nullptr == mod) return 1;

	const char** types = protobuf_types;
	switch (obj->_basic_type)
	{
	case eBasicObjType_Stream:
	case eBasicObjType_Enum:
		mod->fprintf(mod->getname().c_str(), srcfile_module_protobuf, "	%s %s = %u;\n",
			types[obj->_basic_type], obj->m_cName.c_str(), ++_protobuf_count);
		break;
	case eBasicObjType_Interface:
		if (is_array(obj->m_ArrayType)) {
			mod->fputs(mod->getname().c_str(), srcfile_module_protobuf, "\trepeated ");
		} else mod->fputs(mod->getname().c_str(),srcfile_module_protobuf, "\t");
		mod->fprintf(mod->getname().c_str(), srcfile_module_protobuf, 
			"%s %s = %u;\n", obj->m_pUserDefType->getname().c_str(), obj->m_cName.c_str(), ++_protobuf_count);
		break;
	case eBasicObjType_Typedefed:
	case eBasicObjType_UserDefined:
		if (is_array(obj->m_ArrayType)) 
			mod->fputs(mod->getname().c_str(), srcfile_module_protobuf, "\trepeated ");
		else mod->fputs(mod->getname().c_str(),srcfile_module_protobuf, "\t");
		mod->fprintf(mod->getname().c_str(),srcfile_module_protobuf, "%s %s = %u;\n", obj->m_pUserDefType->getname().c_str(), obj->m_cName.c_str(),
			++_protobuf_count);
		break;
	default:
		if (is_array(obj->m_ArrayType)) {
			mod->fputs(mod->getname().c_str(), srcfile_module_protobuf, "\trepeated ");
		}
		else mod->fputs(mod->getname().c_str(), srcfile_module_protobuf, "\t");
		mod->fprintf(mod->getname().c_str(), srcfile_module_protobuf, "%s %s = %u;\n",
			types[obj->_basic_type], obj->m_cName.c_str(), ++_protobuf_count);
		break;
	}
	return 0;
}

bool userdeftype_object_java_code_generator::generate_code(void)
{
	if (test_flags(grammar_object_flags_code_generated)) {
		return true;
	}
	set_flags(grammar_object_flags_code_generated);

	if (!generate_related_object_code()) {
		return false;
	}
	if (generate_class_definition()) {
		return false;
	}
	return true;
}

int userdeftype_object_java_code_generator::generate_class_definition(void)
{
	module* mod = GetModule();
	if (nullptr == mod) return 1;
	if (generate_class_dependency()) return 2;

	const char* objname = getname().c_str();
	const char* modname = mod->getname().c_str();


	// generate related protobuffer defintion file
	mod->fprintf(mod->getname().c_str(), srcfile_module_protobuf,
		"message %s\n{\n", objname);

	// variable list
	list_item *item = VariableListHeader().getnext();
	for (; item != &VariableListHeader(); item = item->getnext())
	{
		auto* var = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		if (generate_protobuf_variable_class(var)) {
			return 3;
		}
	}

	// finalize the class generation
	mod->fputs(mod->getname().c_str(), srcfile_module_protobuf, "}\n\n");	
	return 0;
}

int userdeftype_object_java_code_generator::generate_variable_class_int(
	TVariableObject *obj, basic_objtype type)
{
	module* mod = GetModule();
	if (nullptr == mod) return 1;
	
	const char* etype_str = (!obj->m_pEnumDefType) ? nullptr
		: obj->m_pEnumDefType->get_fullname(mod).c_str();

	const char* types[] = {
		nullptr,	// unknown
		nullptr,	// bool
		"uint8_t",	// uint8
		"int16_t",	// int16
		"int32_t",	// int32
		"int64_t",	//long
		"uint32_t",
		"double",	// Float
		nullptr,	// stream
		nullptr,	// string
		etype_str,	// enum
		nullptr,
		nullptr,
		nullptr,
	};
	const char* types_array[] = {
		nullptr,	// unknown
		nullptr,	// bool
		"uint32",	// uint8
		"int32",	// int16
		"int32",	// int32
		"int64",	//long
		"uint32",
		"double",	// Float
		nullptr,	// stream
		nullptr,	// string
		etype_str,	// enum
		nullptr,
		nullptr,
		nullptr,
	};
	
	const char* objname = getname().c_str();
	const char* var_name = obj->m_cName.c_str();

	return 0;
}

int userdeftype_object_java_code_generator::generate_variable_class_userdefined(
	TVariableObject *obj, basic_objtype type)
{
	module* mod = GetModule();
	if (nullptr == mod) return 1;
	
	const char* objname = getname().c_str();
	const char* var_name = obj->m_cName.c_str();
	assert(nullptr != obj->m_pUserDefType);
	const char* objtype = obj->m_pUserDefType->getname().c_str();
	const char* modname = obj->m_pUserDefType->GetModule()->getname().c_str();

	return 0;
}

int
userdeftype_object_java_code_generator::
generate_variable_class_string(TVariableObject *obj, basic_objtype type)
{
	module* mod = GetModule();
	if (nullptr == mod) return 1;
	
	const char* objname = getname().c_str();
	const char* var_name = obj->m_cName.c_str();
	const char* modname = mod->getname().c_str();
	const char* var_type = "string";

	return 0;
}

int
userdeftype_object_java_code_generator::
generate_variable_class_constructor_userdefined(
	TVariableObject *obj, basic_objtype type, bool param)
{
	module* mod = GetModule();
	if (nullptr == mod) return 1;
	
	// const char* objname = getname().c_str();
	const char* var_name = obj->m_cName.c_str();
	// const char* modname = mod->getname().c_str();
	// assert(nullptr != obj->m_pUserDefType);
	// const char* objtype = obj->m_pUserDefType->getname().c_str();

	if (is_array(obj->m_ArrayType))
	{
		// TBD.
	}
	else if (eArrayType_NotArray == obj->m_ArrayType.eArrayType)
	{
		if (param) {
			mod->fprintf(srcfile_module_proxy_structs,
				", %s(wrapper, pbfmsg->mutable_%s())\n",
				var_name, var_name);
		} else {
			mod->fprintf(srcfile_module_proxy_structs,
				"	%s.___bind(_pbf_wrapper, _pbfobj->mutable_%s());\n",
				var_name, var_name);
		}
	}
	else return 2;
	return 0;
}

int userdeftype_object_java_code_generator::generate_generic_array(TVariableObject* var)
{
	module* mod = GetModule();
	if (nullptr == mod) return 1;

	const char* modname = mod->getname().c_str();
	assert(nullptr != var->m_pUserDefType);
	const char* objname = var->m_pUserDefType->getname().c_str();

	return 0;
}
