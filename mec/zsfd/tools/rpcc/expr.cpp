#include <assert.h>
#include "expr.h"

expression::expression(token_parser& parser, TErrorInfo& errinfo)
: m_cTokenParser(parser)
, m_cErrInfo(errinfo)
, m_NumStackPos(0)
, m_OptStackPos(0)
, m_eResultType(eBasicObjType_Unknown)
, m_Result(0)
{
}

inline bool expression::IsNumStackEmpty(void)
{
	return (m_NumStackPos <= 0) ? true : false;
}

inline bool expression::IsOptStackEmpty(void)
{
	return (m_OptStackPos <= 0) ? true : false;
}

inline bool expression::PushNumber(double& val)
{
	assert(m_NumStackPos >= 0);
	
	if (m_NumStackPos >= EXPR_STACK_SZ)
	{
		m_cErrInfo.PrintError(eErrID_ExpressionStackOverflow);
		return false;
	}

	m_NumberStack[m_NumStackPos++] = val;
	return true;
}

bool expression::PopNumber(double& val)
{
	assert(m_NumStackPos <= EXPR_STACK_SZ);

	if (m_NumStackPos <= 0)
	{
		m_cErrInfo.PrintError(eErrID_ExpressionStackEmpty);
		return false;
	}
	val = m_NumberStack[--m_NumStackPos];
	return true;
}

inline bool expression::PushOperator(EExprTokenType eType)
{
	assert(m_OptStackPos >= 0);
	
	if (m_OptStackPos >= EXPR_STACK_SZ)
	{
		m_cErrInfo.PrintError(eErrID_ExpressionStackOverflow);
		return false;
	}

	m_OperatorStack[m_OptStackPos++] = char(((unsigned int)eType) & 0xFF);
	return true;
}

bool expression::PopOperator(EExprTokenType& eType)
{
	assert(m_OptStackPos <= EXPR_STACK_SZ);

	if (m_OptStackPos <= 0)
	{
		m_cErrInfo.PrintError(eErrID_ExpressionStackEmpty);
		return false;
	}
	eType = (EExprTokenType)m_OperatorStack[--m_OptStackPos];
	return true;
}

EExprTokenType expression::GetOpTop(void)
{
	if (m_OptStackPos <= 0)
		return eExprTokenType_Unknown;

	return (EExprTokenType)m_OperatorStack[m_OptStackPos - 1];
}

void expression::Reset(void)
{
	m_NumStackPos = 0;
	m_OptStackPos = 0;
	m_eResultType = eBasicObjType_Unknown;
	m_Result = 0;
}

void expression::PushBackToken(void)
{
	m_cTokenParser.TokenPushBack();
}

unsigned int expression::ConvertNumberTypeToLevel(basic_objtype eType)
{
	unsigned int ret;
	switch (eType)
	{
	case eBasicObjType_Int32: ret = 4; break;
	case eBasicObjType_Int16: ret = 2; break;
	case eBasicObjType_Int64: ret = 5; break;
	case eBasicObjType_UInt32: ret = 3; break;
	case basic_objtype_uint8: ret = 1; break;
	case eBasicObjType_Float: ret = 6; break;
	default: ret = 0; break;
	}
	return ret;
}

void expression::RefreshFinalResultType(basic_objtype eType)
{
	assert(eBasicObjType_Int32 == eType	\
		|| eBasicObjType_UInt32 == eType \
		|| eBasicObjType_Int16 == eType \
		|| eBasicObjType_Int64 == eType \
		|| eBasicObjType_Float == eType	\
		|| basic_objtype_uint8 == eType);

	unsigned int PrevLevel = ConvertNumberTypeToLevel(m_eResultType);
	unsigned int CurrLevel = ConvertNumberTypeToLevel(eType);
	if (PrevLevel < CurrLevel) m_eResultType = eType;
}

basic_objtype expression::NumberTypeToBasicObjectType(number_type eType)
{
	basic_objtype eRetType;
	switch (eType)
	{
	case UChar:
	case UInt: eRetType = eBasicObjType_UInt32; break;
	case Int: eRetType = eBasicObjType_Int32; break;
	case Float: eRetType = eBasicObjType_Float; break;	
	default: eRetType = eBasicObjType_Unknown;
	}
	return eRetType;
}

basic_objtype expression::TokenIDToBasicObjectType(ETokenID eId)
{
	basic_objtype eType;
	switch (eId)
	{
	case eTokenID_Number_Int: eType = eBasicObjType_Int32; break;
	case eTokenID_Number_UInt: eType = eBasicObjType_UInt32; break;
	case eTokenID_Number_Float: eType = eBasicObjType_Float; break;	
	default: eType = eBasicObjType_Unknown;
	}
	return eType;
}

EGetExprTokenResult expression::GetNextToken(double& value, EExprTokenType& eType, string& tokenstr)
{
	number_type eNumType = UInt;
	token tkn = m_cTokenParser.GetToken();
	if (eTokenType_Number == tkn)
	{	
		tokenstr = tkn;
		eType = eExprTokenType_Number;
		tkn.GetNumber(eNumType, value);
		RefreshFinalResultType(TokenIDToBasicObjectType(tkn.GetID()));
		return eGetExprRes_Success;
	}

	switch (tkn.GetID())
	{
	case eTokenID_Add:
		eType = eExprTokenType_Add;
		break;

	case eTokenID_Sub:
		eType = eExprTokenType_Sub;
		break;

	case eTokenID_Mul:
		eType = eExprTokenType_Mul;
		break;

	case eTokenID_Div:
		eType = eExprTokenType_Div;
		break;

	case eTokenID_LeftBracket:
		eType = eExprTokenType_LeftBracket;
		break;

	case eTokenID_RightBracket:
		eType = eExprTokenType_RightBracket;
		break;

	default: {
		// see if it is something like: common::MAX_ID
		PushBackToken();
		if (!ParseNumberIdentifier(eNumType, value))
			return eGetExprRes_EndOfExpr;
		eType = eExprTokenType_Number;
		RefreshFinalResultType(NumberTypeToBasicObjectType(eNumType));
	}	break;
	}
	tokenstr = tkn;
	return eGetExprRes_Success;
}

bool expression::InAllowList(EExprTokenType eType, EExprTokenType* list)
{
	for (; *list != eExprTokenType_Unknown; list++)
		if (*list == eType) return true;
	return false;
}

bool expression::IsInAllowList(double& value, EExprTokenType& eType, string& tokenstr, EExprTokenType *list)
{
	if (InAllowList(eType, list))
		return true;

	// handle 1 [+ -3]
	if (eExprTokenType_Sub == eType)
	{
		if (!InAllowList(eExprTokenType_Number, list))
			return false;

		double tmp_val;
		string tmp_tokenstr;
		EExprTokenType tmp_eType;
		if (eGetExprRes_Success != GetNextToken(tmp_val, tmp_eType, tmp_tokenstr))
			return false;
		if (eExprTokenType_Number != tmp_eType)
		{
			PushBackToken();
			return false;
		}

		RefreshFinalResultType(eBasicObjType_Int32);
		value = -tmp_val;
		eType = tmp_eType;
		tokenstr = tmp_tokenstr;
		return true;
	}
	return false;
}

EExprTokenType* expression::GetNextAllowTypeList(EExprTokenType curr)
{
	static EExprTokenType _for_Unknown[] = {
		eExprTokenType_Number, eExprTokenType_LeftBracket, eExprTokenType_Unknown
	};
	static EExprTokenType _for_Add[] = {
		eExprTokenType_Number, eExprTokenType_LeftBracket, eExprTokenType_Unknown
	};
	static EExprTokenType _for_Sub[] = {
		eExprTokenType_Number, eExprTokenType_LeftBracket, eExprTokenType_Unknown
	};
	static EExprTokenType _for_Mul[] = {
		eExprTokenType_Number, eExprTokenType_LeftBracket, eExprTokenType_Unknown
	};
	static EExprTokenType _for_Div[] = {
		eExprTokenType_Number, eExprTokenType_LeftBracket, eExprTokenType_Unknown
	};
	static EExprTokenType _for_Number[] = {
		eExprTokenType_Add, eExprTokenType_Sub,
		eExprTokenType_Mul, eExprTokenType_Div,
		eExprTokenType_RightBracket, eExprTokenType_Unknown
	};
	static EExprTokenType _for_LeftBracket[] = {
		eExprTokenType_Number, eExprTokenType_LeftBracket, eExprTokenType_Unknown
	};
	static EExprTokenType _for_RightBracket[] = {
		eExprTokenType_Add, eExprTokenType_Sub,
		eExprTokenType_Mul, eExprTokenType_Div,
		eExprTokenType_RightBracket, eExprTokenType_Unknown
	};

	static EExprTokenType* list[] = {
		_for_Unknown,		// eExprTokenType_Unknown
		_for_Add,			// eExprTokenType_Add
		_for_Sub,			// eExprTokenType_Sub
		_for_Mul,			// eExprTokenType_Mul
		_for_Div,			// eExprTokenType_Div
		_for_Number,		// eExprTokenType_Number
		_for_LeftBracket,	// eExprTokenType_LeftBracket
		_for_RightBracket,	// eExprTokenType_RightBracket
	};
	return list[int(curr)];
}

bool expression::ConvertResultTo(basic_objtype eTargetType, double& value)
{
	if (eBasicObjType_Unknown == m_eResultType)
		return false;

	static const char *type_str[] = {
		"<unknown>",
		"boolean",
		"unsigned char",
		"int",
		"short",
		"long"
		"unsigned int",
		"float",
		"string",
		"typedefed type",
		"user defined",
	};

	if (eBasicObjType_Int32 != eTargetType && eBasicObjType_UInt32 != eTargetType
		&& basic_objtype_uint8 != eTargetType && eBasicObjType_Float != eTargetType
		&& eBasicObjType_Int16 != eTargetType && eBasicObjType_Int64 != eTargetType)
	{
		m_cErrInfo.PrintError(eErrID_CannotCovertType,
			type_str[int(m_eResultType)], type_str[int(eTargetType)]);
		return false;
	}

	value = m_Result;

	if (basic_objtype_uint8 == eTargetType)
	{
		if (eBasicObjType_Int32 == m_eResultType)
		{
			if (m_Result < 0)
			{
				m_cErrInfo.PrintError(eErrID_FailConvertMinusToUInt);
				return false;
			}
		}

		if (eBasicObjType_Int16 == m_eResultType)
		{
			if (m_Result < 0)
			{
				m_cErrInfo.PrintError(eErrID_FailConvertMinusToUInt);
				return false;
			}
		}

		if (eBasicObjType_Int64 == m_eResultType)
		{
			if (m_Result < 0)
			{
				m_cErrInfo.PrintError(eErrID_FailConvertMinusToUInt);
				return false;
			}
		}
		
		else if (eBasicObjType_Float == m_eResultType)
		{
			if (m_Result < 0)
			{
				m_cErrInfo.PrintError(eErrID_FailConvertMinusToUInt);
				return false;
			}
			else if (m_Result > double(_MAXUCHAR))
			{
				m_cErrInfo.PrintError(eErrID_ValueOverflowFromNumConvert, 
					type_str[int(eTargetType)]);
				return false;
			}
			else if (double(((unsigned char)m_Result)) != m_Result)
			{
				m_cErrInfo.PrintError(eErrID_PrecisionLossFromNumConvert,
					type_str[int(m_eResultType)],
					type_str[int(eTargetType)]);
				value = double(((unsigned char)m_Result));
			}
		}
		else if (basic_objtype_uint8 == m_eResultType || eBasicObjType_UInt32 == m_eResultType)
		{
			if (m_Result > double(_MAXUCHAR))
			{
				m_cErrInfo.PrintError(eErrID_ValueOverflowFromNumConvert, 
					type_str[int(eTargetType)]);
				return false;
			}
		}
	}
	if (eBasicObjType_UInt32 == eTargetType)
	{
		if (eBasicObjType_Int32 == m_eResultType)
		{
			if (m_Result < 0)
			{
				m_cErrInfo.PrintError(eErrID_FailConvertMinusToUInt);
				return false;
			}
		}

		if (eBasicObjType_Int16 == m_eResultType)
		{
			if (m_Result < 0)
			{
				m_cErrInfo.PrintError(eErrID_FailConvertMinusToUInt);
				return false;
			}
		}

		if (eBasicObjType_Int64 == m_eResultType)
		{
			if (m_Result < 0)
			{
				m_cErrInfo.PrintError(eErrID_FailConvertMinusToUInt);
				return false;
			}
		}
		
		else if (eBasicObjType_Float == m_eResultType)
		{
			if (m_Result < 0)
			{
				m_cErrInfo.PrintError(eErrID_FailConvertMinusToUInt);
				return false;
			}
			else if (m_Result > double(_MAXUINT))
			{
				m_cErrInfo.PrintError(eErrID_ValueOverflowFromNumConvert, 
					type_str[int(eTargetType)]);
				return false;
			}
			else if (double(((unsigned int)m_Result)) != m_Result)
			{
				m_cErrInfo.PrintError(eErrID_PrecisionLossFromNumConvert,
					type_str[int(m_eResultType)],
					type_str[int(eTargetType)]);
				value = double(((unsigned int)m_Result));
			}
		}
		else if (basic_objtype_uint8 == m_eResultType || eBasicObjType_UInt32 == m_eResultType)
		{
			if (m_Result > double(_MAXUINT))
			{
				m_cErrInfo.PrintError(eErrID_ValueOverflowFromNumConvert, 
					type_str[int(eTargetType)]);
				return false;
			}
		}
	}
	else if (eBasicObjType_Int32 == eTargetType)
	{
		if (eBasicObjType_Int32 == m_eResultType)
		{
			if (m_Result < double(_MAXINT) && m_Result > double(_MAXINT))
			{
				m_cErrInfo.PrintError(eErrID_ValueOverflowFromNumConvert, 
					type_str[int(eTargetType)]);
				return false;
			}
		}

		if (eBasicObjType_Int16 == m_eResultType)
		{
			if (m_Result < double(_MAXINT) && m_Result > double(_MAXINT))
			{
				m_cErrInfo.PrintError(eErrID_ValueOverflowFromNumConvert, 
					type_str[int(eTargetType)]);
				return false;
			}
		}

		if (eBasicObjType_Int64 == m_eResultType)
		{
			if (m_Result < double(_MAXINT) && m_Result > double(_MAXINT))
			{
				m_cErrInfo.PrintError(eErrID_ValueOverflowFromNumConvert, 
					type_str[int(eTargetType)]);
				return false;
			}
		}
		
		else if (basic_objtype_uint8 == m_eResultType || eBasicObjType_UInt32 == m_eResultType)
		{
			if (m_Result > double(_MAXINT))
			{
				m_cErrInfo.PrintError(eErrID_ValueOverflowFromNumConvert, 
					type_str[int(eTargetType)]);
				return false;
			}
		}
		else if (eBasicObjType_Float == m_eResultType)
		{
			if (m_Result < double(_MAXINT) && m_Result > double(_MAXINT))
			{
				m_cErrInfo.PrintError(eErrID_ValueOverflowFromNumConvert, 
					type_str[int(eTargetType)]);
				return false;
			}
			else if (double(((int)m_Result)) != m_Result)
			{
				m_cErrInfo.PrintError(eErrID_PrecisionLossFromNumConvert,
					type_str[int(m_eResultType)],
					type_str[int(eTargetType)]);
				value = double(((int)m_Result));
			}
		}
	}
	return true;
}

bool expression::AnalyzeExpression(void)
{
	Reset();

	double value;
	string tkn;
	EExprTokenType eType;
	EGetExprTokenResult res = eGetExprRes_Unknown;
	EExprTokenType* pNextAllowList = GetNextAllowTypeList(eExprTokenType_Unknown);

	while (eGetExprRes_EndOfExpr != (res = GetNextToken(value, eType, tkn)))
	{
		if (eGetExprRes_Error == res) return false;

		if (!IsInAllowList(value, eType, tkn, pNextAllowList))
		{
			m_cErrInfo.PrintError(eErrID_UnexpectedExpressionToken);
			return false;
		}
		pNextAllowList = GetNextAllowTypeList(eType);
		if (!HandleToken(value, eType, tkn))
			return false;
	}

	while (!IsOptStackEmpty())
	{
		double v1, v2, r;
		EExprTokenType op;
		if (!PopOperator(op)) return false;
		if (eExprTokenType_LeftBracket == op || eExprTokenType_RightBracket == op)
		{
			m_cErrInfo.PrintError(eErrID_ExpressionMismatchWithBracket);
			return false;
		}
		else if (!IsOperator(op))
		{
			m_cErrInfo.PrintError(eErrID_ExpressionInternalError);
			return false;
		}
		if (!PopNumber(v2)) return false;
		if (!PopNumber(v1)) return false;
		if (!BasicCalc(v1, v2, op, r)) return false;
		if (!PushNumber(r)) return false;
	}

	// finally get the result
	if (!PopNumber(m_Result)) return false;

	// the operator stack comes empty means
	// the number stack also become empty, make sure this happened
	assert(IsNumStackEmpty());
	return true;
}

unsigned int expression::GetPriority(EExprTokenType eType)
{
	unsigned int ret;
	switch (eType)
	{
	case eExprTokenType_Add:
	case eExprTokenType_Sub:
		ret = 1;
		break;

	case eExprTokenType_Mul:
	case eExprTokenType_Div:
		ret = 2;
		break;

	case eExprTokenType_LeftBracket:
		ret = 0;
		break;

	case eExprTokenType_RightBracket:
		ret = 3;
		break;

	default: ret = 4; break;
	}
	assert(ret != 4);
	return ret;
}

bool expression::HandleToken(double& value, EExprTokenType& eType, const string& tkn)
{
	if (eExprTokenType_Number == eType)
		PushNumber(value);
	else if (IsOperator(eType))
	{
		if (IsOptStackEmpty())
		{
			if (!PushOperator(eType)) return false;
			return true;
		}

		EExprTokenType ePrevType = GetOpTop();
		assert(eExprTokenType_Unknown != ePrevType);

		if (GetPriority(ePrevType) >= GetPriority(eType))
		{
			// conduct a calculation
			double v1, v2, r;
			EExprTokenType op;
			if (!PopNumber(v2)) return false;
			if (!PopNumber(v1)) return false;
			if (!PopOperator(op)) return false;
			assert(IsOperator(op));
			if (!BasicCalc(v1, v2, op, r)) return false;
			if (!PushNumber(r)) return false;
		}
		if (!PushOperator(eType)) return false;
	}
	else if (eExprTokenType_LeftBracket == eType)
	{
		if (!PushOperator(eType))
			return false;
	}
	else if (eExprTokenType_RightBracket == eType)
	{
		EExprTokenType ePrevType;
		while (PopOperator(ePrevType))
		{
			if (eExprTokenType_LeftBracket == ePrevType)
				return true;
			assert(IsOperator(ePrevType));
			double v1, v2, r;
			if (!PopNumber(v2)) return false;
			if (!PopNumber(v1)) return false;
			if (!BasicCalc(v1, v2, ePrevType, r)) return false;
			if (!PushNumber(r)) return false;
		}
		m_cErrInfo.PrintError(eErrID_ExpressionMismatchWithBracket);
		return false;
	}
	else return false;
	return true;
}

bool expression::BasicCalc(double& v1, double& v2, EExprTokenType eType, double& result)
{
	switch (eType)
	{
	case eExprTokenType_Add:
		result = v1 + v2;
		break;

	case eExprTokenType_Sub:
		result = v1 - v2;
		break;

	case eExprTokenType_Mul:
		result = v1 * v2;
		break;

	case eExprTokenType_Div:
	{
		if (Double_IsZero(v2))
		{
			m_cErrInfo.PrintError(eErrID_ExpressionDivideByZero);
			return false;
		}
		if (eBasicObjType_Int32 == m_eResultType
			|| basic_objtype_uint8 == m_eResultType
			|| eBasicObjType_UInt32 == m_eResultType)
			// convert to int/uint
			result = double(int(v1) / int(v2));
		else result = v1 / v2;
	}	break;

	default: return false;
	}
	return true;
}

bool expression::ParseNumberIdentifier(number_type& eNumType, double& value)
{
	return false;
}