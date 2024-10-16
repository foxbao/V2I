/** @file inc/cgjava-module.h
 * implementation of java code generation of module for zrpc
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

event_object_java_code_generator::event_object_java_code_generator(
	module *mod, const string name)
	: TEventObject(mod, name)
{
}

event_object_java_code_generator::~event_object_java_code_generator()
{
}

bool event_object_java_code_generator::generate_code(void)
{

	if (!generate_related_object_code()) {
		return false;
	}
	
	if (generate_event_protobuf())
		return false;
	if (generate_event_source())
		return false;
	return true;
}

int event_object_java_code_generator::generate_event_header(void)
{
	return 0;
}

int event_object_java_code_generator::variable_full_name(
	module* cmod, TVariableObject* var, string& ret, bool skeleton, bool defined)
{
	const char* variable_types[] = {
		"void",	// unknown
		"BoolRef",		// bool
		"ByteRef",	// uint8
		"ShortRef",	// int16
		"IntRef",	// int32
		"LongRef",	// long
		"IntRef",	// uint32
		"DoubleRef",	// Float
		nullptr,	// stream
		"StrRef",	// string
		nullptr,	// enum - special handling
		nullptr,	// interface - shall not have this
		nullptr,	// typedefed - special handling
		nullptr,	// userdefined - special handling
	};

	const char* variable_array_types[] = {
		nullptr,	// unknown
		"BoolRef[]",		// bool
		"ByteRef[]",	// uint8
		"ShortRef[]",	// int16
		"IntRef[]",	// int32
		"LongRef[]",	// long
		"IntRef[]",	// uint32
		"DoubleRef[]",	// Float
		nullptr,	// stream
		"StrRef[]",	// string
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
			return 0;
		}
	} else {
		type = variable_types[var->_basic_type];
		if (type) {
			ret += type;
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
			ret += mod->getname();
		}
		ret += interf->getname();
		} break;
	case eBasicObjType_Typedefed:
		// todo:
		break;

	case eBasicObjType_UserDefined: {
		auto* usrdef = var->m_pUserDefType;
		ret += usrdef->getname();
	}	break;
	}

	return 0;
}

int event_object_java_code_generator::generate_event_header_skeleton(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* modname = mod->getname().c_str();
	const char* evtname = getname().c_str();

	mod->fprintf(srcfile_module_proxy_header,
		"namespace skeleton {\n"
		"	class %s\n"
		"	{\n"
		"	public:\n"
		"		%s(){}\n"
		"		~%s(){}\n\n",
		evtname,
		evtname,
		evtname);
	if(!has_param()) {
		mod->fprintf(srcfile_module_proxy_header,
			"		void trigger(void);\n",
			evtname);
	} else {
		mod->fprintf(srcfile_module_proxy_header,
			"		void trigger(",
			evtname);
		list_item *subitem = NodeList().getnext();
		bool bfirst = true;
		for (; subitem != &NodeList(); subitem = subitem->getnext())
		{
			TVariableObject *obj = LIST_ENTRY(TVariableObject, m_OwnerList, subitem);
			std::string tmp;
			variable_full_name(mod, obj, tmp);
			if (bfirst) {
				mod->fprintf(srcfile_module_proxy_header,
					"%s %s", tmp.c_str(), obj->getname().c_str());
				bfirst = false;
			} else {
				mod->fprintf(srcfile_module_proxy_header,
					", %s %s", tmp.c_str(), obj->getname().c_str());
			}
		}
		mod->fputs(srcfile_module_proxy_header,
			");\n");
	}
	mod->fputs(srcfile_module_proxy_header, "	};\n};\n\n");
	return 0;
}

int event_object_java_code_generator::generate_event_source(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* modname = mod->getname().c_str();
	const char* evtname = getname().c_str();

	string content = "	@RPC_event(\"%s.%s\")\n";
	mod->fprintf(evtname, scrfile_module_event_source, 
		content.c_str(),modname,evtname);
	if (has_param()) {
		content = "	@RPC_function(iparam = %s.___%s.class)\n";
		mod->fprintf(evtname, scrfile_module_event_source, 
		content.c_str(),modname,evtname);
	}
	if (!has_param()) {
		content = "	public void %s() ";
		mod->fprintf(evtname, scrfile_module_event_source, content.c_str(), 
			evtname);
	} else {
		content = "	public void %s(";
		mod->fprintf(evtname, scrfile_module_event_source, content.c_str(), 
			evtname);
		list_item *subitem = NodeList().getnext();
		std::string tmp;
		bool bfrist = true;
		for (; subitem != &NodeList(); subitem = subitem->getnext())
		{
			auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, subitem);
			if (bfrist) {
				content = "@RPC_param(name = \"%s\", type = RPCParamType.in) %s.%s %s";
				
				bfrist = false;
			} else {
				content = ",@RPC_param(name = \"%s\", type = RPCParamType.in) %s.%s %s";
			}
			string ret = "";
			generate_source_call_method_var(mod,obj,ret);
			mod->fprintf(evtname, scrfile_module_event_source, 
					content.c_str(), obj->getname().c_str(),
					modname, ret.c_str(), obj->getname().c_str());
		}
		mod->fputs(evtname, scrfile_module_event_source, ");");
	}
	mod->fputs(evtname, scrfile_module_event_source, "\n	{\n\n	}\n\n");
	return 0;
}

int event_object_java_code_generator::generate_source_call_method_var(
	module* cmod, TVariableObject* var, string& ret)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	ret.clear();
	int retval = 1;

	std::string tmp;
	variable_full_name(cmod, var, tmp,
		false, true);
	ret = tmp;
	return retval;
}

int event_object_java_code_generator::generate_event_source_skeleton(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* modname = mod->getname().c_str();
	const char* evtname = getname().c_str();

	if(!has_param()) {
		mod->fprintf(scrfile_module_skeleton_source,
			"void %s::trigger(void) {\n",
			evtname);
	} else {
		mod->fprintf(scrfile_module_skeleton_source,
			"void %s::trigger(",
			evtname);
		list_item *subitem = NodeList().getnext();
		bool bfirst = true;
		for (; subitem != &NodeList(); subitem = subitem->getnext())
		{
			TVariableObject *obj = LIST_ENTRY(TVariableObject, m_OwnerList, subitem);
			std::string tmp;
			variable_full_name(mod, obj, tmp);
			if (bfirst) {
				mod->fprintf(scrfile_module_skeleton_source,
					"%s %s", tmp.c_str(), obj->getname().c_str());
				bfirst = false;
			} else {
				mod->fprintf(scrfile_module_skeleton_source,
					", %s %s", tmp.c_str(), obj->getname().c_str());
			}
		}
		mod->fputs(scrfile_module_skeleton_source,
			") {\n");
	}
	if(has_param()) {
		mod->fprintf(scrfile_module_skeleton_source,
			"	::%s_pbf::___%s inparam;\n",
			modname, evtname);
		list_item *subitem = NodeList().getnext();
		for (; subitem != &NodeList(); subitem = subitem->getnext())
		{
			TVariableObject *obj = LIST_ENTRY(TVariableObject, m_OwnerList, subitem);
			std::string tmp;
			generate_source_method_body_inparam(mod, obj, tmp);
			mod->fprintf(scrfile_module_skeleton_source,
				"	%s\n", tmp.c_str());
		}
	}
	if(has_param()) {
		mod->fprintf(scrfile_module_skeleton_source,
			"	trigger_event(\"%s.%s\", &inparam);\n"
			"}\n\n",
			modname, evtname);
	} else {
		mod->fprintf(scrfile_module_skeleton_source,
			"	trigger_event(\"%s.%s\", nullptr);\n"
			"}\n\n",
			modname, evtname);
	}
	return 0;
}

int event_object_java_code_generator::generate_event_protobuf(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* modname = mod->getname().c_str();
	const char* evtname = getname().c_str();

	if(!has_param())
		return 0;

	list_item *subitem = NodeList().getnext();
	int first_inout_param = 0;
	mod->fprintf(mod->getname().c_str(), srcfile_module_protobuf,
		"message ___%s\n{\n", evtname);
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		TVariableObject *obj = LIST_ENTRY(TVariableObject, m_OwnerList, subitem);
		std::string tmp;
		variable_protobuf_type_name(mod, obj, tmp);
		mod->fprintf(mod->getname().c_str(), srcfile_module_protobuf,
			"\t%s %s = %u;\n", tmp.c_str(),
			obj->getname().c_str(), ++first_inout_param);
	}
	mod->fputs(mod->getname().c_str(), srcfile_module_protobuf,
		"}\n\n");
	return 0;
}

int event_object_java_code_generator::generate_source_method_body_inparam(
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
		ret += "inparam.set_";
		ret += var->getname();
		ret +="(" +var->getname();
		ret += ");";
		return 0;
		break;
	case eBasicObjType_Stream:
	case eBasicObjType_Enum:
		break;
	case eBasicObjType_Interface: {
		ret += "inparam.set_";
		ret += var->getname();
		ret +="((size_t)& " +var->getname();
		ret += ");\n";
		return 0;	
		} break;
	case eBasicObjType_Typedefed:
		// todo:
		break;

	case eBasicObjType_UserDefined: {
		ret += var->getname();
		ret += ".copyto(*inparam.mutable_";
		ret += var->getname() + "());";
		return 0;
	}	break;
	}
	return -1;
}

bool event_object_java_code_generator::has_param()
{
	list_item *subitem = NodeList().getnext();
	for (; subitem != &NodeList(); subitem = subitem->getnext())
	{
		auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, subitem);
		if (obj && (obj->_basic_type != eBasicObjType_Unknown))
			return true;
	}
	return false;
}
