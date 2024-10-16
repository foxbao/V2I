/** @file inc/codegenjava.h
 * implementation of java code generation for zrpc
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

typedef_object_java_code_generator::typedef_object_java_code_generator(
	module* mod, const string& name, basic_objtype type)
	: typedef_object(mod, name, type)
{
}

typedef_object_java_code_generator::typedef_object_java_code_generator(
	module* mod, const string& name, typedef_object *reftype)
	: typedef_object(mod, name, reftype)
{
}

typedef_object_java_code_generator::typedef_object_java_code_generator(
	module* mod,
	const string& name, userdef_type_object *reftype)
	: typedef_object(mod, name, reftype)
{
}

typedef_object_java_code_generator::typedef_object_java_code_generator(
	module* mod,
	const string& name, enumdef_object* enum_def)
	: typedef_object(mod, name, enum_def)
{
}

typedef_object_java_code_generator::typedef_object_java_code_generator(
	module* mod, const string& name, interface_object* ifobj)
	: typedef_object(mod, name, ifobj)
{
}

typedef_object_java_code_generator::~typedef_object_java_code_generator()
{
}

bool typedef_object_java_code_generator::generate_code(void)
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
		else obj->fprintf(srcfile_module_proxy_header, "typedef uint32_t %s;\n", getname().c_str());
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
	default: return false;
	}
	return true;
}

constitem_object_java_code_generator::constitem_object_java_code_generator(
	module* mod,
	const string& name, number_type type, double& value)
	: TConstItemObject(mod, name, type, value)
{
}

constitem_object_java_code_generator::~constitem_object_java_code_generator()
{
}

bool constitem_object_java_code_generator::generate_code(void)
{
	if (test_flags(grammar_object_flags_code_generated))
		return true;

	module* mod = GetModule();
	if (NULL == mod) return false;
	if (!m_pRefType) {
		switch (m_ConstType)
		{
		case Float:
			mod->fprintf(getname().c_str(), scrfile_module_final_source, "	%s %s = %e;\n", "double", getname().c_str(), m_Value);
			break;
		case Int:
			mod->fprintf(getname().c_str(), scrfile_module_final_source, "	%s %s = %d;\n", "int", getname().c_str(), (int)m_Value);
			break;
		case UInt:
			mod->fprintf(getname().c_str(), scrfile_module_final_source, "	%s %s = %u;\n", "int", getname().c_str(), (unsigned int)m_Value);
			break;
		case UChar:
			mod->fprintf(getname().c_str(), scrfile_module_final_source, "	%s %s = %u;\n", "char", getname().c_str(), ((unsigned int)m_Value) & 0xFF);
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

enumdef_object_java_code_generator::enumdef_object_java_code_generator(
	module *mod, const string name)
	: enumdef_object(mod, name)
{
}

enumdef_object_java_code_generator::~enumdef_object_java_code_generator()
{
}

bool enumdef_object_java_code_generator::generate_code(void)
{
	if (test_flags(grammar_object_flags_code_generated))
		return true;
	set_flags(grammar_object_flags_code_generated);

	module *mod = GetModule();
	if (NULL == mod) return false;

	mod->fprintf(getname().c_str(),scrfile_module_enum_source,
		"public enum %s\n{\n", getname().c_str());

	list_item *item = m_cEnumNodeList.getnext();
	bool bfirst = true;
	for (; item != &m_cEnumNodeList; item = item->getnext())
	{
		TEnumNode *node = LIST_ENTRY(TEnumNode, m_cEnumOwnerList, item);
		if (bfirst) {
			mod->fprintf(getname().c_str(),scrfile_module_enum_source,
				"	%s(\"%s\",%d)", node->m_cEnumNodeName.c_str(), 
					node->m_cEnumNodeName.c_str(), node->m_Value);
			bfirst = false;
		} else {
			mod->fprintf(getname().c_str(),scrfile_module_enum_source,
				",\n	%s(\"%s\",%d)", node->m_cEnumNodeName.c_str(), 
					node->m_cEnumNodeName.c_str(), node->m_Value);
		}
	}
	mod->fputs(getname().c_str(),scrfile_module_enum_source,
		";\n\n	private String name;\n	private int index;\n");
	mod->fprintf(getname().c_str(),scrfile_module_enum_source,
		"	private %s(String name, int index) { \n		this.name = name;\n		this.index = index;\n	 }\n", getname().c_str());	
	mod->fprintf(getname().c_str(), scrfile_module_enum_source, "	public static String getName(int index) { \n		for (%s c : %s.values()) { \n			if (c.getIndex() == index) { \n				return c.name; \n			} \n		} \n		return null; \n	}\n",getname().c_str(),
		getname().c_str());
	mod->fputs(getname().c_str(),scrfile_module_enum_source, "	public String getName() {\n		return name;\n	} \n");
	mod->fputs(getname().c_str(),scrfile_module_enum_source, "	public void setName(String name) { \n		this.name = name; \n	}\n");
	mod->fputs(getname().c_str(),scrfile_module_enum_source, "	public int getIndex() { \n		return index; \n	}\n");
	mod->fputs(getname().c_str(),scrfile_module_enum_source, "	public void setIndex(int index) { \n		this.index = index; \n	}\n");
	mod->fputs(getname().c_str(),scrfile_module_enum_source, "\n}\n");
	mod->closefile(scrfile_module_enum_source);
	return true;
}

/* EOF */
