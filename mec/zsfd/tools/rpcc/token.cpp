#include <assert.h>
#include <stdlib.h>
#include "string.h"

#include "error.h"
#include "token.h"

srcfile_stack::srcfile_node::srcfile_node()
: m_pCurPos(NULL)
, m_LineNum(0)
, m_LinePosition(0)
{
}

srcfile_stack::srcfile_node::~srcfile_node()
{
	if (NULL != fp)
		fclose(fp);
}

const string& srcfile_stack::GetFileName(void)
{
	static string _null = "<null>";
	srcfile_node *node = GetTop();
	if (NULL == node) return _null;
	return node->cFileName;
}

unsigned int srcfile_stack::GetLineNumber(void)
{
	srcfile_node *node = GetTop();
	if (NULL == node) return 0;
	return node->m_LineNum;
}

unsigned int srcfile_stack::GetLinePosition(void)
{
	srcfile_node *node = GetTop();
	if (NULL == node) return 0;
	return node->m_LinePosition;
}

static const char _space_identifier[] = " \t\r\n";
static const char _basic_identifier[] = "{}()[];,#+-*/=:";

void srcfile_stack::srcfile_node::KillSpace(void)
{	
	while (*m_pCurPos && strchr(_space_identifier, *m_pCurPos))
		m_pCurPos++;
}

inline void srcfile_stack::srcfile_node::OmitCurrentLine(void)
{
	m_pCurPos = NULL;
}

bool srcfile_stack::srcfile_node::OmitComments(void)
{
	if (*m_pCurPos == '/')
	{
		char *tmp = m_pCurPos + 1;
		if (!*tmp) return false;

		// handle the C++ style // comments
		if (*tmp == '/')
		{
			m_pCurPos = tmp + 1;

			for (;;)
			{
				if (!m_pCurPos || !*m_pCurPos)	// an empty line
					break;

				// move to the end of the line to see if there is a '\\'
				size_t len = strlen(m_pCurPos);
				if (m_pCurPos[len - 1] == '\\')
				{
					// this means we need to load a new line, which is still a comment line
					if (!LoadLine())
						return false;
				}
				else break;
			}
			// just omit the current line
			OmitCurrentLine();
			return true;
		}
		else if (*tmp == '*')
		{
			m_pCurPos = tmp + 1;

			// seeking for '*/'
			char* eoc;
			while (NULL == (eoc = strstr(m_pCurPos, "*/")))
			{
				if (!LoadLine())
					return false;
			}
			m_pCurPos = eoc + 2;
			return true;
		}
	}
	return false;
}

const char* srcfile_stack::srcfile_node::CheckDoubleColon(size_t& sz)
{
	sz = 2;
	char *ret = m_pCurPos;
	if (m_pCurPos[0] == ':' && m_pCurPos[1] == ':')
	{
		m_pCurPos += 2;
		return ret;
	}
	return NULL;
}

const char* srcfile_stack::srcfile_node::CheckNumber(size_t& sz)
{
	sz = 0;
	char *ret = m_pCurPos, *start = ret;
	if (*start == '+' || *start == '-') { sz++; start++; }

	if ((*start < '0' || *start > '9') && *start != '.')
		return NULL;

	// at a high possibility of a number (here regard as a number)
	for (; (*start >= '0' && *start <= '9') || *start == '.'; ++start, sz++);

	// see if there is a 'e' or 'E'
	if (*start == 'e' || *start == 'E') ++start, ++sz;
	else { m_pCurPos = start; return ret; }

	// see power of a number (for float)
	if (*start == '+' || *start == '-') ++start, ++sz;
	for (; *start >= '0' && *start <= '9'; ++start, sz++);

	m_pCurPos = start;
	return ret;
}

const char *srcfile_stack::srcfile_node::GetSymbol(size_t& sz)
{
	do
	{
		if (NULL == m_pCurPos)
			return NULL;

		if (*m_pCurPos == '\0')
			return NULL;

		KillSpace();
		if (*m_pCurPos == '\0')
			return NULL;
	}
	while (OmitComments());
	
	// this check must be prior to basic identifier check
	// this is to see if it is a double-colon ('::')
	const char *double_colon = CheckDoubleColon(sz);
	if (double_colon) return double_colon;

	// check if it is a basic identifier
	if (strchr(_basic_identifier, *m_pCurPos))
	{
		sz = 1;
		return m_pCurPos++;
	}

	// this is to see if it is a number
	const char *num_start = CheckNumber(sz);
	if (num_start) return num_start;

	// as a normal case, find the end of an identifier
	char *end = m_pCurPos;
	while (*end && !strchr(_space_identifier, *end)
		&& !strchr(_basic_identifier, *end))
		end++;
	
	sz = end - m_pCurPos;
	const char *ret = m_pCurPos;
	m_LinePosition = m_pCurPos - m_CurrLine;
	m_pCurPos = end;

	return ret;
}

srcfile_stack::srcfile_stack()
{
}

srcfile_stack::~srcfile_stack()
{
	while (Pop());
}

bool srcfile_stack::Pop(void)
{
	srcfile_node *node = GetTop();
	if (!node) return false;

	node->OwnerList.Delete();
	delete node;
	return true;
}

srcfile_stack::srcfile_node* srcfile_stack::GetTop(void)
{
	if (m_FileList.is_empty())
		return NULL;

	list_item *pItem = m_FileList.GetPrev();
	return LIST_ENTRY(srcfile_node, OwnerList, pItem);
}

bool srcfile_stack::IsFullNameExist(string fullname)
{
	list_item *pItem = m_FileList.getnext();
	for (; pItem != &m_FileList; pItem = pItem->getnext())
	{
		srcfile_node* node = LIST_ENTRY(srcfile_node, OwnerList, pItem);
		if (node->cFullFileName == fullname)
			return true;
	}
	return false;
}

bool srcfile_stack::push(string fullname)
{
	FILE *fp = fopen(fullname.c_str(), "r");
	if (NULL == fp)
		return false;

	srcfile_node *node = new srcfile_node;
	node->fp = fp;
	node->cFullFileName = fullname;

	// extract the file name from full name
	const char *start = fullname.c_str();
	const char *end = fullname.c_str() + fullname.length();
	for (; end > start && *end != '\\' && *end != '/'; --end);
	if (*end == '\\' || *end == '/') ++end;

	node->cFileName = fullname.assign(end, fullname.length() - (end - start));
	node->OwnerList.AddTo(m_FileList);
	return true;
}

bool srcfile_stack::srcfile_node::LoadLine(void)
{
	assert(NULL != fp);
	if (feof(fp) || !fgets(m_CurrLine, LN_MAX_CHAR, fp))
	{
		m_pCurPos = NULL;
		return false;
	}

	size_t sz = strlen(m_CurrLine);
	if (m_CurrLine[sz - 1] != '\n')
		; // todo, error
	else m_CurrLine[sz - 1] = '\0';

	m_pCurPos = m_CurrLine;
	m_LineNum++;

	return true;
}


bool srcfile_stack::GotoNextLine(void)
{
	srcfile_node *node = GetTop();
	if (NULL == node) return false;

	size_t sz;
	while (node->GetSymbol(sz));
	return true;
}

const char* srcfile_stack::GetSymbol(size_t& sz)
{
	srcfile_node *node;
	
	while (NULL != (node = GetTop()))
	{
		const char *ret = node->GetSymbol(sz);
		if (NULL == ret)
		{
			// try to load a new line
			if (!node->LoadLine())
			{
				// try to load a new file
				Pop();
				node = GetTop();
			}
		}
		else return ret;
	}
	return NULL;
}

token_parser::TReservedIdentifier token_parser::s_RsvIdList[] =
{
	{ ",", eTokenType_Basic, eTokenID_Comma },
	{ ";", eTokenType_Basic, eTokenID_Semicolon },
	{ "{", eTokenType_Basic, eTokenID_LeftCurlyBracket },
	{ "}", eTokenType_Basic, eTokenID_RightCurlyBracket },
	{ "[", eTokenType_Basic, eTokenID_LeftSquareBracket },
	{ "]", eTokenType_Basic, eTokenID_RightSquareBracket },
	{ "(", eTokenType_Basic, eTokenID_LeftBracket },
	{ ")", eTokenType_Basic, eTokenID_RightBracket },
	{ ":", eTokenType_Basic, eTokenID_Colon },
	{ "::", eTokenType_Basic, eTokenID_DoubleColon },
	{ "+", eTokenType_Operator, eTokenID_Add },
	{ "-", eTokenType_Operator, eTokenID_Sub },
	{ "*", eTokenType_Operator, eTokenID_Mul },
	{ "/", eTokenType_Operator, eTokenID_Div },
	{ "=", eTokenType_Basic, eTokenID_Equal },
	{ "module", eTokenType_Reserved, eTokenID_Module },
	{ "interface", eTokenType_Reserved, eTokenID_Interface },
	{ "import", eTokenType_Reserved, eTokenID_Import },
	{ "const", eTokenType_Reserved, eTokenID_Const },
	{ "unsigned", eTokenType_Reserved, eTokenID_Unsigned },
	{ "int", eTokenType_Reserved, eTokenID_Int },
	{ "short", eTokenType_Reserved, eTokenID_Short},
	{ "long", eTokenType_Reserved, eTokenID_Long },
	{ "float", eTokenType_Reserved, eTokenID_Float },
	{ "typedef", eTokenType_Reserved, eTokenID_Typedef },
	{ "string", eTokenType_Reserved, eTokenID_String },
	{ "struct", eTokenType_Reserved, eTokenID_Struct },
	{ "void", eTokenType_Reserved, eTokenID_Void },
	{ "in", eTokenType_Reserved, eTokenID_In },
	{ "out", eTokenType_Reserved, eTokenID_Out },
	{ "inout", eTokenType_Reserved, eTokenID_Inout },
	{ "bool", eTokenType_Reserved, eTokenID_Boolean },
	{ "byte", eTokenType_Reserved, eTokenID_Octet },
	{ "enum", eTokenType_Reserved, eTokenID_Enum },
	{ "property", eTokenType_Reserved, eTokenID_Property },
	{ "attribute", eTokenType_Reserved, eTokenID_Attribute },
	{ "singleton", eTokenType_Reserved, eTokenID_Singleton },
	{ "true", eTokenType_Reserved, eTokenID_True },
	{ "false", eTokenType_Reserved, eTokenID_False },
	{ "virtual", eTokenType_Reserved, eTokenID_Virtual },
	{ "observable", eTokenType_Reserved, eTokenID_Observable },
	{ "event", eTokenType_Reserved, eTokenID_Event },
	{ "bytebuff", eTokenType_Reserved, eTokenID_Stream },
};

token_parser::token_parser(srcfile_stack& cFileStack)
: m_cFileStack(cFileStack)
, m_Flags(0)
{
}

token_parser::~token_parser()
{
}

const token& token_parser::GetToken(void)
{
	if (test_flags(eTokenParserFlag_TokenPushedBack))
	{
		clear_flags(eTokenParserFlag_TokenPushedBack);
		return m_cCurrToken;
	}

	size_t sz;
	const char *tkn = m_cFileStack.GetSymbol(sz);

	if (NULL == tkn)
	{
		m_cCurrToken = token();
		return m_cCurrToken;
	}

	// make a bsearch to identify the type of token
	string tmp(tkn, sz);
	TReservedIdentifier *tbl = NULL;
	int mid, start = 0;
	int end = sizeof(s_RsvIdList) / sizeof(TReservedIdentifier) - 1;

	while (start <= end)
	{
		mid = (start + end) / 2;
		int ret = strcmp(tmp.c_str(), s_RsvIdList[mid].pIdName);
		
		if (!ret) { tbl = s_RsvIdList + mid; break; }
		else if (ret > 0) start = mid + 1;
		else end = mid - 1;
	}
	if (NULL == tbl) AnalyzeUserToken(tkn, sz);
	else m_cCurrToken = token(tbl->pIdName, tbl->eType, tbl->eId);
	return m_cCurrToken;
}

inline bool token_parser::IsNumber(char c)
{
	return (c >= '0' && c <= '9') ? true : false;
}

bool token_parser::SetNumber(const char *tkn, size_t sz, double &num, bool minus, bool is_float)
{
	if (minus)
	{
		m_cCurrToken.m_Data.Number.NumValue = -num;
		m_cCurrToken.m_Data.Number.eNumType = (is_float) ? Float : Int;
		m_cCurrToken.m_eId = (is_float) ? eTokenID_Number_Float : eTokenID_Number_Int;
	}
	else
	{
		m_cCurrToken.m_Data.Number.NumValue = num;
		m_cCurrToken.m_Data.Number.eNumType = (is_float) ? Float : UInt;
		m_cCurrToken.m_eId = (is_float) ? eTokenID_Number_Float : eTokenID_Number_UInt;
	}
	m_cCurrToken.m_cName.assign(tkn, sz);
	m_cCurrToken.m_eType = eTokenType_Number;
	return true;
}

bool token_parser::ErrorNumber(const char *tkn, size_t sz)
{
	string tmp;
	tmp.assign(tkn, sz);
	TErrorInfo cErrInfo(*this);
	cErrInfo.PrintError(errid_invalid_number, tmp.c_str());
	return false;
}

bool token_parser::CheckAsIdentifier(const char *tkn, size_t sz)
{
	if (IsNumber(*tkn))
		return false;

	size_t size = 0;
	const char *start = tkn;

	for (;sz && ((*tkn >= 'a' && *tkn <= 'z') ||
		(*tkn >= 'A' && *tkn <= 'Z') || IsNumber(*tkn)
		|| *tkn == '_'); tkn++, sz--, ++size);

	if (sz) return false;

	string name;
	name.assign(start, size);
	m_cCurrToken = token(name, eTokenType_Identifier, eTokenID_User);
	return true;
}

bool token_parser::CheckAsNumber(const char *tkn, size_t sz)
{
	double num = 0;
	bool minus = false;
	bool is_float = false;

	size_t org_sz = sz;
	const char *org_token = tkn;

	if (*tkn == '+') { minus = false; ++tkn; --sz; }
	else if (*tkn == '-') { minus = true; ++tkn; --sz; }
	if (!sz) return false;	// not a number

	// analyze the [100].32e+12 part
	const char *part1 = tkn;
	for (; sz && IsNumber(*part1); ++part1, --sz)
		num = num * 10 + (*part1 - '0');
	if (!sz) return SetNumber(org_token, org_sz, num, minus, is_float);

	if (part1 == tkn)
	{
		// see if there is a ".", like -.2, or there is an error
		if (*part1 != '.') return false; // not a number
	}


	tkn = part1;
	// like -10.2
	if (*tkn == '.')
	{
		// handle 100.[32]e+12
		is_float = true;
		double tmp = 10;
		for (++tkn, --sz; sz && IsNumber(*tkn); ++tkn, --sz, tmp *= 10)
			num += double(*tkn - '0') / tmp;
	}
	if (!sz) return SetNumber(org_token, org_sz, num, minus, is_float);

	// handle 100.32[e]+12
	if (*tkn != 'e' && *tkn != 'E')
		return ErrorNumber(org_token, org_sz);

	// must be a float number
	is_float = true;
	tkn++; sz--;
	if (!sz) return ErrorNumber(org_token, org_sz);

	// handle 100.32e[+]12
	bool minus_power = false;
	if (*tkn == '+') { tkn++; sz--; }
	else if (*tkn == '-') { minus_power = true; tkn++; sz--; }
	if (!sz) return ErrorNumber(org_token, org_sz);

	// handle 100.32e+[12]
	unsigned int power = 0;
	for (; sz && IsNumber(*tkn); ++tkn, --sz)
		power = power * 10 + (*tkn - '0');
	if (sz) return ErrorNumber(org_token, org_sz);
	if (power > 100) return ErrorNumber(org_token, org_sz);

	for (; power; --power)
		num = (minus_power) ? (num / 10) : (num * 10);

	return SetNumber(org_token, org_sz, num, minus, is_float);
}

void token_parser::AnalyzeUserToken(const char *tkn, size_t sz)
{
	if (CheckAsNumber(tkn, sz))
		return;
	else if (CheckAsIdentifier(tkn, sz))
		return;
	else
	{
		string name;
		name.assign(tkn, sz);
		m_cCurrToken = token(name, tokentype_unknown, eTokenID_Unknown);
	}
}

int token_parser::compare(const void* a, const void* b)
{
	token_parser::TReservedIdentifier *pa = (token_parser::TReservedIdentifier*)a;
	token_parser::TReservedIdentifier *pb = (token_parser::TReservedIdentifier*)b;
	return strcmp(pa->pIdName, pb->pIdName);
}

void token_parser::Initialize(void)
{
	qsort(s_RsvIdList, sizeof(s_RsvIdList) / sizeof(token_parser::TReservedIdentifier),
		sizeof(token_parser::TReservedIdentifier), token_parser::compare);
}

static class TTokenParserInitializer
{
public:
	TTokenParserInitializer()
	{
		token_parser::Initialize();
	}
}
___sInitializer;