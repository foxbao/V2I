#ifndef __CXX_RPCC_EXPRESSION_H__
#define __CXX_RPCC_EXPRESSION_H__

#include "token.h"
#include "error.h"

#define EXPR_STACK_SZ	(256)

enum EGetExprTokenResult
{
	eGetExprRes_Unknown,
	eGetExprRes_Success,
	eGetExprRes_Error,
	eGetExprRes_EndOfExpr,
};

enum EExprTokenType
{
	eExprTokenType_Unknown = 0,
	eExprTokenType_Add,
	eExprTokenType_Sub,
	eExprTokenType_Mul,
	eExprTokenType_Div,
	eExprTokenType_Number,
	eExprTokenType_LeftBracket,
	eExprTokenType_RightBracket,
};

#define IsOperator(t)	(int(t) >= int(eExprTokenType_Add) && int(t) <= int(eExprTokenType_Div))
#define EPSILON			(1e-5)
#define Double_IsZero(v) ((v) > -EPSILON && (v) < EPSILON)

class expression
{
public:
	expression(token_parser& parser, TErrorInfo& errinfo);
	void Reset(void);
	bool AnalyzeExpression(void);
	
	// convert to the target type
	bool ConvertResultTo(basic_objtype eTargetType, double& value);

	virtual bool ParseNumberIdentifier(number_type& eNumType, double& value);

private:
	bool IsNumStackEmpty(void);
	bool IsOptStackEmpty(void);

	bool PushNumber(double& val);
	bool PushOperator(EExprTokenType eType);
	bool PopNumber(double& val);
	bool PopOperator(EExprTokenType& eType);
	EExprTokenType GetOpTop(void);
	unsigned int GetPriority(EExprTokenType eType);
	bool BasicCalc(double& v1, double& v2, EExprTokenType eType, double& result);

	EGetExprTokenResult GetNextToken(double& value, EExprTokenType& eType, string& tokenstr);
	void PushBackToken(void);
	EExprTokenType* GetNextAllowTypeList(EExprTokenType curr);
	bool InAllowList(EExprTokenType eType, EExprTokenType* list);
	bool IsInAllowList(double& value, EExprTokenType& eType, string& tokenstr, EExprTokenType *list);
	bool HandleToken(double& value, EExprTokenType& eType, const string& token);

	unsigned int ConvertNumberTypeToLevel(basic_objtype eType);
	basic_objtype NumberTypeToBasicObjectType(number_type eType);
	basic_objtype TokenIDToBasicObjectType(ETokenID eId);
	void RefreshFinalResultType(basic_objtype eType);

private:
	token_parser&	m_cTokenParser;
	TErrorInfo&		m_cErrInfo;
	double			m_NumberStack[EXPR_STACK_SZ];
	unsigned char	m_OperatorStack[EXPR_STACK_SZ];
	int				m_NumStackPos;
	int				m_OptStackPos;
	basic_objtype m_eResultType;
	double			m_Result;
};

#endif
