#include <assert.h>
#include "parser.h"
#include "utilities.h"

using namespace utilities;

grammar_parser::grammar_parser(token_parser& p, config &c)
: m_cTokenParser(p)
, m_cConfig(c)
, expression(p, m_cErrInfo)
, m_cErrInfo(p)
, m_pGlobalScope(NULL)
, m_pCurrentScope(NULL)
{
	// create the global module as global namespace
	m_pGlobalScope = module::create_instance("global", this, m_cConfig);
	assert(NULL != m_pGlobalScope);

	m_pGlobalScope->Add(m_cModuleHeader);
	m_pGlobalScope->set_flags(grammar_object_flags_globalscope);

	m_pCurrentScope = m_pGlobalScope;
}

grammar_parser::~grammar_parser()
{
	ReleaseAllModuleObject(m_cModuleHeader);
	m_pCurrentScope = NULL;
	m_pGlobalScope = NULL;
}

bool grammar_parser::ParseArrayStatement(TArrayTypeObject& obj)
{
	// set a default value
	obj.eArrayType = eArrayType_NotArray;
	obj.ArraySize = 0;

	token tkn = m_cTokenParser.GetToken();
	if (eTokenID_LeftSquareBracket == tkn)
	{
		tkn = m_cTokenParser.GetToken();
		if (eTokenID_RightSquareBracket == tkn)
		{
			obj.eArrayType = array_type_variable;
			obj.ArraySize = 0;
		}
		else
		{
			double value;
			m_cTokenParser.TokenPushBack();
			if (AnalyzeExpression()
				&& ConvertResultTo(eBasicObjType_UInt32, value))
			{
				if (value < 0.0 || Double_IsZero(value))
				{
					m_cErrInfo.PrintError(eErrID_InvalidArraySize);
					m_cTokenParser.OmitCurrentLine();
					return false;
				}
				obj.eArrayType = array_type_fixed;
				obj.ArraySize = (unsigned int)value;
			}
			else
			{
				m_cTokenParser.OmitCurrentLine();
				return false;
			}
			tkn = m_cTokenParser.GetToken();
			if (eTokenID_RightSquareBracket != tkn)
			{
				m_cErrInfo.PrintError(eErrID_ExpectRightSquareBracket);
				m_cTokenParser.OmitCurrentLine();
				return false;
			}	
		}
	}
	else
	{
		m_cTokenParser.TokenPushBack();
		obj.eArrayType = eArrayType_NotArray;
		obj.ArraySize = 0;
	}
	return true;
}

// handle the sub-statements inside the module
bool grammar_parser::ParseModuleSubStatement(void)
{
	token tkn = m_cTokenParser.GetToken();
	while (eTokenID_Unknown != tkn)
	{
		switch (tkn.GetID())
		{
		case eTokenID_RightCurlyBracket:
			return true;

		case eTokenID_Import:
			ParseImportStatement();
			break;

		case eTokenID_Typedef:
			ParseTypedefStatement();
			break;

		case eTokenID_Const:
			ParseConstStatement();
			break;

		case eTokenID_Enum:
			ParseEnumStatement();
			break;

		case eTokenID_Struct:
			ParseStructureStatement();
			break;

		case eTokenID_Interface:
			ParseInterfaceStatement();
			break;

		case eTokenID_Event:
			ParseEventStatement();
			break;

		default:
			m_cErrInfo.PrintError(eErrID_UnrecgnizedStatement,
				tkn.getname().c_str());
			m_cTokenParser.OmitCurrentLine();
			break;
		}
		tkn = m_cTokenParser.GetToken();
	}
	m_cErrInfo.PrintError(eErrID_ReachEOFBeforeMatchingRightCurlyBracket);
	return false;
}

void grammar_parser::Parse(void)
{
	token tkn = m_cTokenParser.GetToken();
	while (eTokenID_Unknown != tkn)
	{
		switch (tkn.GetID())
		{
		case eTokenID_Import:
			ParseImportStatement();
			break;

		case eTokenID_Typedef:
			ParseTypedefStatement();
			break;

		case eTokenID_Const:
			ParseConstStatement();
			break;

		case eTokenID_Enum:
			ParseEnumStatement();
			break;

		case eTokenID_Module:
			ParseModuleStatement();
			break;

		case eTokenID_Struct:
			ParseStructureStatement();
			break;

		default:
			m_cErrInfo.PrintError(eErrID_UnrecgnizedStatement,
				tkn.getname().c_str());
			m_cTokenParser.OmitCurrentLine();
			break;
		}
		tkn = m_cTokenParser.GetToken();
	}

	for (int i = 0; i < HASH_MAX; ++i)
	{
		list_item &h = m_cModuleHeader[i];
		list_item *pItem = h.getnext();
		for (; pItem != &h; pItem = pItem->getnext())
		{
			module *obj = LIST_ENTRY(module, m_OwnerList, pItem);
			if (!obj->generate_code()) {
				printf("error: fail in generating code.\n");
				return;
			}
		}
	}
	ReleaseAllModuleObject(m_cModuleHeader);
}

void grammar_parser::ParseImportStatement(void)
{
	token tkn = m_cTokenParser.GetToken();

	int start = 0, end = tkn.getname().length();
	
	if (tkn[0] != '\"')
		m_cErrInfo.PrintError(eErrID_ImportFileNameWithoutLeftQuotation);
	else start = 1;

	if (tkn[-1] != '\"')
		m_cErrInfo.PrintError(eErrID_ImportFileNameWithoutRightQuotation);
	else { end -= start; end--; }

	string filename;
	filename.assign(tkn.getname(), start, end);

	// see if the file exists in user specified path list
	if (!m_cConfig.FindFileInConfiguratedDir(filename))
	{
		m_cErrInfo.PrintError(eErrID_ImportFileNotFound, filename.c_str());
		m_cTokenParser.OmitCurrentLine();
		return;
	}

	string fullname = utilities::CanonicalizeFileName(filename);

	// see if the file already be imported
	if (m_cTokenParser.get_filestack().IsFullNameExist(fullname))
	{
		m_cTokenParser.OmitCurrentLine();
		return;
	}

	// import the current file
	m_cTokenParser.get_filestack().push(fullname);
}

void grammar_parser::ParseTypedefStatement(void)
{
	basic_objtype eType;
	interface_object* pIFObject = NULL;
	typedef_object* pTypedefType = NULL;
	enumdef_object* pEnumDefType = NULL;
	userdef_type_object* pUserDefType = NULL;
	if (!AnalyzeVariableType(eType, &pTypedefType, &pUserDefType, &pEnumDefType, &pIFObject))
	{
		m_cTokenParser.OmitCurrentLine();
		return;
	}
/*
	if( 
		eBasicObjType_UInt32 != eType &&	//unsigned int
		eBasicObjType_Int32 != eType &&		//int 
		eBasicObjType_Int16 != eType &&		//short will report error other place
		eBasicObjType_Int64 != eType &&		//long will report error other place
		eBasicObjType_Float != eType &&
		eBasicObjType_String != eType &&
		basic_objtype_boolean != eType &&
		basic_objtype_uint8 != eType &&		//byte/char/octet
		eBasicObjType_Enum != eType	
		)
	{
		//[csy 20161110] typedef of Non-basic type
		m_cErrInfo.PrintError(eErrID_TypeDefOnlyBasicTypeNotArrayAllowed);
		m_cTokenParser.OmitCurrentLine();
		return;
	}
*/
	// type int [yourtype][max];
	token tkn = m_cTokenParser.GetToken();
	if (eTokenType_Identifier != tkn)
	{
		m_cErrInfo.PrintError(eErrID_IdentifierExpectedForThisDeclaration,
			tkn.getname().c_str());
		m_cTokenParser.OmitCurrentLine();
		return;
	}
	string identifier = tkn;
	EIdentifierType eIdType;
	if (IsIdentifierUsed(tkn, eIdType))
	{
		m_cErrInfo.PrintError(eErrID_IdentifierRedefined, tkn.getname().c_str(),
			GetDescriptionOfIdentifierType(eIdType));
		m_cTokenParser.OmitCurrentLine();
		return;
	}

	// create the typedef object
	typedef_object* pObj;
	switch (eType)
	{
	case eBasicObjType_Unknown:
		pObj = NULL;
		break;

	case eBasicObjType_Typedefed:
		pObj = typedef_object::create_instance(m_pCurrentScope, identifier, pTypedefType);
		break;

	case eBasicObjType_UserDefined:
		pObj = typedef_object::create_instance(m_pCurrentScope, identifier, pUserDefType);
		break;

	case eBasicObjType_Enum:
		pObj = typedef_object::create_instance(m_pCurrentScope, identifier, pEnumDefType);
		break;

	case eBasicObjType_Interface:
		pObj = typedef_object::create_instance(m_pCurrentScope, identifier, pIFObject);
		break;

	// basic types
	default: pObj = typedef_object::create_instance(m_pCurrentScope, identifier, eType);
		break;
	}
	assert(NULL != pObj && NULL != m_pCurrentScope);

	// for array "[]" handling
	TArrayTypeObject ArrayType;
	if (!ParseArrayStatement(ArrayType))
	{
		delete pObj;
		return;
	}
	/*
	if ( eArrayType_NotArray != ArrayType.eArrayType )
	{
		//[csy 20161110] typedef array not allowed
		m_cErrInfo.PrintError(eErrID_TypeDefOnlyBasicTypeNotArrayAllowed);
		m_cTokenParser.OmitCurrentLine();
		return;
	}*/
	
	pObj->SetArrayType(ArrayType);

	// handle ';'
	tkn = m_cTokenParser.GetToken();
	if (eTokenID_Semicolon != tkn)
	{
		m_cTokenParser.TokenPushBack();
		m_cErrInfo.PrintError(eErrID_ExpectSemiColon);
		delete pObj;
		return;
	}

	// check if it is a 2-dimension array, which is not supported
	basic_objtype eOriginalType;
	pObj->GetOriginalType(eOriginalType, ArrayType, NULL, NULL);
	if (eBasicObjType_Unknown == eOriginalType)
	{
		m_cErrInfo.PrintError(eErrID_2DimemsionArrayNotSupported);
		delete pObj;
		return;
	}
	pObj->Add(m_pCurrentScope->TypedefHeader());
}

number_type grammar_parser::ConvertBasicTypeToNumberType(basic_objtype eType)
{
	number_type ret;

	switch (eType)
	{
	case eBasicObjType_Int32: ret = Int; break;
	case eBasicObjType_UInt32: ret = UInt; break;
	case eBasicObjType_Float: ret = Float; break;
	
	// we are no longer support uint8 any more, from now on,
	// we treat uint8 as uint32
	case basic_objtype_uint8: ret = UInt; break;
	default: assert(0);	break;
	}
	return ret;
}

void grammar_parser::ReportErrorVariableType(void)
{
	m_cErrInfo.PrintError(eErrID_InvalidVariableType,
		m_cTokenParser.GetToken().getname().c_str(), "only 'int', 'unsigned int'"
		", 'octet', and 'float' is allowed as a type for const variable declaration");
	m_cTokenParser.OmitCurrentLine();

}

void grammar_parser::ParseEnumStatement(void)
{
	// enum [name] { abc = 3, };
	token tkn = m_cTokenParser.GetToken();
	if (eTokenType_Identifier != tkn)
	{
		m_cErrInfo.PrintError(eErrID_ExpectEnumName);
		m_cTokenParser.OmitCurrentLine();
		return;
	}

	EIdentifierType eIdType;
	if (IsIdentifierUsed(tkn, eIdType))
	{
		m_cErrInfo.PrintError(eErrID_IdentifierRedefined, tkn.getname().c_str(),
			GetDescriptionOfIdentifierType(eIdType));
		m_cTokenParser.OmitCurrentLine();
		return;
	}
	string cName = tkn.getname();
	assert(NULL != m_pCurrentScope);
	enumdef_object *pEnumObject = enumdef_object::create_instance(m_pCurrentScope, cName);
	assert(NULL != pEnumObject);

	// enum name [{] abc = 3, };
	tkn = m_cTokenParser.GetToken();
	if (eTokenID_LeftCurlyBracket != tkn)
	{
		m_cErrInfo.PrintError(eErrID_ExpectLeftCurlyBracket);
		return;
	}

	bool bHasComma = true;
	int CurrEnumNodeValue = 0;
	for (;;)
	{
		tkn = m_cTokenParser.GetToken();
		if (eTokenID_RightCurlyBracket == tkn)
			break;

		if (!bHasComma)
			m_cErrInfo.PrintError(eErrID_ExpectComma);

		// enum name { [abc] = 3, };
		if (eTokenType_Identifier != tkn)
		{
			m_cErrInfo.PrintError(eErrID_IdentifierExpectedForThisDeclaration,
				tkn.getname().c_str());
			m_cTokenParser.OmitCurrentLine();
			continue;
		}
		string cEnumNodeName = tkn.getname();
		if (m_pCurrentScope->Find(cEnumNodeName))
		{
			m_cErrInfo.PrintError(eErrID_IdentifierRedefined, cEnumNodeName.c_str(),
				GetDescriptionOfIdentifierType(eIdentifier_EnumNodeName));
			m_cTokenParser.OmitCurrentLine();
			continue;
		}

		// see if there is a "=" (enum name { abc [=] 3, };)
		tkn = m_cTokenParser.GetToken();
		if (eTokenID_Equal == tkn)
		{
			// handle the number (enum name { abc = [3], };)
			double value;
			if (!(AnalyzeExpression() && ConvertResultTo(eBasicObjType_Int32, value)))
			{
				m_cTokenParser.OmitCurrentLine();
				continue;
			}
			CurrEnumNodeValue = (int)value;
			tkn = m_cTokenParser.GetToken();
		}

		// create the enum node
		TEnumNode *pEnumNode = new TEnumNode(cEnumNodeName, CurrEnumNodeValue);
		assert(NULL != pEnumNode);
		pEnumObject->AddNode(pEnumNode);

		// handle the "," (enum name { abc = 3[,] };)
		if (eTokenID_Comma != tkn)
		{
			bHasComma = false;
			m_cTokenParser.TokenPushBack();
		}
		else bHasComma = true;

		CurrEnumNodeValue++;
	}

	pEnumObject->Add(m_pCurrentScope->EnumDefHeader());

	// handle ';'
	tkn = m_cTokenParser.GetToken();
	if (eTokenID_Semicolon != tkn)
	{
		m_cTokenParser.TokenPushBack();
		m_cErrInfo.PrintError(eErrID_ExpectSemiColon);
		return;
	}
}

void grammar_parser::ParseConstStatement(void)
{
	basic_objtype eType;
	typedef_object* pTypedefType = NULL;
	if (!AnalyzeVariableType(eType, &pTypedefType, NULL, NULL, NULL))
		return;

	// all the following condition is NOT valid for a const statement
		// 1) is a user defined type (struct {})
	if (eBasicObjType_UserDefined == eType)
	{
		m_cTokenParser.TokenPushBack();
		ReportErrorVariableType();
		return;
	}
	
		// 2) is a typedef type but its original type is not one in {int/uint/float}
	else if (eBasicObjType_Typedefed == eType)
	{
		if (NULL == pTypedefType)
		{
			m_cTokenParser.TokenPushBack();
			ReportErrorVariableType();
			return;
		}

		TArrayTypeObject ArrayType;
		pTypedefType->GetOriginalType(eType, ArrayType, NULL, NULL);
		if ( (eBasicObjType_Int32 != eType && eBasicObjType_UInt32 != eType &&
			basic_objtype_uint8 != eType && eBasicObjType_Float != eType &&
			eBasicObjType_Int16 != eType && eBasicObjType_Int64 != eType)
			|| is_array(ArrayType))
		{
			m_cTokenParser.TokenPushBack();
			ReportErrorVariableType();
			return;
		}
	}

		// 3) not a basic type of int/uint/uchar/float/long/short
	else if (eBasicObjType_Int32 != eType && eBasicObjType_UInt32 != eType &&
		basic_objtype_uint8 != eType && eBasicObjType_Float != eType &&
		eBasicObjType_Int16 != eType && eBasicObjType_Int64 != eType)
	{
		m_cTokenParser.TokenPushBack();
		ReportErrorVariableType();
		return;
	}

	// get the identifier (const int [abc] = 123)
	token tkn = m_cTokenParser.GetToken();
	if (eTokenType_Identifier != tkn)
	{
		m_cErrInfo.PrintError(eErrID_IdentifierExpectedForThisDeclaration,
		tkn.getname().c_str());
		m_cTokenParser.OmitCurrentLine();
		return;
	}
	EIdentifierType eIdType;
	if (IsIdentifierUsed(tkn, eIdType))
	{
		m_cErrInfo.PrintError(eErrID_IdentifierRedefined, tkn.getname().c_str(),
			GetDescriptionOfIdentifierType(eIdType));
		m_cTokenParser.OmitCurrentLine();
		return;
	}
	string cName = tkn.getname();

	// get the identifier (const int abc [=] 123)
	tkn = m_cTokenParser.GetToken();
	if (eTokenID_Equal != tkn)
	{
		m_cErrInfo.PrintError(errid_expect_equal);
		m_cTokenParser.OmitCurrentLine();
		return;
	}

	// get the value (const int abc = [123])
	double value;
	bool success = (AnalyzeExpression()
		&& ConvertResultTo(eType, value))
		? true : false;

	// handle ';'
	tkn = m_cTokenParser.GetToken();
	if (eTokenID_Semicolon != tkn)
	{
		m_cTokenParser.TokenPushBack();
		m_cErrInfo.PrintError(eErrID_ExpectSemiColon);
		return;
	}

	// create the const item object
	if (success)
	{
		TConstItemObject *pConstObj = TConstItemObject::create_instance(m_pCurrentScope,
			cName, ConvertBasicTypeToNumberType(eType), value);
		assert(NULL != pConstObj);

		pConstObj->SetRefType(pTypedefType);
		assert(NULL != m_pCurrentScope);
		pConstObj->Add(m_pCurrentScope->ConstItemHeader());
	}
}

// handle the parameter variable name in an event
bool grammar_parser::ParseEventParameterVariableName
	(TEventObject *pEvObject, string& cName)
{
	assert(NULL != pEvObject);
	token tkn = m_cTokenParser.GetToken();
	if (eTokenType_Identifier != tkn)
	{
		m_cErrInfo.PrintError(eErrID_ExpectVariableName,
			"'struct' declaration");
		m_cTokenParser.OmitCurrentLine();
		return false;
	}
	EIdentifierType eIdType = eIdentifier_Unknown;
	if (IsIdentifierUsed(tkn, eIdType)
		|| IsIdentifierUsedInCurrentEvent(pEvObject, tkn, eIdType)
		|| pEvObject->getname() == tkn)
	{
		if (eIdentifier_Unknown == eIdType)
			eIdType = eIdentifier_EventName;

		m_cErrInfo.PrintError(eErrID_IdentifierRedefined, tkn.getname().c_str(),
			GetDescriptionOfIdentifierType(eIdType));
		m_cTokenParser.OmitCurrentLine();
		return false;
	}
	cName = tkn.getname();
	return true;
}

// handle the parameter variable name in a method of an interface
bool grammar_parser::ParseIFMethodParameterVariableName
	(interface_object *pObject, TInterfaceNode* pNode, string& cName)
{
	assert(NULL != pObject && NULL != pNode);
	token tkn = m_cTokenParser.GetToken();
	if (eTokenType_Identifier != tkn)
	{
		m_cErrInfo.PrintError(eErrID_ExpectVariableName,
			"'struct' declaration");
		m_cTokenParser.OmitCurrentLine();
		return false;
	}
	EIdentifierType eIdType = eIdentifier_Unknown;
	if (IsIdentifierUsed(tkn, eIdType) || IsIdentifierUsedInCurrentInterface(pObject, tkn, eIdType)
		|| IsIdentifierUsedInCurrentMethod(pNode, tkn, eIdType)
		|| pNode->m_cName == tkn)
	{
		if (eIdentifier_Unknown == eIdType)
			eIdType = eIdentifier_MethodName;

		m_cErrInfo.PrintError(eErrID_IdentifierRedefined, tkn.getname().c_str(),
			GetDescriptionOfIdentifierType(eIdType));
		m_cTokenParser.OmitCurrentLine();
		return false;
	}
	cName = tkn.getname();
	return true;
}

// handle the variable name in a sub-statement of a struct
bool grammar_parser::ParseStructSubStatementVariableName(
		userdef_type_object* pUserDef, string& cName)
{
	assert(NULL != pUserDef);
	token tkn = m_cTokenParser.GetToken();
	if (eTokenType_Identifier != tkn)
	{
		m_cErrInfo.PrintError(eErrID_ExpectVariableName,
			"'struct' declaration");
		m_cTokenParser.OmitCurrentLine();
		return false;
	}
	EIdentifierType eIdType;
	if (IsIdentifierUsed(tkn, eIdType) || IsIdentifierUsedInCurrentStruct(pUserDef, tkn, eIdType))
	{
		m_cErrInfo.PrintError(eErrID_IdentifierRedefined, tkn.getname().c_str(),
			GetDescriptionOfIdentifierType(eIdType));
		m_cTokenParser.OmitCurrentLine();
		return false;
	}
	cName = tkn.getname();
	return true;
}

TInterfaceNode* grammar_parser::IsIdentifierUsedInCurrentInterface(interface_object* pIFObject,
	const string& cName, EIdentifierType& eType)
{
	assert(NULL != pIFObject);
	list_item *pItem = pIFObject->NodeList().getnext();
	for (; pItem != &(pIFObject->NodeList()); pItem = pItem->getnext())
	{
		TInterfaceNode* pNode = LIST_ENTRY(TInterfaceNode, m_OwnerList, pItem);
		if (pNode->m_cName == cName)
		{
			eType = eIdentifier_MethodName;
			return pNode;
		}
	}
	eType = eIdentifier_Unknown;
	return NULL;
}

TVariableObject* grammar_parser::IsIdentifierUsedInCurrentEvent
	(TEventObject* pEvObject, const string& cName, EIdentifierType& eType)
{
	assert(NULL != pEvObject);
	list_item *pItem = pEvObject->NodeList().getnext();
	for (; pItem != &(pEvObject->NodeList()); pItem = pItem->getnext())
	{
		TVariableObject* pNode = LIST_ENTRY(TVariableObject, m_OwnerList, pItem);
		if (pNode->m_cName == cName)
		{
			eType = eIdentifier_EventVariableName;
			return pNode;
		}
	}
	eType = eIdentifier_Unknown;
	return NULL;
}

TVariableObject* grammar_parser::IsIdentifierUsedInCurrentMethod(TInterfaceNode* pIFNode,
	const string& cName, EIdentifierType& eType)
{
	assert(NULL != pIFNode);
	list_item *pItem = pIFNode->m_VariableList.getnext();
	for (; pItem != &(pIFNode->m_VariableList); pItem = pItem->getnext())
	{
		TVariableObject* pNode = LIST_ENTRY(TVariableObject, m_OwnerList, pItem);
		if (pNode->m_cName == cName)
		{
			eType = eIdentifier_MethodVariableName;
			return pNode;
		}
	}
	eType = eIdentifier_Unknown;
	return NULL;
}

TVariableObject* grammar_parser::IsIdentifierUsedInCurrentStruct(
	userdef_type_object* pUserDef, const string& cName, EIdentifierType& eType)
{
	assert(NULL != pUserDef);
	list_item *pItem = pUserDef->VariableListHeader().getnext();
	for (; pItem != &pUserDef->VariableListHeader(); pItem = pItem->getnext())
	{
		TVariableObject *obj = LIST_ENTRY(TVariableObject, m_OwnerList, pItem);
		if (obj->m_cName == cName)
		{
			eType = eIdentifier_MemberVariableName;
			return obj;
		}
	}
	eType = eIdentifier_Unknown;
	return NULL;
}

// handle the sub-statements inside the struct
bool grammar_parser::ParseStructSubStatement(userdef_type_object* pUserDef)
{
	assert(NULL != pUserDef);
	token tkn = m_cTokenParser.GetToken();
	for (; eTokenID_Unknown != tkn; tkn = m_cTokenParser.GetToken())
	{
		if (eTokenID_RightCurlyBracket == tkn)
			return true;

		m_cTokenParser.TokenPushBack();

		basic_objtype eType;
		interface_object* pIFObject = NULL;
		typedef_object *pTypedefType = NULL;
		enumdef_object *pEnumDefType = NULL;
		userdef_type_object *pUserDefType = NULL;
		if (!AnalyzeVariableType(eType, &pTypedefType, &pUserDefType, &pEnumDefType, &pIFObject)
			|| eBasicObjType_Unknown == eType)
		{
			m_cTokenParser.OmitCurrentLine();
			continue;
		}

		// an observable interface could not be a member of structure
		if (pIFObject && pIFObject->TestInterfaceFlags(interface_object::eInterfaceFlag_Observable))
		{
			m_cErrInfo.PrintError(eErrID_ObservableIFCannotBeStruMember);
			m_cTokenParser.OmitCurrentLine();
			continue;
		}

		// int [abc]; -> handle the variable name
		string cName;
		if (!ParseStructSubStatementVariableName(pUserDef, cName))
			continue;
		
		// create the variable object
		TArrayTypeObject cArrayType;
		TVariableObject* pVariable = NULL;
		if (IsBasicObjectType(eType))
			pVariable = new TVariableObject(eType, cName, cArrayType);
		else if (eBasicObjType_Typedefed == eType)
			pVariable = new TVariableObject(pTypedefType, cName, cArrayType);
		else if (eBasicObjType_UserDefined == eType)
			pVariable = new TVariableObject(pUserDefType, cName, cArrayType);
		else if (eBasicObjType_Enum == eType)
			pVariable = new TVariableObject(pEnumDefType, cName, cArrayType);
		else if (eBasicObjType_Interface == eType)
			pVariable = new TVariableObject(pIFObject, cName, cArrayType);

		if (NULL == pVariable) Panic_OutOfMemory();

		// for array "[]" handling
		if (!ParseArrayStatement(pVariable->m_ArrayType))
			return false;

		pVariable->set_flags(varobj_flag_method_param_in);
		pVariable->set_flags(varobj_flag_method_param_out);
		pVariable->AddTo(pUserDef->VariableListHeader());

		// handle ';'
		tkn = m_cTokenParser.GetToken();
		if (eTokenID_Semicolon != tkn)
		{
			m_cTokenParser.TokenPushBack();
			m_cErrInfo.PrintError(eErrID_ExpectSemiColon);
			continue;
		}
	}
	m_cErrInfo.PrintError(eErrID_ReachEOFBeforeMatchingRightCurlyBracket);
	return false;
}

void grammar_parser::ParseEventStatement(void)
{
	// get the identifier (event [abc] (...);)
	token tkn = m_cTokenParser.GetToken();
	if (eTokenType_Identifier != tkn)
	{
		m_cErrInfo.PrintError(eErrID_IdentifierExpectedForThisDeclaration,
			tkn.getname().c_str());
		m_cTokenParser.OmitCurrentLine();
		return;
	}
	EIdentifierType eIdType;
	if (IsIdentifierUsed(tkn, eIdType))
	{
		m_cErrInfo.PrintError(eErrID_IdentifierRedefined, tkn.getname().c_str(),
			GetDescriptionOfIdentifierType(eIdType));
		m_cTokenParser.OmitCurrentLine();
		return;
	}
	string cName = tkn.getname();

	// event abc [(] ... };
	tkn = m_cTokenParser.GetToken();
	if (eTokenID_LeftBracket != tkn)
	{
		m_cErrInfo.PrintError(eErrID_ExpectLeftBracket);
		m_cTokenParser.OmitCurrentLine();
		return;
	}

	// create the event object
	assert(NULL != m_pCurrentScope);
	TEventObject *pEventObject = TEventObject::create_instance(m_pCurrentScope, cName);
	if (NULL == pEventObject) Panic_OutOfMemory();

	// handle the parameters parsing
	tkn = m_cTokenParser.GetToken();
	if (eTokenID_Void == tkn)
	{
		// see if matching with a ')'
		tkn = m_cTokenParser.GetToken();
		if (eTokenID_RightBracket != tkn)
		{
			m_cErrInfo.PrintError(eErrID_ExpectRightBracket);
			delete pEventObject;
			return;
		}
	}
	else if (eTokenID_RightBracket != tkn)
	{
		m_cTokenParser.TokenPushBack();
		for (;;)
		{
			// in/out/inout or default
			tkn = m_cTokenParser.GetToken();
			switch (tkn.GetID())
			{
			case eTokenID_In:
				break;

			case eTokenID_Out:
			case eTokenID_Inout:
				m_cErrInfo.PrintError(eErrID_EventParameterCouldbeInOnly);
				break;

			default:
				m_cTokenParser.TokenPushBack();
				break;
			}

			// handle 'event EventName([int] val)'
			basic_objtype eType;
			interface_object *_pIFObject = NULL;
			typedef_object *pTypedefType = NULL;
			enumdef_object *pEnumDefType = NULL;
			userdef_type_object *pUserDefType = NULL;
			if (!AnalyzeVariableType(eType, &pTypedefType, &pUserDefType, &pEnumDefType, &_pIFObject)
				|| eBasicObjType_Unknown == eType)
			{
				m_cTokenParser.OmitCurrentLine();
				delete pEventObject;
				return;
			}

			// handle 'event EventName(int [val])'
			string cVariableName;
			if (!ParseEventParameterVariableName(pEventObject, cVariableName))
			{
				delete pEventObject;
				return;
			}

			// create the variable object
			TArrayTypeObject cArrayType;
			TVariableObject* pVariable = NULL;
			if (IsBasicObjectType(eType))
				pVariable = new TVariableObject(eType, cVariableName, cArrayType);
			else if (eBasicObjType_Typedefed == eType)
				pVariable = new TVariableObject(pTypedefType, cVariableName, cArrayType);
			else if (eBasicObjType_UserDefined == eType)
				pVariable = new TVariableObject(pUserDefType, cVariableName, cArrayType);
			else if (eBasicObjType_Enum == eType)
				pVariable = new TVariableObject(pEnumDefType, cVariableName, cArrayType);
			else if (eBasicObjType_Interface == eType)
				pVariable = new TVariableObject(_pIFObject, cVariableName, cArrayType);

			if (NULL == pVariable) Panic_OutOfMemory();
			pVariable->set_flags(varobj_flag_method_param_in);

			// for array "[]" handling
			if (!ParseArrayStatement(pVariable->m_ArrayType))
			{
				delete pEventObject;
				return;
			}
			pVariable->AddTo(pEventObject->NodeList());

			tkn = m_cTokenParser.GetToken();
			if (eTokenID_RightBracket == tkn)
				break;
			else if (eTokenID_Comma != tkn)
			{
				m_cErrInfo.PrintError(eErrID_UnrecgnizedStatement,
					tkn.getname().c_str());
				m_cTokenParser.OmitCurrentLine();
				delete pEventObject;
				return;
			}
		}
	}
	pEventObject->Add(m_pCurrentScope->EventHeader());

	// finally, handle ';' (event EventName(int a)[;]
	tkn = m_cTokenParser.GetToken();
	if (eTokenID_Semicolon != tkn)
	{
		m_cTokenParser.TokenPushBack();
		m_cErrInfo.PrintError(eErrID_ExpectSemiColon);
		return;
	}
}

void grammar_parser::ParseInterfaceStatement(void)
{
	// get the identifier (interface [abc] {...};)
	token tkn = m_cTokenParser.GetToken();
	if (eTokenType_Identifier != tkn)
	{
		m_cErrInfo.PrintError(eErrID_IdentifierExpectedForThisDeclaration,
		tkn.getname().c_str());
		m_cTokenParser.OmitCurrentLine();
		return;
	}
	EIdentifierType eIdType;
	if (IsIdentifierUsed(tkn, eIdType))
	{
		m_cErrInfo.PrintError(eErrID_IdentifierRedefined, tkn.getname().c_str(),
			GetDescriptionOfIdentifierType(eIdType));
		m_cTokenParser.OmitCurrentLine();
		return;
	}
	string cName = tkn.getname();

	// interface abc [{] ... };
	tkn = m_cTokenParser.GetToken();
	if (eTokenID_LeftCurlyBracket != tkn)
	{
		m_cErrInfo.PrintError(eErrID_ExpectLeftCurlyBracket);
		m_cTokenParser.OmitCurrentLine();
		return;
	}

	// create the interface object
	assert(NULL != m_pCurrentScope);
	interface_object *pIFObject = interface_object::create_instance(m_pCurrentScope, cName);
	if (NULL == pIFObject) Panic_OutOfMemory();
	pIFObject->Add(m_pCurrentScope->InterfaceHeader());

	// handle each variable definition
	if (ParseInterfaceSubStatement(pIFObject))
	{
		// handle ';' (interface { ... }[;]
		tkn = m_cTokenParser.GetToken();
		if (eTokenID_Semicolon != tkn)
		{
			m_cTokenParser.TokenPushBack();
			m_cErrInfo.PrintError(eErrID_ExpectSemiColon);
			return;
		}

		// set the node id for each node
		list_item* pSubItem = pIFObject->NodeList().getnext();
		for (unsigned int i = 1; pSubItem != &(pIFObject->NodeList()); pSubItem = pSubItem->getnext(), ++i)
		{
			TInterfaceNode *pNode = LIST_ENTRY(TInterfaceNode, m_OwnerList, pSubItem);
			pNode->SetNodeID(i);
			if (eIFNodeType_Attribute == pNode->GetType()) ++i;
		}
	}
}

bool grammar_parser::AnalyzeInterfacePropertyBoolValue(bool &ret)
{
	token tkn = m_cTokenParser.GetToken();
	if (eTokenID_True == tkn) ret = true;
	else if (eTokenID_False == tkn) ret = false;
	else
	{
		m_cTokenParser.TokenPushBack();
		return false;
	}
	return true;
}

bool grammar_parser::AnalyzeInterfaceProperty(interface_object* pIFObject)
{
	if (NULL == pIFObject) return false;

	token tkn = m_cTokenParser.GetToken();
	enum { apUnknown, Singleton, Observable } ap = apUnknown;

	if (eTokenID_Singleton == tkn)
		ap = Singleton;
	else if (eTokenID_Observable == tkn)
		ap = Observable;
	else
	{
		m_cErrInfo.PrintError(eErrID_UnrecognizedPropertyName,
			tkn.getname().c_str(), pIFObject->getname().c_str());
		m_cTokenParser.OmitCurrentLine();
		return false;
	}
	string cPropertyName = tkn.getname();

	tkn = m_cTokenParser.GetToken();
	if (eTokenID_Equal != tkn)
	{
		m_cErrInfo.PrintError(errid_expect_equal);
		m_cTokenParser.OmitCurrentLine();
		return false;
	}

	// analyze the value of property
	switch (ap)
	{
	case Singleton: {
		bool boolret;
		if (!AnalyzeInterfacePropertyBoolValue(boolret))
		{
			m_cErrInfo.PrintError(eErrID_ExpectBooleanPropertyValue,
				cPropertyName.c_str(), pIFObject->getname().c_str());
			m_cTokenParser.OmitCurrentLine();
			return false;
		}
		if (boolret)
		{
			if (pIFObject->TestInterfaceFlags(interface_object::eInterfaceFlag_Observable))
				m_cErrInfo.PrintError(eErrID_ObservableIFCouldnotBeSingleton);
			else pIFObject->SetInterfaceFlags(interface_object::eInterfaceFlag_Singleton);
		}
		else pIFObject->ClearInterfaceFlags(interface_object::eInterfaceFlag_Singleton);
	}	break;

	case Observable: {
		bool boolret;
		if (!AnalyzeInterfacePropertyBoolValue(boolret))
		{
			m_cErrInfo.PrintError(eErrID_ExpectBooleanPropertyValue,
				cPropertyName.c_str(), pIFObject->getname().c_str());
			m_cTokenParser.OmitCurrentLine();
			return false;
		}
		if (boolret)
		{
			if (pIFObject->TestInterfaceFlags(interface_object::eInterfaceFlag_Singleton))
				m_cErrInfo.PrintError(eErrID_SingletonIFCouldnotBeObservable);
			else pIFObject->SetInterfaceFlags(interface_object::eInterfaceFlag_Observable);
		}
		else pIFObject->ClearInterfaceFlags(interface_object::eInterfaceFlag_Observable);
	}	break;

	default: assert(0); break;
	}
	// todo: any other "else"
	return true;
}

bool grammar_parser::AnalyzeInterfaceAttributeStatement(interface_object *pIFObject)
{
	if (NULL == pIFObject) return false;

	TInterfaceNode *pNode = new TInterfaceNode(pIFObject, eIFNodeType_Attribute);
	if (NULL == pNode) Panic_OutOfMemory();

	basic_objtype eType;
	interface_object *_pIFObject = NULL;
	typedef_object *pTypedefType = NULL;
	enumdef_object *pEnumDefType = NULL;
	userdef_type_object *pUserDefType = NULL;
	if (!AnalyzeVariableType(eType, &pTypedefType, &pUserDefType, &pEnumDefType, &_pIFObject)
		|| eBasicObjType_Unknown == eType)
	{
		m_cTokenParser.OmitCurrentLine();
		delete pNode;
		return false;
	}
	
	// array is not support by 'attribute', check it
	if (eBasicObjType_Typedefed == eType)
	{
		// only a typedef object could be 'implicitly' declared as an array
		assert(NULL != pTypedefType);
		TArrayTypeObject ArrayType;
		basic_objtype eOriginalType;
		pTypedefType->GetOriginalType(eOriginalType, ArrayType, NULL, NULL);
		if (eBasicObjType_Unknown == eOriginalType)
		{
			m_cErrInfo.PrintError(eErrID_2DimemsionArrayNotSupported);
			m_cTokenParser.OmitCurrentLine();
			delete pNode;
			return false;
		}
		if (is_array(ArrayType))
		{
			m_cErrInfo.PrintError(eErrID_AttributeCannotBeArray);
			m_cTokenParser.OmitCurrentLine();
			delete pNode;
			return false;
		}
	}

	// initialization
	pNode->m_Data.attribute._basic_type = eType;
	pNode->m_Data.attribute.m_pIFObject = _pIFObject;
	pNode->m_Data.attribute.m_pTypedefType = pTypedefType;
	pNode->m_Data.attribute.m_pEnumDef = pEnumDefType;
	pNode->m_Data.attribute.m_pUserDefType = pUserDefType;

	// get the interface method name
	token tkn = m_cTokenParser.GetToken();
	if (eTokenType_Identifier != tkn)
	{
		m_cErrInfo.PrintError(eErrID_IdentifierExpectedForThisDeclaration,
			tkn.getname().c_str());
		m_cTokenParser.OmitCurrentLine();
		delete pNode;
		return false;
	}

	EIdentifierType eIdType;
	if (IsIdentifierUsed(tkn, eIdType) || IsIdentifierUsedInCurrentInterface(pIFObject, tkn, eIdType))
	{
		m_cErrInfo.PrintError(eErrID_IdentifierRedefined, tkn.getname().c_str(),
			GetDescriptionOfIdentifierType(eIdType));
		m_cTokenParser.OmitCurrentLine();
		delete pNode;
		return false;
	}

	pNode->m_cName = tkn.getname();

	// make sure it is not an array
	tkn = m_cTokenParser.GetToken();
	if (eTokenID_LeftSquareBracket == tkn)
	{
		m_cErrInfo.PrintError(eErrID_AttributeCannotBeArray);
		m_cTokenParser.OmitCurrentLine();
		delete pNode;
		return false;
	}

	m_cTokenParser.TokenPushBack();
	pNode->AddTo(pIFObject->NodeList());
	return true;
}

bool grammar_parser::AnalyzeInterfaceMethod(interface_object* pIFObject, bool bIsConstructor)
{
	if (NULL == pIFObject) return false;

	TInterfaceNode *pNode = new TInterfaceNode(pIFObject, eIFNodeType_Method);
	if (NULL == pNode) Panic_OutOfMemory();

	// see if it is a virtual method
	pNode->m_Data.method.m_Flags = 0;
	token tkn = m_cTokenParser.GetToken();
	if (eTokenID_Virtual == tkn)
	{
		if (bIsConstructor)
			m_cErrInfo.PrintError(eErrID_ConstructorCouldntBeVirtual);
		else
			pNode->m_Data.method.set_flags(TInterfaceNode::eMethodFlag_Virtual);
		tkn = m_cTokenParser.GetToken();
	}

	// get the return variable type
	userdef_type_object* pRetType = NULL;
	if (!bIsConstructor && eTokenID_Void == tkn)
	{
		// this is a "void" type normal method definition
		pNode->m_Data.method.m_pRetValIFObject = NULL;
		pNode->m_Data.method.m_pRetValTypedefType = NULL;
		pNode->m_Data.method.m_pRetValEnumDef = NULL;
		pNode->m_Data.method.m_pRetValUserDefType = NULL;
		pNode->m_Data.method.m_eRetValType = eBasicObjType_Unknown;
	}
	else if (!bIsConstructor)
	{
		// this is normal method definition with return
		// value type declared
		m_cTokenParser.TokenPushBack();

		basic_objtype eType;
		interface_object *_pIFObject = NULL;
		typedef_object *pTypedefType = NULL;
		enumdef_object *pEnumDefType = NULL;
		userdef_type_object *pUserDefType = NULL;
		if (!AnalyzeVariableType(eType, &pTypedefType, &pUserDefType, &pEnumDefType, &_pIFObject)
			|| eBasicObjType_Unknown == eType)
		{
			m_cTokenParser.OmitCurrentLine();
			delete pNode;
			return false;
		}

		// a method cannot return an array, check it
		if (eBasicObjType_Typedefed == eType)
		{
			// only a typedef object could be 'implicitly' declared as an array
			assert(NULL != pTypedefType);
			TArrayTypeObject ArrayType;
			basic_objtype eOriginalType;
			pTypedefType->GetOriginalType(eOriginalType, ArrayType, NULL, NULL);
			if (eBasicObjType_Unknown == eOriginalType)
			{
				m_cErrInfo.PrintError(eErrID_2DimemsionArrayNotSupported);
				m_cTokenParser.OmitCurrentLine();
				delete pNode;
				return false;
			}
			if (is_array(ArrayType))
			{
				m_cErrInfo.PrintError(eErrID_MethodReturnAnArray);
				m_cTokenParser.OmitCurrentLine();
				delete pNode;
				return false;
			}
		}

		pNode->m_Data.method.m_eRetValType = eType;
		pNode->m_Data.method.m_pRetValIFObject = _pIFObject;
		pNode->m_Data.method.m_pRetValTypedefType = pTypedefType;
		pNode->m_Data.method.m_pRetValEnumDef = pEnumDefType;
		pNode->m_Data.method.m_pRetValUserDefType = pUserDefType;
	}
	else m_cTokenParser.TokenPushBack();

	// get the interface method name
	tkn = m_cTokenParser.GetToken();
	if (eTokenType_Identifier != tkn)
	{
		m_cErrInfo.PrintError(eErrID_IdentifierExpectedForThisDeclaration,
			tkn.getname().c_str());
		m_cTokenParser.OmitCurrentLine();
		delete pNode;
		return false;
	}

	if (bIsConstructor)
	{
		if (tkn.getname() != pIFObject->getname())
		{
			m_cErrInfo.PrintError(eErrID_ConstructorNameNotEqualToInterfaceName);
			m_cTokenParser.OmitCurrentLine();
			delete pNode;
			return false;
		}
		pNode->m_Data.method.set_flags(TInterfaceNode::eMethodFlag_Constructor);
	}
	else
	{
		EIdentifierType eIdType;
		if (IsIdentifierUsed(tkn, eIdType) || IsIdentifierUsedInCurrentInterface(pIFObject, tkn, eIdType))
		{
			m_cErrInfo.PrintError(eErrID_IdentifierRedefined, tkn.getname().c_str(),
				GetDescriptionOfIdentifierType(eIdType));
			m_cTokenParser.OmitCurrentLine();
			delete pNode;
			return false;
		}
		pNode->m_cName = tkn.getname();
	}

	// handle int abc[(]int ret);
	tkn = m_cTokenParser.GetToken();
	if (eTokenID_LeftBracket != tkn)
	{
		m_cErrInfo.PrintError(eErrID_ExpectLeftBracket);
		m_cTokenParser.OmitCurrentLine();
		delete pNode;
		return false;
	}

	// handle the parameters parsing
	tkn = m_cTokenParser.GetToken();
	if (eTokenID_Void == tkn)
	{
		// see if matching with a ')'
		tkn = m_cTokenParser.GetToken();
		if (eTokenID_RightBracket != tkn)
		{
			m_cErrInfo.PrintError(eErrID_ExpectRightBracket);
			delete pNode;
			return false;
		}
		if (bIsConstructor)
		{
			m_cErrInfo.PrintError(eErrID_NoNeedOfDefaultCtorDefinition,
				pIFObject->getname().c_str());
			delete pNode;
		}
		else pNode->AddTo(pIFObject->NodeList());
	}
	else if (eTokenID_RightBracket == tkn)
	{
		// something like 'void func()'
		if (bIsConstructor)
		{
			m_cErrInfo.PrintError(eErrID_NoNeedOfDefaultCtorDefinition,
				pIFObject->getname().c_str());
			delete pNode;
		}
		else pNode->AddTo(pIFObject->NodeList());
	}
	else
	{
		m_cTokenParser.TokenPushBack();
		for (;;)
		{
			// in/out/inout or default
			unsigned int in_out_flags = 0;
			tkn = m_cTokenParser.GetToken();
			switch (tkn.GetID())
			{
			case eTokenID_In:
				in_out_flags |= varobj_flag_method_param_in;
				break;

			case eTokenID_Inout:
				in_out_flags |= varobj_flag_method_param_in;
				if (bIsConstructor)
					m_cErrInfo.PrintError(eErrID_ConstructorParameterCouldbeInOnly);
				else
					in_out_flags |= varobj_flag_method_param_out;
				break;

			case eTokenID_Out:
				if (bIsConstructor)
					m_cErrInfo.PrintError(eErrID_ConstructorParameterCouldbeInOnly);
				else
					in_out_flags |= varobj_flag_method_param_out;
				break;

			default:
				in_out_flags |= varobj_flag_method_param_in;
				m_cTokenParser.TokenPushBack();
				break;
			}

			// handle 'int func([int] val)
			basic_objtype eType;
			interface_object *_pIFObject = NULL;
			typedef_object *pTypedefType = NULL;
			enumdef_object *pEnumDefType = NULL;
			userdef_type_object *pUserDefType = NULL;
			if (!AnalyzeVariableType(eType, &pTypedefType, &pUserDefType, &pEnumDefType, &_pIFObject)
				|| eBasicObjType_Unknown == eType)
			{
				m_cTokenParser.OmitCurrentLine();
				delete pNode;
				return false;
			}

			// // handle 'int func(int [val])'
			string cVariableName;
			if (!ParseIFMethodParameterVariableName(pIFObject, pNode, cVariableName))
			{
				delete pNode;
				return false;
			}

			// create the variable object
			TArrayTypeObject cArrayType;
			TVariableObject* pVariable = NULL;
			if (IsBasicObjectType(eType))
				pVariable = new TVariableObject(eType, cVariableName, cArrayType);
			else if (eBasicObjType_Typedefed == eType)
				pVariable = new TVariableObject(pTypedefType, cVariableName, cArrayType);
			else if (eBasicObjType_UserDefined == eType)
				pVariable = new TVariableObject(pUserDefType, cVariableName, cArrayType);
			else if (eBasicObjType_Enum == eType)
				pVariable = new TVariableObject(pEnumDefType, cVariableName, cArrayType);
			else if (eBasicObjType_Interface == eType)
				pVariable = new TVariableObject(_pIFObject, cVariableName, cArrayType);

			if (NULL == pVariable) Panic_OutOfMemory();
			pVariable->set_flags(in_out_flags);

			// for array "[]" handling
			if (!ParseArrayStatement(pVariable->m_ArrayType))
			{
				delete pNode;
				return false;
			}
			pVariable->AddTo(pNode->m_VariableList);

			tkn = m_cTokenParser.GetToken();
			if (eTokenID_RightBracket == tkn)
			{
				pNode->AddTo(pIFObject->NodeList());
				return true;
			}
			else if (eTokenID_Comma != tkn)
			{
				m_cErrInfo.PrintError(eErrID_UnrecgnizedStatement,
					tkn.getname().c_str());
				m_cTokenParser.OmitCurrentLine();
				delete pNode;
				return false;
			}
		}
	}
	return true;
}

bool grammar_parser::ParseInterfaceSubStatement(interface_object* pIFObject)
{
	assert(NULL != pIFObject);
	token tkn = m_cTokenParser.GetToken();
	for (; eTokenID_Unknown != tkn; tkn = m_cTokenParser.GetToken())
	{
		if (eTokenID_RightCurlyBracket == tkn)
			return true;

		bool retval;
		// handle property definition
		if (eTokenID_Property == tkn)
			retval = AnalyzeInterfaceProperty(pIFObject);

		// handle attribute
		else if (eTokenID_Attribute == tkn)
			retval = AnalyzeInterfaceAttributeStatement(pIFObject);

		// handle a constructor
		else if (eTokenType_Identifier == tkn
			&& tkn.getname() == pIFObject->getname())
		{
			if (pIFObject->TestInterfaceFlags(interface_object::eInterfaceFlag_Observable))
				m_cErrInfo.PrintError(eErrID_CtorNotAllowedInObservableIF);
			m_cTokenParser.TokenPushBack();
			retval = AnalyzeInterfaceMethod(pIFObject, true);
		}
		// handle a method
		else
		{
			m_cTokenParser.TokenPushBack();
			retval = AnalyzeInterfaceMethod(pIFObject, false);
		}

		if (retval)
		{
			// handle ';' at the end of method definition
			tkn = m_cTokenParser.GetToken();
			if (eTokenID_Semicolon != tkn)
			{
				m_cErrInfo.PrintError(eErrID_ExpectSemiColon);
				m_cTokenParser.TokenPushBack();
			}
		}
	}
	m_cErrInfo.PrintError(eErrID_ReachEOFBeforeMatchingRightCurlyBracket);
	return false;
}

void grammar_parser::ParseStructureStatement(void)
{
	// get the identifier (struct [abc] { ... };)
	token tkn = m_cTokenParser.GetToken();
	if (eTokenType_Identifier != tkn)
	{
		m_cErrInfo.PrintError(eErrID_IdentifierExpectedForThisDeclaration,
		tkn.getname().c_str());
		m_cTokenParser.OmitCurrentLine();
		return;
	}
	EIdentifierType eIdType;
	if (IsIdentifierUsed(tkn, eIdType))
	{
		m_cErrInfo.PrintError(eErrID_IdentifierRedefined, tkn.getname().c_str(),
			GetDescriptionOfIdentifierType(eIdType));
		m_cTokenParser.OmitCurrentLine();
		return;
	}
	string cName = tkn.getname();

	// struct abc [{] ... }
	tkn = m_cTokenParser.GetToken();
	if (eTokenID_LeftCurlyBracket != tkn)
	{
		m_cErrInfo.PrintError(eErrID_ExpectLeftCurlyBracket);
		m_cTokenParser.OmitCurrentLine();
		return;
	}

	// create the user define type
	assert(NULL != m_pCurrentScope);
	userdef_type_object *pUserDefType = userdef_type_object::create_instance
		(m_pCurrentScope, cName);
	if (NULL == pUserDefType) Panic_OutOfMemory();
	pUserDefType->Add(m_pCurrentScope->UserDefTypeItemHeader());

	// handle each vaiable definition
	if (ParseStructSubStatement(pUserDefType))
	{
		// handle ';' (struct abc { ... }[;]
		tkn = m_cTokenParser.GetToken();
		if (eTokenID_Semicolon != tkn)
		{
			m_cTokenParser.TokenPushBack();
			m_cErrInfo.PrintError(eErrID_ExpectSemiColon);
			return;
		}
	}
}

void grammar_parser::ParseModuleStatement(void)
{
	// get the identifier (module abc {};)
	token tkn = m_cTokenParser.GetToken();
	if (eTokenType_Identifier != tkn)
	{
		m_cErrInfo.PrintError(eErrID_IdentifierExpectedForThisDeclaration,
		tkn.getname().c_str());
		m_cTokenParser.OmitCurrentLine();
		return;
	}
	EIdentifierType eIdType;
	bool bIsIdentifierExist = IsIdentifierUsed(tkn, eIdType);
	bool bIsModuleExist = (bIsIdentifierExist && (eIdentifier_ModuleName == eIdType));
	if (bIsIdentifierExist && !bIsModuleExist)
	{
		m_cErrInfo.PrintError(eErrID_IdentifierRedefined, tkn.getname().c_str(),
			GetDescriptionOfIdentifierType(eIdType));
		m_cTokenParser.OmitCurrentLine();
		return;
	}
	string cName = tkn.getname();

	// module abc [{] };
	tkn = m_cTokenParser.GetToken();
	if (eTokenID_LeftCurlyBracket != tkn)
	{
		m_cErrInfo.PrintError(eErrID_ExpectLeftCurlyBracket);
		m_cTokenParser.OmitCurrentLine();
		return;
	}

	// create (or get) the module object
	module *pModuleObj;
	if (!bIsModuleExist)
	{
		pModuleObj = module::create_instance(cName, this, m_cConfig);
		assert(NULL != pModuleObj);
		pModuleObj->Add(m_cModuleHeader);
	}
	else
	{
		pModuleObj = module::Find(m_cModuleHeader, cName);
		if (NULL == pModuleObj)
			Panic_InternalError("module name not found");
	}
	assert(m_pCurrentScope != pModuleObj);
	m_pCurrentScope = pModuleObj;

	// handle all sub-statements inside the module
	if (ParseModuleSubStatement())
	{
		m_pCurrentScope = m_pGlobalScope;

		// module abc {}[;]
		tkn = m_cTokenParser.GetToken();
		if (eTokenID_Semicolon != tkn)
		{
			m_cTokenParser.TokenPushBack();
			m_cErrInfo.PrintError(eErrID_ExpectSemiColon);
			return;
		}
	}
}

userdef_type_object* grammar_parser::FindUserDefObject(const string& name, module *mod)
{
	userdef_type_object *ret;

	if (mod)
		return userdef_type_object::Find(mod, name);

	assert(NULL != m_pCurrentScope && NULL != m_pGlobalScope);

	// search from current scope
	ret = userdef_type_object::Find(m_pCurrentScope, name);
	if (NULL == ret)
	{
		// try to search the global scope
		if (m_pGlobalScope == m_pCurrentScope)
			return NULL;
		return userdef_type_object::Find(m_pGlobalScope, name);
	}
	return ret;
}

typedef_object* grammar_parser::FindTypedefObject(const string& name, module *mod)
{
	typedef_object *ret;

	if (mod)
		return typedef_object::Find(mod, name);

	assert(NULL != m_pCurrentScope && NULL != m_pGlobalScope);

	// search from current scope
	ret = typedef_object::Find(m_pCurrentScope, name);
	if (NULL == ret)
	{
		// try to search the global scope
		if (m_pGlobalScope == m_pCurrentScope)
			return NULL;
		return typedef_object::Find(m_pGlobalScope, name);
	}
	return ret;
}

TConstItemObject* grammar_parser::FindConstItemObject(const string& name)
{
	assert(NULL != m_pCurrentScope && NULL != m_pGlobalScope);

	TConstItemObject *ret;

	// search from current scope
	ret = TConstItemObject::Find(m_pCurrentScope, name);
	if (NULL == ret)
	{
		// try to search the global scope
		if (m_pGlobalScope == m_pCurrentScope)
			return NULL;
		return TConstItemObject::Find(m_pGlobalScope, name);
	}
	return ret;
}

enumdef_object* grammar_parser::FindEnumItemObject(const string& name, module *mod)
{
	enumdef_object *ret;

	if (mod)
		return enumdef_object::Find(mod, name);

	assert(NULL != m_pCurrentScope && NULL != m_pGlobalScope);

	// search from current scope
	ret = enumdef_object::Find(m_pCurrentScope, name);
	if (NULL == ret)
	{
		// try to search the global scope
		if (m_pGlobalScope == m_pCurrentScope)
			return NULL;
		return enumdef_object::Find(m_pGlobalScope, name);
	}
	return ret;
}

interface_object* grammar_parser::FindInterfaceItemObject(const string& name, module *mod)
{
	interface_object *ret;

	if (mod)
		return interface_object::Find(mod, name);

	assert(NULL != m_pCurrentScope && NULL != m_pGlobalScope);

	// search from current scope
	ret = interface_object::Find(m_pCurrentScope, name);
	if (NULL == ret)
	{
		// try to search the global scope
		if (m_pGlobalScope == m_pCurrentScope)
			return NULL;
		return interface_object::Find(m_pGlobalScope, name);
	}
	return ret;
}

bool grammar_parser::AnalyzeBasicVariableType(basic_objtype& eType)
{
	eType = eBasicObjType_Unknown;
	userdef_type_object *obj = NULL;

	token tkn = m_cTokenParser.GetToken();
	if (eTokenID_Unsigned == tkn)
	{
		eType = eBasicObjType_UInt32;
		tkn = m_cTokenParser.GetToken();
		if (eTokenID_Int == tkn)
			return true;	// it is 'unsigned int'
	}
	
	if (eTokenID_Int == tkn)
		eType = eBasicObjType_Int32;
	
	else if (eTokenID_Short == tkn)
	{
		eType = eBasicObjType_Int16;
		m_cErrInfo.PrintError(eErrID_LongShortNotSupport,
			tkn.getname().c_str());
	}

	else if (eTokenID_Long == tkn)
	{
		eType = eBasicObjType_Int64;
		// m_cErrInfo.PrintError(eErrID_LongShortNotSupport,
		// 	tkn.getname().c_str());

	}

	else if (eTokenID_Float == tkn)
	{
		if (eBasicObjType_Unknown != eType)
			m_cErrInfo.PrintError(eErrID_UnsignedCannotWorkWithSpecificType,
			tkn.getname().c_str());

		eType = eBasicObjType_Float;
	}
	else if (eTokenID_String == tkn)
	{
		if (eBasicObjType_Unknown != eType)
			m_cErrInfo.PrintError(eErrID_UnsignedCannotWorkWithSpecificType,
			tkn.getname().c_str());
		
		eType = eBasicObjType_String;
	}
	else if (eTokenID_Stream == tkn)
	{
		if (eBasicObjType_Unknown != eType)
			m_cErrInfo.PrintError(eErrID_UnsignedCannotWorkWithSpecificType,
			tkn.getname().c_str());

		eType = eBasicObjType_Stream;
	}
	else if (eTokenID_Boolean == tkn)
	{
		if (eBasicObjType_Unknown != eType)
			m_cErrInfo.PrintError(eErrID_UnsignedCannotWorkWithSpecificType,
			tkn.getname().c_str());
		
		eType = basic_objtype_boolean;
	}
	else if (eTokenID_Octet == tkn)
	{
		if (eBasicObjType_Unknown != eType)
			m_cErrInfo.PrintError(eErrID_UnsignedCannotWorkWithSpecificType,
			tkn.getname().c_str());
		
		// we no longer support uint8 any more. From now on,
		// we treat uint8 as uint32
		// m_cErrInfo.PrintError(errid_treat_uint8_as_uint32);
		eType = basic_objtype_uint8;
	}
	else
	{
		m_cTokenParser.TokenPushBack();
		return false;
	}
	return true;
}

enumdef_object* grammar_parser::AnalyzeEnumDefVariableType(module *mod)
{
	token tkn = m_cTokenParser.GetToken();
	if (eTokenType_Identifier != tkn)
	{
		m_cErrInfo.PrintError(eErrID_InvalidVariableType,
			tkn.getname().c_str(), "expect an identifier");
		m_cTokenParser.TokenPushBack();
		return NULL;
	}

	// check if it is a enum type
	enumdef_object* pEnumDefObj = FindEnumItemObject(tkn, mod);
	if (pEnumDefObj) return pEnumDefObj;

	m_cTokenParser.TokenPushBack();
	return NULL;
}

interface_object* grammar_parser::AnalyzeIFObjectVariableType(module *mod)
{
	token tkn = m_cTokenParser.GetToken();

	if (eTokenType_Identifier != tkn)
	{
		m_cErrInfo.PrintError(eErrID_InvalidVariableType,
			tkn.getname().c_str(), "expect an identifier");
		m_cTokenParser.TokenPushBack();
		return NULL;
	}
	// check if it is a interface type
	interface_object* pIFObj = FindInterfaceItemObject(tkn, mod);
	if (pIFObj) return pIFObj;

	m_cTokenParser.TokenPushBack();
	return NULL;
}

userdef_type_object* grammar_parser::AnalyzeUserDefVariableType(module *mod)
{
	token tkn = m_cTokenParser.GetToken();

	if (eTokenType_Identifier != tkn)
	{
		m_cErrInfo.PrintError(eErrID_InvalidVariableType,
			tkn.getname().c_str(), "expect an identifier");
		m_cTokenParser.TokenPushBack();
		return NULL;
	}
	// check if it is a userdef type
	userdef_type_object* pUserDefObj = FindUserDefObject(tkn, mod);
	if (pUserDefObj) return pUserDefObj;

	m_cTokenParser.TokenPushBack();
	return NULL;
}

typedef_object* grammar_parser::AnalyzeTypedefVariableType(module *mod)
{
	token tkn = m_cTokenParser.GetToken();

	if (eTokenType_Identifier != tkn)
	{
		m_cErrInfo.PrintError(eErrID_InvalidVariableType,
			tkn.getname().c_str(), "expect an identifier");
		m_cTokenParser.TokenPushBack();
		return NULL;
	}
	// check if it is a typedefed type
	typedef_object *pTypedefObj = FindTypedefObject(tkn, mod);
	if (pTypedefObj) return pTypedefObj;

	m_cTokenParser.TokenPushBack();
	return NULL;
}

// In this method, next tkn has already taken
bool grammar_parser::AnalyzeVariableType(basic_objtype& eType,
		typedef_object **ppTypedefObj, userdef_type_object **ppUserDefObj,
		enumdef_object **ppEnumDefObj, interface_object **ppIFObj)
{
	eType = eBasicObjType_Unknown;
	if (ppTypedefObj) *ppTypedefObj = NULL;
	if (ppUserDefObj) *ppUserDefObj = NULL;
	if (ppEnumDefObj) *ppEnumDefObj = NULL;

	// see if it is a namespace
	token tkn = m_cTokenParser.GetToken();
	module *mod = module::Find(m_cModuleHeader, tkn.getname());
	if (mod)
	{
		// expect a "::"
		tkn = m_cTokenParser.GetToken();
		if (eTokenID_DoubleColon != tkn)
		{
			m_cErrInfo.PrintError(eErrID_ExpectDoubleColon);
			m_cTokenParser.OmitCurrentLine();
			return false;
		}
	}
	else m_cTokenParser.TokenPushBack();

	bool ret = AnalyzeBasicVariableType(eType);
	if (ret)
	{
		if (mod)
		{
			m_cTokenParser.TokenPushBack();
			tkn = m_cTokenParser.GetToken();
			m_cErrInfo.PrintError(eErrID_UnrecgnizedStatement, tkn.getname().c_str());
			m_cTokenParser.OmitCurrentLine();
			return false;
		}
		return true;
	}
	
	typedef_object *pTypedefObj = AnalyzeTypedefVariableType(mod);
	if (pTypedefObj)
	{
		eType = eBasicObjType_Typedefed;
		if (ppTypedefObj) *ppTypedefObj = pTypedefObj;
		return true;
	}
	
	userdef_type_object *pUserDefObj = AnalyzeUserDefVariableType(mod);
	if (pUserDefObj)
	{
		eType = eBasicObjType_UserDefined;
		if (ppUserDefObj) *ppUserDefObj = pUserDefObj;
		return true;
	}

	enumdef_object *pEnumDefObj = AnalyzeEnumDefVariableType(mod);
	if (pEnumDefObj)
	{
		eType = eBasicObjType_Enum;
		if (ppEnumDefObj) *ppEnumDefObj = pEnumDefObj;
		return true;
	}

	interface_object *pIFObj = AnalyzeIFObjectVariableType(mod);
	if (pIFObj)
	{
		eType = eBasicObjType_Interface;
		if (ppEnumDefObj) *ppIFObj = pIFObj;
		return true;
	}

	m_cErrInfo.PrintError(eErrID_InvalidVariableType,
		m_cTokenParser.GetToken().getname().c_str(),
		"unrecognized type name");
	return false;
}

bool grammar_parser::IsIdentifierUsed(const string& cName, EIdentifierType& eType)
{
	// todo: 
	if (module::Find(m_cModuleHeader, cName))
	{
		eType = eIdentifier_ModuleName;
		return true;
	}
	else if (FindTypedefObject(cName, NULL))
	{
		eType = eIdentifier_TypedefName;
		return true;
	}
	else if (FindConstItemObject(cName))
	{
		eType = eIdentifier_ConstItemName;
		return true;
	}
	else if (FindEnumItemObject(cName, NULL))
	{
		eType = eIdentifier_EnumName;
		return true;
	}
	else if (userdef_type_object::Find(m_pCurrentScope, cName))
	{
		eType = eIdentifier_UserDefTypeName;
		return true;
	}
	else if (interface_object::Find(m_pCurrentScope, cName))
	{
		eType = eIdentifier_InterfaceName;
		return true;
	}
	else if (TEventObject::Find(m_pCurrentScope, cName))
	{
		eType = eIdentifier_EventName;
		return true;
	}
	// todo:
	eType = eIdentifier_Unknown;
	return false;
}

bool grammar_parser::CheckNumberIdentifier(token& tkn, module *pScope, number_type& eNumType, double& value)
{
	assert(NULL != pScope);

	// see if it is const identifier
	TConstItemObject *pConstObject = TConstItemObject::Find(pScope, tkn.getname());
	if (pConstObject)
	{
		typedef_object *pRefObj = pConstObject->GetRefType();
		if (pRefObj)
		{
			TArrayTypeObject eArrayType;
			basic_objtype eOriginalType;
			pRefObj->GetOriginalType(eOriginalType, eArrayType, NULL, NULL);
			if (IsBasicNumberObjectType(eOriginalType) && !is_array(eArrayType))
			{
				eNumType = ConvertBasicTypeToNumberType(eOriginalType);
				pConstObject->GetValue(value);
				return true;
			}
		}
		else if (IsBasicNumberObjectType(pConstObject->GetConstType()))
		{
			eNumType = pConstObject->GetConstType();
			pConstObject->GetValue(value);
			return true;
		}
	}
	
	// see if it is enum identifier
	TEnumNode *pEnumNode = pScope->Find(tkn.getname());
	if (pEnumNode)
	{
		value = pEnumNode->GetValue();
		eNumType = Int;
		return true;
	}
	return false;
}

bool grammar_parser::ParseNumberIdentifier(number_type& eNumType, double& value)
{
	token tkn = m_cTokenParser.GetToken();

	// see if it is in current scope
	assert(NULL != m_pCurrentScope);
	if (CheckNumberIdentifier(tkn, m_pCurrentScope, eNumType, value))
		return true;

	// see if it is in global scope
	assert(NULL != m_pGlobalScope);
	if (CheckNumberIdentifier(tkn, m_pGlobalScope, eNumType, value))
		return true;

	// see if it is a namespace
	module *mod = module::Find(m_cModuleHeader, tkn.getname());
	if (NULL == mod)
	{
		m_cTokenParser.TokenPushBack();
		return false;
	}

	// expect a "::"
	tkn = m_cTokenParser.GetToken();
	if (eTokenID_DoubleColon != tkn)
	{
		m_cErrInfo.PrintError(eErrID_ExpectDoubleColon);
		m_cTokenParser.OmitCurrentLine();
		return false;
	}

	// handler module::[identifier]
	tkn = m_cTokenParser.GetToken();
	if (!CheckNumberIdentifier(tkn, mod, eNumType, value))
	{
		m_cErrInfo.PrintError(eErrID_UnrecgnizedStatement, tkn.getname().c_str());
		m_cTokenParser.OmitCurrentLine();
		return false;
	}
	return true;
}

const char *grammar_parser::GetDescriptionOfIdentifierType(EIdentifierType eType)
{
	const char *ret;
	switch (eType)
	{
	case eIdentifier_EnumName:
		ret = "an enum name";
		break;

	case eIdentifier_ModuleName:
		ret = "a module name";
		break;

	case eIdentifier_EnumNodeName:
		ret = "an enum entry";
		break;

	case eIdentifier_MethodName:
		ret = "a method name";
		break;

	case eIdentifier_TypedefName:
		ret = "a type name";
		break;

	case eIdentifier_ConstItemName:
		ret = "a const variable name";
		break;

	case eIdentifier_InterfaceName:
		ret = "an interface name";
		break;

	case eIdentifier_UserDefTypeName:
		ret = "a struct name";
		break;

	case eIdentifier_MethodVariableName:
		ret = "a method parameter";
		break;

	case eIdentifier_MemberVariableName:
		ret = "a member variable";
		break;

	case eIdentifier_EventName:
		ret = "an event";
		break;

	case eIdentifier_EventVariableName:
		ret = "an event parameter";
		break;

	default:
		ret = "<unkonwn>"; break;
	}
	return ret;
}
