/** @file inc/cgjava-interface.h
 * implementation of c++ code generation of interface for zrpc
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

interface_object_java_code_generator::interface_object_java_code_generator(
	module* mod, const string& name)
	: interface_object(mod, name)
{
}

interface_object_java_code_generator::~interface_object_java_code_generator()
{
}

bool interface_object_java_code_generator::generate_code(void)
{
	if (test_flags(grammar_object_flags_code_generated))
		return true;
	set_flags(grammar_object_flags_code_generated);

	if (generate_method_protobuf_structs()) {
		return false;
	}
	if (generate_source_code()) {
		return false;
	}
	return true;
}

bool interface_object_java_code_generator::generate_proxy_code(void)
{
	if (TestInterfaceFlags(eInterfaceFlag_Observable)){
		if (generate_proxy_observable_header())
			return false;
	} else {
		if (generate_proxy_header())
			return false;
	}
	return true;
}

bool interface_object_java_code_generator::generate_skeleton_code(void)
{
	return true;
}

int interface_object_java_code_generator::
	generate_single_method_protobuf(TInterfaceNode* node)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;

	const char** types = protobuf_types;
	auto *ifobj = node->GetInterfaceObject();
	assert(nullptr != ifobj);

	string tmp;
	int first_inparam = 0;
	list_item *item = node->m_VariableList.getnext();
	for (; item != &(node->m_VariableList); item = item->getnext())
	{
		auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		// if it is not "inout/out", then it shall be "in"
		if (obj->test_flags(varobj_flag_method_param_out)) {
			continue;
		}

		if (!first_inparam) {
			mod->fprintf(mod->getname().c_str(), srcfile_module_protobuf,
				"message ___%s_%s_inparam\n{\n", ifobj->getname().c_str(),
				node->getname().c_str());
		}
		variable_protobuf_type_name(mod, obj, tmp);
		mod->fprintf(mod->getname().c_str(), srcfile_module_protobuf,
			"\t%s %s = %u;\n", tmp.c_str(),
			obj->getname().c_str(), ++first_inparam);
		if (obj->_basic_type == eBasicObjType_Interface 
			&& obj->m_pIFType->TestInterfaceFlags(eInterfaceFlag_Observable)) {
			mod->fprintf(mod->getname().c_str(), srcfile_module_protobuf,
			"\tstring client_name_%s = %u;\n", obj->getname().c_str(), ++first_inparam);
			mod->fprintf(mod->getname().c_str(), srcfile_module_protobuf,
			"\tstring inst_name_%s = %u;\n", obj->getname().c_str(), ++first_inparam);
		}
	}
	if (first_inparam) {
		
		mod->fputs(mod->getname().c_str(), srcfile_module_protobuf, "}\n\n");
	}

	int first_inout_param = 0;
	item = node->m_VariableList.getnext();
	for (; item != &(node->m_VariableList); item = item->getnext())
	{
		auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		if (!obj->test_flags(varobj_flag_method_param_out)) {
			continue;
		}

		if (!first_inout_param) {
			mod->fprintf(mod->getname().c_str(), srcfile_module_protobuf,
				"message ___%s_%s_inout_param\n{\n", ifobj->getname().c_str(),
				node->getname().c_str());
		}
		variable_protobuf_type_name(mod, obj, tmp);
		mod->fprintf(mod->getname().c_str(), srcfile_module_protobuf,
			"\t%s %s = %u;\n", tmp.c_str(),
			obj->getname().c_str(), ++first_inout_param);
	}
	// if the method has a retval, add it to inout_param
	if (node->m_Data.method.m_eRetValType != eBasicObjType_Unknown) {
		if (!first_inout_param) {
			mod->fprintf(mod->getname().c_str(), srcfile_module_protobuf,
				"message ___%s_%s_inout_param\n{\n", ifobj->getname().c_str(),
				node->getname().c_str());
		}
		method_retval_protobuf_type_name(mod, node, tmp);
		mod->fprintf(mod->getname().c_str(), srcfile_module_protobuf,
			"\t%s ___retval = %u;\n", tmp.c_str(), ++first_inout_param);
	}
	if (first_inout_param) {
		mod->fputs(mod->getname().c_str(), srcfile_module_protobuf, "}\n\n");
	}
	return 0;
}

int interface_object_java_code_generator::generate_method_protobuf_structs(void)
{
	list_item* subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		// check if this is a method
		if (eIFNodeType_Method != node->GetType()) {
			continue;
		}
		if (generate_single_method_protobuf(node)) {
			return 1;
		}
	}
	return 0;
}

int interface_object_java_code_generator::generate_header_code(void)
{
	if (TestInterfaceFlags(eInterfaceFlag_Observable)){
		if (generate_proxy_observable_header())
			return false;
	} else {
		if (generate_proxy_header())
			return false;
	}

	if (TestInterfaceFlags(eInterfaceFlag_Observable)) {
		if (generate_skeleton_observable_header())
			return -3;
	} else {
		if (generate_skeleton_header())
			return -4;
	}


	return 0;
}

int interface_object_java_code_generator::generate_source_code(void)
{

	if (TestInterfaceFlags(eInterfaceFlag_Observable)) {
		if (generate_observable_skeleton_source())
			return -3;
	} 
	if (!TestInterfaceFlags(eInterfaceFlag_Observable)) {
		if (generate_interface_source())
			return -5;
	} 
	return 0;
}

int interface_object_java_code_generator::generate_uid(const char* name,
	std::string &uid)
{
	uint128_t tuuid;
	utilities::md5Encode(tuuid, (void*)name, strlen(name));
	// char buf[128];
	// snprintf(buf, 127, "{0x%X, 0x%X, 0x%X, {0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X}}",
	// 	tuuid.Data1, tuuid.Data2, tuuid.Data3, tuuid.Data4[0], tuuid.Data4[1],
	// 	tuuid.Data4[2], tuuid.Data4[3], tuuid.Data4[4], tuuid.Data4[5],
	// 	tuuid.Data4[6], tuuid.Data4[7]);
	return uint128_to_hex(tuuid, uid);
}

int interface_object_java_code_generator::method_retval_variable_name(
	module* cmod, TInterfaceNode* node, string& ret,
	bool out, bool proxy, bool observable)
{
	TVariableObject varobj;

	varobj._basic_type = node->m_Data.method.m_eRetValType;
	varobj.m_pIFType = node->m_Data.method.m_pRetValIFObject;
	varobj.m_pRefType = node->m_Data.method.m_pRetValTypedefType;
	varobj.m_pEnumDefType = node->m_Data.method.m_pRetValEnumDef;
	varobj.m_pUserDefType = node->m_Data.method.m_pRetValUserDefType;
	return variable_full_name(cmod, &varobj, ret, out, proxy, observable, true);
}

int interface_object_java_code_generator::variable_full_name(
	module* cmod, TVariableObject* var, string& ret,
	bool out, bool proxy, bool observable, bool breturn)
{
	const char* variable_types[] = {
		"void",	// unknown
		"BoolRef",// bool
		"ByteRef",	// uint8
		"ShortRef",	// int16
		"IntRef",	// int32
		"LongRef",	// long
		"IntRef",	// uint32
		"DoubleRef",	// Float
		"Object",	// stream
		"StrRef",	// string
		nullptr,	// enum - special handling
		nullptr,	// interface - shall not have this
		nullptr,	// typedefed - special handling
		nullptr,	// userdefined - special handling
	};

	const char* variable_array_types[] = {
		"void",	// unknown
		"BoolRef[]",		// bool
		"IntRef[]",	// uint8
		"IntRef[]",	// int16
		"IntRef[]",	// int32
		"LongRef[]",	// long
		"IntRef[]",	// uint32
		"DoubleRef[]",	// Float
		"Object[]",	// stream
		"StrRef[]",	// string
		nullptr,	// enum - special handling
		nullptr,	// interface - shall not have this
		nullptr,	// typedefed - special handling
		nullptr,	// userdefined - special handling
	};

	ret.clear();
	if (breturn && eBasicObjType_Unknown == var->_basic_type) {
		ret += "void";
		return 0;
	}
	const char* type = nullptr;
	if (is_array(var->m_ArrayType)) {
		type = variable_array_types[var->_basic_type];
		if (type) {
			ret += type;
			return 0;
		}

		assert(eBasicObjType_Unknown != var->_basic_type);
		switch (var->_basic_type)
		{
		case eBasicObjType_Enum: {
			// TBD. need enum_array template
			}break;
		case eBasicObjType_Interface: {
			// TBD. need interface_array template

			} break;
		case eBasicObjType_Typedefed:
			// todo:
			break;

		case eBasicObjType_UserDefined: {
			auto* usrdef = var->m_pUserDefType;
			auto* mod = usrdef->GetModule();
			assert(nullptr != mod);
			if (mod != cmod ||
				proxy || observable) {
				ret += mod->getname();
			}
			ret += usrdef->getname();
		}	break;
		}

	} else {
		type = variable_types[var->_basic_type];
		if (type) {
			ret += type;
			return 0;
		}

		assert(eBasicObjType_Unknown != var->_basic_type);
		switch (var->_basic_type)
		{
		case eBasicObjType_Enum: {
			auto* enumdeft = var->m_pEnumDefType;
			auto* mod = enumdeft->GetModule();
			assert(nullptr != mod);
			ret += enumdeft->getname();
		}break;
		case eBasicObjType_Interface: {
			auto* interf = var->m_pIFType;
			auto* mod = interf->GetModule();
			assert(nullptr != mod);
			ret += interf->getname();
			} break;
		case eBasicObjType_Typedefed:{
			auto* reft = var->m_pRefType;
			ret += reft->get_fullname(cmod);
		}break;

		case eBasicObjType_UserDefined: {

			auto* usrdef = var->m_pUserDefType;
			auto* mod = usrdef->GetModule();
			assert(nullptr != mod);
			ret += mod->getname();
			ret += ".";
			ret += usrdef->getname();
		}	break;
		}
	}

	return 0;
}

int interface_object_java_code_generator::variable_define_full_name(
	module* cmod, TVariableObject* var, string& ret,
	bool proxy, bool observable, bool brefs)
{
	const char* variable_types[] = {
		"void",	// unknown
		"bool",		// bool
		"uint8_t",	// uint8
		"int16_t",	// int16
		"int32_t",	// int32
		"int64_t",	// long
		"uint32_t",	// uint32
		"double",	// Float
		nullptr,	// stream
		"std::string",	// string
		nullptr,	// enum - special handling
		nullptr,	// interface - shall not have this
		nullptr,	// typedefed - special handling
		nullptr,	// userdefined - special handling
	};

	const char* variable_array_types[] = {
		nullptr,	// unknown
		"boolean_array",		// bool
		"uint32_array",	// uint8
		"int32_array",	// int16
		"int32_array",	// int32
		"int64_array",	// long
		"uint32_array",	// uint32
		"double_array",	// Float
		nullptr,	// stream
		"string_array",	// string
		nullptr,	// enum - special handling
		nullptr,	// interface - shall not have this
		nullptr,	// typedefed - special handling
		nullptr,	// userdefined - special handling
	};

	ret.clear();
	const char* type = nullptr;
	if (is_array(var->m_ArrayType)) {
		type = variable_array_types[var->_basic_type];
		if (type) {
			ret += type;
			if (brefs)
				ret += "&";
			return 0;
		}
	} else {
		type = variable_types[var->_basic_type];
		if (type) {
			ret += type;
			if (brefs)
				ret += "&";
			return 0;
		}
	}

	assert(eBasicObjType_Unknown != var->_basic_type);
	switch (var->_basic_type)
	{
	case eBasicObjType_Interface: {
		auto* interf = var->m_pIFType;
		auto* mod = interf->GetModule();
		assert(nullptr != mod);
		if (mod != cmod) {
			ret += "::";
			ret += mod->getname();
			ret += "::";
		}
		if (is_array(var->m_ArrayType)) {
			if (mod != cmod)
				ret += "array::";
			else 
				ret += "::array::";
		} else {
			if (proxy && mod != cmod)
				ret += "proxy::";
			else if (!proxy && mod != cmod) {
				if (interf->TestInterfaceFlags(eInterfaceFlag_Observable))
					ret += "skeleton::";
			} else if (!proxy && mod == cmod) {
				if (!interf->TestInterfaceFlags(eInterfaceFlag_Observable)) {
					if (observable) {
						ret += "::";
						ret += mod->getname();
					}
				} else {
					if (!observable) {
						ret += "skeleton::";
					}
				}
			}
		}
		ret += interf->getname();
		if (brefs)
			ret += "&";
		} break;
	case eBasicObjType_Typedefed:
		// todo:
		break;

	case eBasicObjType_UserDefined: {
		auto* usrdef = var->m_pUserDefType;
		auto* mod = usrdef->GetModule();
		assert(nullptr != mod);
		if (proxy || mod != cmod || observable) {
			ret += "::";
			ret += mod->getname();
			ret += "::";
		}
		if (is_array(var->m_ArrayType))
			ret += "array::";
		ret += usrdef->getname();
		if (brefs)
			ret += "&";
	}	break;
	}

	return 0;
}

int interface_object_java_code_generator::generate_proxy_header(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* clsname = getname().c_str();

	if (TestInterfaceFlags(eInterfaceFlag_Singleton))
		mod->fprintf(srcfile_module_proxy_header,
			"	static %s& inst(void);\n",
			clsname);	

	list_item* subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		// check if this is a method
		if (eIFNodeType_Method != node->GetType()) {
			continue;
		}
		if (generate_proxy_header_method(node)) {
			return 1;
		}
	}
	return 0;
}

int interface_object_java_code_generator::generate_proxy_observable_header(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* clsname = getname().c_str();

	mod->fprintf(srcfile_module_proxy_header,
		"namespace proxy {\n\n"
		"class %s\n"
		"{\n"
		"	%s();\n"
		"	virtual ~%s();\n\n"
		"public:\n",
		clsname,
		clsname,
		clsname);

	list_item* subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		// check if this is a method
		if (eIFNodeType_Method != node->GetType()) {
			continue;
		}

		string tmp;
		method_retval_variable_name(mod, node, tmp, false, true, true);
		mod->fprintf(srcfile_module_proxy_header,
			"	virtual %s %s(", tmp.c_str(), node->getname().c_str());
		bool first = true;
		list_item *item = node->m_VariableList.getnext();
		for (; item != &(node->m_VariableList); item = item->getnext())
		{
			auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
			variable_full_name(mod, obj, tmp,
				obj->test_flags(varobj_flag_method_param_out), true, true);
			if (first) {
				first = false;
				mod->fprintf(srcfile_module_proxy_header,
					"%s %s", tmp.c_str(), obj->getname().c_str());
			}
			else
				mod->fprintf(srcfile_module_proxy_header,
					", %s %s", tmp.c_str(), obj->getname().c_str());
		}
		mod->fputs(srcfile_module_proxy_header, ");\n");
	}

	mod->fprintf(srcfile_module_proxy_header,
		"};} // end of namespace proxy\n\n");

	return 0;
}

int interface_object_java_code_generator::generate_proxy_header_method(
	TInterfaceNode* node)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;

	string tmp;

	method_retval_variable_name(mod, node, tmp, false, true);
	mod->fprintf(srcfile_module_proxy_header,
		"	%s %s(", tmp.c_str(), node->getname().c_str());
	bool first = true;
	list_item *item = node->m_VariableList.getnext();
	for (; item != &(node->m_VariableList); item = item->getnext())
	{
		auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		variable_full_name(mod, obj, tmp,
			obj->test_flags(varobj_flag_method_param_out), true);
		if (first) {
			first = false;
			mod->fprintf(srcfile_module_proxy_header,
				"%s %s", tmp.c_str(), obj->getname().c_str());
		}
		else
			mod->fprintf(srcfile_module_proxy_header,
				", %s %s", tmp.c_str(), obj->getname().c_str());
	}
	mod->fputs(srcfile_module_proxy_header, ");\n");

	return 0;
}

int interface_object_java_code_generator::generate_skeleton_header_method(
	TInterfaceNode* node)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;

	string tmp;

	method_retval_variable_name(mod, node, tmp, false, false);
	mod->fprintf(srcfile_module_proxy_header,
		"	virtual %s %s(", tmp.c_str(), node->getname().c_str());
	bool first = true;
	list_item *item = node->m_VariableList.getnext();
	for (; item != &(node->m_VariableList); item = item->getnext())
	{
		auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		variable_full_name(mod, obj, tmp,
			obj->test_flags(varobj_flag_method_param_out), false);
		if (first) {
			first = false;
			mod->fprintf(srcfile_module_proxy_header,
				"%s %s", tmp.c_str(), obj->getname().c_str());
		}
		else
			mod->fprintf(srcfile_module_proxy_header,
				", %s %s", tmp.c_str(), obj->getname().c_str());
	}
	mod->fputs(srcfile_module_proxy_header, ");\n");

	return 0;
}

int interface_object_java_code_generator::generate_skeleton_header(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* clsname = getname().c_str();

	mod->fprintf(srcfile_module_proxy_header,
		"namespace skeleton {\n"
		"	void import_%s(void);\n"
		"};\n\n",
		clsname);
	mod->fprintf(srcfile_module_proxy_header,
		"class %s\n"
		"{\n"
		"public:\n"
		"	%s();\n"
		"	~%s();\n\n"

		"	%s(const %s&) = delete;\n"
		"	void operator=(const %s&) = delete;\n\n"

		"	static %s* create_instance(void);\n"
		"	static void destroy_instance(%s*);\n\n"

		"public:\n",
		clsname,
		clsname,
		clsname,
		clsname, clsname,
		clsname,
		clsname,
		clsname);


	list_item* subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		// check if this is a method
		if (eIFNodeType_Method != node->GetType()) {
			continue;
		}
		if (generate_skeleton_header_method(node)) {
			return 1;
		}
	}

	mod->fprintf(srcfile_module_proxy_header,
		"};\n\n");

	return 0;
}

int interface_object_java_code_generator::generate_skeleton_observable_header(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* clsname = getname().c_str();

	mod->fprintf(srcfile_module_proxy_header,
		"namespace skeleton {\n"
		"class %s\n"
		"{\n"
		"public:\n"
		"	%s();\n"
		"	%s(void* inst_id, const char* cliname, const char* inst_name);\n"
		"	%s(const %s&);\n"
		"	const %s& operator=(const %s&);\n"
		"	~%s();\n\n"

		"public:\n"
		"	void import_%s(void);\n",
		clsname,
		clsname,
		clsname,
		clsname, clsname,
		clsname, clsname,
		clsname,
		clsname);

	list_item* subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		// check if this is a method
		if (eIFNodeType_Method != node->GetType()) {
			continue;
		}
		string tmp;

		method_retval_variable_name(mod, node, tmp, false, false, true);
		mod->fprintf(srcfile_module_proxy_header,
			"	virtual %s %s(", tmp.c_str(), node->getname().c_str());
		bool first = true;
		list_item *item = node->m_VariableList.getnext();
		for (; item != &(node->m_VariableList); item = item->getnext())
		{
			auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
			variable_full_name(mod, obj, tmp,
				obj->test_flags(varobj_flag_method_param_out), false, true);
			if (first) {
				first = false;
				mod->fprintf(srcfile_module_proxy_header,
					"%s %s", tmp.c_str(), obj->getname().c_str());
			}
			else
				mod->fprintf(srcfile_module_proxy_header,
					", %s %s", tmp.c_str(), obj->getname().c_str());
		}
		mod->fputs(srcfile_module_proxy_header, ");\n");
	}

	mod->fprintf(srcfile_module_proxy_header,
		"private:\n"
		"	void* _inst_id;\n"
		"	std::string _cliname;\n"
		"	std::string _inst_name;\n"
		"};}\n\n");


	return 0;
}

int interface_object_java_code_generator::generate_proxy_source(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* clsname = getname().c_str();

	mod->fprintf(scrfile_module_proxy_source,
		"const char* %s::%s_clsname_ = \"%s.%s\";\n"
		"rpcproxy_static_data %s::%s_static_data_ = {\n"
		"	nullptr,\n"
		"	nullptr,\n"
		"	\"%s\",\n"
		"	\"%s\",\n"
		"	nullptr,\n"
		"	%s_clsname_,\n"
		"};\n\n",
		clsname, clsname, mod->getname().c_str(), clsname,
		clsname, clsname,
		mod->getconfig().get_package_name().c_str(),
		mod->getconfig().get_service_name().c_str(),
		clsname);

	if (TestInterfaceFlags(eInterfaceFlag_Singleton)) {
		mod->fprintf(scrfile_module_proxy_source,
			"%s& %s::inst(void)\n"
			"{\n"
			"	static %s _inst;\n"
			"	if (!_inst._proxy_base) {\n"
			"		_inst._proxy_base = rpcproxy::get_singleton_instance(\n"
			"			&%s_static_data_);\n"
			"		assert(_inst._proxy_base->is_vaild());\n"
			"	}\n"
			"	return _inst;\n"
			"}\n\n",
			clsname, clsname,
			clsname,
			clsname);
	}

	std::string tmp;
	list_item* subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		// check if this is a method
		if (eIFNodeType_Method != node->GetType()) {
			continue;
		}
		generate_source_method_header(scrfile_module_proxy_source, node);
		mod->fputs(scrfile_module_proxy_source, "\n{\n");
		generate_source_method_body_header(scrfile_module_proxy_source, node);
		list_item *item = node->m_VariableList.getnext();
		for (; item != &(node->m_VariableList); item = item->getnext())
		{
			auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
			if (!generate_source_method_body_inparam(mod, obj, tmp))
				mod->fprintf(scrfile_module_proxy_source, "	%s\n", tmp.c_str());
		}
		mod->fprintf(scrfile_module_proxy_source,
			"\n	auto* ret = rpcproxy::invoke(get_instid(), uid,\n"
			"		&inparam, &inout_param, &%s_static_data_, 1);\n"
			"	if (nullptr != ret) _proxy_base = ret;\n\n",
			clsname);
		item = node->m_VariableList.getnext();
		for (; item != &(node->m_VariableList); item = item->getnext())
		{
			auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
			if (!generate_source_method_body_outparam(mod, obj, tmp))
				mod->fprintf(scrfile_module_proxy_source, "	%s\n", tmp.c_str());
		}
		if (!generate_source_method_body_ret_param(mod, node, tmp))
			mod->fprintf(scrfile_module_proxy_source, "	%s\n", tmp.c_str());

		mod->fputs(scrfile_module_proxy_source, "}\n\n");
	}

	return 0;
}

int interface_object_java_code_generator::generate_interface_source(void)
{
	module *mod = GetModule();
	
	if (nullptr == mod) return 1;
	const char* clsname = getname().c_str();

	string content = "import com.civip.rpc.%s.entity.*;\n\n";

	if(has_virtual()) {
		mod->fprintf(("I" + getname() + "_virtual").c_str(), scrfile_module_source_virtual, content.c_str(), mod->getname().c_str());
		mod->fputs(("I" + getname() + "_virtual").c_str(), scrfile_module_source_virtual, "import com.civip.rpc.core.type.*;\n\n");
		mod->fprintf(("I" + getname() + "_virtual").c_str(), scrfile_module_source_virtual, "public interface %s {\n\n",("I" + getname() + "_virtual").c_str());
	}

	if(has_observer_param(true, nullptr, nullptr)){
		content += "import org.springframework.beans.factory.annotation";
		content += ".Autowired;\n";
		content += "import com.civip.rpc.client.aop.RPCDynamicService;\n\n";
	}

	content += "@Component\n";
	content += "@RPC_service(pkg = \"%s\","; 
	content += "name = \"%s\", className = \"%s.%s\")\n";

	if(TestInterfaceFlags(eInterfaceFlag_Singleton)){
		content += "@RPC_type(RPCClassType.singleton)\n";
	}
	content += "public class %s extends RPCService {\n\n";
	if(has_observer_param(true, nullptr, nullptr)) {
		content += "	@Autowired\n";
		content += "	private transient RPCDynamicService dynamicService;\n\n";
	}

	mod->fprintf(clsname, scrfile_module_source,content.c_str(),
		mod->getname().c_str(), mod->getconfig().get_package_name().c_str(), 
		mod->getconfig().get_service_name().c_str(), 
		mod->getname().c_str(), clsname, 
		clsname);

	if(has_virtual()) {
		mod->fprintf(clsname, scrfile_module_source, "	public %s %s;\n\n", 
			("I" + getname() + "_virtual").c_str() , 
			(getname() + "_virtual").c_str());
	}

	if(has_construct()){
		mod->fprintf(clsname, scrfile_module_source,
			"\tpublic %s () \n	{\n	}\n", clsname);
	}

	std::string tmp;
	list_item* subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		// check if this is a method
		if (eIFNodeType_Attribute == node->GetType()) {
			generate_attribute_body(clsname, node);
			continue;
		} else if (eIFNodeType_Method != node->GetType()) {
			continue;
		}
		generate_source_method_emtpy(mod, node, scrfile_module_source,
			false, false, true);
	}

	mod->fputs(clsname, scrfile_module_source, "}\n");
	if(has_virtual()) {
		mod->fputs(("I" + getname() + "_virtual").c_str(), 
			scrfile_module_source_virtual, "}\n");
		mod->closefile(scrfile_module_source_virtual);
	}

	mod->closefile(scrfile_module_source);
	return 0;
}

bool interface_object_java_code_generator::has_construct() {
	list_item* subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);
		if (eIFNodeType_Method != node->GetType()) {
			continue;
		}

		if (node->m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Constructor)) {
			return true;
		}
	}
	return	false;
}
bool interface_object_java_code_generator::has_virtual() {
	list_item* subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);
		if(!node->GetType() == eIFNodeType_Method)
			continue;
		if (node->m_Data.method.test_flags
		(TInterfaceNode::eMethodFlag_Virtual))  {
			return true;
		}
	}
	return false;
}

int interface_object_java_code_generator::generate_attribute_body(
	const char* clsname, TInterfaceNode* node)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;

	string tmp;
	attribute_variable_name(mod, node, tmp);

	mod->fprintf(clsname, scrfile_module_source,
		"	private %s %s;\n", tmp.c_str(), node->getname().c_str());
	mod->fprintf(clsname, scrfile_module_source,
		"	public %s get_%s() \n	{\n		return this.%s;\n	}\n", 
		tmp.c_str(), node->getname().c_str(), node->getname().c_str());
	mod->fprintf(clsname, scrfile_module_source,
		"	public void set_%s(%s %s) \n	{\n\n	}\n", node->getname().c_str(), tmp.c_str(), node->getname().c_str());

}

int interface_object_java_code_generator::attribute_variable_name(
	module* cmod, TInterfaceNode* node, string& ret)
{
	TVariableObject varobj;

	varobj._basic_type = node->m_Data.attribute._basic_type;
	varobj.m_pIFType = node->m_Data.attribute.m_pIFObject;
	varobj.m_pRefType = node->m_Data.attribute.m_pTypedefType;
	varobj.m_pEnumDefType = node->m_Data.attribute.m_pEnumDef;
	varobj.m_pUserDefType = node->m_Data.attribute.m_pUserDefType;

	return get_attribute_full_name(cmod, &varobj, ret);
}

int interface_object_java_code_generator::get_attribute_full_name(
	module* cmod, TVariableObject* var, string& ret)
{
	const char* var_types[] = {
		"void",	// unknown
		"boolean",	// bool
		"byte",	// uint8
		"short",	// int16
		"int",	// int32
		"long",	// long
		"int",	// uint32
		"double",	// Float
		"Object",	// stream
		"String",	// string
		nullptr,	// enum - special handling
		nullptr,	// interface - shall not have this
		nullptr,	// typedefed - special handling
		nullptr,	// userdefined - special handling
	};

	const char* var_array_types[] = {
		nullptr,	// unknown
		"boolean[]",		// bool
		"int[]",	// uint8
		"int[]",	// int16
		"int[]",	// int32
		"long[]",	// long
		"int[]",	// uint32
		"double[]",	// Float
		"Object[]",	// stream
		"String[]",	// string
		nullptr,	// enum - special handling
		nullptr,	// interface - shall not have this
		nullptr,	// typedefed - special handling
		nullptr,	// userdefined - special handling
	};

	ret.clear();
	if (eBasicObjType_Unknown == var->_basic_type) {
		ret += "void";
		return 0;
	}
	const char* type = nullptr;
	if (is_array(var->m_ArrayType)) {
		type = var_array_types[var->_basic_type];
		if (type) {
			ret += type;
			return 0;
		}

		assert(eBasicObjType_Unknown != var->_basic_type);
		switch (var->_basic_type)
		{
		case eBasicObjType_Enum: {
			auto* enumtype = var->m_pEnumDefType;
			auto* mod = enumtype->GetModule();
			assert(nullptr != mod);
			if (mod != cmod) {
				ret += mod->getname();
			}
			ret += enumtype->getname();
			}break;
		case eBasicObjType_Interface: {
			auto* iftype = var->m_pIFType;
			auto* mod = iftype->GetModule();
			assert(nullptr != mod);
			if (mod != cmod) {
				ret += mod->getname();
			}
			ret += iftype->getname();
			} break;
		case eBasicObjType_Typedefed:
			// todo:
			break;
		case eBasicObjType_UserDefined: {
			auto* usrdef = var->m_pUserDefType;
			auto* mod = usrdef->GetModule();
			assert(nullptr != mod);
			ret += mod->getname();
			ret += ".";
			ret += usrdef->getname();
		}	break;
		}

	} else {
		type = var_types[var->_basic_type];
		if (type) {
			ret += type;
			return 0;
		}

		assert(eBasicObjType_Unknown != var->_basic_type);
		switch (var->_basic_type)
		{
		case eBasicObjType_Enum: {
			auto* enumdeft = var->m_pEnumDefType;
			auto* mod = enumdeft->GetModule();
			assert(nullptr != mod);
			if (mod != cmod) {
				ret += mod->getname();
			} else {
				ret += cmod->getname();
			}
			ret += enumdeft->getname();
		}break;
		case eBasicObjType_Interface: {
			auto* interf = var->m_pIFType;
			auto* mod = interf->GetModule();
			assert(nullptr != mod);
			if (mod != cmod) {
				ret += mod->getname();
			} 
			ret += interf->getname();
			} break;
		case eBasicObjType_Typedefed:{
			auto* reft = var->m_pRefType;
			ret += reft->get_fullname(cmod);
		}break;

		case eBasicObjType_UserDefined: {
			auto* usrdef = var->m_pUserDefType;
			auto* mod = usrdef->GetModule();
			assert(nullptr != mod);
			
			ret += mod->getname();
			ret += ".";
			ret += usrdef->getname();
		}	break;
		}
	}
	return 0;
}

int interface_object_java_code_generator::generate_observable_proxy_source(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* clsname = getname().c_str();

	list_item* subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		// check if this is a method
		if (eIFNodeType_Method != node->GetType()) {
			continue;
		}
		generate_source_call_mehtod(scrfile_module_proxy_source, mod, node, true);
	}

	mod->fprintf(scrfile_module_proxy_source,
		"%s::%s()\n"
		"{\n"
		"	static bool registered = false;\n"
		"	if (!registered)\n"
		"	{\n"
		"		pbf_wrapper inparam, inout_param;\n"
		"		rpcinterface interface = rpcobservable::inst()->\n"
		"			register_observable_interface(\"%s.%s\");\n\n",
		clsname, clsname,
		mod->getname().c_str(), clsname);

	std::string classname = mod->getname() + clsname;
	std::string methoduid;
	std::string mname;

	subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		// check if this is a method
		if (eIFNodeType_Method != node->GetType()) {
			continue;
		}
		mname = classname + node->getname();
		generate_uid(mname.c_str(), methoduid);
		mod->fprintf(scrfile_module_proxy_source,
			"		uint128_t u_%s = %s;\n\n",
			node->getname().c_str(), methoduid.c_str());
		mod->fprintf(scrfile_module_proxy_source,
		"		inparam = new ::%s::struct_wrapper::___%s_%s_inparam_wrapper;\n"
		"		inout_param = new ::%s::struct_wrapper::___%s_%s_inout_param_wrapper;\n"
		"		interface->add_method(u_%s, inparam, inout_param, (void*)___%s_%s_obsvhdr);\n",
		mod->getname().c_str(), clsname, node->getname().c_str(),
		mod->getname().c_str(), clsname, node->getname().c_str(),
		node->getname().c_str(),
		clsname, node->getname().c_str());
	}
	mod->fprintf(scrfile_module_proxy_source,
		"		registered = true;\n"
		"	}\n"
		"	rpcobservable::inst()->add_observable_instance(\"%s.%s\",(void*)this);\n"
		"}\n",
	mod->getname().c_str(), clsname);


	mod->fprintf(scrfile_module_proxy_source,
		"%s::%s()\n"
		"{\n"
		"}\n\n",
		clsname, clsname);

	subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		// check if this is a method
		if (eIFNodeType_Method != node->GetType()) {
			continue;
		}
		generate_source_method_emtpy(mod, node, scrfile_module_proxy_source,
			true, true, false);
	}

	return 0;
}

int interface_object_java_code_generator::generate_source_method_header(
	srcfile_type id, TInterfaceNode *node)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	
	auto* interf = node->m_pIFObject;
	if (nullptr == interf) return 2;
	string tmp;

	method_retval_variable_name(mod, node, tmp, false, true, false);
	mod->fprintf(id,
		"%s %s::%s(", 
		tmp.c_str(), interf->getname().c_str(), node->getname().c_str());
	bool first = true;
	list_item *item = node->m_VariableList.getnext();
	for (; item != &(node->m_VariableList); item = item->getnext())
	{
		auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		variable_full_name(mod, obj, tmp,
			obj->test_flags(varobj_flag_method_param_out), true, false);
		if (first) {
			first = false;
			mod->fprintf(id,
				"%s %s", tmp.c_str(), obj->getname().c_str());
		}
		else
			mod->fprintf(id,
				", %s %s", tmp.c_str(), obj->getname().c_str());
	}
	mod->fprintf(id,
	")");
}

int interface_object_java_code_generator::generate_source_method_body_header(
	srcfile_type id, TInterfaceNode *node)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	
	auto* interf = node->m_pIFObject;
	if (nullptr == interf) return 2;

	bool has_input = false;
	bool has_out = false;
	method_has_in_out_param(node, has_input, has_out);
	
	if (has_input)
		mod->fprintf(id,
			"	::%s_pbf::___%s_%s_inparam inparam;\n",
			mod->getname().c_str(), interf->getname().c_str(), node->getname().c_str());
	if (has_out)
		mod->fprintf(id,
			"	::%s_pbf::___%s_%s_inout_param inout_param;\n",
			mod->getname().c_str(), interf->getname().c_str(), node->getname().c_str());
	std::string name = mod->getname() + interf->getname() + node->getname();
	std::string methoduid;
	generate_uid(name.c_str(), methoduid);
	mod->fprintf(id,
		"	uint128_t uid = %s;\n\n", methoduid.c_str());
	return 0;
}

int interface_object_java_code_generator::generate_source_method_body_inparam(
	module* cmod, TVariableObject* var, string& ret)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	ret.clear();
	
	switch (var->_basic_type)
	{
	case basic_objtype_boolean:
	case basic_objtype_uint8:
	case eBasicObjType_Int16:
	case eBasicObjType_Int32:
	case eBasicObjType_Int64:
	case eBasicObjType_UInt32:
	case eBasicObjType_Float:
	case eBasicObjType_String:
		if (var->test_flags(varobj_flag_method_param_out)){
			ret += "inout_param.set_";
			ret += var->getname();
			ret +="(" +var->getname();
			ret += ");";
		} else {
			ret += "inparam.set_";
			ret += var->getname();
			ret +="(" +var->getname();
			ret += ");";
		}
		return 0;
		break;
	case eBasicObjType_Stream:
	case eBasicObjType_Enum:
		break;
	case eBasicObjType_Interface: {
		if (var->test_flags(varobj_flag_method_param_out)){
			ret += "inout_param.set_";
			ret += var->getname();
			ret +="((size_t)& " +var->getname();
			ret += ");\n";
			ret += "	rpcobservable_data* data = rpcobservable::inst()->get_observable_data();\n";
			ret += "	assert(NULL != data);\n";
			ret += "	std::string cliname = data->pkg_name;\n";
			ret += "	std::string instname = data->inst_name;\n";
			ret += "	inout_param.set_client_name(cliname);\n";
			ret += "	inout_param.set_inst_name(instname);";
		} else {
			ret += "inparam.set_";
			ret += var->getname();
			ret +="((size_t)& " +var->getname();
			ret += ");\n";
			ret += "	rpcobservable_data* data = rpcobservable::inst()->get_observable_data();\n";
			ret += "	assert(NULL != data);\n";
			ret += "	std::string cliname = data->pkg_name;\n";
			ret += "	std::string instname = data->inst_name;\n";
			ret += "	inparam.set_client_name(cliname);\n";
			ret += "	inparam.set_inst_name(instname);";
		}
		return 0;	
		} break;
	case eBasicObjType_Typedefed:
		// todo:
		break;

	case eBasicObjType_UserDefined: {
		if (var->test_flags(varobj_flag_method_param_out)){
			ret += var->getname();
			ret += ".copyto(*inout_param.mutable_";
			ret += var->getname() + "());";
		} else {
			ret += var->getname();
			ret += ".copyto(*inparam.mutable_";
			ret += var->getname() + "());";
		}
		return 0;
	}	break;
	}
	return -1;
}

int interface_object_java_code_generator::generate_source_method_body_outparam(
	module* cmod, TVariableObject* var, string& ret)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	ret.clear();
	

	switch (var->_basic_type)
	{
	case basic_objtype_boolean:
	case basic_objtype_uint8:
	case eBasicObjType_Int16:
	case eBasicObjType_Int32:
	case eBasicObjType_Int64:
	case eBasicObjType_UInt32:
	case eBasicObjType_Float:
	case eBasicObjType_String:
		if (var->test_flags(varobj_flag_method_param_out)){
			ret += var->getname();
			ret += " = inout_param.get_";
			ret += var->getname();
			ret += "());";
			return 0;
		}
		break;
	case eBasicObjType_Stream:
	case eBasicObjType_Enum:
		break;
	case eBasicObjType_Interface: {
	
		} break;
	case eBasicObjType_Typedefed:
		// todo:
		break;

	case eBasicObjType_UserDefined: {
		if (var->test_flags(varobj_flag_method_param_out)){
			ret += var->getname();
			ret += ".copyfrom(*inout_param.mutable_";
			ret += var->getname() + "());";
			return 0;
		}
	}	break;
	}
	return -1;
}

int interface_object_java_code_generator::generate_source_call_method_var(
	module* cmod, TVariableObject* var, string& ret)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	ret.clear();
	int retval = 1;

	std::string tmp;
	variable_define_full_name(cmod, var, tmp,
		true, true);

	switch (var->_basic_type)
	{
	case basic_objtype_boolean:
	case basic_objtype_uint8:
	case eBasicObjType_Int16:
	case eBasicObjType_Int32:
	case eBasicObjType_Int64:
	case eBasicObjType_UInt32:
	case eBasicObjType_Float:
	case eBasicObjType_String:{
		ret += tmp + " ";
		ret += var->getname();
		if (var->test_flags(varobj_flag_method_param_out)){
			ret += " = inoutp->get_";
		} else {
			ret += " = inp->get_";
		}
		ret += var->getname();
		ret += "();";
		retval = 0;
	}
	break;
	case eBasicObjType_Stream:
	case eBasicObjType_Enum:
		break;
	case eBasicObjType_Interface: {
		ret += tmp + " ";
		ret += var->getname();
		if (var->test_flags(varobj_flag_method_param_out)){
			ret += "(inoutp->get_";
			ret += var->getname();
			ret += "(), inoutp->get_client_name(), inoutp->get_inst_name());";
		} else {
			ret += "(inp->get_";
			ret += var->getname();
			ret += "(), inp->get_client_name(), inp->get_inst_name());";
		}
		retval = 0;	
		} break;
	case eBasicObjType_Typedefed:
		// todo:
		break;

	case eBasicObjType_UserDefined: {
		ret += tmp + " ";
		ret += var->getname();
		if (var->test_flags(varobj_flag_method_param_out)){
			ret += "(inout_parm, inoutp->mutable_";
		} else {
			ret += "(inparm, inp->mutable_";
		}
		ret += var->getname();
		ret += "());";
		retval = 0;
	}	break;
	}
	return retval;
}

int interface_object_java_code_generator::generate_source_call_method_end_var(
	module* cmod, TVariableObject* var, string& ret)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	ret.clear();
	int retval = 1;

	std::string tmp;
	variable_full_name(cmod, var, tmp,
		var->test_flags(varobj_flag_method_param_out), true, true, false);

	switch (var->_basic_type)
	{
	case basic_objtype_boolean:
	case basic_objtype_uint8:
	case eBasicObjType_Int16:
	case eBasicObjType_Int32:
	case eBasicObjType_Int64:
	case eBasicObjType_UInt32:
	case eBasicObjType_Float:
	case eBasicObjType_String:{
		ret += tmp + " ";
		ret += var->getname();
		if (var->test_flags(varobj_flag_method_param_out)){
			ret += " = inoutp.get_";
		} else {
			ret += " = inp.get_";
		}
		ret += var->getname();
		ret += "();";
		retval = 0;
	}
	break;
	case eBasicObjType_Stream:
	case eBasicObjType_Enum:
		break;
	case eBasicObjType_Interface: {
		ret += "inoutp->set_";
		ret += var->getname();
		ret += "(&";
		ret += var->getname();
		ret += ");";
		retval = 0;	
		} break;
	case eBasicObjType_Typedefed:
		// todo:
		break;

	case eBasicObjType_UserDefined: {
		ret += var->getname();
		ret += ".copyto(*(inoutp->mutable_";
		ret += var->getname();
		ret += "()));";
		retval = 0;	
	}	break;
	}
	return retval;
}

int interface_object_java_code_generator::generate_source_method_body_ret_param(
	module* cmod, TInterfaceNode* obj, string& ret)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	ret.clear();
	
	switch (obj->m_Data.method.m_eRetValType)
	{
	case basic_objtype_boolean:
	case basic_objtype_uint8:
	case eBasicObjType_Int16:
	case eBasicObjType_Int32:
	case eBasicObjType_Int64:
	case eBasicObjType_UInt32:
	case eBasicObjType_Float:
	case eBasicObjType_String:
		ret += "return inout_param.___retval();";
		return 0;
		break;
	case eBasicObjType_Stream:
	case eBasicObjType_Enum:
		break;
	case eBasicObjType_Interface: {
		std::string tmp;
		method_retval_variable_name(mod, obj, tmp, false, true, false);
		ret += tmp;
		ret += " retval(inout_param.___retval());\n";
		ret += "	return retval;";
		return 0;
		} break;
	case eBasicObjType_Typedefed:
		// todo:
		break;

	case eBasicObjType_UserDefined: {
		std::string tmp;
		method_retval_variable_name(mod, obj, tmp, false, true, false);
		ret += tmp;
		ret += " retval;\n";
		ret += "	retval.copyfrom(*inout_param.mutable____retval());\n";
		ret += " return retval;";
		return 0;
	}	break;
	}
	return -1;
}

int interface_object_java_code_generator::generate_source_method_body_return_default(
	module* cmod, TInterfaceNode* obj, string& ret)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	ret.clear();
	int retval = 0;
	switch (obj->m_Data.method.m_eRetValType)
	{
	case basic_objtype_boolean:
	case basic_objtype_uint8:
	case eBasicObjType_Int16:
	case eBasicObjType_Int32:
	case eBasicObjType_Int64:
	case eBasicObjType_UInt32:
		ret += "	return null;";
		break;
	case eBasicObjType_Float:
		ret += "	return null;";
		break;
	case eBasicObjType_String:
		ret += "	return null;";
		break;
	case eBasicObjType_Stream:
	case eBasicObjType_Enum:
		ret += "	return null;";
		break;
	case eBasicObjType_Interface:
	case eBasicObjType_Typedefed:
	case eBasicObjType_UserDefined: {
		ret += "	return null;";
	}	break;
	default:
		retval = 2;
	}
	return retval;
}



bool interface_object_java_code_generator::has_observer_param (bool isGlobal, 
	TInterfaceNode* node, TVariableObject *varObj) {
	
	if(isGlobal == true){
		std::string tmp;
		list_item* subitem = NodeList().getnext();
		for (; subitem != &NodeList(); subitem = subitem->getnext())
		{
			auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
			EIFNodeType eType = node->GetType();
			assert(eIFNodeType_Unknown != eType);

			// check if this is a method
			if (eIFNodeType_Method != node->GetType()) {
				continue;
			}
			list_item *item = node->m_VariableList.getnext();
			for (; item != &(node->m_VariableList); item = item->getnext())
			{
				auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
				if(eBasicObjType_Interface == obj->_basic_type)
				{
					if(obj->m_pIFType->TestInterfaceFlags(
						eInterfaceFlag_Observable)){
						return true;
					}
				}
			}
		}
		return false;
	} else {
		if(nullptr != node) {
			list_item *item = node->m_VariableList.getnext();
			for (; item != &(node->m_VariableList); item = item->getnext())
			{
				auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
				if(eBasicObjType_Interface == obj->_basic_type)
				{
					if(obj->m_pIFType->TestInterfaceFlags(
						eInterfaceFlag_Observable)){
						return true;
					}
				}
			}
		}
		if(nullptr != varObj) {
			if(eBasicObjType_Interface == varObj->_basic_type)
			{
				if(varObj->m_pIFType->TestInterfaceFlags(
					eInterfaceFlag_Observable)){
					return true;
				}
			}
		}
		return false;
	}
}

int interface_object_java_code_generator::generate_source_method_emtpy(
	module* cmod, TInterfaceNode* node, srcfile_type id,
	bool proxy, bool bobservable, bool bcomments)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	if (nullptr == node) return 1;
	const char* clsname = getname().c_str();
	list_item* subitem = NodeList().getnext();

	if (node->m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Constructor)) {
		mod->fprintf(clsname, id,
			"\tpublic %s (", node->m_pIFObject->getname().c_str());

		string tmp;
		bool first = true;
		list_item *item = node->m_VariableList.getnext();
		for (; item != &(node->m_VariableList); item = item->getnext())
		{
			auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
			variable_full_name(mod, obj, tmp, false, false, false);
			if (first) {
				first = false;
				mod->fprintf(clsname, id,
					"%s %s", tmp.c_str(), obj->getname().c_str());
			}
			else
				mod->fprintf(clsname, id,
					", %s %s", tmp.c_str(), obj->getname().c_str());
		}
		mod->fputs(clsname, id, ") \n	{\n	}\n");
	} else {

		string tmp;
		method_retval_variable_name(mod, node, tmp, false, proxy, bobservable);

		std::string mname;
		std::string methoduid;

		mname = mod->getname() + clsname + node->getname();
		generate_uid(mname.c_str(), methoduid);

		if (TestInterfaceFlags(eInterfaceFlag_Observable) && 
			id == scrfile_module_observer_source) {
			mod->fputs(clsname, id, "	@Override\n");	
		}

		string method_anno = "	@RPC_function(uid = \"%s\"";
		bool has_in = false;
		bool has_out = false;
		method_has_in_out_param(node, has_in, has_out);
		if (has_in) {
			method_anno += ", iparam = %s.___%s_%s_inparam.class";
		}
		if (has_out) {
			method_anno += ", oparam = %s.___%s_%s_inout_param.class";
		}
		method_anno += ")\n";

		if (has_in & has_out) {
			mod->fprintf(clsname, id, method_anno.c_str(), methoduid.c_str(),
				mod->getname().c_str(),clsname,node->getname().c_str(),
				mod->getname().c_str(),clsname,node->getname().c_str());
		} else if (has_in | has_out) {
			mod->fprintf(clsname, id, method_anno.c_str(), methoduid.c_str(),
				mod->getname().c_str(), clsname,node->getname().c_str());
		} else {
			mod->fprintf(clsname, id, method_anno.c_str(), methoduid.c_str());
		}

		mod->fprintf(clsname, id,
			"	public %s %s(", tmp.c_str(), node->getname().c_str());
		if (node->m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Virtual)) {
			mod->fprintf(("I" + getname() + "_virtual").c_str(), 
				scrfile_module_source_virtual, "	public %s %s(", 
				tmp.c_str(), node->getname().c_str());
		}

		bool first = true;
		list_item *item = node->m_VariableList.getnext();
		for (; item != &(node->m_VariableList); item = item->getnext())
		{
			auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
			variable_full_name(mod, obj, tmp,
				obj->test_flags(varobj_flag_method_param_out), proxy, bobservable);

			const char *param_type = "RPCParamType.in";
			if(obj->test_flags(varobj_flag_method_param_out)){
				param_type = "RPCParamType.inout";
			}
			
			string content = "@RPC_param(name = \"%s\", type = %s) %s %s";
			if(has_observer_param(false, nullptr, obj)){
				content = "@RPC_param(name = \"%s\", type = %s) @RPC_observer %s %s";
			}
			if (first) {
				first = false;
				mod->fprintf(clsname, id,
					content.c_str(), 
					obj->getname().c_str(), param_type, 
					tmp.c_str(), obj->getname().c_str());
				if (node->m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Virtual)) {
					mod->fprintf(("I" + getname() + "_virtual").c_str(),
						scrfile_module_source_virtual,"%s %s",
						tmp.c_str(), obj->getname().c_str());
				}
			} else {
				mod->fprintf(clsname, id,
					(", "+content).c_str(), 
					obj->getname().c_str(), param_type, 
					tmp.c_str(), obj->getname().c_str());
				if (node->m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Virtual)) {
					mod->fprintf(("I" + getname() + "_virtual").c_str(), 
						scrfile_module_source_virtual, ", %s %s", 
						tmp.c_str(), obj->getname().c_str());
				}
			}
			
		}
		if(id == scrfile_module_observer_interface_source ) {
			mod->fputs(clsname, id, ");\n\n");
		} else {
			if (node->m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Virtual)) {
				mod->fputs(clsname, scrfile_module_source_virtual, ");\n\n");
			} 
			mod->fputs(clsname, id, ")\n	{\n");

			item = node->m_VariableList.getnext();
			for (; item != &(node->m_VariableList); item = item->getnext())
			{
				auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
				if(has_observer_param(false, nullptr, obj)) {
					variable_full_name(mod, obj, tmp,
						obj->test_flags(varobj_flag_method_param_out), proxy, bobservable);
					mod->fprintf(clsname,id,"		I%s %s = dynamicService.getProxyObject(%s);\n\n", tmp.c_str(), tmp.c_str(), obj->getname().c_str());
				}
			}

			if (bcomments)
				mod->fputs(clsname, id,
					"		// TODO: write your code for implementation\n");

			if (!generate_source_method_body_return_default(mod, node, tmp))
				mod->fprintf(clsname, id,
				"	%s", tmp.c_str());

			mod->fputs(clsname, id, "\n	}\n\n");
		}
	}
	return -1;
}

int interface_object_java_code_generator::generate_source_call_mehtod(
	srcfile_type id, module* cmod, TInterfaceNode* node, bool obersvable)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	if (nullptr == node) return 1;
	const char* clsname = getname().c_str();
	const char* modname = mod->getname().c_str();
	const char* methodname = node->getname().c_str();

	if (obersvable) {
		mod->fprintf(id,
			"static int ___%s_%s_obsvhdr(void* obj, pbf_wrapper inparam, pbf_wrapper inout_param)\n",
			clsname, methodname);
	} else {
		mod->fprintf(id,
			"static int ___%s_%s_handler(void* obj, pbf_wrapper inparam, pbf_wrapper inout_param)\n",
			clsname, methodname);
	}

	mod->fprintf(id,
		"{\n"
		"	assert(nullptr != obj);\n\n"
	
		"	auto* inp = zas_downcast(google::protobuf::Message,	\\\n"
		"		::%s_pbf::___%s_%s_inparam,	\\\n"
		"		inparam->get_pbfmessage());\n"
		"	auto* inoutp = zas_downcast(google::protobuf::Message,	\\\n"
		"		::%s_pbf::___%s_%s_inout_param,	\\\n"
		"		inout_param->get_pbfmessage());\n\n",
	modname, clsname, methodname,
	modname, clsname, methodname);

	std::string tmp;
	list_item *item = node->m_VariableList.getnext();
	for (; item != &(node->m_VariableList); item = item->getnext())
	{
		auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		generate_source_call_method_var(cmod, obj, tmp);
		mod->fprintf(id,
			"	%s\n", tmp.c_str());
	}

	generate_source_call_mehtod_end(mod, node, tmp);
	mod->fprintf(id,
		"	%s\n", tmp.c_str());
	mod->fputs(id, 
		"	retrurn 1;\n"
		"}\n\n");
	return -1;
}

int interface_object_java_code_generator::generate_source_call_mehtod_end(
	module* cmod, TInterfaceNode* node, string &ret)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	if (nullptr == node) return 1;
	const char* clsname = getname().c_str();
	const char* modname = mod->getname().c_str();
	const char* methodname = node->getname().c_str();

	ret.clear();
	switch (node->m_Data.method.m_eRetValType)
	{
	case eBasicObjType_Unknown:{
		ret += "((";
		ret += clsname;
		ret += "*)obj)->";
		ret += methodname;
		ret += "(";
	}break;
	case basic_objtype_boolean:
	case basic_objtype_uint8:
	case eBasicObjType_Int16:
	case eBasicObjType_Int32:
	case eBasicObjType_Int64:
	case eBasicObjType_UInt32:
	case eBasicObjType_Float:
	case eBasicObjType_String:{
		ret += "inoutp->set____retval(((";
		ret += clsname;
		ret += "*)obj)->";
		ret += methodname;
		ret += "(";
	}
	break;
	case eBasicObjType_Stream:
	case eBasicObjType_Enum:
		break;
	case eBasicObjType_Interface:
	case eBasicObjType_UserDefined: {
		string tmp;
		method_retval_variable_name(mod, node, tmp, false, false, true);
		ret += tmp;
		ret += " ___retval = ((";
		ret += clsname;
		ret += "*)obj)->";
		ret += methodname;
		ret += "(";
		} break;
	case eBasicObjType_Typedefed:
		// todo:
		break;
	}

	bool first = true;
	list_item *item = node->m_VariableList.getnext();
	for (; item != &(node->m_VariableList); item = item->getnext())
	{
		auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		if (first) {
			ret += obj->getname();
			first = false;
		} else {
			ret += ", ";
			ret += obj->getname();
		}
	}

	switch (node->m_Data.method.m_eRetValType)
	{
	case eBasicObjType_Unknown:
	case basic_objtype_boolean:
	case basic_objtype_uint8:
	case eBasicObjType_Int16:
	case eBasicObjType_Int32:
	case eBasicObjType_Int64:
	case eBasicObjType_UInt32:
	case eBasicObjType_Float:
	case eBasicObjType_String:{
		ret += ");\n";
	}
	break;
	case eBasicObjType_Stream:
	case eBasicObjType_Enum:
		break;
	case eBasicObjType_Interface:{
		ret += ");\n";
		ret += "	inoutp->set____retval(&___retval);\n";
	}break;
	case eBasicObjType_UserDefined: {
		ret += ");\n";
		ret += "	___retval.copyto(*(inoutp->mutable____retval()));\n";
		} break;
	case eBasicObjType_Typedefed:
		// todo:
		break;
	}

	item = node->m_VariableList.getnext();
	for (; item != &(node->m_VariableList); item = item->getnext())
	{
		auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		if (!obj->test_flags(varobj_flag_method_param_out))
			continue;
		string tmp;
		generate_source_call_method_end_var(mod, obj, tmp);
		ret += "	";
		ret += tmp;
		tmp += "\n";
	}
	return -1;
}

int interface_object_java_code_generator::method_has_in_out_param(
	TInterfaceNode *node, bool &has_in, bool &has_out)
{
	if (nullptr == node) return 1;

	if (node->m_Data.method.m_eRetValType != eBasicObjType_Unknown)
		has_out = true;
	list_item *item = node->m_VariableList.getnext();
	for (; item != &(node->m_VariableList); item = item->getnext())
	{
		auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		if (obj->test_flags(varobj_flag_method_param_out))
			has_out = true;
		if (obj->test_flags(varobj_flag_method_param_in))
			has_in = true;
		if (has_out && has_in)
			return 0;
	}
	return 0;
}

int interface_object_java_code_generator::generate_skeleton_source(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* clsname = getname().c_str();
	const char* modname = mod->getname().c_str();

	mod->fprintf(scrfile_module_skeleton_source,
		"void* %s_factory(void *destroy, pbf_wrapper param)\n"
		"{\n"
		"	if (destroy) {\n"
		"		%s::destroy_instance((%s*)destroy);\n"
		"		return nullptr;\n"
		"	}\n"
		"	return (void *)%s::create_instance();\n"
		"}\n",
		clsname,
		clsname, clsname,
		clsname);

	list_item* subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		// check if this is a method
		if (eIFNodeType_Method != node->GetType()) {
			continue;
		}
		generate_source_call_mehtod(scrfile_module_skeleton_source, mod, node, false);
	}

	mod->fprintf(scrfile_module_skeleton_source,
		"void import_%s(void)\n"
		"{\n"
		"	pbf_wrapper param;\n"
		"	rpchost host = rpcmgr::inst()->get_service(\"%s\", nullptr);\n"
		"	rpcinterface rif = host->register_interface(\"%s.%s\", (void*)%s_factory, param);\n"
		"	pbf_wrapper inparam, inout_param;\n",
		clsname,
		mod->getconfig().get_service_name().c_str(),
		modname, clsname, clsname);

	std::string mname;
	std::string methoduid;
	subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		// check if this is a method
		if (eIFNodeType_Method != node->GetType()) {
			continue;
		}
		mname = clsname + node->getname();
		generate_uid(mname.c_str(), methoduid);
		mod->fprintf(scrfile_module_skeleton_source,
			"	uint128_t u_%s = %s;\n\n",
			node->getname().c_str(), methoduid.c_str());

		mod->fprintf(scrfile_module_skeleton_source,
		"	inparam = new ::%s::struct_wrapper::___%s_%s_inparam_wrapper;\n"
		"	inout_param = new ::%s::struct_wrapper::___%s_%s_inout_param_wrapper;\n"
		"	rif->add_method(u_%s, inparam, inout_param, (void*)___%s_%s_handler);\n",
		modname, clsname, node->getname().c_str(),
		modname, clsname, node->getname().c_str(),
		node->getname().c_str(),
		clsname, node->getname().c_str());
	}

	mod->fputs(scrfile_module_skeleton_source, "}\n\n");
	return 0;
}

int
interface_object_java_code_generator::generate_observable_skeleton_source(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* clsname = getname().c_str();
	const char* modname = mod->getname().c_str();

	string observerInterfaceName = "I" + getname();
	mod->fputs(clsname, scrfile_module_observer_source,
		"import com.civip.rpc.client.service.RPCService;\n");
	mod->fprintf(observerInterfaceName.c_str(),
		scrfile_module_observer_interface_source,
		"import com.civip.rpc.%s.entity.*;\n\n",mod->getname().c_str());
	mod->fprintf(observerInterfaceName.c_str(), 
		scrfile_module_observer_interface_source,
		"public interface I%s {\n\n", clsname);
	mod->fputs(clsname,scrfile_module_observer_source,"import org.springframework.stereotype.Component;\n");
	mod->fprintf(clsname,scrfile_module_observer_source,
		"import com.civip.rpc.%s.entity.*;\n\n",mod->getname().c_str());
	mod->fputs(clsname,scrfile_module_observer_source,"@Component\n");
	mod->fprintf(clsname,scrfile_module_observer_source,"@RPC_service(pkg = \"%s\", name = \"%s\", className = \"%s.%s\")\n",
		mod->getconfig().get_package_name().c_str(),
		mod->getconfig().get_service_name().c_str(),
		mod->getname().c_str(),clsname);
	mod->fprintf(clsname,scrfile_module_observer_source,"public class %s extends RPCService implements I%s {\n\n", clsname, clsname);

	std::string tmp;
	list_item* subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		// check if this is a method
		if (eIFNodeType_Method != node->GetType()) {
			continue;
		}

		generate_source_method_emtpy(mod, node, scrfile_module_observer_source,
			false, false, true);
		generate_source_method_emtpy(mod, node, 
			scrfile_module_observer_interface_source, false, false, true);
	}
	mod->fputs(observerInterfaceName.c_str(),
		scrfile_module_observer_interface_source,"}");
	mod->fputs(clsname,scrfile_module_observer_source,"}");

	return 0;
}
