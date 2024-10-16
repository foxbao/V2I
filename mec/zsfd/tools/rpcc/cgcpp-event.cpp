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

event_object_cpp_code_generator::event_object_cpp_code_generator(
	module *mod, const string name)
	: TEventObject(mod, name)
{
}

event_object_cpp_code_generator::~event_object_cpp_code_generator()
{
}

bool event_object_cpp_code_generator::generate_code(void)
{
	if (test_flags(grammar_object_flags_code_generated))
		return true;
	set_flags(grammar_object_flags_code_generated);
	
	if (!generate_related_object_code()) {
		return false;
	}

	if (generate_event_protobuf())
		return false;
	if (generate_event_header())
		return false;
	if (generate_event_header_skeleton())
		return false;
	if (generate_event_source())
		return false;
	if (generate_event_source_skeleton())
		return false;
	return true;
}

int event_object_cpp_code_generator::generate_event_header(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* modname = mod->getname().c_str();
	const char* evtname = getname().c_str();

	mod->fprintf(srcfile_module_proxy_header,
		"class ZIDL_PREFIX %s : public rpcevent\n"
		"{\n"
		"public:\n"
		"	%s(bool auto_listen = false) {\n"
		"		if (!auto_listen) return;\n"
		"		start_listen();\n"
		"	}\n\n"

		"	virtual ~%s() {\n"
		"		rpcevent::end_listen(\"%s.%s\");\n"
		"	}\n\n",
		evtname,
		evtname,
		evtname,
		modname, evtname);

	mod->fprintf(srcfile_module_proxy_header,
		"public:\n"
		"	void start_listen(void);\n\n"

		"	void end_listen(void) {\n"
		"		rpcevent::end_listen(\"%s.%s\");\n"
		"	}\n\n"

		"	void force_trigger_last_event(void) {\n"
		"		rpcevent::force_trigger_last_event(\n"
		"			\"%s.%s\");\n"
		"	}\n\n",
		modname, evtname,
		modname, evtname);

	if(!has_param()) {
		mod->fprintf(srcfile_module_proxy_header,
			"	virtual void on_%s_triggered(void) {\n"
			"	}\n\n",
			evtname);
	} else {
		mod->fprintf(srcfile_module_proxy_header,
			"	virtual void on_%s_triggered(",
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
			")\n	{\n	}\n\n");
	}

	mod->fprintf(srcfile_module_proxy_header,
		"protected:\n"
		"	void on_triggered(pbf_wrapper event);\n\n"

		"private:\n"
		"	/* disable evil constructors */\n"
		"	%s(const %s&);\n"
		"	void operator=(const %s&);\n"
		"};\n\n", 
		evtname, evtname,
		evtname);

	return 0;
}

int event_object_cpp_code_generator::variable_full_name(
	module* cmod, TVariableObject* var, string& ret, bool skeleton, bool defined)
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
			return 0;
		}
	} else {
		type = variable_types[var->_basic_type];
		if (type) {
			ret += type;
			if (var->_basic_type == eBasicObjType_String && !defined)
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
			if (skeleton){
				ret += "::";
				ret += mod->getname();
				ret += "::";
			}
		}
		ret += interf->getname();
		if (!defined)
			ret += "&";
		} break;
	case eBasicObjType_Typedefed:
		// todo:
		break;

	case eBasicObjType_UserDefined: {
		auto* usrdef = var->m_pUserDefType;
		auto* mod = usrdef->GetModule();
		assert(nullptr != mod);
		if (mod != cmod) {
			ret += "::";
			ret += mod->getname();
			ret += "::";
		}
		if (is_array(var->m_ArrayType))
			ret += "array::";
		ret += usrdef->getname();
		if (!defined)
			ret += "&";
	}	break;
	}

	return 0;
}

int event_object_cpp_code_generator::generate_event_header_skeleton(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* modname = mod->getname().c_str();
	const char* evtname = getname().c_str();

	mod->fprintf(srcfile_module_proxy_header,
		"namespace skeleton {\n"
		"	class ZIDL_PREFIX %s\n"
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

int event_object_cpp_code_generator::generate_event_source(void)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	const char* modname = mod->getname().c_str();
	const char* evtname = getname().c_str();

	mod->fprintf(srcfile_module_proxy_structs,
		"void %s::start_listen(void)\n"
		"{\n"
		"	pbf_wrapper event_data;\n",
		evtname);

	if (has_param()) {
		mod->fprintf(srcfile_module_proxy_structs,
			"	event_data = new ::%s::struct_wrapper::___%s_wrapper;\n",
			modname, evtname);
		mod->fprintf(srcfile_module_proxy_internal_header,
			"class ___%s_wrapper : public pbf_wrapper_object\n"
			"{\n"
			"public:\n"
			"	google::protobuf::Message* get_pbfmessage(void) {\n"
			"		return &_protobuf_object;\n"
			"	}\n"

			"private:\n"
			"	%s_pbf::___%s _protobuf_object;\n"
			"};\n",
			evtname,
			mod->getname().c_str(),
			evtname);
	}
	mod->fprintf(srcfile_module_proxy_structs,
		"	rpcevent::start_listen(\"%s.%s\",\n"
		"		event_data);\n"
		"}\n\n",
		modname, evtname);
	mod->fprintf(srcfile_module_proxy_structs,
		"void %s::on_triggered(pbf_wrapper event)\n"
		"{\n",
		evtname);
	if (!has_param()) {
		mod->fprintf(srcfile_module_proxy_structs,
			"	on_%s_triggered();\n",
			evtname);
	} else {
		mod->fprintf(srcfile_module_proxy_structs,
		"	if (!event.get()) {\n"
		"		return;\n"
		"	}\n"
		"	auto* inp = zas_downcast(google::protobuf::Message,	\\\n"
		"		::%s_pbf::___%s,	\\\n"
		"		event->get_pbfmessage());\n",
		mod->getname().c_str(), evtname);
		list_item *subitem = NodeList().getnext();
		std::string tmp;
		for (; subitem != &NodeList(); subitem = subitem->getnext())
		{
			auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, subitem);
			if (generate_source_call_method_var(mod, obj, tmp))
				break;
			mod->fprintf(srcfile_module_proxy_structs,
			"	%s\n", tmp.c_str());
		}
		mod->fprintf(srcfile_module_proxy_structs,
			"	on_%s_triggered(",
			evtname);
		subitem = NodeList().getnext();
		bool bfrist = true;
		for (; subitem != &NodeList(); subitem = subitem->getnext())
		{
			auto *obj = LIST_ENTRY(TVariableObject, m_OwnerList, subitem);
			if (bfrist) {
				mod->fprintf(srcfile_module_proxy_structs,
					"%s", obj->getname().c_str());
				bfrist = false;
			} else {
				mod->fprintf(srcfile_module_proxy_structs,
					", %s",  obj->getname().c_str());
			}
		}
		mod->fputs(srcfile_module_proxy_structs, ");\n");
	}
	mod->fputs(srcfile_module_proxy_structs, "}\n\n");


	return 0;
}

int event_object_cpp_code_generator::generate_source_call_method_var(
	module* cmod, TVariableObject* var, string& ret)
{
	module *mod = GetModule();
	if (nullptr == mod) return 1;
	ret.clear();
	int retval = 1;

	std::string tmp;
	variable_full_name(cmod, var, tmp,
		false, true);

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
		ret += " = inp->get_";
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
		ret += "(inp->get_";
		ret += var->getname();
		ret += "(), inp->get_client_name(), inp->get_inst_name());";
		retval = 0;	
		} break;
	case eBasicObjType_Typedefed:
		// todo:
		break;

	case eBasicObjType_UserDefined: {
		ret += tmp + " ";
		ret += var->getname();
		ret += "(event, inp->mutable_";
		ret += var->getname();
		ret += "());";
		retval = 0;
	}	break;
	}
	return retval;
}

int event_object_cpp_code_generator::generate_event_source_skeleton(void)
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

int event_object_cpp_code_generator::generate_event_protobuf(void)
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

int event_object_cpp_code_generator::generate_source_method_body_inparam(
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

bool event_object_cpp_code_generator::has_param()
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
