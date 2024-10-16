/** @file inc/codegencxx.h
 * implementation of c++ code generation for zrpc
 */

#include <assert.h>
#include <string.h>

#include "error.h"
#include "parser.h"
#include "codegencxx.h"
#include "utilities.h"
#include "config.h"

using namespace std;
using namespace utilities;

typedef_object_cpp_code_generator::typedef_object_cpp_code_generator(
	module* mod, const string& name, basic_objtype type)
	: typedef_object(mod, name, type)
{
}

typedef_object_cpp_code_generator::typedef_object_cpp_code_generator(
	module* mod, const string& name, typedef_object *reftype)
	: typedef_object(mod, name, reftype)
{
}

typedef_object_cpp_code_generator::typedef_object_cpp_code_generator(
	module* mod,
	const string& name, userdef_type_object *reftype)
	: typedef_object(mod, name, reftype)
{
}

typedef_object_cpp_code_generator::typedef_object_cpp_code_generator(
	module* mod,
	const string& name, enumdef_object* enum_def)
	: typedef_object(mod, name, enum_def)
{
}

typedef_object_cpp_code_generator::typedef_object_cpp_code_generator(
	module* mod, const string& name, interface_object* ifobj)
	: typedef_object(mod, name, ifobj)
{
}

typedef_object_cpp_code_generator::~typedef_object_cpp_code_generator()
{
}

bool typedef_object_cpp_code_generator::generate_code(void)
{
	if (test_flags(grammar_object_flags_code_generated)) {
		return true;
	}
	set_flags(grammar_object_flags_code_generated);

	if (!generate_related_object_code())
		return false;

	module *obj = GetModule();
	if (NULL == obj) return false;

	assert(array_type_unknown != m_ArrayType.eArrayType);
	switch (_basic_type)
	{
	case basic_objtype_boolean:
		if (array_type_fixed == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef bool_array %s(%u);\n",
			getname().c_str(), m_ArrayType.ArraySize);
		else if (array_type_variable == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef bool_array %s;\n",
			getname().c_str());
		else obj->fprintf(srcfile_module_proxy_header,
			"typedef bool %s;\n", getname().c_str());
		break;

	case basic_objtype_uint8:
		if (array_type_fixed == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef uint32_array %s[%u];\n",
			getname().c_str(), m_ArrayType.ArraySize);
		else if (array_type_variable == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef uint32_array %s;\n",
			getname().c_str());
		else obj->fprintf(srcfile_module_proxy_header, "typedef uint8_t %s;\n", getname().c_str());
		break;

	case eBasicObjType_Int32:
		if (array_type_fixed == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef int32_array %s(%u);\n",
			getname().c_str(), m_ArrayType.ArraySize);
		else if (array_type_variable == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef int32_array %s;\n",
			getname().c_str());
		else obj->fprintf(srcfile_module_proxy_header, "typedef int32_t %s;\n", getname().c_str());
		break;

	case eBasicObjType_Int16:
		if (array_type_fixed == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef int32_array %s(%u);\n",
			getname().c_str(), m_ArrayType.ArraySize);
		else if (array_type_variable == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef int32_array %s;\n",
			getname().c_str());
		else obj->fprintf(srcfile_module_proxy_header, "typedef int32_t %s;\n", getname().c_str());
		break;

	case eBasicObjType_Int64:
		if (array_type_fixed == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef int64_array %s[%u];\n",
			getname().c_str(), m_ArrayType.ArraySize);
		else if (array_type_variable == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef int64_array %s;\n",
			getname().c_str());
		else obj->fprintf(srcfile_module_proxy_header, "typedef int64_t %s;\n", getname().c_str());
		break;

	case eBasicObjType_UInt32:
		if (array_type_fixed == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef uint32_array %s(%u);\n",
			getname().c_str(), m_ArrayType.ArraySize);
		else if (array_type_variable == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef uint32_array %s;\n",
			getname().c_str());
		else obj->fprintf(srcfile_module_proxy_header, "typedef uint32_t %s;\n", getname().c_str());
		break;

	case eBasicObjType_Enum:
		assert(NULL != m_pEnumDef);
		if (array_type_fixed == m_ArrayType.eArrayType) {
			// obj->fprintf(srcfile_module_proxy_header, "typedef %s %s[%u];\n",
			// m_pEnumDef->GetFullName(obj).c_str(),GetName().c_str(), m_ArrayType.ArraySize);
		} else if (array_type_variable == m_ArrayType.eArrayType) {
			// obj->fprintf(srcfile_module_proxy_header, "typedef map<uint, %s> %s;\n",
			// m_pEnumDef->GetFullName(obj).c_str(), GetName().c_str());
		} else {
			obj->fprintf(SrcFileHdr, "typedef %s %s;\n",
			m_pEnumDef->get_fullname(obj).c_str(), getname().c_str());
		}
		break;

	case eBasicObjType_Float:
		if (array_type_fixed == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef double_array %s(%u);\n",
			getname().c_str(), m_ArrayType.ArraySize);
		else if (array_type_variable == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef double_array %s;\n",
			getname().c_str());
		else obj->fprintf(srcfile_module_proxy_header, "typedef double %s;\n", getname().c_str());
		break;
		

	case eBasicObjType_String:
		if (array_type_fixed == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef string_array %s(%u);\n",
			getname().c_str(), m_ArrayType.ArraySize);
		else if (array_type_variable == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef string_array %s;\n",
			getname().c_str());
		else obj->fprintf(srcfile_module_proxy_header, "typedef std::string %s;\n", getname().c_str());
		break;

	case eBasicObjType_Stream:
		if (array_type_fixed == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef bytebuff_array %s(%u);\n",
			getname().c_str(), m_ArrayType.ArraySize);
		else if (array_type_variable == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef bytebuff_array %s;\n",
			getname().c_str());
		else obj->fprintf(srcfile_module_proxy_header, "typedef bytebuff %s;\n", getname().c_str());
		break;

	case eBasicObjType_Typedefed:
		assert(NULL != m_pRefType);
		if (array_type_fixed == m_ArrayType.eArrayType){

		} else if (array_type_variable == m_ArrayType.eArrayType){
			
		} else {
			obj->fprintf(srcfile_module_proxy_header, "typedef %s %s;\n", m_pRefType->get_fullname(obj).c_str(), getname().c_str());
		}
		break;

	case eBasicObjType_UserDefined:
		if (array_type_fixed == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef array::%s %s(%u);\n",
			m_pUserDefType->getname().c_str(),
			 getname().c_str(), m_ArrayType.ArraySize);
		else if (array_type_variable == m_ArrayType.eArrayType)
			obj->fprintf(srcfile_module_proxy_header, "typedef array::%s %s;\n",
			m_pUserDefType->getname().c_str(),
			 getname().c_str(), m_ArrayType.ArraySize);
		else obj->fprintf(srcfile_module_proxy_header, "typedef %s %s;\n", 			m_pUserDefType->getname().c_str(),
			 getname().c_str(), m_ArrayType.ArraySize);
		break;

	case eBasicObjType_Interface:
		assert(NULL != m_pIFObject);
		if (array_type_fixed == m_ArrayType.eArrayType){
			//TODO.
		}
		else if (array_type_variable == m_ArrayType.eArrayType){
			//TODO.
		}
		else obj->fprintf(SrcFileHdr, "typedef %s %s;\n", m_pIFObject->get_fullname(obj).c_str(), getname().c_str());
		break;

	default: return false;
	}
	return true;
}

constitem_object_cpp_code_generator::constitem_object_cpp_code_generator(
	module* mod,
	const string& name, number_type type, double& value)
	: TConstItemObject(mod, name, type, value)
{
}

constitem_object_cpp_code_generator::~constitem_object_cpp_code_generator()
{
}

bool constitem_object_cpp_code_generator::generate_code(void)
{
	if (test_flags(grammar_object_flags_code_generated))
		return true;

	module* mod = GetModule();
	if (NULL == mod) return false;

	if (!m_pRefType) {
		switch (m_ConstType)
		{
		case Float:
			mod->fprintf(srcfile_module_proxy_header, "const %s %s = %e;\n",
				"double", getname().c_str(), m_Value);
			break;

		case Int:
			mod->fprintf(srcfile_module_proxy_header, "const %s %s = %d;\n",
				"int", getname().c_str(), (int)m_Value);
			break;

		case UInt:
			mod->fprintf(srcfile_module_proxy_header, "const %s %s = %u;\n",
				"unsigned int", getname().c_str(), (unsigned int)m_Value);
			break;

		case UChar:
			mod->fprintf(srcfile_module_proxy_header, "const %s %s = %u;\n",
				"unsigned char", getname().c_str(), ((unsigned int)m_Value) & 0xFF);
			break;
		}
	}

	// if (m_pRefType)
	// {
	// 	EBasicObjectType eType;
	// 	TArrayTypeObject eArrayType;
	// 	TUserDefTypeObject *pUsrDefObj = m_pRefType->GetOriginalType(eType, eArrayType, NULL, NULL);
	// 	if (pUsrDefObj || !IsBasicObjectType(eType) || IsArray(eArrayType))
	// 		return false;

	// 	obj->fprintf(SrcFileHdr, "const %s %s = ",
	// 		m_pRefType->GetName().c_str(), GetName().c_str());

	// 	switch (eType)
	// 	{
	// 	case eBasicObjType_Float:
	// 		obj->fprintf(SrcFileHdr, "%e;\n", m_Value);
	// 		break;

	// 	case eBasicObjType_Int32:
	// 		obj->fprintf(SrcFileHdr, "%d;\n", (int)m_Value);
	// 		break;

	// 	case eBasicObjType_Int16:
	// 		obj->fprintf(SrcFileHdr, "%u;\n", ((short)m_Value));
	// 		break;

	// 	case eBasicObjType_Int64:
	// 		obj->fprintf(SrcFileHdr, "%u;\n", ((long)m_Value));
	// 		break;

	// 	case eBasicObjType_UInt32:
	// 		obj->fprintf(SrcFileHdr, "%u;\n", (unsigned int)m_Value);
	// 		break;

	// 	case eBasicObjType_UInt8:
	// 		obj->fprintf(SrcFileHdr, "%u;\n", ((unsigned int)m_Value) & 0xFF);
	// 		break;

	// 	// boolean, enum is not supported by const
	// 	default: return false;
	// 	}
	// }
	
	return true;
}

enumdef_object_cpp_code_generator::enumdef_object_cpp_code_generator(
	module *mod, const string name)
	: enumdef_object(mod, name)
{
}

enumdef_object_cpp_code_generator::~enumdef_object_cpp_code_generator()
{
}

bool enumdef_object_cpp_code_generator::generate_code(void)
{
	if (test_flags(grammar_object_flags_code_generated))
		return true;
	set_flags(grammar_object_flags_code_generated);

	module *mod = GetModule();
	if (NULL == mod) return false;

	mod->fprintf(srcfile_module_proxy_header,
		"enum %s\n{\n", getname().c_str());

	list_item *item = m_cEnumNodeList.getnext();
	bool bfirst = true;
	for (; item != &m_cEnumNodeList; item = item->getnext())
	{
		TEnumNode *node = LIST_ENTRY(TEnumNode, m_cEnumOwnerList, item);
		if (bfirst) {
			mod->fprintf(srcfile_module_proxy_header,
				"	%s = %d", node->m_cEnumNodeName.c_str(), node->m_Value);
			bfirst = false;
		} else {
			mod->fprintf(srcfile_module_proxy_header,
				",\n	%s = %d", node->m_cEnumNodeName.c_str(), node->m_Value);
		}
	}
	mod->fputs(srcfile_module_proxy_header, "\n};\n");
	mod->fprintf(srcfile_module_proxy_header,
		"typedef enum_array<%s> %s_array;\n\n", 
		getname().c_str(), getname().c_str());
	return true;
}

/* EOF */
