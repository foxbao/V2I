/** @file inc/cgcpp-module.h
 * implementation of c++ code generation of module for zrpc
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

userdeftype_object_cpp_code_generator::userdeftype_object_cpp_code_generator(
	module* mod, const string& name)
	: userdef_type_object(mod, name)
	, _protobuf_count(0)
{
}

userdeftype_object_cpp_code_generator::~userdeftype_object_cpp_code_generator()
{
}

int userdeftype_object_cpp_code_generator::generate_global_scope_header_file_code(void)
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

bool userdeftype_object_cpp_code_generator::global_scope_generate_code(void)
{
	if (generate_global_scope_header_file_code()) {
		return false;
	}
	return true;
}

int userdeftype_object_cpp_code_generator::generate_class_dependency(void)
{
	module* mod = GetModule();
	if (nullptr == mod) return 1;

	auto* mod_codegen = zrpc_downcast( \
		module, module_object_cpp_code_generator, mod);
	
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
				if (generate_generic_array(var)) {
					return 2;
				}
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

//no used
int userdeftype_object_cpp_code_generator::generate_variable_class_boolean(
	TVariableObject *obj)
{
	return 0;
}

int userdeftype_object_cpp_code_generator::generate_variable_class(
	TVariableObject *obj)
{
	assert(nullptr != obj);

	int ret = 1;
	switch (obj->_basic_type) {
	case basic_objtype_boolean:
	case basic_objtype_uint8:
	case eBasicObjType_Int16:
	case eBasicObjType_Int32:
	case eBasicObjType_Int64:
	case eBasicObjType_UInt32:
	case eBasicObjType_Float:
		ret = generate_variable_class_int(obj, obj->_basic_type);
		break;
	case eBasicObjType_Enum:
		ret = generate_variable_class_enum(obj, obj->_basic_type);
		break;

	case eBasicObjType_String:
		ret = generate_variable_class_string(obj, obj->_basic_type);
		break;
	case eBasicObjType_Stream:
		ret = generate_variable_class_string(obj, obj->_basic_type);
		break;

	case eBasicObjType_Typedefed:
		break;

	case eBasicObjType_UserDefined:
		ret = generate_variable_class_userdefined(obj, obj->_basic_type);
		break;

	case eBasicObjType_Interface:
		ret = generate_variable_class_interface(obj, obj->_basic_type);
		break;

	default: return 33;
	}
	return ret;
}

int userdeftype_object_cpp_code_generator::generate_variable_class_constructor(
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

int userdeftype_object_cpp_code_generator::generate_protobuf_variable_class(
	TVariableObject *obj)
{
	assert(nullptr != obj);
	module* mod = GetModule();
	if (nullptr == mod) return 1;

	const char** types = protobuf_types;
	switch (obj->_basic_type)
	{
	case eBasicObjType_Interface:
	case eBasicObjType_Typedefed:
		break;
	case eBasicObjType_UserDefined:
		if (is_array(obj->m_ArrayType)) {
			mod->fputs(srcfile_module_protobuf, "\trepeated ");
		}
		else mod->fputs(srcfile_module_protobuf, "\t");
		mod->fprintf(srcfile_module_protobuf, "%s %s = %u;\n",
			obj->m_pUserDefType->getname().c_str(), obj->m_cName.c_str(),
			++_protobuf_count);
		break;
		break;

	default:
		if (is_array(obj->m_ArrayType)) {
			mod->fputs(srcfile_module_protobuf, "\trepeated ");
		}
		else mod->fputs(srcfile_module_protobuf, "\t");
		mod->fprintf(srcfile_module_protobuf, "%s %s = %u;\n",
			types[obj->_basic_type], obj->m_cName.c_str(), ++_protobuf_count);
		break;
	}
	return 0;
}

bool userdeftype_object_cpp_code_generator::generate_code(void)
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

int userdeftype_object_cpp_code_generator::generate_class_definition(void)
{
	module* mod = GetModule();
	if (nullptr == mod) return 1;
	if (generate_class_dependency()) return 2;

	const char* objname = getname().c_str();
	const char* modname = mod->getname().c_str();

	// generate the class start methods
	mod->fprintf(srcfile_module_proxy_header,
		"class ZIDL_PREFIX %s\n"
		"{\n"
		"public:\n"
		"	%s();\n"
		"	%s(pbf_wrapper wrapper, ::%s_pbf::%s* pbfmsg);\n"
		"	%s(const %s& obj) {\n"
		"		assign(obj);\n"
		"	}\n\n"
		"	void copyfrom(const ::%s_pbf::%s& obj);\n"
		"	void copyto(::%s_pbf::%s& obj) const;\n\n"

		"	inline const %s& operator=(const %s& obj) {\n"
		"		return assign(obj);\n"
		"	}\n\n"

		"	void ___bind(pbf_wrapper wrapper,\n"
		"		::%s_pbf::%s* pbfmsg) {\n"
		"		_pbfobj = pbfmsg;\n"
		"		_pbf_wrapper = wrapper;\n"
		"	}\n\n",
		objname,
		objname,
		objname, modname, objname,
		objname, objname,
		modname, objname,
		modname, objname,
		objname, objname,
		modname, objname);

	// generate the class start methods implmentation
	mod->fprintf(srcfile_module_proxy_structs,
		"%s::%s()\n" 
		": _pbfobj(NULL)\n"
		"{\n"
		"	_pbf_wrapper = new struct_wrapper::___%s_wrapper();\n"
		"	assert(_pbf_wrapper.get());\n"
		"	_pbfobj = reinterpret_cast<%s_pbf::%s*>\n"
		"		(_pbf_wrapper->get_pbfmessage());\n"
		"	assert(_pbfobj);\n", 
		objname, objname,
		objname,
		modname, objname);

	list_item *item = VariableListHeader().getnext();
	for (; item != &VariableListHeader(); item = item->getnext())
	{
		auto* var = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		if (generate_variable_class_constructor(var, false)) {
			return 2;
		}
	}

	mod->fprintf(srcfile_module_proxy_structs,
		"}\n\n"

		"%s::%s(pbf_wrapper wrapper, ::%s_pbf::%s* pbfmsg)\n"
		": _pbf_wrapper(wrapper), _pbfobj(pbfmsg)\n",
		objname, objname, modname, objname);

	item = VariableListHeader().getnext();
	for (; item != &VariableListHeader(); item = item->getnext())
	{
		auto* var = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		if (generate_variable_class_constructor(var, true)) {
			return 2;
		}
	}

	mod->fprintf(srcfile_module_proxy_structs,
		"{\n"
		"	assert(nullptr != pbfmsg);\n"
		"}\n\n"

		"const %s& %s::assign(const %s& obj)\n"
		"{\n"
		"	if (this == &obj || _pbfobj == obj._pbfobj) {\n"
		"		return *this;\n"
		"	}\n"
		"	*_pbfobj = *obj._pbfobj;\n"
		"	return *this;\n"
		"}\n\n"

		"void %s::copyfrom(const ::%s_pbf::%s& obj)\n"
		"{\n"
		"	if (&obj == _pbfobj) return;\n"
		"	*_pbfobj = obj;\n"
		"}\n\n"
		
		"void %s::copyto(::%s_pbf::%s& obj) const\n"
		"{\n"
		"	if (&obj == _pbfobj) return;\n"
		"	obj = *_pbfobj;\n"
		"}\n\n",
		objname, objname, objname,
		objname, modname, objname,
		objname, modname, objname);

	// variable list
	item = VariableListHeader().getnext();
	for (; item != &VariableListHeader(); item = item->getnext())
	{
		auto* var = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		if (generate_variable_class(var)) {
			return 2;
		}
	}

	// finailize the class generation
	mod->fprintf(srcfile_module_proxy_header,
		"private:\n"
		"	const %s& assign(const %s& obj);\n"
		"	::%s_pbf::%s* _pbfobj;\n"
		"	pbf_wrapper _pbf_wrapper;\n"
		"};\n\n",
		objname, objname,
		modname, objname);

	// generate related protobuffer defintion file
	mod->fprintf(srcfile_module_protobuf,
		"message %s\n{\n", objname);

	// variable list
	item = VariableListHeader().getnext();
	for (; item != &VariableListHeader(); item = item->getnext())
	{
		auto* var = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		if (generate_protobuf_variable_class(var)) {
			return 3;
		}
	}

	// finalize the class generation
	mod->fputs(srcfile_module_protobuf, "}\n\n");	
	mod->fprintf(srcfile_module_protobuf,
		"message %s_array\n{\n"
		"	repeated %s array = 1;\n"
		"}\n\n", 
		objname,
		objname);

	mod->fprintf(srcfile_module_proxy_internal_header,
		"class ___%s_wrapper : public pbf_wrapper_object\n"
		"{\n"
		"public:\n"
		"	google::protobuf::Message* get_pbfmessage(void) {\n"
		"		return &_protobuf_object;\n"
		"	}\n\n"

		"private:\n"
		"	%s_pbf::%s _protobuf_object;\n"
		"};\n\n",
		objname,
		modname, objname);	
	return 0;
}

int userdeftype_object_cpp_code_generator::generate_variable_class_int(
	TVariableObject *obj, basic_objtype type)
{
	module* mod = GetModule();
	if (nullptr == mod) return 1;
	string enum_type;
	const char* etype_str = nullptr;
	if (obj->m_pEnumDefType) {
		enum_type = obj->m_pEnumDefType->get_fullname(mod);
		etype_str = enum_type.c_str();
	}

	const char* types[] = {
		nullptr,	// unknown
		"bool",	// bool
		"uint8_t",	// uint8
		"int16_t",	// int16
		"int32_t",	// int32
		"int64_t",	//long
		"uint32_t",
		"double",	// Float
		"bytebuff",	// stream
		nullptr,	// string
		etype_str,	// enum
		nullptr,
		nullptr,
		nullptr,
	};

	const char* types_array[] = {
		nullptr,	// unknown
		"bool",	// bool
		"uint32",	// uint8
		"int32",	// int16
		"int32",	// int32
		"int64",	//long
		"uint32",
		"double",	// Float
		"bytebuff",	// stream
		nullptr,	// string
		"int32",	// enum
		nullptr,
		nullptr,
		nullptr,
	};

	const char* arrtypes[] = {
		nullptr,	// unknown
		nullptr,	// bool
		"uint32_t",	// uint8
		"int32_t",	// int16
		"int32_t",	// int32
		"int64_t",	//long
		"uint32_t",
		"double_t",	// Float
		"std::string",	// stream
		nullptr,	// string
		"int32_t",	// enum
		nullptr,
		nullptr,
		nullptr,
	};	

	const char* objname = getname().c_str();
	const char* var_name = obj->m_cName.c_str();

	if (is_array(obj->m_ArrayType))
	{
		mod->fprintf(srcfile_module_proxy_header,
			"	class ___clazz_%s : public zas::mware::rpc::%s_array\n"
			"	{\n"
			"	public:\n"
			"		const zas::mware::rpc::%s_array& operator=(\n"
			"			const zas::mware::rpc::%s_array& val);\n"
			"		%s& operator[](int index);\n\n"

			"		void clear(void);\n"
			"		int size(void) const;\n"
			"		%s get(int index) const;\n"
			"		%s append(%s val);\n\n"

			"	private:\n"
			"		inline %s* ancestor(void) const {\n"
			"			return _ANCESTOR(%s, %s);\n"
			"		}\n"
			"	} %s;\n\n",
			var_name, types_array[type],
			types_array[type],
			types_array[type],
			arrtypes[type],
			arrtypes[type],
			arrtypes[type], arrtypes[type],
			objname,
			objname, var_name,
			var_name);

		// generate the cpp file contents
		mod->fprintf(srcfile_module_proxy_structs,
			"const zas::mware::rpc::%s_array&\n"
			"%s::___clazz_%s::operator=(\n"
			"	const zas::mware::rpc::%s_array& val)\n"
			"{\n"
			"	return zas::mware::rpc::%s_array::operator=(val);\n"
			"}\n\n",
			types_array[type], 
			objname, var_name,
			types_array[type],
			types_array[type]);

		mod->fprintf(srcfile_module_proxy_structs,
			"%s& %s::___clazz_%s::operator[](int index)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	if (index >= 10) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	if (index < 0 || index >= pobj->%s_size()) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	return *ancestor()->_pbfobj->mutable_%s()->Mutable(index);\n"
			"}\n\n",
			arrtypes[type], objname, var_name,
			var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"void %s::___clazz_%s::clear(void)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	pobj->clear_%s();\n"
			"}\n\n",
			objname, var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"int %s::___clazz_%s::size(void) const\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	return pobj->%s_size();\n"
			"}\n\n",
			objname, var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"%s %s::___clazz_%s::get(int index) const\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	if (index >= 10) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	if (index < 0 || index >= pobj->%s_size()) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	return pobj->%s(index);\n"
			"}\n\n",
			arrtypes[type], objname, var_name,
			var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"%s %s::___clazz_%s::append(%s val)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	if (pobj->%s_size() >= 10) {\n"
			"		throw new zas::mware::rpc::rpc_error(\n"
			"			__FILE__, __LINE__,\n"
			"			errcat_proxy, err_scenario_others,\n"
			"			errid_array_exceed_boundary);		\n"
			"	}\n\n"

			"	pobj->add_%s(val);\n"
			"	return val;\n"
			"}\n\n",
			arrtypes[type], objname, var_name, arrtypes[type],
			var_name,
			var_name);
	}
	else if (eArrayType_NotArray == obj->m_ArrayType.eArrayType)
	{
		mod->fprintf(srcfile_module_proxy_header,
			"	class ___clazz_%s\n"
			"	{\n"
			"	public:\n"
			"		___clazz_%s& operator=(%s val);\n"
			"		operator %s(void) const;\n\n"

			"	private:\n"
			"		inline %s* ancestor(void) const {\n"
			"			return _ANCESTOR(%s, %s);\n"
			"		}\n"
			"	} %s;\n\n",
			var_name,
			var_name, types[type],
			types[type],
			objname,
			objname, var_name,
			var_name);

		// generate code for cpp file
		mod->fprintf(srcfile_module_proxy_structs,
			"%s::___clazz_%s& %s::___clazz_%s::operator=(%s val)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	pobj->set_%s(val);\n"
			"	return *this;\n"
			"}\n\n",
			objname, var_name, objname, var_name, types[type],
			var_name);
		
		mod->fprintf(srcfile_module_proxy_structs,
			"%s::___clazz_%s::operator %s(void) const\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	return pobj->%s();\n"
			"}\n\n",
			objname, var_name, types[type],
			var_name);

	}
	else return 2;
	return 0;
}

int userdeftype_object_cpp_code_generator::generate_variable_class_enum(
	TVariableObject *obj, basic_objtype type)
{
	module* mod = GetModule();
	if (nullptr == mod) return 1;

	assert(nullptr != obj->m_pEnumDefType);
	string fullname = obj->m_pEnumDefType->get_fullname(mod).c_str();
	const char* enumtype = fullname.c_str();

	const char* objname = getname().c_str();
	const char* var_name = obj->m_cName.c_str();

	if (is_array(obj->m_ArrayType))
	{
		mod->fprintf(srcfile_module_proxy_header,
			"	class ___clazz_%s : public %s_array\n"
			"	{\n"
			"	public:\n"
			"		const %s_array& operator=(\n"
			"			const %s_array& val);\n"
			"		%s& operator[](int index);\n\n"

			"		void clear(void);\n"
			"		int size(void) const;\n"
			"		%s get(int index) const;\n"
			"		%s append(%s val);\n\n"

			"	private:\n"
			"		inline %s* ancestor(void) const {\n"
			"			return _ANCESTOR(%s, %s);\n"
			"		}\n"
			"	} %s;\n\n",
			var_name, enumtype,
			enumtype,
			enumtype,
			enumtype,
			enumtype,
			enumtype, enumtype,
			objname,
			objname, var_name,
			var_name);

		// generate the cpp file contents
		mod->fprintf(srcfile_module_proxy_structs,
			"const %s_array&\n"
			"%s::___clazz_%s::operator=(\n"
			"	const %s_array& val)\n"
			"{\n"
			"	return %s_array::operator=(val);\n"
			"}\n\n",
			enumtype, 
			objname, var_name,
			enumtype,
			enumtype);

		mod->fprintf(srcfile_module_proxy_structs,
			"%s& %s::___clazz_%s::operator[](int index)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	if (index >= 10) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	if (index < 0 || index >= pobj->%s_size()) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	return *((%s*)ancestor()->_pbfobj->mutable_%s()->Mutable(index));\n"
			"}\n\n",
			enumtype, objname, var_name,
			var_name,
			enumtype, var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"void %s::___clazz_%s::clear(void)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	pobj->clear_%s();\n"
			"}\n\n",
			objname, var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"int %s::___clazz_%s::size(void) const\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	return pobj->%s_size();\n"
			"}\n\n",
			objname, var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"%s %s::___clazz_%s::get(int index) const\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	if (index >= 10) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	if (index < 0 || index >= pobj->%s_size()) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	return (%s)pobj->%s(index);\n"
			"}\n\n",
			enumtype, objname, var_name,
			var_name,
			enumtype, var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"%s %s::___clazz_%s::append(%s val)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	if (pobj->%s_size() >= 10) {\n"
			"		throw new zas::mware::rpc::rpc_error(\n"
			"			__FILE__, __LINE__,\n"
			"			errcat_proxy, err_scenario_others,\n"
			"			errid_array_exceed_boundary);		\n"
			"	}\n\n"

			"	pobj->add_%s((int32_t)val);\n"
			"	return val;\n"
			"}\n\n",
			enumtype, objname, var_name, enumtype,
			var_name,
			var_name);
	}
	else if (eArrayType_NotArray == obj->m_ArrayType.eArrayType)
	{
		mod->fprintf(srcfile_module_proxy_header,
			"	class ___clazz_%s\n"
			"	{\n"
			"	public:\n"
			"		___clazz_%s& operator=(%s val);\n"
			"		operator %s(void) const;\n\n"

			"	private:\n"
			"		inline %s* ancestor(void) const {\n"
			"			return _ANCESTOR(%s, %s);\n"
			"		}\n"
			"	} %s;\n\n",
			var_name,
			var_name, enumtype,
			enumtype,
			objname,
			objname, var_name,
			var_name);

		// generate code for cpp file
		mod->fprintf(srcfile_module_proxy_structs,
			"%s::___clazz_%s& %s::___clazz_%s::operator=(%s val)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	pobj->set_%s((int32_t)val);\n"
			"	return *this;\n"
			"}\n\n",
			 objname, var_name, objname, var_name, enumtype,
			var_name);
		

		mod->fprintf(srcfile_module_proxy_structs,
			"%s::___clazz_%s::operator %s(void) const\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	return (%s)pobj->%s();\n"
			"}\n\n",
			objname, var_name, enumtype,
			enumtype, var_name);	
	}
	else return 2;
	return 0;
}

int userdeftype_object_cpp_code_generator::generate_variable_class_interface(
	TVariableObject *obj, basic_objtype type)
{
	module* mod = GetModule();
	if (nullptr == mod) return 1;

	assert(nullptr != obj->m_pIFType);
	string fullname = obj->m_pIFType->get_fullname(mod).c_str();
	const char* iftype = fullname.c_str();

	const char* objname = getname().c_str();
	const char* var_name = obj->m_cName.c_str();

	if (is_array(obj->m_ArrayType))
	{
		mod->fprintf(srcfile_module_proxy_header,
			"	class ___clazz_%s : public %s_array\n"
			"	{\n"
			"	public:\n"
			"		const %s_array& operator=(\n"
			"			const %s_array& val);\n"
			"		%s& operator[](int index);\n\n"

			"		void clear(void);\n"
			"		int size(void) const;\n"
			"		%s get(int index) const;\n"
			"		%s append(%s val);\n\n"

			"	private:\n"
			"		inline %s* ancestor(void) const {\n"
			"			return _ANCESTOR(%s, %s);\n"
			"		}\n"
			"	} %s;\n\n",
			var_name, iftype,
			iftype,
			iftype,
			iftype,
			iftype,
			iftype, iftype,
			objname,
			objname, var_name,
			var_name);

		// generate the cpp file contents
		mod->fprintf(srcfile_module_proxy_structs,
			"const %s_array&\n"
			"%s::___clazz_%s::operator=(\n"
			"	const %s_array& val)\n"
			"{\n"
			"	return %s_array::operator=(val);\n"
			"}\n\n",
			iftype, 
			objname, var_name,
			iftype,
			iftype);

		mod->fprintf(srcfile_module_proxy_structs,
			"%s& %s::___clazz_%s::operator[](int index)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	if (index >= 10) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	if (index < 0 || index >= pobj->%s_size()) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	return *((%s*)ancestor()->_pbfobj->mutable_%s()->Mutable(index));\n"
			"}\n\n",
			iftype, objname, var_name,
			var_name,
			iftype, var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"void %s::___clazz_%s::clear(void)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	pobj->clear_%s();\n"
			"}\n\n",
			objname, var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"int %s::___clazz_%s::size(void) const\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	return pobj->%s_size();\n"
			"}\n\n",
			objname, var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"%s %s::___clazz_%s::get(int index) const\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	if (index >= 10) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	if (index < 0 || index >= pobj->%s_size()) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	return *((%s*)pobj->%s(index));\n"
			"}\n\n",
			iftype, objname, var_name,
			var_name,
			iftype, var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"%s %s::___clazz_%s::append(%s val)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	if (pobj->%s_size() >= 10) {\n"
			"		throw new zas::mware::rpc::rpc_error(\n"
			"			__FILE__, __LINE__,\n"
			"			errcat_proxy, err_scenario_others,\n"
			"			errid_array_exceed_boundary);		\n"
			"	}\n\n"

			"	pobj->add_%s((int64_t)&val);\n"
			"	return val;\n"
			"}\n\n",
			iftype, objname, var_name, iftype,
			var_name,
			var_name);
	}
	else if (eArrayType_NotArray == obj->m_ArrayType.eArrayType)
	{
		mod->fprintf(srcfile_module_proxy_header,
			"	class ___clazz_%s\n"
			"	{\n"
			"	public:\n"
			"		___clazz_%s& operator=(%s val);\n"
			"		operator %s(void) const;\n\n"

			"	private:\n"
			"		inline %s* ancestor(void) const {\n"
			"			return _ANCESTOR(%s, %s);\n"
			"		}\n"
			"	} %s;\n\n",
			var_name,
			var_name, iftype,
			iftype,
			objname,
			objname, var_name,
			var_name);

		// generate code for cpp file
		mod->fprintf(srcfile_module_proxy_structs,
			"%s::___clazz_%s& %s::___clazz_%s::operator=(%s val)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	pobj->set_%s((int64_t)&val);\n"
			"	return *this;\n"
			"}\n\n",
			objname, var_name, objname, var_name, iftype,
			var_name);
	
		mod->fprintf(srcfile_module_proxy_structs,
			"%s::___clazz_%s::operator %s(void) const\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	return *((%s*)pobj->%s());\n"
			"}\n\n",
			objname, var_name, iftype,
			iftype, var_name);
	}
	else return 2;
	return 0;
}

int userdeftype_object_cpp_code_generator::generate_variable_class_userdefined(
	TVariableObject *obj, basic_objtype type)
{
	module* mod = GetModule();
	if (nullptr == mod) return 1;
	
	const char* objname = getname().c_str();
	const char* var_name = obj->m_cName.c_str();
	assert(nullptr != obj->m_pUserDefType);
	const char* objtype = obj->m_pUserDefType->getname().c_str();
	const char* modname = obj->m_pUserDefType->GetModule()->getname().c_str();

	if (is_array(obj->m_ArrayType))
	{
		mod->fprintf(srcfile_module_proxy_header,
			"	class ___clazz_%s : public array::%s\n"
			"	{\n"
			"	public:\n"
			"		const array::%s& operator=(const array::%s& val);\n"
			"		::%s::%s operator[](int index);\n\n"

			"		int count(void) const { return size(); }\n"
			"		const ::%s::%s get(int index) const;\n"
			"		const ::%s::%s append(const ::%s::%s& val);\n\n"

			"		void clear(void);\n"
			"		int size(void) const;\n\n"

			"	protected:\n"
			"		const %s_pbf::%s& array_object(int index) const;\n"
			"		void add(const %s_pbf::%s& obj);\n"
			"		%s_pbf::%s& add(void);\n"
			"		%s_pbf::%s& mutable_array_object(int index);\n\n"

			"	private:\n"
			"		inline %s* ancestor(void) const {\n"
			"			return _ANCESTOR(%s, %s);\n"
			"		}\n"
			"	} %s;\n\n",
			var_name, objtype,
			objtype, objtype,
			modname, objtype,
			modname, objtype,
			modname, objtype, modname, objtype,
			modname, objtype,
			modname, objtype,
			modname, objtype,
			modname, objtype,
			objname,
			objname, var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"const array::%s&\n"
			"%s::___clazz_%s::operator=(\n"
			"	const array::%s& val)\n"
			"{\n"
			"	return array::%s::operator=(val);\n"
			"}\n\n",
			objtype,
			objname, var_name,
			objtype,
			objtype);

		mod->fprintf(srcfile_module_proxy_structs,
			"::%s::%s\n"
			"%s::___clazz_%s::operator[](int index)\n"
			"{\n"
			"	if (index >= 10) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	if (index < 0 || index >= size()) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	return ::%s::%s(\n"
			"		ancestor()->_pbf_wrapper, \n"
			"		&mutable_array_object(index));\n"
			"}\n\n",
			modname, objtype,
			objname, var_name,
			modname, objtype);

		mod->fprintf(srcfile_module_proxy_structs,
			"const ::%s::%s\n"
			"%s::___clazz_%s::get(int index) const\n"
			"{\n"
			"	if (index >= 10) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	if (index < 0 || index >= size()) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	%s_pbf::%s* item = const_cast<\n"
			"		%s_pbf::%s*>(&array_object(index));\n"
			"	return ::%s::%s(\n"
			"		ancestor()->_pbf_wrapper, item);\n"
			"}\n\n",
			modname, objtype,
			objname, var_name,
			modname, objtype,
			modname, objtype,
			modname, objtype);

		mod->fprintf(srcfile_module_proxy_structs,
			"const ::%s::%s\n"
			"%s::___clazz_%s::append(const ::%s::%s& val)\n"
			"{\n"
			"	if (size() >= 10) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	auto& item = add();\n"
			"	val.copyto(item);\n"
			"	return ::%s::%s(ancestor()->_pbf_wrapper, &item);\n"
			"}\n\n",
			modname, objtype,
			objname, var_name, modname, objtype,
			modname, objtype);

		mod->fprintf(srcfile_module_proxy_structs,
			"void %s::___clazz_%s::clear(void)\n"
			"{\n"
			"	auto* p = ancestor()->_pbfobj;\n"
			"	p->clear_%s();\n"
			"}\n\n",
			objname, var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"int %s::___clazz_%s::size(void) const\n"
			"{\n"
			"	auto* p = ancestor()->_pbfobj;\n"
			"	return p->%s_size();\n"
			"}\n\n",
			objname, var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"const %s_pbf::%s&\n"
			"%s::___clazz_%s::array_object(int index) const\n"
			"{\n"
			"	auto* p = ancestor()->_pbfobj;\n"
			"	return p->%s(index);\n"
			"}\n\n",
			modname, objtype,
			objname, var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"void %s::___clazz_%s::add(const %s_pbf::%s& obj)\n"
			"{\n"
			"	auto* p = ancestor()->_pbfobj;\n"
			"	auto* new_obj = p->add_%s();\n"
			"	*new_obj = obj;\n"
			"}\n\n",
			objname, var_name, modname, objtype,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"%s_pbf::%s& %s::___clazz_%s::add(void)\n"
			"{\n"
			"	auto* p = ancestor()->_pbfobj;\n"
			"	return *p->add_%s();\n"
			"}\n\n",
			modname, objtype, objname, var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"%s_pbf::%s&\n"
			"%s::___clazz_%s::mutable_array_object(int index)\n"
			"{\n"
			"	auto* p = ancestor()->_pbfobj;\n"
			"	return *p->mutable_%s(index);\n"
			"}\n\n",
			modname, objtype,
			objname, var_name,
			var_name);
	}
	else if (eArrayType_NotArray == obj->m_ArrayType.eArrayType)
	{
		mod->fprintf(srcfile_module_proxy_header,
			"	::%s::%s %s;\n\n",
			modname, objtype, var_name);
	}
	else return 2;
	return 0;
}

int
userdeftype_object_cpp_code_generator::
generate_variable_class_string(TVariableObject *obj, basic_objtype type)
{
	module* mod = GetModule();
	if (nullptr == mod) return 1;
	
	const char* objname = getname().c_str();
	const char* var_name = obj->m_cName.c_str();
	const char* modname = mod->getname().c_str();
	const char* var_types = "string";
	const char* var_type = "std::string";

	if (is_array(obj->m_ArrayType))
	{
		mod->fprintf(srcfile_module_proxy_header,
			"	class ___clazz_%s : public zas::mware::rpc::%s_array\n"
			"	{\n"
			"	public:\n"
			"		const zas::mware::rpc::%s_array& operator=(\n"
			"			const zas::mware::rpc::%s_array& val);\n"
			"		std::%s& operator[](int index);\n\n"

			"		void clear(void);\n"
			"		int size(void) const;\n"
			"		std::%s get(int index) const;\n"
			"		std::%s append(std::%s val);\n\n"

			"	private:\n"
			"		inline %s* ancestor(void) const {\n"
			"			return _ANCESTOR(%s, %s);\n"
			"		}\n"
			"	} %s;\n\n",
			var_name, var_types,
			var_types,
			var_types,
			var_types,
			var_types,
			var_types, var_types,
			objname,
			objname, var_name,
			var_name);

		// generate the cpp file contents
		mod->fprintf(srcfile_module_proxy_structs,
			"const zas::mware::rpc::%s_array&\n"
			"%s::___clazz_%s::operator=(\n"
			"	const zas::mware::rpc::%s_array& val)\n"
			"{\n"
			"	return zas::mware::rpc::%s_array::operator=(val);\n"
			"}\n\n",
			var_types, 
			objname, var_name,
			var_types,
			var_types);

		mod->fprintf(srcfile_module_proxy_structs,
			"std::%s& %s::___clazz_%s::operator[](int index)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	if (index >= 10) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	if (index < 0 || index >= pobj->%s_size()) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	return *ancestor()->_pbfobj->mutable_%s()->Mutable(index);\n"
			"}\n\n",
			var_types, objname, var_name,
			var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"void %s::___clazz_%s::clear(void)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	pobj->clear_%s();\n"
			"}\n\n",
			objname, var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"int %s::___clazz_%s::size(void) const\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	return pobj->%s_size();\n"
			"}\n\n",
			objname, var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"std::%s %s::___clazz_%s::get(int index) const\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	if (index >= 10) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	if (index < 0 || index >= pobj->%s_size()) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	return pobj->%s(index);\n"
			"}\n\n",
			var_types, objname, var_name,
			var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"std::%s %s::___clazz_%s::append(std::%s val)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	if (pobj->%s_size() >= 10) {\n"
			"		throw new zas::mware::rpc::rpc_error(\n"
			"			__FILE__, __LINE__,\n"
			"			errcat_proxy, err_scenario_others,\n"
			"			errid_array_exceed_boundary);		\n"
			"	}\n\n"

			"	pobj->add_%s(val);\n"
			"	return val;\n"
			"}\n\n",
			var_types, objname, var_name, var_types,
			var_name,
			var_name);
	}
	else if (eArrayType_NotArray == obj->m_ArrayType.eArrayType)
	{
		mod->fprintf(srcfile_module_proxy_header,
			"	class ___clazz_%s\n"
			"	{\n"
			"	public:\n"
			"		___clazz_%s& operator=(%s val);\n"
			"		operator %s(void) const;\n\n"

			"	private:\n"
			"		inline %s* ancestor(void) const {\n"
			"			return _ANCESTOR(%s, %s);\n"
			"		}\n"
			"	} %s;\n\n",
			var_name,
			var_name, var_type,
			var_type,
			objname,
			objname, var_name,
			var_name);

		// generate code for cpp file
		mod->fprintf(srcfile_module_proxy_structs,
			"%s::___clazz_%s& %s::___clazz_%s::operator=(%s val)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	pobj->set_%s(val);\n"
			"	return *this;\n"
			"}\n\n"

			"%s::___clazz_%s::operator %s(void) const\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	return pobj->%s();\n"
			"}\n\n",
			objname, var_name, objname, var_name, var_type,
			var_name,
			objname, var_name, var_type,
			var_name);
	}
	else return 2;
	return 0;
}

int
userdeftype_object_cpp_code_generator::
generate_variable_class_stream(TVariableObject *obj, basic_objtype type)
{
	module* mod = GetModule();
	if (nullptr == mod) return 1;
	
	const char* objname = getname().c_str();
	const char* var_name = obj->m_cName.c_str();
	const char* modname = mod->getname().c_str();
	const char* var_types = "bytebuff";
	const char* var_type = "string";
	const char* vtype = "bytebuff";

	if (is_array(obj->m_ArrayType))
	{
		mod->fprintf(srcfile_module_proxy_header,
			"	class ___clazz_%s : public zas::mware::rpc::%s_array\n"
			"	{\n"
			"	public:\n"
			"		const zas::mware::rpc::%s_array& operator=(\n"
			"			const zas::mware::rpc::%s_array& val);\n"
			"		std::%s& operator[](int index);\n\n"

			"		void clear(void);\n"
			"		int size(void) const;\n"
			"		std::%s get(int index) const;\n"
			"		std::%s append(std::%s val);\n\n"

			"	private:\n"
			"		inline %s* ancestor(void) const {\n"
			"			return _ANCESTOR(%s, %s);\n"
			"		}\n"
			"	} %s;\n\n",
			var_name, var_types,
			var_types,
			var_types,
			var_type,
			var_type,
			var_type, var_type,
			objname,
			objname, var_name,
			var_name);

		// generate the cpp file contents
		mod->fprintf(srcfile_module_proxy_structs,
			"const zas::mware::rpc::%s_array&\n"
			"%s::___clazz_%s::operator=(\n"
			"	const zas::mware::rpc::%s_array& val)\n"
			"{\n"
			"	return zas::mware::rpc::%s_array::operator=(val);\n"
			"}\n\n",
			var_types, 
			objname, var_name,
			var_types,
			var_types);

		mod->fprintf(srcfile_module_proxy_structs,
			"std::%s& %s::___clazz_%s::operator[](int index)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	if (index >= 10) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	if (index < 0 || index >= pobj->%s_size()) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	return *ancestor()->_pbfobj->mutable_%s()->Mutable(index);\n"
			"}\n\n",
			var_type, objname, var_name,
			var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"void %s::___clazz_%s::clear(void)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	pobj->clear_%s();\n"
			"}\n\n",
			objname, var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"int %s::___clazz_%s::size(void) const\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	return pobj->%s_size();\n"
			"}\n\n",
			objname, var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"std::%s %s::___clazz_%s::get(int index) const\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	if (index >= 10) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	if (index < 0 || index >= pobj->%s_size()) {\n"
			"		throw_rpc_error_array_exceed_boundary();\n"
			"	}\n"
			"	return pobj->%s(index);\n"
			"}\n\n",
			var_type, objname, var_name,
			var_name,
			var_name);

		mod->fprintf(srcfile_module_proxy_structs,
			"std::%s %s::___clazz_%s::append(std::%s val)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	if (pobj->%s_size() >= 10) {\n"
			"		throw new zas::mware::rpc::rpc_error(\n"
			"			__FILE__, __LINE__,\n"
			"			errcat_proxy, err_scenario_others,\n"
			"			errid_array_exceed_boundary);		\n"
			"	}\n\n"

			"	pobj->add_%s(val);\n"
			"	return val;\n"
			"}\n\n",
			var_type, objname, var_name, var_types,
			var_name,
			var_name);
	}
	else if (eArrayType_NotArray == obj->m_ArrayType.eArrayType)
	{
		mod->fprintf(srcfile_module_proxy_header,
			"	class ___clazz_%s\n"
			"	{\n"
			"	public:\n"
			"		___clazz_%s& operator=(%s val);\n"
			"		operator %s(void);\n\n"

			"	private:\n"
			"		inline %s* ancestor(void) const {\n"
			"			return _ANCESTOR(%s, %s);\n"
			"		}\n"
			"	} %s;\n\n",
			var_name,
			var_name, vtype,
			vtype,
			objname,
			objname, var_name,
			var_name);

		// generate code for cpp file
		mod->fprintf(srcfile_module_proxy_structs,
			"%s::___clazz_%s& %s::___clazz_%s::operator=(%s val)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	pobj->set_%s(val);\n"
			"	return *this;\n"
			"}\n\n"

			"%s::___clazz_%s::operator %s(void)\n"
			"{\n"
			"	auto* pobj = ancestor()->_pbfobj;\n"
			"	return pobj->%s();\n"
			"}\n\n",
			objname, var_name, objname, var_name, vtype,
			var_name,
			objname, var_name, vtype,
			var_name);
	}
	else return 2;
	return 0;
}

int
userdeftype_object_cpp_code_generator::
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

int userdeftype_object_cpp_code_generator::generate_generic_array(TVariableObject* var)
{
	module* mod = GetModule();
	if (nullptr == mod) return 1;

	const char* modname = mod->getname().c_str();
	assert(nullptr != var->m_pUserDefType);
	const char* objname = var->m_pUserDefType->getname().c_str();

	mod->fprintf(srcfile_module_proxy_header,
		"namespace array {\n\n"
		"class ZIDL_PREFIX %s\n"
		"{\n"
		"public:\n"
		"	%s(int maxsz = -1);\n"
		"	~%s();\n"
		"	const %s& operator=(const %s& val);\n"
		"	::%s::%s operator[](int index);\n\n"

		"	virtual void clear(void);\n"
		"	virtual int size(void) const;\n\n"

		"protected:\n"
		"	virtual const %s_pbf::%s& array_object(int index) const;\n"
		"	virtual void add(const %s_pbf::%s& obj);\n"
		"	virtual %s_pbf::%s& add(void);\n"
		"	virtual %s_pbf::%s& mutable_array_object(int index);\n\n"

		"private:\n"
		"	%s_pbf::%s_array* _array;\n"
		"	int _max_size;\n"
		"};\n\n"

		"} // end of namespace array\n\n",
		objname,
		objname,
		objname,
		objname, objname,
		modname, objname,
		modname, objname,
		modname, objname,
		modname, objname,
		modname, objname,
		modname, objname);

	mod->fprintf(srcfile_module_proxy_structs,
		"namespace array {\n\n");

	mod->fprintf(srcfile_module_proxy_structs,
		"%s::%s(int maxsz)\n"
		": _array(new %s_pbf::%s_array())\n"
		", _max_size(maxsz) {\n"
		"	assert(nullptr != _array);\n"
		"}\n\n",
		objname, objname,
		modname, objname);

	mod->fprintf(srcfile_module_proxy_structs,
		"%s::~%s()\n"
		"{\n"
		"	if (nullptr != _array) {\n"
		"		delete _array;\n"
		"		_array = nullptr;\n"
		"	}\n"
		"}\n\n",
		objname, objname);

	mod->fprintf(srcfile_module_proxy_structs,
		"const %s& %s::operator=(\n"
		"	const %s& val)\n"
		"{\n"
		"	if (this == &val) {\n"
		"		return *this;\n"
		"	}\n"
		"	int sz = val.size();\n"
		"	if (_max_size > 0 && sz > _max_size) {\n"
		"		sz = _max_size;\n"
		"	}\n"
		"	// copy the array\n"
		"	clear();\n"
		"	for (int i = 0; i < sz; ++i) {\n"
		"		add(val.array_object(i));\n"
		"	}\n"
		"	return *this;\n"
		"}\n\n",
		objname, objname,
		objname);

	mod->fprintf(srcfile_module_proxy_structs,
		"::%s::%s %s::operator[](int index)\n"
		"{\n"
		"	if (_max_size > 0 && index >= _max_size) {\n"
		"		throw_rpc_error_array_exceed_boundary();\n"
		"	}\n"
		"	if (index >= size()) {\n"
		"		throw_rpc_error_array_exceed_boundary();\n"
		"	}\n"
		"	return ::%s::%s(\n"
		"		zas::mware::rpc::pbf_wrapper(nullptr),\n"
		"		&mutable_array_object(index));\n"
		"}\n\n",
		modname, objname, objname,
		modname, objname);

	mod->fprintf(srcfile_module_proxy_structs,
		"void %s::clear(void)\n"
		"{\n"
		"	if (_array) {\n"
		"		_array->clear_array();\n"
		"	}\n"
		"}\n\n",
		objname);

	mod->fprintf(srcfile_module_proxy_structs,
		"int %s::size(void) const\n"
		"{\n"
		"	if (!_array) {\n"
		"		return 0;\n"
		"	}\n"
		"	int ret = _array->array_size();\n"
		"	if (_max_size > 0 && _max_size < ret) {\n"
		"		ret = _max_size;\n"
		"	}\n"
		"	return ret;\n"
		"}\n\n",
		objname);

	mod->fprintf(srcfile_module_proxy_structs,
		"const %s_pbf::%s& %s::array_object(int index) const\n"
		"{\n"
		"	if (index < 0 || index >= size()) {\n"
		"		throw_rpc_error_array_exceed_boundary();\n"
		"	}\n"
		"	assert(nullptr != _array);\n"
		"	return _array->array(index);\n"
		"}\n\n",
		modname, objname, objname);

	mod->fprintf(srcfile_module_proxy_structs,
		"void %s::add(const %s_pbf::%s& obj)\n"
		"{\n"
		"	assert(nullptr != _array);\n"
		"	if (_max_size > 0 && size() >= _max_size) {\n"
		"		throw_rpc_error_array_exceed_boundary();\n"
		"	}\n"
		"	auto* new_obj = _array->add_array();\n"
		"	*new_obj = obj;\n"
		"}\n\n",
		objname, modname, objname);

	mod->fprintf(srcfile_module_proxy_structs,
		"%s_pbf::%s& %s::add(void)\n"
		"{\n"
		"	assert(nullptr != _array);\n"
		"	if (_max_size > 0 && size() >= _max_size) {\n"
		"		throw_rpc_error_array_exceed_boundary();\n"
		"	}\n"
		"	return *_array->add_array();\n"
		"}\n\n",
		modname, objname, objname);

	mod->fprintf(srcfile_module_proxy_structs,
		"%s_pbf::%s& %s::mutable_array_object(int index)\n"
		"{\n"
		"	if (index < 0 || index >= size()) {\n"
		"		throw_rpc_error_array_exceed_boundary();\n"
		"	}\n"
		"	assert(nullptr != _array);\n"
		"	return *_array->mutable_array(index);\n"
		"}\n\n",
		modname, objname, objname);

	mod->fprintf(srcfile_module_proxy_structs,
		"} // end of namespace array\n\n");

	return 0;
}
