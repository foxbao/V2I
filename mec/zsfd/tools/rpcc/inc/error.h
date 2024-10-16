#ifndef __CXX_RPCC_ERROR_H__
#define __CXX_RPCC_ERROR_H__

#include "rpcc.h"
#include "token.h"
#include <stdarg.h>

enum EErrorInfoID
{
	eErrID_ImportFileNameWithoutLeftQuotation = 0,
	eErrID_ImportFileNameWithoutRightQuotation,
	errid_invalid_number,
	eErrID_IdentifierExpectedForThisDeclaration,
	eErrID_UnsignedCannotWorkWithSpecificType,
	eErrID_InvalidVariableType,
	eErrID_ExpectSemiColon,
	eErrID_InternalError,
	eErrID_IdentifierRedefined,
	errid_expect_equal,
	eErrID_CannotCovertType,
	eErrID_FailConvertMinusToUInt,
	eErrID_PrecisionLossFromNumConvert,
	eErrID_ValueOverflowFromNumConvert,
	eErrID_ExpectLeftCurlyBracket,
	eErrID_ReachEOFBeforeMatchingRightCurlyBracket,
	eErrID_UnrecgnizedStatement,
	eErrID_ExpectVariableName,
	eErrID_ExpectRightSquareBracket,
	eErrID_InvalidArraySize,
	eErrID_ExpectLeftBracket,
	eErrID_ExpectRightBracket,
	eErrID_2DimemsionArrayNotSupported,
	eErrID_MethodReturnAnArray,
	eErrID_ExpressionStackOverflow,
	eErrID_ExpressionStackEmpty,
	eErrID_UnexpectedExpressionToken,
	eErrID_ExpressionMismatchWithBracket,
	eErrID_ExpressionInternalError,
	eErrID_ExpressionDivideByZero,
	eErrID_ExpectDoubleColon,
	eErrID_ExpectEnumName,
	eErrID_ExpectComma,
	eErrID_ExpectBooleanPropertyValue,
	eErrID_ConstructorCouldntBeVirtual,
	eErrID_ConstructorNameNotEqualToInterfaceName,
	eErrID_ConstructorParameterCouldbeInOnly,
	eErrID_UnrecognizedPropertyName,
	eErrID_ObservableIFCannotBeStruMember,
	eErrID_CtorNotAllowedInObservableIF,
	eErrID_NoNeedOfDefaultCtorDefinition,
	eErrID_ObservableIFCouldnotBeSingleton,
	eErrID_SingletonIFCouldnotBeObservable,
	eErrID_AttributeCannotBeArray,
	eErrID_EventParameterCouldbeInOnly,
	eErrID_ImportFileNotFound,
	eErrID_LongShortNotSupport,
	eErrID_TypeDefOnlyBasicTypeNotArrayAllowed,
	errid_treat_uint8_as_uint32,
};

#define Panic_OutOfMemory()	_Panic_OutOfMemory(__FILE__, __LINE__)
void _Panic_OutOfMemory(const char *f, unsigned int l);
#define Panic_InternalError(e) _Panic_InternalError(__FILE__, __LINE__, e)
void _Panic_InternalError(const char *f, unsigned int l, const char *desc);

class TErrorInfo
{
public:
	TErrorInfo(token_parser &p);

	void PrintError(EErrorInfoID eid, ...);
	static bool IsThereErrors();

private:
	void doPrintError(EErrorInfoID eid, va_list ap);
	token_parser& m_cTokenParser;
	static bool m_bIsErrorFlag;
};
#endif
