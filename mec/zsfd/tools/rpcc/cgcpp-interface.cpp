/** @file inc/cgcpp-interface.h
 * implementation of c++ code generation of interface for zrpc
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

interface_object_cpp_code_generator::interface_object_cpp_code_generator(
	module* mod, const string& name)
	: interface_object(mod, name)
{
}

interface_object_cpp_code_generator::~interface_object_cpp_code_generator()
{
}

bool interface_object_cpp_code_generator::generate_code(void)
{
	if (test_flags(grammar_object_flags_code_generated))
		return true;
	set_flags(grammar_object_flags_code_generated);

	if (!generate_related_object_code()) {
		return false;
	}
	if (generate_method_protobuf_structs()) {
		return false;
	}
	if (generate_header_code()) {
		return false;
	}
	if (generate_source_code()) {
		return false;
	}
	return true;
}

bool interface_object_cpp_code_generator::generate_proxy_code(void)
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

bool interface_object_cpp_code_generator::generate_skeleton_code(void)
{
	return true;
}

int interface_object_cpp_code_generator::
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
			mod->fprintf(srcfile_module_protobuf,
				"message ___%s_%s_inparam\n{\n", ifobj->getname().c_str(),
				node->getname().c_str());
			mod->fprintf(srcfile_module_proxy_internal_header,
				"class ___%s_%s_inparam_wrapper : public pbf_wrapper_object\n"
				"{\n"
				"public:\n"
				"	google::protobuf::Message* get_pbfmessage(void) {\n"
				"		return &_protobuf_object;\n"
				"	}\n\n"

				"private:\n"
				"	%s_pbf::___%s_%s_inparam _protobuf_object;\n"
				"};\n\n",
				ifobj->getname().c_str(), node->getname().c_str(),
				mod->getname().c_str(),ifobj->getname().c_str(),
				node->getname().c_str());		
		}
		variable_protobuf_type_name(mod, obj, tmp);
		mod->fprintf(srcfile_module_protobuf,
			"\t%s %s = %u;\n", tmp.c_str(),
			obj->getname().c_str(), ++first_inparam);
		if (obj->_basic_type == eBasicObjType_Interface &&
			obj->m_pIFType->TestInterfaceFlags(eInterfaceFlag_Observable)) {
			mod->fprintf(srcfile_module_protobuf,
				"\tstring client_name_%s = %u;\n",
				obj->getname().c_str(), ++first_inparam);
			mod->fprintf(srcfile_module_protobuf,
				"\tstring inst_name_%s = %u;\n",
				obj->getname().c_str(), ++first_inparam);
		}
	}
	if (first_inparam) {
		mod->fputs(srcfile_module_protobuf, "}\n\n");
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
			mod->fprintf(srcfile_module_protobuf,
				"message ___%s_%s_inout_param\n{\n", ifobj->getname().c_str(),
				node->getname().c_str());

			mod->fprintf(srcfile_module_proxy_internal_header,
				"class ___%s_%s_inout_param_wrapper : public pbf_wrapper_object\n"
				"{\n"
				"public:\n"
				"	google::protobuf::Message* get_pbfmessage(void) {\n"
				"		return &_protobuf_object;\n"
				"	}\n\n"

				"private:\n"
				"	%s_pbf::___%s_%s_inout_param _protobuf_object;\n"
				"};\n\n",
				ifobj->getname().c_str(), node->getname().c_str(),
				mod->getname().c_str(),ifobj->getname().c_str(),
				node->getname().c_str());	
		}
		variable_protobuf_type_name(mod, obj, tmp);
		mod->fprintf(srcfile_module_protobuf,
			"\t%s %s = %u;\n", tmp.c_str(),
			obj->getname().c_str(), ++first_inout_param);
		if (obj->_basic_type == eBasicObjType_Interface &&
			obj->m_pIFType->TestInterfaceFlags(eInterfaceFlag_Observable)) {
			mod->fprintf(srcfile_module_protobuf,
				"\tstring client_name_%s = %u;\n",
				obj->getname().c_str(), ++first_inparam);
			mod->fprintf(srcfile_module_protobuf,
				"\tstring inst_name_%s = %u;\n",
				obj->getname().c_str(), ++first_inparam);
		}
	}
	// if the method has a retval, add it to inout_param
	if (node->m_Data.method.m_eRetValType != eBasicObjType_Unknown) {
		if (!first_inout_param) {
			mod->fprintf(srcfile_module_protobuf,
				"message ___%s_%s_inout_param\n{\n", ifobj->getname().c_str(),
				node->getname().c_str());

			mod->fprintf(srcfile_module_proxy_internal_header,
				"class ___%s_%s_inout_param_wrapper : public pbf_wrapper_object\n"
				"{\n"
				"public:\n"
				"	google::protobuf::Message* get_pbfmessage(void) {\n"
				"		return &_protobuf_object;\n"
				"	}\n\n"

				"private:\n"
				"	%s_pbf::___%s_%s_inout_param _protobuf_object;\n"
				"};\n\n",
				ifobj->getname().c_str(), node->getname().c_str(),
				mod->getname().c_str(),ifobj->getname().c_str(),
				node->getname().c_str());	
		}
		method_retval_protobuf_type_name(mod, node, tmp);
		mod->fprintf(srcfile_module_protobuf,
			"\t%s ___retval = %u;\n", tmp.c_str(), ++first_inout_param);
	}
	if (first_inout_param) {
		mod->fputs(srcfile_module_protobuf, "}\n\n");
	}
	return 0;
}

int interface_object_cpp_code_generator::generate_method_protobuf_structs(void)
{
	module* mod = GetModule();
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
		if (node->
			m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Constructor)) {
			string tmp;
			int first_inparam = 0;
			list_item *item = node->m_VariableList.getnext();
			for (; item != &(node->m_VariableList); item = item->getnext())
			{
				auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);

				if (!first_inparam) {
					mod->fprintf(srcfile_module_protobuf,
						"message ___%s_%s_inparam\n{\n",
						node->m_pIFObject->getname().c_str(),
						node->m_pIFObject->getname().c_str());
					mod->fprintf(srcfile_module_proxy_internal_header,
						"class ___%s_%s_inparam_wrapper : public pbf_wrapper_object\n"
						"{\n"
						"public:\n"
						"	google::protobuf::Message* get_pbfmessage(void) {\n"
						"		return &_protobuf_object;\n"
						"	}\n\n"

						"private:\n"
						"	%s_pbf::___%s_%s_inparam _protobuf_object;\n"
						"};\n\n",
						node->m_pIFObject->getname().c_str(),
						node->m_pIFObject->getname().c_str(),
						mod->getname().c_str(),
						node->m_pIFObject->getname().c_str(),
						node->m_pIFObject->getname().c_str());		
				}
				variable_protobuf_type_name(mod, obj, tmp);
				mod->fprintf(srcfile_module_protobuf,
					"\t%s %s = %u;\n", tmp.c_str(),
					obj->getname().c_str(), ++first_inparam);
			}
			if (first_inparam) {
				mod->fputs(srcfile_module_protobuf, "}\n\n");
			}
		} else {
			if (generate_single_method_protobuf(node)) {
				return 1;
			}
		}
	}
	return 0;
}

int interface_object_cpp_code_generator::generate_header_code(void)
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

int interface_object_cpp_code_generator::generate_source_code(void)
{
	if (TestInterfaceFlags(eInterfaceFlag_Observable)){
		if (generate_observable_proxy_source())
			return false;
	} else {
		if (generate_proxy_source())
			return false;
	}
	
	if (TestInterfaceFlags(eInterfaceFlag_Observable)) {
		if (generate_observable_skeleton_source())
			return -3;
	} else {
		if (generate_skeleton_source())
			return -4;
	}

	if (!TestInterfaceFlags(eInterfaceFlag_Observable)) {
		if (generate_interface_source())
			return -5;
	} 
	return 0;
}

int interface_object_cpp_code_generator::generate_uid(const char* name,
	std::string &uid)
{
	uint128_t tuuid;
	utilities::md5Encode(tuuid, (void*)name, strlen(name));
	char buf[128];
	snprintf(buf, 127, "{0x%X, 0x%X, 0x%X, {0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X}}",
		tuuid.Data1, tuuid.Data2, tuuid.Data3, tuuid.Data4[0], tuuid.Data4[1],
		tuuid.Data4[2], tuuid.Data4[3], tuuid.Data4[4], tuuid.Data4[5],
		tuuid.Data4[6], tuuid.Data4[7]);
	uid = buf;
	return 0;
}

int interface_object_cpp_code_generator::method_retval_variable_name(
	module* cmod, TInterfaceNode* node, string& ret,
	bool proxy, bool observable, bool ref, bool declaration)
{
	TVariableObject varobj;

	varobj._basic_type = node->m_Data.method.m_eRetValType;
	varobj.m_pIFType = node->m_Data.method.m_pRetValIFObject;
	varobj.m_pRefType = node->m_Data.method.m_pRetValTypedefType;
	varobj.m_pEnumDefType = node->m_Data.method.m_pRetValEnumDef;
	varobj.m_pUserDefType = node->m_Data.method.m_pRetValUserDefType;

	return get_variable_full_name(cmod, &varobj, ret, proxy, observable,
		false, false, ref, declaration, !declaration);
}

int interface_object_cpp_code_generator::method_attribute_variable_name(
	module* cmod, TInterfaceNode* node, string& ret,
	bool proxy, bool observable, bool ref, bool declaration, bool param)
{
	TVariableObject varobj;

	varobj._basic_type = node->m_Data.attribute._basic_type;
	varobj.m_pIFType = node->m_Data.attribute.m_pIFObject;
	varobj.m_pRefType = node->m_Data.attribute.m_pTypedefType;
	varobj.m_pEnumDefType = node->m_Data.attribute.m_pEnumDef;
	varobj.m_pUserDefType = node->m_Data.attribute.m_pUserDefType;

	return get_variable_full_name(cmod, &varobj, ret, proxy, observable,
		param, false, ref, declaration, !(declaration && param));
}

int interface_object_cpp_code_generator::get_variable_return_default_value(
	module* cmod, TInterfaceNode* obj, string& ret, bool proxy, bool observable)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	ret.clear();

	const char* var_types[] = {
		nullptr,	// unknown
		nullptr,		// bool
		"uint32_array",	// uint8
		"int32_array",	// int16
		"int32_array",	// int32
		"int64_array",	// long
		"uint32_array",	// uint32
		"double_array",	// Float
		"bytebuff_array",	// stream
		"string_array",	// string
		nullptr,	// enum - special handling
		nullptr,	// interface - shall not have this
		nullptr,	// typedefed - special handling
		nullptr,	// userdefined - special handling
	};
	int retval = 0;
	TVariableObject var;
	var._basic_type = obj->m_Data.method.m_eRetValType;
	var.m_pIFType = obj->m_Data.method.m_pRetValIFObject;
	var.m_pRefType = obj->m_Data.method.m_pRetValTypedefType;
	var.m_pEnumDefType = obj->m_Data.method.m_pRetValEnumDef;
	var.m_pUserDefType = obj->m_Data.method.m_pRetValUserDefType;

	if (is_array(var.m_ArrayType)) {
		switch (var._basic_type)
		{
		case basic_objtype_boolean:
			ret += "return new ";
			ret += var_types[var._basic_type];
			ret += ";";
			break;
		case basic_objtype_uint8:
		case eBasicObjType_Int16:
		case eBasicObjType_Int32:
		case eBasicObjType_Int64:
		case eBasicObjType_UInt32:
		case eBasicObjType_Float:
		case eBasicObjType_String:
		case eBasicObjType_Stream:
			ret += var_types[var._basic_type];
			ret += " retval;\n";
			ret += "	return retval;";
			break;
		case eBasicObjType_Enum: {
			//TBD.
			// auto* enumdeft = var.m_pEnumDefType;
			// auto* mod = enumdeft->GetModule();
			// assert(nullptr != mod);
			// ret += "return new";
			// if (mod != cmod) {
			// 	ret += "::";
			// 	ret += mod->getname();
			// 	ret += "::";
			// } else if (proxy || (!proxy && observable)) {
			// 	ret += "::";
			// 	ret += cmod->getname();
			// 	ret += "::";
			// }
			// ret += enumdeft->getname();
			// ret += ";\n";
		}break;
			break;
		case eBasicObjType_Interface: {
			//TBD.
			// auto* interf = var.m_pIFType;
			// auto* mod = interf->GetModule();
			// assert(nullptr != mod);
			// if (mod != cmod) {
			// 	ret += "::";
			// 	ret += mod->getname();
			// 	ret += "::";
			// }
			// ret += interf->getname();
			// ret += " retval;\n";
			// ret += "	return retval;";
			} break;
		case eBasicObjType_Typedefed:
			retval = 1;
			break;

		case eBasicObjType_UserDefined: {
			auto* usrdef = var.m_pUserDefType;
			auto* mod = usrdef->GetModule();
			assert(nullptr != mod);
			ret += "auto* retval = new ";
			if (proxy || mod != cmod || observable) {
				ret += "::";
				ret += mod->getname();
				ret += "::";
			}
			ret += "array::";
			ret += usrdef->getname();
			ret += "();\n";
			ret += "	return *retval;";
		}	break;
		default:
			retval = 2;
		}

	} else {

		switch (var._basic_type)
		{
		case basic_objtype_boolean:
			ret += "return false;";
			break;
		case basic_objtype_uint8:
		case eBasicObjType_Int16:
		case eBasicObjType_Int32:
		case eBasicObjType_Int64:
		case eBasicObjType_UInt32:
			ret += "return 0;";
			break;
		case eBasicObjType_Float:
			ret += "return 0.0;";
			break;
		case eBasicObjType_String:
			ret += "return "";";
			break;
		case eBasicObjType_Enum: {
			auto* enumdeft = var.m_pEnumDefType;
			auto* mod = enumdeft->GetModule();
			assert(nullptr != mod);
			ret += "return ";
			if (mod == cmod && (proxy || (!proxy && observable))) {
				ret += "::";
				ret += cmod->getname();
				ret += "::";
			}
			ret += var.m_pEnumDefType->GetDefault(cmod);
			ret += ";\n";
		}break;
		case eBasicObjType_Stream:
			ret += "auto* retval = new bytebuff;\n";
			ret += "\treturn *retval;";
			break;
		case eBasicObjType_Interface: {
			std::string tmp;
			auto* interf = var.m_pIFType;
			auto* mod = interf->GetModule();
			ret += "auto* retval = new ";
			assert(nullptr != mod);
			if (mod != cmod) {
				ret += "::";
				ret += mod->getname();
				ret += "::";
				if (proxy){
					ret += "proxy::";
				} else {
					if (interf->TestInterfaceFlags(eInterfaceFlag_Observable))
						ret += "skeleton::";
				}
			} else {
				if (!proxy) {
					if (!interf->TestInterfaceFlags(eInterfaceFlag_Observable)) {
						if (observable) {
							ret += "::";
							ret += mod->getname();
							ret += "::";
						}
					} else {
						if (!observable) {
							ret += "skeleton::";
						}
					}
				}
			}
			ret += interf->getname();
			ret += "();\n";
			ret += "	return *retval;";
			} break;
		case eBasicObjType_Typedefed:
			retval = 1;
			break;

		case eBasicObjType_UserDefined: {
			auto* usrdef = var.m_pUserDefType;
			auto* mod = usrdef->GetModule();
			assert(nullptr != mod);
			ret += "auto* retval = new ";
			if (proxy || mod != cmod || observable) {
				ret += "::";
				ret += mod->getname();
				ret += "::";
			}
			ret += usrdef->getname();
			ret += "();\n";
			ret += "	return *retval;";
		}	break;
		default:
			retval = 2;
		}
	}
	return retval;
}

int interface_object_cpp_code_generator::generate_attribute_body_default_ret(
	module* cmod, TInterfaceNode* obj, string& ret)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	ret.clear();

	const char* var_types[] = {
		nullptr,	// unknown
		nullptr,		// bool
		"uint32_array",	// uint8
		"int32_array",	// int16
		"int32_array",	// int32
		"int64_array",	// long
		"uint32_array",	// uint32
		"double_array",	// Float
		"bytebuff_array",	// stream
		"string_array",	// string
		nullptr,	// enum - special handling
		nullptr,	// interface - shall not have this
		nullptr,	// typedefed - special handling
		nullptr,	// userdefined - special handling
	};
	int retval = 0;
	TVariableObject var;
	var._basic_type = obj->m_Data.attribute._basic_type;
	var.m_pIFType = obj->m_Data.attribute.m_pIFObject;
	var.m_pRefType = obj->m_Data.attribute.m_pTypedefType;
	var.m_pEnumDefType = obj->m_Data.attribute.m_pEnumDef;
	var.m_pUserDefType = obj->m_Data.attribute.m_pUserDefType;

	if (is_array(var.m_ArrayType)) {
		switch (var._basic_type)
		{
		case basic_objtype_boolean:
		case basic_objtype_uint8:
		case eBasicObjType_Int16:
		case eBasicObjType_Int32:
		case eBasicObjType_Int64:
		case eBasicObjType_UInt32:
		case eBasicObjType_Float:
		case eBasicObjType_String:
		case eBasicObjType_Stream:
			ret += "return _";
			ret += obj->getname();
			ret += ";";
			break;
		case eBasicObjType_Enum: {
			//TBD.

		}break;
			break;
		case eBasicObjType_Interface: {
			//TBD.
			} break;
		case eBasicObjType_Typedefed:
			retval = 1;
			break;

		case eBasicObjType_UserDefined: {
			ret += "return _";
			ret += obj->getname();
			ret += ";";
		}	break;
		default:
			retval = 2;
		}

	} else {

		switch (var._basic_type)
		{
		case basic_objtype_boolean:
		case basic_objtype_uint8:
		case eBasicObjType_Int16:
		case eBasicObjType_Int32:
		case eBasicObjType_Int64:
		case eBasicObjType_UInt32:
		case eBasicObjType_Float:
			ret += "return _";
			ret += obj->getname();
			ret += ";";
			break;
		case eBasicObjType_String:
			ret += "return &_";
			ret += obj->getname();
			ret += ";";
			break;
		case eBasicObjType_Enum: {
			ret += "return _";
			ret += obj->getname();
			ret += ";";
		}break;
		case eBasicObjType_Stream:
			ret += "return _";
			ret += obj->getname();
			ret += ";";
			break;
		case eBasicObjType_Interface: {
			ret += "return _";
			ret += obj->getname();
			ret += ";";
			} break;
		case eBasicObjType_Typedefed:
			retval = 1;
			break;

		case eBasicObjType_UserDefined: {
			ret += "return _";
			ret += obj->getname();
			ret += ";";
		}	break;
		default:
			retval = 2;
		}
	}
	return retval;
}

int interface_object_cpp_code_generator::get_variable_full_name(
	module* cmod, TVariableObject* var, string& ret,
		bool proxy, bool observable, bool param, bool out_type, 
		bool ref, bool declaration, bool breturn)
{
	const char* var_types[] = {
		"void",	// unknown
		"bool",		// bool
		"uint8_t",	// uint8
		"int16_t",	// int16
		"int32_t",	// int32
		"int64_t",	// long
		"uint32_t",	// uint32
		"double",	// Float
		"bytebuff",	// stream
		"std::string",	// string
		nullptr,	// enum - special handling
		nullptr,	// interface - shall not have this
		nullptr,	// typedefed - special handling
		nullptr,	// userdefined - special handling
	};

	const char* var_array_types[] = {
		nullptr,	// unknown
		"boolean_array",		// bool
		"uint32_array",	// uint8
		"int32_array",	// int16
		"int32_array",	// int32
		"int64_array",	// long
		"uint32_array",	// uint32
		"double_array",	// Float
		"bytebuff_array",	// stream
		"string_array",	// string
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
		type = var_array_types[var->_basic_type];
		if (type) {
			ret += type;
			if (breturn || out_type || ref || param)
				ret += "&";
			return 0;
		}

		assert(eBasicObjType_Unknown != var->_basic_type);
		switch (var->_basic_type)
		{
		case eBasicObjType_Enum: {
			auto* enumtype = var->m_pEnumDefType;
			auto* mod = enumtype->GetModule();
			assert(nullptr != mod);
			if (mod != cmod ||
				proxy || observable) {
				ret += "::";
				ret += mod->getname();
				ret += "::";
			}
			ret += enumtype->getname();
			ret += "_array";
			if (breturn || out_type || ref || param)
					ret += "&";
			}break;
		case eBasicObjType_Interface: {
			auto* iftype = var->m_pIFType;
			auto* mod = iftype->GetModule();
			assert(nullptr != mod);
			if (mod != cmod ||
				proxy || observable) {
				ret += "::";
				ret += mod->getname();
				ret += "::";
			}
			ret += iftype->getname();
			ret += "_array";
			if (breturn || out_type || ref || param)
					ret += "&";

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
				ret += "::";
				ret += mod->getname();
				ret += "::";
			}
			if (is_array(var->m_ArrayType))
				ret += "array::";
			ret += usrdef->getname();
			if (breturn || out_type || ref || param)
				ret += "&";
		}	break;
		}

	} else {
		type = var_types[var->_basic_type];
		if (type) {
			ret += type;
			if (eBasicObjType_String == var->_basic_type
			|| out_type || ref || (breturn && ref))
				ret += "&";
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
				ret += "::";
				ret += mod->getname();
				ret += "::";
			} else if (proxy || (!proxy && observable)) {
				ret += "::";
				ret += cmod->getname();
				ret += "::";
			}
			ret += enumdeft->getname();
			if ((!breturn && (out_type || ref)))
				ret += "&";
		}break;
		case eBasicObjType_Interface: {
			auto* interf = var->m_pIFType;
			auto* mod = interf->GetModule();
			assert(nullptr != mod);
			if (mod != cmod) {
				ret += "::";
				ret += mod->getname();
				ret += "::";
				if (proxy) {
					ret += "proxy::";
				} else {
					if (interf->TestInterfaceFlags(eInterfaceFlag_Observable))
						ret += "skeleton::";
				}
			} else {
				if (!proxy) {
					if (!interf->TestInterfaceFlags(eInterfaceFlag_Observable)) {
						if (observable) {
							ret += "::";
							ret += mod->getname();
							ret += "::";
						}
					} else {
						if (!observable) {
							ret += "skeleton::";
						}
					}
				}
	
			}
			ret += interf->getname();
			if (breturn || out_type || ref || param)
				ret += "&";
			} break;
		case eBasicObjType_Typedefed:{
			auto* reft = var->m_pRefType;
			ret += reft->get_fullname(cmod);
			if (breturn || out_type || ref || param)
				ret += "&";
		}break;

		case eBasicObjType_UserDefined: {
			auto* usrdef = var->m_pUserDefType;
			auto* mod = usrdef->GetModule();
			assert(nullptr != mod);
			if (proxy || mod != cmod || observable) {
				ret += "::";
				ret += mod->getname();
				ret += "::";
			}
			ret += usrdef->getname();
			if (breturn || out_type || ref || param)
				ret += "&";
		}	break;
		}
	}
	return 0;
}

int interface_object_cpp_code_generator::generate_proxy_header(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* clsname = getname().c_str();

	mod->fprintf(srcfile_module_proxy_header,
		"namespace proxy {\n"
		"class ZIDL_PREFIX %s\n"
		"{\n",
		clsname);

	mod->fprintf(srcfile_module_proxy_header,
		"public:\n"
		"	%s()\n"
		"	: _proxy_base(nullptr) {\n"
		"	}\n\n"

		"	%s(const %s& obj)\n"
		"	: _proxy_base(nullptr) {\n"
		"		if (nullptr == obj._proxy_base) {\n"
		"			return;\n"
		"		}\n"
		"		_proxy_base = obj._proxy_base;\n"
		"		_proxy_base->addref();\n"
		"	}\n\n"

		"	%s(void* instid)\n"
		"	: _proxy_base(rpcproxy::from_instid(instid,\n"
		"		&%s_static_data_)) {\n"
		"		assert(nullptr != _proxy_base);\n"
		"		_proxy_base->addref();\n"
		"	}\n\n"

		"	~%s() {\n"
		"		if (nullptr != _proxy_base) {\n"
		"			_proxy_base->release();\n"
		"			_proxy_base = nullptr;\n"
		"		}\n"
		"	}\n\n"

		"	const %s& operator=(const %s& obj) {\n"
		"		if (this == &obj) {\n"
		"			return *this;\n"
		"		}\n"
		"		if (_proxy_base) _proxy_base->release();\n"
		"		if (obj._proxy_base) obj._proxy_base->addref();\n"
		"		_proxy_base = obj._proxy_base;\n"
		"		return *this;\n"
		"	}\n\n"

		"	void* get_instid(void) {\n"
		"		if (_proxy_base) {\n"
		"			return _proxy_base->get_instid();\n"
		"		}\n"
		"		_proxy_base = rpcproxy::get_instance(nullptr, &%s_static_data_);\n"
		"		return (_proxy_base)\n"
		"			? _proxy_base->get_instid()\n"
		"			: nullptr;\n"
		"	}\n\n"
		"public:\n",
		clsname,
		clsname, clsname,
		clsname,
		clsname,
		clsname,
		clsname, clsname,
		clsname);

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
		if (eIFNodeType_Attribute == node->GetType()) {
			generate_attribute_header(node, true);
			continue;
		} else if (eIFNodeType_Method != node->GetType()) {
			continue;
		}
		if (generate_proxy_header_method(node)) {
			return 1;
		}
	}

	mod->fprintf(srcfile_module_proxy_header,
		"\nprivate:\n"
		"	rpcproxy*	_proxy_base;\n"
		"	static const char* %s_clsname_;\n"
		"	static rpcproxy_static_data %s_static_data_;\n",
		clsname,
		clsname);

	mod->fprintf(srcfile_module_proxy_header,
		"};} // end of namespace proxy\n\n");

	return 0;
}

int interface_object_cpp_code_generator::generate_proxy_observable_header(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* clsname = getname().c_str();

	mod->fprintf(srcfile_module_proxy_header,
		"namespace proxy {\n\n"
		"class ZIDL_PREFIX %s\n"
		"{\n"
		"public:\n"
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
		method_retval_variable_name(mod, node, tmp, true, true);
		mod->fprintf(srcfile_module_proxy_header,
			"	virtual %s %s(", tmp.c_str(), node->getname().c_str());
		bool first = true;
		list_item *item = node->m_VariableList.getnext();
		for (; item != &(node->m_VariableList); item = item->getnext())
		{
			auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
			get_variable_full_name(mod, obj, tmp, true, true, true,
				obj->test_flags(varobj_flag_method_param_out));
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

int interface_object_cpp_code_generator::generate_proxy_header_method(
	TInterfaceNode* node)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;

	string tmp;

	if (node->m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Constructor)) {
		mod->fprintf(srcfile_module_proxy_header,
			"\t%s(", node->m_pIFObject->getname().c_str());
	}else {
		method_retval_variable_name(mod, node, tmp, true);
		if (node->m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Virtual)) {
			mod->fprintf(srcfile_module_proxy_header,
				"	virtual %s %s(", tmp.c_str(), node->getname().c_str());
		} else {
			mod->fprintf(srcfile_module_proxy_header,
				"	%s %s(", tmp.c_str(), node->getname().c_str());
		}
	}

	bool first = true;
	list_item *item = node->m_VariableList.getnext();
	for (; item != &(node->m_VariableList); item = item->getnext())
	{
		auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		get_variable_full_name(mod, obj, tmp, true, false, true,
			obj->test_flags(varobj_flag_method_param_out));
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

int interface_object_cpp_code_generator::generate_attribute_header(
	TInterfaceNode* node, bool proxy)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;

	string tmp;
	method_attribute_variable_name(mod, node, tmp, proxy);
	mod->fprintf(srcfile_module_proxy_header,
		"	%s get_%s();\n", tmp.c_str(), node->getname().c_str());
	method_attribute_variable_name(mod, node, tmp, proxy, false, false, false,true);
	mod->fprintf(srcfile_module_proxy_header,
		"	void set_%s(%s %s);\n", node->getname().c_str(), tmp.c_str(), node->getname().c_str());

	//genetate proto & struct
	if (proxy){
		auto* ifobj = node->m_pIFObject;
		TVariableObject varobj;

		varobj._basic_type = node->m_Data.attribute._basic_type;
		varobj.m_pIFType = node->m_Data.attribute.m_pIFObject;
		varobj.m_pRefType = node->m_Data.attribute.m_pTypedefType;
		varobj.m_pEnumDefType = node->m_Data.attribute.m_pEnumDef;
		varobj.m_pUserDefType = node->m_Data.attribute.m_pUserDefType;
		variable_protobuf_type_name(mod, &varobj, tmp);



		mod->fprintf(srcfile_module_protobuf,
			"message ___%s_set_%s_inparam\n{\n",
			ifobj->getname().c_str(),
			node->getname().c_str());

		mod->fprintf(srcfile_module_protobuf,
			"\t%s %s = 1;\n", tmp.c_str(), node->getname().c_str());
		
		mod->fputs(srcfile_module_protobuf, "}\n");

		mod->fprintf(srcfile_module_proxy_internal_header,
			"class ___%s_set_%s_inparam_wrapper : public pbf_wrapper_object\n"
			"{\n"
			"public:\n"
			"	google::protobuf::Message* get_pbfmessage(void) {\n"
			"		return &_protobuf_object;\n"
			"	}\n\n"

			"private:\n"
			"	%s_pbf::___%s_set_%s_inparam _protobuf_object;\n"
			"};\n\n",
			ifobj->getname().c_str(), node->getname().c_str(),
			mod->getname().c_str(),ifobj->getname().c_str(),
			node->getname().c_str());	


		mod->fprintf(srcfile_module_protobuf,
			"message ___%s_get_%s_inout_param\n{\n", 
			ifobj->getname().c_str(),
			node->getname().c_str());

		mod->fprintf(srcfile_module_protobuf,
			"\t%s ___retval = 1;\n", tmp.c_str());
		
		mod->fputs(srcfile_module_protobuf, "}\n");

		mod->fprintf(srcfile_module_proxy_internal_header,
			"class ___%s_get_%s_inout_param_wrapper : public pbf_wrapper_object\n"
			"{\n"
			"public:\n"
			"	google::protobuf::Message* get_pbfmessage(void) {\n"
			"		return &_protobuf_object;\n"
			"	}\n\n"

			"private:\n"
			"	%s_pbf::___%s_get_%s_inout_param _protobuf_object;\n"
			"};\n\n",
			ifobj->getname().c_str(), node->getname().c_str(),
			mod->getname().c_str(),ifobj->getname().c_str(),
			node->getname().c_str());	
	}


	return 0;
}

int interface_object_cpp_code_generator::generate_attribute_body(
	TInterfaceNode* node, bool proxy)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;

	string tmp;
	auto* ifobj = node->m_pIFObject;
	string methodname = mod->getname();
	methodname += ifobj->getname();
	methodname += "get_";
	methodname += node->getname();
	string getid;
	generate_uid(methodname.c_str(), getid);
	methodname = mod->getname() + ifobj->getname() + "set_" + node->getname();
	string setid;
	generate_uid(methodname.c_str(), setid);
	if (proxy) {
		TVariableObject varobj;

		varobj._basic_type = node->m_Data.attribute._basic_type;
		varobj.m_pIFType = node->m_Data.attribute.m_pIFObject;
		varobj.m_pRefType = node->m_Data.attribute.m_pTypedefType;
		varobj.m_pEnumDefType = node->m_Data.attribute.m_pEnumDef;
		varobj.m_pUserDefType = node->m_Data.attribute.m_pUserDefType;
		
		method_attribute_variable_name(mod, node, tmp, proxy);
		mod->fprintf(scrfile_module_proxy_source,		
			"%s %s::get_%s(void)\n"
			"{\n"
			"	::%s_pbf::___%s_get_%s_inout_param inout_param;\n"
			"	uint128_t uid = %s;\n\n"

			"	auto* ret = rpcproxy::invoke(get_instid(), uid,\n"
			"		nullptr, &inout_param, &%s_static_data_, 1);\n"
			"	if (nullptr != ret) _proxy_base = ret;\n",
			tmp.c_str(), ifobj->getname().c_str(), node->getname().c_str(),
			mod->getname().c_str(), ifobj->getname().c_str(),
			node->getname().c_str(), getid.c_str(),
			ifobj->getname().c_str());
		generate_attribute_body_ret_param(mod, node, tmp);
		mod->fprintf(scrfile_module_proxy_source,		
			"\t%s\n}\n\n", tmp.c_str());

		method_attribute_variable_name(mod, node, tmp, proxy, false, false, false, true);
		mod->fprintf(scrfile_module_proxy_source,		
			"void %s::set_%s(%s %s)\n"
			"{\n"
			"	::%s_pbf::___%s_set_%s_inparam inparam;\n"
			"	uint128_t uid = %s;\n\n"

			"	auto* ret = rpcproxy::invoke(get_instid(), uid,\n"
			"		&inparam, nullptr, &%s_static_data_, 1);\n"
			"	if (nullptr != ret) _proxy_base = ret;\n"
			"}\n\n",
			ifobj->getname().c_str(), node->getname().c_str(), tmp.c_str(), 
			node->getname().c_str(),
			mod->getname().c_str(), ifobj->getname().c_str(),
			node->getname().c_str(), setid.c_str(),
			ifobj->getname().c_str());
	} else {
		mod->fprintf(scrfile_module_skeleton_source,
			"static int ___%s_get_%s_handler(void* obj, pbf_wrapper inparam, pbf_wrapper inout_param)\n"
			"{\n"
			"	assert(nullptr != obj);\n\n"

			"	auto* inoutp = zas_downcast(google::protobuf::Message,	\\\n"
			"		::%s_pbf::___%s_get_%s_inout_param,	\\\n"
			"		inout_param->get_pbfmessage());\n",
			ifobj->getname().c_str(), node->getname().c_str(),
			mod->getname().c_str(), ifobj->getname().c_str(),
			node->getname().c_str());
		generate_attribute_body_end(mod, node, tmp, true);
		mod->fprintf(scrfile_module_skeleton_source, "\t%s\n", tmp.c_str());
		mod->fputs(scrfile_module_skeleton_source, "\t return 1;\n}\n\n");

		mod->fprintf(scrfile_module_skeleton_source,
			"static int ___%s_set_%s_handler(void* obj, pbf_wrapper inparam, pbf_wrapper inout_param)\n"
			"{\n"
			"	assert(nullptr != obj);\n\n"

			"	auto* inp = zas_downcast(google::protobuf::Message,	\\\n"
			"		::%s_pbf::___%s_set_%s_inparam,	\\\n"
			"		inparam->get_pbfmessage());\n",
			ifobj->getname().c_str(), node->getname().c_str(),
			mod->getname().c_str(), ifobj->getname().c_str(),
			node->getname().c_str());
		generate_attribute_body_end(mod, node, tmp, false);
		mod->fprintf(scrfile_module_skeleton_source, "\t%s\n", tmp.c_str());
		mod->fputs(scrfile_module_skeleton_source, "\t return 1;\n}\n\n");
	}

	return 0;
}

int interface_object_cpp_code_generator::generate_skeleton_header_method(
	TInterfaceNode* node)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;

	string tmp;
	if (node->m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Constructor)) return 0;


	method_retval_variable_name(mod, node, tmp, false, false);
	if (node->m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Virtual)) {
		mod->fprintf(srcfile_module_proxy_header,
			"	virtual %s %s(", tmp.c_str(), node->getname().c_str());
	} else {
		mod->fprintf(srcfile_module_proxy_header,
			"	%s %s(", tmp.c_str(), node->getname().c_str());
	}

	bool first = true;
	list_item *item = node->m_VariableList.getnext();
	for (; item != &(node->m_VariableList); item = item->getnext())
	{
		auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		get_variable_full_name(mod, obj, tmp, false, false, true,
			obj->test_flags(varobj_flag_method_param_out));
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

int interface_object_cpp_code_generator::generate_method_header_param(
	TInterfaceNode* node, string &ret)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;

	string tmp;
	bool first = true;
	ret.clear();
	list_item *item = node->m_VariableList.getnext();
	for (; item != &(node->m_VariableList); item = item->getnext())
	{
		auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		get_variable_full_name(mod, obj, tmp, false, false, true,
			obj->test_flags(varobj_flag_method_param_out));
		if (first) {
			first = false;
			ret += tmp + " ";
			ret += obj->getname();
		}
		else{
			ret += ", ";
			ret += tmp + " ";
			ret += obj->getname();
		}
	}

	return 0;
}

int interface_object_cpp_code_generator::generate_skeleton_header(void)
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
		"class ZIDL_PREFIX %s : public skeleton_base\n"
		"{\n"
		"public:\n"
		"	%s();\n"
		"	virtual ~%s();\n\n"

		"	%s(const %s&) = delete;\n"
		"	void operator=(const %s&) = delete;\n\n"

		"	static %s* create_instance(void);\n"
		"	static void destroy_instance(%s*);\n\n",
		clsname,
		clsname,
		clsname,
		clsname, clsname,
		clsname,
		clsname,
		clsname);
	string tmp;
	list_item* subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		if (eIFNodeType_Method != node->GetType()) {
			continue;
		}
		if (!node->m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Constructor)) {
			continue;
		}
		mod->fprintf(srcfile_module_proxy_header,
			"	%s(", clsname);
		generate_method_header_param(node, tmp);
		mod->fprintf(srcfile_module_proxy_header,
			"%s);\n", tmp.c_str());
		mod->fprintf(srcfile_module_proxy_header,
			"	static %s* create_instance(", clsname);
		generate_method_header_param(node, tmp);
		mod->fprintf(srcfile_module_proxy_header,
			"%s);\n\n", tmp.c_str());
	}
	mod->fputs(srcfile_module_proxy_header, "public:\n");
	subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		if (eIFNodeType_Attribute == node->GetType()) {
			generate_attribute_header(node, false);
			continue;
		} else if (eIFNodeType_Method != node->GetType()) {
			continue;
		}
		if (generate_skeleton_header_method(node)) {
			return 1;
		}
	}
	
	bool bfirst = true;
	subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		if (eIFNodeType_Attribute == node->GetType()) {
			if (bfirst) {
				mod->fprintf(srcfile_module_proxy_header,
					"\n private:\n");
			}
			method_attribute_variable_name(mod, node, tmp, 
				false, false, false,true);
			mod->fprintf(srcfile_module_proxy_header,
				"	%s _%s;\n", tmp.c_str(), node->getname().c_str());
		} 
	}
	mod->fprintf(srcfile_module_proxy_header,
		"};\n\n");

	return 0;
}

int interface_object_cpp_code_generator::generate_skeleton_observable_header(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* clsname = getname().c_str();

	mod->fprintf(srcfile_module_proxy_header,
		"namespace skeleton {\n"
		"class ZIDL_PREFIX %s\n"
		"{\n"
		"public:\n"
		"	%s();\n"
		"	%s(void* inst_id, const char* cliname, const char* inst_name);\n"
		"	%s(const %s&);\n"
		"	const %s& operator=(const %s&);\n"
		"	~%s();\n\n"

		"public:\n",
		clsname,
		clsname,
		clsname,
		clsname, clsname,
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
		string tmp;

		method_retval_variable_name(mod, node, tmp, false, true);
		mod->fprintf(srcfile_module_proxy_header,
			"	virtual %s %s(", tmp.c_str(), node->getname().c_str());
		bool first = true;
		list_item *item = node->m_VariableList.getnext();
		for (; item != &(node->m_VariableList); item = item->getnext())
		{
			auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
			get_variable_full_name(mod, obj, tmp, false, true, true,
				obj->test_flags(varobj_flag_method_param_out));
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

int interface_object_cpp_code_generator::generate_proxy_source(void)
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

		if (eIFNodeType_Attribute == node->GetType()) {
			generate_attribute_body(node, true);
			continue;
		} else if (eIFNodeType_Method != node->GetType()) {
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
		if (node->
			m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Constructor)) {
			mod->fprintf(scrfile_module_proxy_source,
				"	_proxy_base = nullptr;	\n"
				"	_proxy_base = rpcproxy::get_instance(&inparam, &%s_static_data_);\n",
				clsname);
		} else {
			bool has_in, has_out;
			method_has_in_out_param(node, has_in, has_out);
			mod->fputs(scrfile_module_proxy_source,
				"\n	auto* ret = rpcproxy::invoke(get_instid(), uid,\n");
			if (has_in && has_out)
				mod->fputs(scrfile_module_proxy_source,"\t\t&inparam, &inout_param, ");
			else if(has_in)
				mod->fputs(scrfile_module_proxy_source,"\t\t&inparam, nullptr, ");
			else if (has_out)
				mod->fputs(scrfile_module_proxy_source,"\t\tnullptr, &inout_param, ");
			else
				mod->fputs(scrfile_module_proxy_source,"\t\tnullptr, nullptr, ");
			mod->fprintf(scrfile_module_proxy_source,
				"&%s_static_data_, 1);\n"
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
		}

		mod->fputs(scrfile_module_proxy_source, "}\n\n");
	}

	return 0;
}

int interface_object_cpp_code_generator::generate_interface_source(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* clsname = getname().c_str();
	string full_clsname = mod->getname() + ".";
	full_clsname += clsname;

	mod->fprintf(scrfile_module_source,
		"%s::%s()\n"
		":skeleton_base(\"%s\", \"%s\")\n"
		"{\n"
		"	// TODO: implement your constructor\n"
		"}\n\n"

		"%s::~%s()\n"
		"{\n"
		"	// TODO: implement your destructor\n"
		"}\n\n"

		"%s* %s::create_instance(void)\n"
		"{\n"
		"	// TODO: Check if this meets your need\n"
		"	// Change to any possible derived class if necessary\n"
		"	return new %s();\n"
		"}\n\n"

		"void %s::destroy_instance(%s* obj)\n"
		"{\n"
		"	delete obj;\n"
		"}\n\n",
		clsname, clsname, 
		mod->getconfig().get_service_name().c_str(), full_clsname.c_str(), 
		clsname, clsname, 
		clsname, clsname, 
		clsname, 
		clsname, clsname);

	std::string tmp;
	list_item* subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		if (eIFNodeType_Attribute == node->GetType()) {
			method_attribute_variable_name(mod, node, tmp, false);
			mod->fprintf(scrfile_module_source,
				"%s %s::get_%s()\n{\n", tmp.c_str(),
				clsname,
				node->getname().c_str());
			generate_attribute_body_default_ret(mod, node, tmp);
			mod->fprintf(scrfile_module_source,
				"\t%s\n}\n", tmp.c_str());

			method_attribute_variable_name(mod, node, tmp, false,
				false, false, false,true);
			mod->fprintf(scrfile_module_source,
				"void %s::set_%s(%s %s)\n{\n\t_%s = %s;\n}\n\n",
				clsname,
				node->getname().c_str(), tmp.c_str(), 
				node->getname().c_str(),
				node->getname().c_str(),node->getname().c_str());
				continue;
		} else if (eIFNodeType_Method != node->GetType()) {
			continue;
		}
		if (node->m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Constructor)) {
			string tmp;
			generate_method_header_param(node, tmp);
			mod->fprintf(scrfile_module_source,
				"%s* %s::create_instance(%s)\n"
				"{\n"
				"	// TODO: Check if this meets your need\n"
				"	// Change to any possible derived class if necessary\n"
				"	return new %s(", clsname, clsname, tmp.c_str(), clsname);
			bool first = true;
			list_item *item = node->m_VariableList.getnext();
			for (; item != &(node->m_VariableList); item = item->getnext())
			{
				auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
				if (first) {
					first = false;
					mod->fprintf(scrfile_module_source,
						"%s", obj->getname().c_str());
				}
				else
					mod->fprintf(scrfile_module_source,
						", %s", obj->getname().c_str());
			}
			mod->fputs(scrfile_module_source, ");\n}\n\n");
			mod->fprintf(scrfile_module_source,
				"%s::%s(", clsname, clsname);
			generate_method_header_param(node, tmp);
			mod->fprintf(scrfile_module_source,
				"%s)\n"
				":skeleton_base(\"%s\", \"%s\")\n"
				"{\n\t// TODO: implement your constructor\n\n}\n\n",
				tmp.c_str(),
				mod->getconfig().get_service_name().c_str(),
				full_clsname.c_str());
		}else {
			generate_source_method_emtpy(mod, node, scrfile_module_source,
				false, false, true);
		}
	}

	return 0;
}

int interface_object_cpp_code_generator::generate_observable_proxy_source(void)
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
		bool has_in, has_out;
		method_has_in_out_param(node, has_in, has_out);
		mod->fprintf(scrfile_module_proxy_source,
			"		uint128_t u_%s = %s;\n\n",
			node->getname().c_str(), methoduid.c_str());
		if (has_in) {
			mod->fprintf(scrfile_module_proxy_source,
				"		inparam = new ::%s::struct_wrapper::___%s_%s_inparam_wrapper;\n",
				mod->getname().c_str(), clsname, node->getname().c_str());
		}
		if (has_out) {
			mod->fprintf(scrfile_module_proxy_source,
				"		inout_param = new ::%s::struct_wrapper::___%s_%s_inout_param_wrapper;\n",
				mod->getname().c_str(), clsname, node->getname().c_str());
		}
		mod->fprintf(scrfile_module_proxy_source,
			"		interface->add_method(u_%s, ",
			node->getname().c_str());
		if (has_in && has_out)
			mod->fputs(scrfile_module_proxy_source, "inparam, inout_param, ");
		else if (has_in)
			mod->fputs(scrfile_module_proxy_source, "inparam, nullptr, ");
		else if (has_out)
			mod->fputs(scrfile_module_proxy_source, "nullptr, inout_param, ");
		else 
			mod->fputs(scrfile_module_proxy_source, "nullptr, nullptr, ");
		mod->fprintf(scrfile_module_proxy_source,
			"(void*)___%s_%s_obsvhdr);\n",
			clsname, node->getname().c_str());
	}
	mod->fprintf(scrfile_module_proxy_source,
		"		registered = true;\n"
		"	}\n"
		"	rpcobservable::inst()->add_observable_instance(\"%s.%s\",(void*)this);\n"
		"}\n\n",
	mod->getname().c_str(), clsname);
	mod->fprintf(scrfile_module_proxy_source,
		"%s::~%s()\n"
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

int interface_object_cpp_code_generator::generate_source_method_header(
	srcfile_type id, TInterfaceNode *node)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	
	auto* interf = node->m_pIFObject;
	if (nullptr == interf) return 2;
	string tmp;

	if (node->
		m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Constructor)) {
		method_retval_variable_name(mod, node, tmp, true);
		mod->fprintf(id,
			"%s::%s(", 
			interf->getname().c_str(), interf->getname().c_str());
	} else {
		method_retval_variable_name(mod, node, tmp, true);
		mod->fprintf(id,
			"%s %s::%s(", 
			tmp.c_str(), interf->getname().c_str(), node->getname().c_str());
	}
	bool first = true;
	list_item *item = node->m_VariableList.getnext();
	for (; item != &(node->m_VariableList); item = item->getnext())
	{
		auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		get_variable_full_name(mod, obj, tmp, true, false, true,
			obj->test_flags(varobj_flag_method_param_out));
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

int interface_object_cpp_code_generator::generate_source_method_body_header(
	srcfile_type id, TInterfaceNode *node)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	
	auto* interf = node->m_pIFObject;
	if (nullptr == interf) return 2;

	bool has_input = false;
	bool has_out = false;
	const char* mtdname = node->getname().c_str();
	method_has_in_out_param(node, has_input, has_out);
	if (node->
		m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Constructor)) {
		mtdname = interf->getname().c_str();
	}
		
	if (has_input)
		mod->fprintf(id,
			"	::%s_pbf::___%s_%s_inparam inparam;\n",
			mod->getname().c_str(), interf->getname().c_str(), mtdname);
	if (has_out)
		mod->fprintf(id,
			"	::%s_pbf::___%s_%s_inout_param inout_param;\n",
			mod->getname().c_str(), interf->getname().c_str(), mtdname);
	std::string name = mod->getname() + interf->getname() + mtdname;
	std::string methoduid;
	generate_uid(name.c_str(), methoduid);
	mod->fprintf(id,
		"	uint128_t uid = %s;\n\n", methoduid.c_str());
	return 0;
}

int interface_object_cpp_code_generator::generate_source_method_body_inparam(
	module* cmod, TVariableObject* var, string& ret)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	ret.clear();
	int retval = -1;
	if (is_array(var->m_ArrayType)) {
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
			if (var->test_flags(varobj_flag_method_param_in)) {
				ret += "for(int i = 0; i < ";
				ret += var->getname();
				ret += ".count(); i++) {\n";
				if (var->test_flags(varobj_flag_method_param_out)){
					ret += "\t\tinout_param.add_";
					ret += var->getname();
					ret +="(" + var->getname();
					ret += ".get(i));\n";
				} else {
					ret += "\t\tinparam.add_";
					ret += var->getname();
					ret +="(" +var->getname();
					ret += ".get(i));\n";
				}
				ret += "\t}";
			}
			retval = 0;
			break;
		case eBasicObjType_Enum:
			if (var->test_flags(varobj_flag_method_param_in)) {
				ret += "for(int i = 0; i < ";
				ret += var->getname();
				ret += ".count(); i++) {\n";
				if (var->test_flags(varobj_flag_method_param_out)){
					ret += "\t\tinout_param.add_";
					ret += var->getname();
					ret +="((int32_t)" + var->getname();
					ret += ".get(i));\n";
				} else {
					ret += "\t\tinparam.add_";
					ret += var->getname();
					ret +="((int32_t)" +var->getname();
					ret += ".get(i));\n";
				}
				ret += "\t}";
			}
			retval = 0;
			break;
		case eBasicObjType_Stream:
			break;
		case eBasicObjType_Interface: {
			if (var->test_flags(varobj_flag_method_param_in)) {
				ret += "for(int i = 0; i < ";
				ret += var->getname();
				ret += ".count(); i++) {\n";
				if (var->test_flags(varobj_flag_method_param_out)){
					ret += "\t\tinout_param.add_";
					ret += var->getname();
					ret +="((int64_t)" + var->getname();
					ret += ".get(i));\n";

				} else {
					ret += "\t\tinparam.add_";
					ret += var->getname();
					ret +="((int64_t)" +var->getname();
					ret += ".get(i));\n";
				}
				ret += "\t}";
			}
			retval = 0;	
			} break;
		case eBasicObjType_Typedefed:
			// todo:
			break;

		case eBasicObjType_UserDefined: {
			if (var->test_flags(varobj_flag_method_param_out)){
				if (var->test_flags(varobj_flag_method_param_in)) {
					ret += var->getname();
					ret += ".copyto(*inout_param.mutable_";
					ret += var->getname() + "());";
				}
			} else {
				ret += var->getname();
				ret += ".copyto(*inparam.mutable_";
				ret += var->getname() + "());";
			}
			retval = 0;
			}	break;
		}
	} else {
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
		case eBasicObjType_Enum:
			if (var->test_flags(varobj_flag_method_param_out)){
				if (var->test_flags(varobj_flag_method_param_in)) {
					ret += "inout_param.set_";
					ret += var->getname();
					ret +="(" + var->getname();
					ret += ");";
				}
			} else {
				ret += "inparam.set_";
				ret += var->getname();
				ret +="(" +var->getname();
				ret += ");";
			}
			retval = 0;
			break;
		case eBasicObjType_Stream:
			if (var->test_flags(varobj_flag_method_param_out)){
				if (var->test_flags(varobj_flag_method_param_in)) {
					ret += "inout_param.set_";
					ret += var->getname();
					ret +="(" + var->getname();
					ret += ".getbuff(), ";
					ret += var->getname();
					ret += ".getlength());";
				}
			} else {
				ret += "inparam.set_";
				ret += var->getname();
				ret +="(" + var->getname();
				ret += ".getbuff(), ";
				ret += var->getname();
				ret += ".getlength());";
			}
			retval = 0;
			break;
		case eBasicObjType_Interface: {
			if (var->test_flags(varobj_flag_method_param_out)){
				if (var->test_flags(varobj_flag_method_param_in)) {
					ret += "inout_param.set_";
					ret += var->getname();
					ret +="((size_t)& " +var->getname();
					ret += ");\n";
					ret += "\trpcobservable_data* data = rpcobservable::inst()->get_observable_data();\n";
					ret += "\tassert(NULL != data);\n";
					ret += "\tstd::string cliname = data->pkg_name;\n";
					ret += "\tstd::string instname = data->inst_name;\n";
					ret += "\tinout_param.set_client_name_";
					ret += var->getname() + "(cliname);\n";
					ret += "\tinout_param.set_inst_name_";
					ret += var->getname() + "(instname);";
				}
			} else {
				ret += "inparam.set_";
				ret += var->getname();
				ret +="((size_t)& " +var->getname();
				ret += ");\n";
				ret += "	rpcobservable_data* data = rpcobservable::inst()->get_observable_data();\n";
				ret += "	assert(NULL != data);\n";
				ret += "	std::string cliname = data->pkg_name;\n";
				ret += "	std::string instname = data->inst_name;\n";
				ret += "	inparam.set_client_name_";
				ret += var->getname() + "(cliname);\n";
				ret += "	inparam.set_inst_name_";
				ret += var->getname() + "(instname);";
			}
			retval = 0;	
			} break;
		case eBasicObjType_Typedefed:
			// todo:
			break;

		case eBasicObjType_UserDefined: {
			if (var->test_flags(varobj_flag_method_param_out)){
				if (var->test_flags(varobj_flag_method_param_in)) {
					ret += var->getname();
					ret += ".copyto(*inout_param.mutable_";
					ret += var->getname() + "());";
				}
			} else {
				ret += var->getname();
				ret += ".copyto(*inparam.mutable_";
				ret += var->getname() + "());";
			}
			retval = 0;
			}	break;
		}
	}

	return retval;
}

int interface_object_cpp_code_generator::generate_source_method_body_outparam(
	module* cmod, TVariableObject* var, string& ret)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	ret.clear();
	int retval = -1;			
	if (!var->test_flags(varobj_flag_method_param_out)) {
		return retval;
	}
	string tmp;
	if (is_array(var->m_ArrayType)) {
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
			ret += "for(int i = 0; i < inout_param.";
			ret += var->getname();
			ret += "_size(); i++) {\n\t\t";
			ret += var->getname();
			ret +=".add_" + var->getname();
			ret += "(";
			ret += var->getname();
			ret += ".get(i));\n";
			ret += "\t}";
			retval = 0;
			break;
		case eBasicObjType_Enum:
			ret += "for(int i = 0; i < inout_param.";
			ret += var->getname();
			ret += "_size(); i++) {\n\t\t";
			ret += var->getname();
			ret +=".add_" + var->getname();
			ret += "((";
			get_variable_full_name(cmod, var, tmp, 
				true, false, false, false);
			ret += tmp + ") " + var->getname();
			ret += ".get(i));\n";
			ret += "\t}";
			retval = 0;
			break;
		case eBasicObjType_Stream:
			break;
		case eBasicObjType_Interface: {
			ret += "for(int i = 0; i < inout_param.";
			ret += var->getname();
			ret += "_size(); i++) {\n\t\t";
			ret += var->getname();
			ret +=".add_" + var->getname();
			ret += "((";
			get_variable_full_name(cmod, var, tmp, 
				true, false, false, false);
			ret += tmp + "*) " + var->getname();
			ret += ".get(i));\n";
			ret += "\t}";
			retval = 0;
			} break;
		case eBasicObjType_Typedefed:
			// todo:
			break;

		case eBasicObjType_UserDefined: {

			ret += var->getname();
			ret += ".copyfrom(*inout_param.mutable_";
			ret += var->getname() + "());";
			retval = 0;

		}	break;
		}
	} else {
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
			ret += var->getname();
			ret += " = inout_param.";
			ret += var->getname();
			ret += "();";
			retval = 0;
			break;
		case eBasicObjType_Enum:
			ret += var->getname();
			get_variable_full_name(cmod, var, tmp, 
				true, false, false, false);
			ret += " = (";
			ret += tmp + ")inout_param.";
			ret += var->getname();
			ret += "();";
			retval = 0;
			break;
		case eBasicObjType_Stream:
			ret += var->getname();
			ret += " = inout_param.";
			ret += var->getname();
			ret += "();";
			retval = 0;
			break;
		case eBasicObjType_Interface: {
			ret += var->getname();
			get_variable_full_name(cmod, var, tmp, 
				true, false, false, false);
			ret += " = (";
			ret += tmp + "*)inout_param.";
			ret += var->getname();
			ret += "();";
			retval = 0;		
			} break;
		case eBasicObjType_Typedefed:
			// todo:
			break;

		case eBasicObjType_UserDefined: {
			ret += var->getname();
			ret += ".copyfrom(*inout_param.mutable_";
			ret += var->getname() + "());";
			retval = 0;
		}	break;
		}
	}
	return retval;
}

int interface_object_cpp_code_generator::generate_source_call_method_var(
	module* cmod, TVariableObject* var, string& ret)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	ret.clear();
	int retval = 1;

	std::string tmp;
	get_variable_full_name(cmod, var, tmp,
		true, true, false, false, false, true);

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
			ret += " = inoutp->";
		} else {
			ret += " = inp->";
		}
		ret += var->getname();
		ret += "();";
		retval = 0;
	}
	break;
	case eBasicObjType_Enum:{
		ret += tmp + " ";
		ret += var->getname();
		if (var->test_flags(varobj_flag_method_param_out)){
			ret += " = (";
			ret += tmp +")inoutp->";
		} else {
			ret += " = (";
			ret += tmp +")inp->";
		}
		ret += var->getname();
		ret += "();";
		retval = 0;
	}
	break;
	case eBasicObjType_Stream:
		ret += tmp + " ";
		ret += var->getname();
		if (var->test_flags(varobj_flag_method_param_out)){
			ret += " = inoutp->";
		} else {
			ret += " = inp->";
		}
		ret += var->getname();
		ret += "();";
		retval = 0;
		break;
	case eBasicObjType_Interface: {
		ret += tmp + " ";
		ret += var->getname();
		if (var->test_flags(varobj_flag_method_param_out)){
			ret += "((void*)inoutp->";
			ret += var->getname();
			ret += "(), inoutp->client_name_";
			ret += var->getname() + "().c_str(), inoutp->inst_name_";
			ret += var->getname() + "().c_str());";
		} else {
			ret += "((void*)inp->";
			ret += var->getname();
			ret += "(), inp->client_name_";
			ret += var->getname() + "().c_str(), inp->inst_name_";
			ret += var->getname() + "().c_str());";
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
			ret += "(inout_param, inoutp->mutable_";
		} else {
			ret += "(inparam, inp->mutable_";
		}
		ret += var->getname();
		ret += "());";
		retval = 0;
	}	break;
	}
	return retval;
}

int interface_object_cpp_code_generator::generate_source_call_method_end_var(
	module* cmod, TVariableObject* var, string& ret)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	ret.clear();
	int retval = 1;

	std::string tmp;
	get_variable_full_name(cmod, var, tmp, true, true, false,
		var->test_flags(varobj_flag_method_param_out));

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
	case eBasicObjType_Enum:
		ret += "inoutp->set_";
		ret += var->getname();
		ret += "(";
		ret += var->getname();
		ret += ");";
		retval = 0;
	}
	break;
	case eBasicObjType_Stream:
		ret += "inoutp->set_";
		ret += var->getname();
		ret += "(";
		ret += var->getname();
		ret += ".getbuff(), ";
		ret += var->getname();
		ret += ".getlength());";
		retval = 0;
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

int interface_object_cpp_code_generator::generate_source_method_body_ret_param(
	module* cmod, TInterfaceNode* obj, string& ret)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	ret.clear();
	int retval = -1;
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
		retval = 0;
		break;
	case eBasicObjType_Enum:{
		std::string tmp;
		method_retval_variable_name(mod, obj, tmp, true, false, false, true);
		ret += "return (";
		ret += tmp +")inout_param.___retval();";
		retval = 0;
		}break;
	case eBasicObjType_Stream:
		ret += "return inout_param.___retval();";
		retval = 0;
		break;
	case eBasicObjType_Interface: {
		std::string tmp;
		method_retval_variable_name(mod, obj, tmp, true, false, false, true);
		ret += "auto *retval = new ";
		ret += tmp + "((void*)inout_param.___retval());\n";
		ret += "	return *retval;";
		retval = 0;
		} break;
	case eBasicObjType_Typedefed:
		// todo:
		break;

	case eBasicObjType_UserDefined: {
		std::string tmp;
		method_retval_variable_name(mod, obj, tmp, true, false, false, true);
		ret += tmp;
		ret += " *retval = new";
		ret += tmp + ";\n";
		ret += "	retval->copyfrom(*inout_param.mutable____retval());\n";
		ret += " return *retval;";
		retval = 0;
	}	break;
	}
	return retval;
}

int interface_object_cpp_code_generator::generate_attribute_body_ret_param(
	module* cmod, TInterfaceNode* obj, string& ret)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	ret.clear();
	int retval = -1;
	switch (obj->m_Data.attribute._basic_type)
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
		retval = 0;
		break;
	case eBasicObjType_Enum:{
		std::string tmp;
		method_attribute_variable_name(mod, obj, tmp, true, false, false, true);
		ret += "return (";
		ret += tmp +")inout_param.___retval();";
		retval = 0;
		}break;
	case eBasicObjType_Stream:
		ret += "return inout_param.___retval();";
		retval = 0;
		break;
	case eBasicObjType_Interface: {
		std::string tmp;
		method_attribute_variable_name(mod, obj, tmp, true, false, false, true);
		ret += "auto *retval = new ";
		ret += tmp + "((void*)inout_param.___retval());\n";
		ret += "	return *retval;";
		retval = 0;
		} break;
	case eBasicObjType_Typedefed:
		// todo:
		break;

	case eBasicObjType_UserDefined: {
		std::string tmp;
		method_attribute_variable_name(mod, obj, tmp, true, false, false, true);
		ret += tmp;
		ret += " *retval = new";
		ret += tmp + ";\n";
		ret += "	retval->copyfrom(*inout_param.mutable____retval());\n";
		ret += " return *retval;";
		retval = 0;
	}	break;
	}
	return retval;
}

int interface_object_cpp_code_generator::generate_source_method_body_return_default(
	module* cmod, TInterfaceNode* obj, string& ret, bool proxy, bool observable)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	ret.clear();
	int retval = 0;
	TVariableObject var;
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
	var._basic_type = obj->m_Data.method.m_eRetValType;
	var.m_pIFType = obj->m_Data.method.m_pRetValIFObject;
	var.m_pRefType = obj->m_Data.method.m_pRetValTypedefType;
	var.m_pEnumDefType = obj->m_Data.method.m_pRetValEnumDef;
	var.m_pUserDefType = obj->m_Data.method.m_pRetValUserDefType;
	if (is_array(var.m_ArrayType)) {
		switch (var._basic_type)
		{
		case basic_objtype_boolean:
		case basic_objtype_uint8:
		case eBasicObjType_Int16:
		case eBasicObjType_Int32:
		case eBasicObjType_Int64:
		case eBasicObjType_UInt32:
		case eBasicObjType_Float:
		case eBasicObjType_String:
			ret += "return new ";
			ret += variable_types[var._basic_type];
			ret += ";";
			break;
		case eBasicObjType_Enum: {
			auto* enumdeft = var.m_pEnumDefType;
			auto* mod = enumdeft->GetModule();
			assert(nullptr != mod);
			ret += "return new";
			if (mod != cmod) {
				ret += "::";
				ret += mod->getname();
				ret += "::";
			} else if (proxy || (!proxy && observable)) {
				ret += "::";
				ret += cmod->getname();
				ret += "::";
			}
			ret += enumdeft->getname();
			ret += ";\n";
		}break;
		case eBasicObjType_Stream:
			break;
		case eBasicObjType_Interface: {
			std::string tmp;
			ret += "auto* retval = new ";
			auto* interf = var.m_pIFType;
			auto* mod = interf->GetModule();
			assert(nullptr != mod);
			if (mod != cmod) {
				ret += "::";
				ret += mod->getname();
				ret += "::";
			}
			if (mod != cmod)
				ret += "array::";
			else 
				ret += "::array::";
			ret += interf->getname();
			ret += ";\n";
			ret += "	return *retval;";
			} break;
		case eBasicObjType_Typedefed:
			retval = 1;
			break;

		case eBasicObjType_UserDefined: {
			auto* usrdef = var.m_pUserDefType;
			auto* mod = usrdef->GetModule();
			assert(nullptr != mod);
			ret += "auto* retval = new ";
			if (proxy || mod != cmod || observable) {
				ret += "::";
				ret += mod->getname();
				ret += "::";
			}
			if (is_array(var.m_ArrayType))
				ret += "array::";
			ret += usrdef->getname();
			ret += ";\n";
			ret += "	return *retval;";
		}	break;
		default:
			retval = 2;
		}

	} else {
		switch (var._basic_type)
		{
		case basic_objtype_boolean:
		case basic_objtype_uint8:
		case eBasicObjType_Int16:
		case eBasicObjType_Int32:
		case eBasicObjType_Int64:
		case eBasicObjType_UInt32:
			ret += "return 0;";
			break;
		case eBasicObjType_Float:
			ret += "return 0.0;";
			break;
		case eBasicObjType_String:
			ret += "return "";";
			break;
		case eBasicObjType_Enum: {
			auto* enumdeft = var.m_pEnumDefType;
			auto* mod = enumdeft->GetModule();
			assert(nullptr != mod);
			ret += "return ";
			if (mod == cmod && (proxy || (!proxy && observable))) {
				ret += "::";
				ret += cmod->getname();
				ret += "::";
			}
			ret += var.m_pEnumDefType->GetDefault(cmod);
			ret += ";\n";
		}break;
		case eBasicObjType_Stream:
			ret += "return 0;";
			break;
		case eBasicObjType_Interface: {
			std::string tmp;
			ret += "auto* retval = new ";
			auto* interf = var.m_pIFType;
			auto* mod = interf->GetModule();
			assert(nullptr != mod);
			if (mod != cmod) {
				ret += "::";
				ret += mod->getname();
				ret += "::";
			}
			if (is_array(var.m_ArrayType)) {
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
							ret += "::";
						}
					} else {
						if (!observable) {
							ret += "skeleton::";
						}
					}
				}
			}
			ret += interf->getname();
			ret += ";\n";
			ret += "	return *retval;";
			} break;
		case eBasicObjType_Typedefed:
			retval = 1;
			break;

		case eBasicObjType_UserDefined: {
			auto* usrdef = var.m_pUserDefType;
			auto* mod = usrdef->GetModule();
			assert(nullptr != mod);
			ret += "auto* retval = new ";
			if (proxy || mod != cmod || observable) {
				ret += "::";
				ret += mod->getname();
				ret += "::";
			}
			if (is_array(var.m_ArrayType))
				ret += "array::";
			ret += usrdef->getname();
			ret += ";\n";
			ret += "	return *retval;";
		}	break;
		default:
			retval = 2;
		}
	}
	return retval;
}

int interface_object_cpp_code_generator::generate_source_method_emtpy(
	module* cmod, TInterfaceNode* node, srcfile_type id,
	bool proxy, bool bobservable, bool bcomments)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	if (nullptr == node) return 1;
	const char* clsname = getname().c_str();
	list_item* subitem = NodeList().getnext();

	string tmp;
	method_retval_variable_name(mod, node, tmp, proxy, bobservable);
	mod->fprintf(id,
		"%s %s::%s(", tmp.c_str(), clsname, node->getname().c_str());

	bool first = true;
	list_item *item = node->m_VariableList.getnext();
	for (; item != &(node->m_VariableList); item = item->getnext())
	{
		auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
		get_variable_full_name(mod, obj, tmp, proxy, bobservable, true,
			obj->test_flags(varobj_flag_method_param_out));
		if (first) {
			first = false;
			mod->fprintf(id,
				"%s %s", tmp.c_str(), obj->getname().c_str());
		}
		else
			mod->fprintf(id,
				", %s %s", tmp.c_str(), obj->getname().c_str());
	}
	mod->fputs(id, ")\n{\n");

	if (bcomments)
		mod->fputs(id,
			"	// TODO: write your code for implementation\n");

	if (!get_variable_return_default_value(mod, node, tmp, proxy,
		bobservable))
		mod->fprintf(id,
		"	%s", tmp.c_str(),tmp.c_str());

	mod->fputs(id, "\n}\n\n");
	return -1;
}

int interface_object_cpp_code_generator::generate_source_call_mehtod(
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
	mod->fputs(id,"{\n\tassert(nullptr != obj);\n\n");
	bool has_in, has_out;
	method_has_in_out_param(node, has_in, has_out);
	if (has_in)
		mod->fprintf(id,
			"	auto* inp = zas_downcast(google::protobuf::Message,	\\\n"
			"		::%s_pbf::___%s_%s_inparam,	\\\n"
			"		inparam->get_pbfmessage());\n",
			modname, clsname, methodname);
	if (has_out)
		mod->fprintf(id,
			"	auto* inoutp = zas_downcast(google::protobuf::Message,	\\\n"
			"		::%s_pbf::___%s_%s_inout_param,	\\\n"
			"		inout_param->get_pbfmessage());\n\n",
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
		"	return 1;\n"
		"}\n\n");
	return -1;
}

int interface_object_cpp_code_generator::generate_source_call_mehtod_end(
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
		ret += "(((";
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
	case eBasicObjType_Enum:
	case eBasicObjType_Stream:
	case eBasicObjType_String:{
		ret += "inoutp->set____retval(((";
		ret += clsname;
		ret += "*)obj)->";
		ret += methodname;
		ret += "(";
	}
	break;
	case eBasicObjType_Interface:
	case eBasicObjType_UserDefined: {
		string tmp;
		method_retval_variable_name(mod, node, tmp, true, false, false, true);
		ret += tmp;
		ret += "& ___retval = ((";
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
	case eBasicObjType_Enum:
	case eBasicObjType_Stream:
	case eBasicObjType_String:{
		ret += "));\n";
	}
	break;
	case eBasicObjType_Interface:{
		ret += ");\n";
		ret += "	inoutp->set____retval((int64_t)&___retval);\n";
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

int interface_object_cpp_code_generator::generate_source_constructor_end(
	module* cmod, TInterfaceNode* node, string &ret)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	if (nullptr == node) return 1;
	const char* clsname = getname().c_str();
	const char* modname = mod->getname().c_str();
	const char* methodname = node->getname().c_str();

	ret.clear();
	ret += "(void*)";
	ret += clsname;
	ret += "::create_instance(";

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
	ret += ");\n";
	return -1;
}

int interface_object_cpp_code_generator::generate_attribute_body_end(
	module* cmod, TInterfaceNode* node, string &ret, bool get)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	if (nullptr == node) return 1;
	string clsname = node->m_pIFObject->getname().c_str();
	string modname = mod->getname().c_str();
	string methodname;
	if (get)
		methodname += "get_";
	else 
		methodname += "set_";
	methodname += node->getname().c_str();

	ret.clear();
	basic_objtype type = node->m_Data.attribute._basic_type;
	assert(eBasicObjType_Unknown != type);
	string tmp;
	method_attribute_variable_name(mod, node, tmp, 
		false, false, false, true);
	if (get) {
		switch (type)
		{
		case basic_objtype_boolean:
		case basic_objtype_uint8:
		case eBasicObjType_Int16:
		case eBasicObjType_Int32:
		case eBasicObjType_Int64:
		case eBasicObjType_UInt32:
		case eBasicObjType_Float:
		case eBasicObjType_Enum:
		case eBasicObjType_String:{
			ret += "inoutp->set____retval(((";
			ret += clsname;
			ret += "*)obj)->";
			ret += methodname;
			ret += "());\n";
		}
		break;
		case eBasicObjType_Stream:
			ret += "inoutp->set____retval(((";
			ret += clsname;
			ret += "*)obj)->";
			ret += methodname;
			ret += "().getbuff(), ";
			ret += methodname;
			ret += "().getlength());\n";
			break;
		case eBasicObjType_Interface:
		case eBasicObjType_UserDefined: {
			ret += tmp;
			ret += "& ___retval = ((";
			ret += clsname;
			ret += "*)obj)->";
			ret += methodname;
			ret += "();\n";
			if (eBasicObjType_Interface == type)
				ret += "\tinoutp->set____retval((int64_t)&___retval);\n";
			else
				ret += "\t___retval.copyto(*(inoutp->mutable____retval()));\n";
			} break;
		case eBasicObjType_Typedefed:
			// todo:
			break;
		}
	} else {
		switch (type)
		{
		case basic_objtype_boolean:
		case basic_objtype_uint8:
		case eBasicObjType_Int16:
		case eBasicObjType_Int32:
		case eBasicObjType_Int64:
		case eBasicObjType_UInt32:
		case eBasicObjType_Float:
		case eBasicObjType_String:{
			ret += "((";
			ret += clsname;
			ret += "*)obj)->";
			ret += methodname;
			ret += "(inp->";
			ret += node->getname();
			ret +="());\n";
		}
		break;
		case eBasicObjType_Stream:
			ret += "((";
			ret += clsname;
			ret += "*)obj)->";
			ret += methodname;
			ret += "(inp->";
			ret += node->getname();
			ret +="());\n";
			break;
		case eBasicObjType_Enum:{
			ret += tmp + " ";
			ret += node->getname();	
			ret += " = (";
			ret += tmp +")inp->";
			ret += node->getname();
			ret += "();\n\t((";
			ret += clsname;
			ret += "*)obj)->";
			ret += methodname;
			ret += "(";
			ret += node->getname();
			ret += ");\n";
		}
		break;
		case eBasicObjType_Interface: {
			ret += tmp + " ";
			ret += node->getname();
			ret += "((void*)inp->";
			ret += node->getname();
			ret += "(), inp->client_name_";
			ret += node->getname() + "().c_str(), inp->inst_name_";
			ret += node->getname() + "().c_str());\n\t((";
			ret += clsname;
			ret += "*)obj)->";
			ret += methodname;
			ret += "(";
			ret += node->getname();
			ret += ");\n";
			} break;
		case eBasicObjType_Typedefed:
			// todo:
			break;

		case eBasicObjType_UserDefined: {
			ret += tmp + " ";
			ret += node->getname();
			ret += "(inparam, inp->mutable_";
			ret += node->getname();
			ret += "());\n\t((";
			ret += clsname;
			ret += "*)obj)->";
			ret += methodname;
			ret += "(";
			ret += node->getname();
			ret += ");\n";
		}	break;
		}
	}
	return 0;
}

int interface_object_cpp_code_generator::method_has_in_out_param(
	TInterfaceNode *node, bool &has_in, bool &has_out)
{
	has_in = false;
	has_out = false;
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

bool interface_object_cpp_code_generator::interface_has_constructor(void)
{
	list_item* subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		if (eIFNodeType_Method != node->GetType()) {
			continue;
		}
		if (node->
			m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Constructor)) {
			return true;
		}
	}
	return false;
}

int interface_object_cpp_code_generator::generate_skeleton_source(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* clsname = getname().c_str();
	const char* modname = mod->getname().c_str();

	mod->fprintf(scrfile_module_skeleton_source,
		"void* %s_factory(void *destroy, pbf_wrapper inparam)\n"
		"{\n"
		"	if (destroy) {\n"
		"		%s::destroy_instance((%s*)destroy);\n"
		"		return nullptr;\n"
		"	}\n",
		clsname,
		clsname, clsname);
	bool has = interface_has_constructor();
	list_item* subitem = nullptr;
	if (has) {
		mod->fprintf(scrfile_module_skeleton_source,
			"	if (inparam.get()) {\n"
			"		auto* inp = zas_downcast(google::protobuf::Message,	\\\n"
			"			::%s_pbf::___%s_%s_inparam,	\\\n"
			"			inparam->get_pbfmessage());\n",
			modname, clsname, clsname);

		string tmp;
		subitem = NodeList().getnext();
		for (; subitem != &NodeList(); subitem = subitem->getnext())
		{
			auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
			EIFNodeType eType = node->GetType();
			assert(eIFNodeType_Unknown != eType);

			if (eIFNodeType_Method != node->GetType()) {
				continue;
			}
			if (node->
				m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Constructor)) {
				list_item *item = node->m_VariableList.getnext();
				for (; item != &(node->m_VariableList); item = item->getnext())
				{
					auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
					generate_source_call_method_var(mod, obj, tmp);
					mod->fprintf(scrfile_module_skeleton_source,
						"	%s\n", tmp.c_str());
				}
				generate_source_constructor_end(mod, node, tmp);
				break;
			}
		}
		mod->fprintf(scrfile_module_skeleton_source,
			"		return %s\n"
			"	} else {\n"
			"		return (void*)%s::create_instance();\n"
			"	}\n"
			"}\n\n",
			tmp.c_str(), clsname);
	} else {
		mod->fprintf(scrfile_module_skeleton_source,
			"	return (void*)%s::create_instance();\n"
			"}\n\n",
			 clsname);
	}

	subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		if (eIFNodeType_Attribute == node->GetType()) {
			generate_attribute_body(node, false);
			continue;
		} else if (eIFNodeType_Method != node->GetType()) {
			continue;
		}
		if (node->
			m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Constructor)){
			continue;
		}
		generate_source_call_mehtod(scrfile_module_skeleton_source, mod, node, false);
	}

	mod->fprintf(scrfile_module_skeleton_source,
		"void import_%s(void)\n"
		"{\n"
		"	pbf_wrapper param;\n"
		"	rpchost host = rpcmgr::inst()->get_service(\"%s\", nullptr);\n"
		"	rpcinterface rif = host->register_interface(\"%s.%s\", (void*)%s_factory, param);\n",
		clsname,
		mod->getconfig().get_service_name().c_str(),
		modname, clsname, clsname);

	if (TestInterfaceFlags(eInterfaceFlag_Singleton)) {
		mod->fprintf(scrfile_module_skeleton_source,
			"	rif->set_singleton(true);\n");		
	}
	
	mod->fprintf(scrfile_module_skeleton_source,
		"	pbf_wrapper inparam, inout_param;\n");
	std::string mname;
	std::string methoduid;
	subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *node = LIST_ENTRY(TInterfaceNode, m_OwnerList, subitem);
		EIFNodeType eType = node->GetType();
		assert(eIFNodeType_Unknown != eType);

		if (eIFNodeType_Attribute == node->GetType()) {
			mname = modname;
			mname += clsname;
			mname += "get_" + node->getname();
			generate_uid(mname.c_str(), methoduid);
			mod->fprintf(scrfile_module_skeleton_source,
				"\n	uint128_t u_get_%s = %s;\n",
				node->getname().c_str(), methoduid.c_str());
			mname = modname;
			mname += clsname;
			mname += "set_" + node->getname();
			generate_uid(mname.c_str(), methoduid);
			mod->fprintf(scrfile_module_skeleton_source,
				"	uint128_t u_set_%s = %s;\n",
				node->getname().c_str(), methoduid.c_str());

			mod->fprintf(scrfile_module_skeleton_source,
			"	inparam = new ::%s::struct_wrapper::___%s_set_%s_inparam_wrapper;\n"
			"	inout_param = new ::%s::struct_wrapper::___%s_get_%s_inout_param_wrapper;\n"
			"	rif->add_method(u_set_%s, inparam, nullptr, (void*)___%s_set_%s_handler);\n"
			"	rif->add_method(u_get_%s, nullptr, inout_param, (void*)___%s_get_%s_handler);\n",
			modname, clsname, node->getname().c_str(),
			modname, clsname, node->getname().c_str(),
			node->getname().c_str(),
			clsname, node->getname().c_str(),
			node->getname().c_str(),
			clsname, node->getname().c_str());
			continue;
		} else if (eIFNodeType_Method != node->GetType()) {
			continue;
		}
		if (node->
			m_Data.method.test_flags(TInterfaceNode::eMethodFlag_Constructor)){
			continue;
		}
		mname = modname;
		mname += clsname + node->getname();
		generate_uid(mname.c_str(), methoduid);
		mod->fprintf(scrfile_module_skeleton_source,
			"	uint128_t u_%s = %s;\n\n",
			node->getname().c_str(), methoduid.c_str());
		bool has_in, has_out;
		method_has_in_out_param(node, has_in, has_out);
		if (has_in)
			mod->fprintf(scrfile_module_skeleton_source,
				"\tinparam = new ::%s::struct_wrapper::___%s_%s_inparam_wrapper;\n",
				modname, clsname, node->getname().c_str());
		if (has_out)
			mod->fprintf(scrfile_module_skeleton_source,
				"	inout_param = new ::%s::struct_wrapper::___%s_%s_inout_param_wrapper;\n",
				modname, clsname, node->getname().c_str());

		mod->fprintf(scrfile_module_skeleton_source,
			"	rif->add_method(u_%s, ",
			node->getname().c_str());
		if (has_in && has_out)
			mod->fputs(scrfile_module_skeleton_source, "inparam, inout_param,");
		else if (has_in)
			mod->fputs(scrfile_module_skeleton_source, "inparam, nullptr,");
		else if (has_out)
			mod->fputs(scrfile_module_skeleton_source, "nullptr, inout_param,");
		else
			mod->fputs(scrfile_module_skeleton_source, "nullptr, nullptr,");
		mod->fprintf(scrfile_module_skeleton_source,
			" (void*)___%s_%s_handler);\n",
			clsname, node->getname().c_str());
	}

	mod->fputs(scrfile_module_skeleton_source, "}\n\n");
	return 0;
}

int
interface_object_cpp_code_generator::generate_observable_skeleton_source(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* clsname = getname().c_str();
	const char* modname = mod->getname().c_str();

		
	mod->fprintf(scrfile_module_skeleton_source,
		"%s::%s()\n"
		": _inst_id(NULL)\n"
		"{\n"
		"}\n\n"
		"%s::%s(void* inst_id, const char* cliname,\n"
		"	const char* inst_name)\n"
		": _inst_id(inst_id)\n"
		", _cliname(cliname)\n"
		", _inst_name(inst_name)\n"
		"{\n"
		"	assert(0 != _inst_id);\n"
		"	rpcobservable::inst()->add_client_instance(cliname, inst_name, inst_id);\n"
		"}\n\n"
		"%s::%s(const %s& c)\n"
		"{\n"
		"	if (this == &c) {\n"
		"		return;\n"
		"	}\n"
		"	if (_inst_id) {\n"
		"		rpcobservable::inst()->release_client_instance(_inst_id);\n"
		"	}\n"
		"	_inst_id = c._inst_id;\n"
		"	_cliname = c._cliname;\n"
		"	_inst_name = c._inst_name;\n"
		"	rpcobservable::inst()->add_client_instance(_cliname.c_str(),\n"
		"		_inst_name.c_str(), _inst_id);\n"
		"}\n\n",
		clsname, clsname,
		clsname, clsname,
		clsname, clsname, clsname);

	mod->fprintf(scrfile_module_skeleton_source,
		"const %s& %s::operator=(const %s& c)\n"
		"{\n"
		"	if (this == &c) {\n"
		"		return *this;\n"
		"	}\n"
		"	if (_inst_id) {\n"
		"		rpcobservable::inst()->release_client_instance(_inst_id);\n"
		"	}\n"
		"	_inst_id = c._inst_id;\n"
		"	_cliname = c._cliname;\n"
		"	_inst_name = c._inst_name;\n"
		"	rpcobservable::inst()->add_client_instance(_cliname.c_str(),\n"
		"		_inst_name.c_str(), _inst_id);\n\n"

		"	return *this;\n"
		"}\n\n"
		"%s::~%s()\n"
		"{\n"
		"	if (_inst_id) {\n"
		"		rpcobservable::inst()->release_client_instance(_inst_id);\n"
		"		_inst_id = nullptr;\n"
		"	}\n"
		"}\n\n",
		clsname, clsname, clsname,
		clsname, clsname);


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
		generate_source_method_header(scrfile_module_skeleton_source, node);
		mod->fputs(scrfile_module_skeleton_source, "\n{\n");
		generate_source_method_body_header(scrfile_module_skeleton_source, node);
		list_item *item = node->m_VariableList.getnext();
		for (; item != &(node->m_VariableList); item = item->getnext())
		{
			auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
			if (!generate_source_method_body_inparam(mod, obj, tmp))
				mod->fprintf(scrfile_module_skeleton_source, "	%s\n", tmp.c_str());
		}
		mod->fprintf(scrfile_module_skeleton_source,
			"\n	rpcobservable::inst()->invoke_observable_method(_inst_id,\n"
			"		\"%s.%s\", uid,\n",
			clsname, node->getname().c_str());
		bool has_in, has_out;
		method_has_in_out_param(node, has_in, has_out);
		if (has_in && has_out)
			mod->fputs(scrfile_module_skeleton_source,"	\t&inparam, &inout_param, 1);\n\n");
		else if (has_in)
			mod->fputs(scrfile_module_skeleton_source,"	\t&inparam, nullptr, 1);\n\n");
		else if (has_out)
			mod->fputs(scrfile_module_skeleton_source,"	\tnullptr, &inout_param, 1);\n\n");
		else
			mod->fputs(scrfile_module_skeleton_source,"	\tnullptr, nullptr, 1);\n\n");
		item = node->m_VariableList.getnext();
		for (; item != &(node->m_VariableList); item = item->getnext())
		{
			auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, item);
			if (!generate_source_method_body_outparam(mod, obj, tmp))
				mod->fprintf(scrfile_module_skeleton_source, "	%s\n", tmp.c_str());
		}
		if (!generate_source_method_body_ret_param(mod, node, tmp))
			mod->fprintf(scrfile_module_skeleton_source, "	%s\n", tmp.c_str());

		mod->fputs(scrfile_module_skeleton_source, "}\n\n");
	}

	return 0;
}
