#ifndef __CXX_RPCC_H__
#define __CXX_RPCC_H__

#define VERSION "V0.110"

#define PATH_LENGTH 300


#define _MAXUCHAR	 (0xFF)
#define _MAXUINT     ((unsigned int)~((unsigned int)0))
#define _MAXINT      ((int)(_MAXUINT >> 1))
#define _MININT      ((int)~_MAXINT)
#define _MAXSHORT	(32767)
#define _MINSHORT	(-32768)
#define _MAXLONG	(9223372036854775807)
#define _MINLONG	(-9223372036854775808)


enum number_type { UChar, UInt, Int, Float, Long, Short };

enum srcfile_type
{
	srcfile_unknown = 0,
	srcfile_module_proxy_header,
	srcfile_module_proxy_internal_header,
	srcfile_module_protobuf,
	srcfile_module_proxy_structs,
	scrfile_module_proxy_source,
	scrfile_module_skeleton_source,
	scrfile_module_source,
	scrfile_module_source_virtual,
	scrfile_module_enum_source,
	scrfile_module_event_source,
	scrfile_module_final_source,
	scrfile_module_observer_source,
	scrfile_module_observer_interface_source,
	SrcFileHdr,
	SrcFileInlineHdr,
	SrcFileProxy,
	SrcFileSkeleton,
	SrcFileUser,
};

enum EIFNodeType
{
	eIFNodeType_Unknown = 0,
	eIFNodeType_Attribute,
	eIFNodeType_Method,
};

enum EIdentifierType
{
	eIdentifier_Unknown,
	eIdentifier_EnumName,
	eIdentifier_ModuleName,
	eIdentifier_MethodName,
	eIdentifier_TypedefName,
	eIdentifier_EnumNodeName,
	eIdentifier_ConstItemName,
	eIdentifier_InterfaceName,
	eIdentifier_UserDefTypeName,
	eIdentifier_MethodVariableName,
	eIdentifier_MemberVariableName,
	eIdentifier_EventName,
	eIdentifier_EventVariableName,
};

enum EArrayType
{
	array_type_unknown,
	array_type_fixed,
	array_type_variable,
	eArrayType_NotArray,
};

#define is_array(t)	\
	(array_type_fixed == (t.eArrayType) || array_type_variable == (t.eArrayType))

enum basic_objtype
{
	eBasicObjType_Unknown = 0,
	basic_objtype_boolean,
	basic_objtype_uint8,
	eBasicObjType_Int16,//short
	eBasicObjType_Int32,
	eBasicObjType_Int64,//long
	eBasicObjType_UInt32,
	eBasicObjType_Float,
	eBasicObjType_Stream,
	eBasicObjType_String,
	eBasicObjType_Enum,
	eBasicObjType_Interface,
	eBasicObjType_Typedefed,
	eBasicObjType_UserDefined,
};

#define IsBasicObjectType(t)	\
	(int(t) >= int(basic_objtype_boolean) && int(t) <= int(eBasicObjType_String))

#define IsBasicNumberObjectType(t)	\
	(int(t) >= int(basic_objtype_uint8) && int(t) <= int(eBasicObjType_Float))

enum ETokenType
{
	tokentype_unknown = 0,
	eTokenType_Basic,
	eTokenType_Operator,
	eTokenType_Reserved,
	eTokenType_Identifier,
	eTokenType_Number,
};

enum ETokenID
{
	eTokenID_Unknown = 0,
	eTokenID_Module,				// module
	eTokenID_Interface,				// Interface
	eTokenID_LeftBracket,			// (
	eTokenID_RightBracket,			// )
	eTokenID_LeftCurlyBracket,		// {
	eTokenID_RightCurlyBracket,		// }
	eTokenID_Comma,					// ,
	eTokenID_Semicolon,				// ;
	eTokenID_Colon,					// :
	eTokenID_Add,					// +
	eTokenID_Sub,					// -
	eTokenID_Mul,					// *
	eTokenID_Div,					// /
	eTokenID_DoubleColon,			// ::
	eTokenID_Equal,					// =
	eTokenID_LeftSquareBracket,		// [
	eTokenID_RightSquareBracket,	// ]
	eTokenID_Import,				// import
	eTokenID_Const,					// const
	eTokenID_Unsigned,				// unsigned
	eTokenID_Int,					// int
	eTokenID_Float,					// float
	eTokenID_String,				// string
	eTokenID_Typedef,				// typedef
	eTokenID_Struct,				// struct
	eTokenID_Void,					// void
	eTokenID_In,					// in
	eTokenID_Inout,					// inout
	eTokenID_Out,					// out
	eTokenID_Boolean,				// boolean
	eTokenID_Octet,					// octet
	eTokenID_Enum,					// enum
	eTokenID_Property,				// property
	eTokenID_Attribute,				// attribute
	eTokenID_Singleton,				// singleton
	eTokenID_Observable,			// observable
	eTokenID_True,					// true
	eTokenID_False,					// false
	eTokenID_Virtual,				// virtual
	eTokenID_Event,					// event
	eTokenID_Stream,				// stream
	eTokenID_Number_Int,
	eTokenID_Number_Short,
	eTokenID_Number_Long,
	eTokenID_Number_UInt,
	eTokenID_Number_Float,
	eTokenID_Short,					// short
	eTokenID_Long,					// long
	eTokenID_User,					// user defined
};


enum EParameterFunc{
	eParameterFunc_Unknown = 0,
	eParameterFunc_Normal,
	eParameterFunc_Help,
	eParameterFunc_Version,
	eParameterFunc_Log,
};


#define DeclareFlagOperations()	\
	void set_flags(unsigned int flags) {	\
		m_Flags |= flags;	\
	}	\
	\
	bool test_flags(unsigned int flags) {	\
		return ((m_Flags & flags) == flags) ? true : false;	\
	}	\
	\
	void clear_flags(unsigned int flags) {	\
		m_Flags &= ~flags;	\
	}

#define PRCC_HELP_COMMENT \
"\n***************************\n\
Help Info for rpcc:\n\
***************************\n\
current support cmd:\n\
[--help]: Get help info.\n\
[--version]: Get current version.\n\
[--java]: generate java code\n\
[--cpp]: generate c++ code\n\
[--package-name]: rpc service package name\n\
[--service-name]: rpc service name\n\
[--input-file]=[filename]: Set input file (Necessary)\n\
[--output-dir]=[dirname]: Set output dir (default as./)\n\
\n\
**IMPORTANT NOTE:\n\
if rpcc is in /root/a,\n\
and input is /root/b/test1.idl,\n\
output default is /root/b/\n\
\n\
Normal use example:\n\
rpcc --input-file=test.idl --output-dir=./test\n"

#endif
/* EOF */
