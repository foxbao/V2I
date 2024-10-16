/** @file inc/cgcpp-common.h
 * implementation of c++ code generation of common features for zrpc
 */

#include <assert.h>
#include <string.h>

#include "error.h"
#include "parser.h"
#include "gencommon.h"
#include "utilities.h"
#include "config.h"

using namespace std;
using namespace utilities;

const char* protobuf_types[] = {
	nullptr,	// unknown
	"bool",		// bool
	"uint32",	// uint8
	"int32",	// int16
	"int32",	// int32
	"int64",	// long
	"uint32",	// uint32
	"double",	// Float
	"bytes",	// stream
	"string",	// string
	"int32",	// enum - special handling
	nullptr,	// interface - shall not have this
	nullptr,	// typedefed - special handling
	nullptr,	// userdefined - special handling
};

void method_retval_protobuf_type_name(
	module* cmod, TInterfaceNode* node, string& ret)
{
	TVariableObject varobj;

	varobj._basic_type = node->m_Data.method.m_eRetValType;
	varobj.m_pIFType = node->m_Data.method.m_pRetValIFObject;
	varobj.m_pRefType = node->m_Data.method.m_pRetValTypedefType;
	varobj.m_pEnumDefType = node->m_Data.method.m_pRetValEnumDef;
	varobj.m_pUserDefType = node->m_Data.method.m_pRetValUserDefType;
	variable_protobuf_type_name(cmod, &varobj, ret);
}

void variable_protobuf_type_name(module* cmod, TVariableObject* var, string& ret)
{
	const char** types = protobuf_types;
	ret.clear();
	if (is_array(var->m_ArrayType)) {
		ret += "repeated ";
	}

	const char* type = types[var->_basic_type];
	if (type) {
		ret += type; return;
	}

	assert(eBasicObjType_Unknown != var->_basic_type);
	switch (var->_basic_type)
	{
	case eBasicObjType_Enum:
		ret += "int32";
		break;
	case eBasicObjType_Interface:
		ret += "int64";
		break;
	case eBasicObjType_Typedefed:
		// todo:
		break;

	case eBasicObjType_UserDefined: {
		auto* usrdef = var->m_pUserDefType;
		auto* mod = usrdef->GetModule();
		assert(nullptr != mod);
		if (cmod != mod) {
			ret += mod->getname();
			ret += "_pbf.";
		}
		ret += usrdef->getname();
	}	break;
	}
}