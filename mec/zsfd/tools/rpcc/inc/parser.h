#ifndef __CXX_RPCC_PARSER_H__
#define __CXX_RPCC_PARSER_H__

#include "expr.h"
#include "rpcc.h"
#include "list.h"
#include "token.h"
#include "error.h"
#include "config.h"
#include "grammar.h"

class grammar_parser : public expression
{
	friend class expression;
public:
	grammar_parser(token_parser &p, config &c);
	~grammar_parser();

public:
	void Parse(void);

public:
	module* get_globalscope(void) {
		return m_pGlobalScope;
	}

private:
	void ParseImportStatement(void);
	void ParseConstStatement(void);
	void ParseEventStatement(void);
	void ParseInterfaceStatement(void);
	void ParseTypedefStatement(void);
	void ParseModuleStatement(void);
	void ParseStructureStatement(void);
	void ParseEnumStatement(void);

	bool ParseArrayStatement(TArrayTypeObject& obj);

	bool ParseModuleSubStatement(void);
	bool ParseStructSubStatement(userdef_type_object* pUserDef);
	bool ParseEventParameterVariableName(TEventObject *pEvObject, string& cName);
	bool ParseIFMethodParameterVariableName(interface_object *pObject, TInterfaceNode* pNode, string& cName);
	bool ParseStructSubStatementVariableName(userdef_type_object* pUserDef, string& cName);
	bool ParseInterfaceSubStatement(interface_object* pIFObject);

	// analyze an interface property definition
	bool AnalyzeInterfacePropertyBoolValue(bool &ret);
	bool AnalyzeInterfaceProperty(interface_object* pIFObject);
	bool AnalyzeInterfaceAttributeStatement(interface_object *pIFObject);

	// analyze an interface method
	bool AnalyzeInterfaceMethod(interface_object* pIFObject, bool bIsConstructor);
	// analyze a variable type
	bool AnalyzeVariableType(basic_objtype& eType,
		typedef_object **ppTypedefObj, userdef_type_object **ppUserDefObj,
		enumdef_object **ppEnumDefObj, interface_object **ppIFObj);

	bool AnalyzeBasicVariableType(basic_objtype& eType);
	typedef_object* AnalyzeTypedefVariableType(module *pModule);
	userdef_type_object* AnalyzeUserDefVariableType(module *pModule);
	enumdef_object* AnalyzeEnumDefVariableType(module *pModule);
	interface_object* AnalyzeIFObjectVariableType(module *pModule);

	void ReportErrorVariableType(void);

	bool IsIdentifierUsed(const string& cName, EIdentifierType& eType);
	TVariableObject* IsIdentifierUsedInCurrentStruct(userdef_type_object* pUserDef,
		const string& cName, EIdentifierType& eType);

	TInterfaceNode* IsIdentifierUsedInCurrentInterface(interface_object* pIFObject,
		const string& cName, EIdentifierType& eType);
	TVariableObject* IsIdentifierUsedInCurrentMethod(TInterfaceNode* pIFNode,
		const string& cName, EIdentifierType& eType);
	TVariableObject* IsIdentifierUsedInCurrentEvent(TEventObject* pEvObject,
		const string& cName, EIdentifierType& eType);

	const char *GetDescriptionOfIdentifierType(EIdentifierType eType);

	number_type ConvertBasicTypeToNumberType(basic_objtype eType);

	typedef_object* FindTypedefObject(const string& name, module *pModule);
	userdef_type_object* FindUserDefObject(const string& name, module *pModule);
	TConstItemObject* FindConstItemObject(const string& name);
	enumdef_object* FindEnumItemObject(const string& name, module *pModule);
	interface_object* FindInterfaceItemObject(const string& name, module *pModule);

	bool ParseNumberIdentifier(number_type& eNumType, double& value);
	bool CheckNumberIdentifier(token& cToken, module *pScope, number_type& eNumType, double& value);

private:
	token_parser&	m_cTokenParser;
	config&			m_cConfig;
	TErrorInfo		m_cErrInfo;

	grammer_object_header m_cModuleHeader;
	module*			m_pGlobalScope;
	module*			m_pCurrentScope;
};

#endif
