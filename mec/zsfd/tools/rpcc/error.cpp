#include "error.h"
#include <stdio.h>
#include <stdlib.h>

struct ErrorInfoDef {
	const char *errstr;
	bool is_error;
} _errstr[] =
{
	/* eErrID_ImportFileNameWithoutLeftQuotation */
	{ "missing left quotation mark (\") for imported filename", true },

	/* eErrID_ImportFileNameWithoutRightQuotation */
	{ "missing right quotation mark (\") for imported filename", true },

	/* errid_invalid_number */
	{ "invalid number '%s'", true },

	/* eErrID_IdentifierExpectedForThisDeclaration */
	{ "invalid variable name '%s', expect an identifier", true },

	/* eErrID_UnsignedCannotWorkWithSpecificType */
	{ "'unsigned' cannot work with '%s'", false },

	/* eErrID_InvalidVariableType */
	{ "invalid type '%s' in the statement, %s", true },

	/* eErrID_ExpectSemiColon */
	{ "expect a ';' in the statement", true },

	/* eErrID_InternalError */
	{ "internal error at %s:%u: %s", true },

	/* eErrID_IdentifierRedefined */
	{ "'%s' redefined, already defined as %s", true },

	/* errid_expect_equal */
	{ "expect a '=' in the statement", true },

	/* eErrID_CannotCovertType */
	{ "cannot covert '%s' to '%s'", true },

	/* eErrID_FailConvertMinusToUInt */
	{ "fail to convert a negative number to 'unsigned int'", true },

	/* eErrID_PrecisionLossFromNumConvert */
	{ "precision loss when converting from '%s' to '%s'", false },

	/* eErrID_ValueOverflowFromNumConvert */
	{ "value exceeds the range of type '%s'", true },

	/* eErrID_ExpectLeftCurlyBracket */
	{ "expect '{' in the statement", true },

	/* eErrID_ReachEOFBeforeMatchingRightCurlyBracket */
	{ "reach end of file before matching with '{'", true },

	/* eErrID_UnrecgnizedStatement */
	{ "'%s': the statement is invalid or not allowed here", true },

	/* eErrID_ExpectVariableName */
	{ "expect a variable name in statement of %s", true },

	/* eErrID_ExpectRightSquareBracket */
	{ "expect a ']' in the statement for array declaration", true },

	/* eErrID_InvalidArraySize */
	{ "array size shall be larger than 0", true },

	/* eErrID_ExpectLeftBracket */
	{ "expect a '(' in the statement", true },

	/* eErrID_ExpectRightBracket */
	{ "expect a ')' in the statement", true },

	/* eErrID_2DimemsionArrayNotSupported */
	{ "2-dimension array is not supported by TinyRPC", true },

	/* eErrID_MethodReturnAnArray */
	{ "method returns an array, which is not allowed", true },

	/* eErrID_ExpressionStackOverflow */
	{ "expression too complicate (internal stack overflow)", true },

	/* eErrID_ExpressionStackEmpty */
	{ "expression stack empty (internal error)", true },

	/* eErrID_UnexpectedExpressionToken */
	{ "unexpected expression token", true },

	/* eErrID_ExpressionMismatchWithBracket */
	{ "expresion mismatch with left/right bracket", true },

	/* eErrID_ExpressionInternalError */
	{ "internal error occurred in parsing expression", true },

	/* eErrID_ExpressionDivideByZero */
	{ "divided by zero occurred when parsing expression", true },

	/* eErrID_ExpectDoubleColon */
	{ "expect a '::' in the statement", true },

	/* eErrID_ExpectEnumName */
	{ "expect a name for \"enum\" statement", true },

	/* eErrID_ExpectComma */
	{ "expect a comma \",\" in the statement", true },

	/* eErrID_ExpectBooleanPropertyValue */
	{ "expect a boolean value for property '%s' of interface '%s'", true },

	/* eErrID_ConstructorCouldntBeVirtual */
	{ "Constructor of an interface could not be declared as 'virtual'", true},

	/* eErrID_ConsturctorNameNotEqualToInterfaceName */
	{ "Constructor name should be the same as the interface name", true },
	
	/* eErrID_ConstructorParameterCouldbeInOnly */
	{ "[out], [inout] could not be applied for parameters of a constructor", false },

	/* eErrID_UnrecognizedPropertyName */
	{ "unrecognized property name '%s' in interface '%s' definition", true },

	/* eErrID_ObservableIFCannotBeStruMember */
	{ "an observable interface could not be a member of structure", true },

	/* eErrID_CtorNotAllowedInObservableIF */
	{ "constructor is not allowed in an observable interface", false },

	/* eErrID_NoNeedOfDefaultCtorDefinition */
	{ "explicit definition of default ctor: '%s()' is not needed", false },

	/* eErrID_ObservableIFCouldnotBeSingleton */
	{ "an observable interface could not be defined as 'singleton'", true },

	/* eErrID_SingletonIFCouldnotBeObservable */
	{ "a singleton interface could not be defined as 'observable'", true },

	/* eErrID_AttributeCannotBeArray */
	{ "an attribute could not be declared as an array", true },

	/* eErrID_EventParameterCouldbeInOnly */
	{ "[out], [inout] could not be applied for parameters of an event (omit and takes no effect)", false },

	/* eErrID_ImportFileNotFound */
	{ "Cannot load the import file '%s': file not found", true },

	/* eErrID_LongShortNotSupport */
	{ "\"long\" or \"short\" type is not support!", true },
	
	/* eErrID_TypeDefOnlyBasicTypeNotArrayAllowed */
	{ "Typedef error: Only BasicType allowed. Any Array is not allowed!", true },

	/* errid_treat_uint8_as_uint32 */
	{ "Octet is deprecated. zrpcc treats it as `unsigned int' in order to support more languages and architectures.", false },
};

TErrorInfo::TErrorInfo(token_parser &p)
: m_cTokenParser(p)
{
}

void TErrorInfo::doPrintError(EErrorInfoID eid, va_list ap)
{
	printf("%s:%u:%u: %s: ",
		m_cTokenParser.get_filestack().GetFileName().c_str(),
		m_cTokenParser.get_filestack().GetLineNumber(),
		m_cTokenParser.get_filestack().GetLinePosition(),
		(_errstr[(int)eid].is_error) ? "error" : "warning"
	);
	vprintf(_errstr[(int)eid].errstr, ap);
	printf("\n");
	if( false == IsThereErrors() )
	{
		if (_errstr[(int)eid].is_error )
			m_bIsErrorFlag = true;
	}
}

void TErrorInfo::PrintError(EErrorInfoID eid, ...)
{
	va_list ap;

	va_start(ap, eid);
	doPrintError(eid, ap);
	va_end(ap);
}

bool TErrorInfo::m_bIsErrorFlag = false;
bool TErrorInfo::IsThereErrors()
{
	return m_bIsErrorFlag;
}

void _Panic_OutOfMemory(const char *f, unsigned int l)
{
	printf("Fatal error(at %s:%u): out of memory.\n", f, l);
	exit(1);
}

void _Panic_InternalError(const char *f, unsigned int l, const char *desc)
{
	printf("Internal error(at %s:%u): %s.\n", f, l, desc);
	exit(2);
}
