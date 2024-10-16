#ifndef __CXX_RPCC_TOKEN_H__
#define __CXX_RPCC_TOKEN_H__

#include <stdio.h>
#include <string>
#include "rpcc.h"
#include "grammar.h"
#include "list.h"

using namespace std;

class srcfile_stack
{
public:
	srcfile_stack();
	~srcfile_stack();

	struct srcfile_node
	{
		static const int LN_MAX_CHAR = 1024;

		srcfile_node();
		~srcfile_node();

		bool LoadLine(void);
		void KillSpace(void);
		const char* GetSymbol(size_t& sz);
		const char* CheckNumber(size_t& sz);
		const char* CheckDoubleColon(size_t& sz);
		bool OmitComments(void);
		void OmitCurrentLine(void);

		list_item	OwnerList;
		FILE*		fp;
		char		m_CurrLine[LN_MAX_CHAR];
		char*		m_pCurPos;
		unsigned int m_LineNum;
		unsigned int m_LinePosition;

		string		cFileName;
		string		cFullFileName;
	};

	bool push(string fullname);
	bool Pop(void);
	const char *GetSymbol(size_t& sz);

	const string& GetFileName(void);
	unsigned int GetLineNumber(void);
	unsigned int GetLinePosition(void);

	bool IsFullNameExist(string fullname);
	bool GotoNextLine(void);

private:
	srcfile_node* GetTop(void);

private:
	list_item	m_FileList;

private:
	srcfile_stack(const srcfile_stack&);
	void operator=(const srcfile_stack&);
};

class token
{
	friend class token_parser;
public:
	token(const string& name, ETokenType eType, ETokenID eId)
		: m_cName(name), m_eType(eType), m_eId(eId) {
	}
	token(const token& tkn) {
		if (this == &tkn)
			return;
		m_cName = tkn.m_cName;
		m_eType = tkn.m_eType;
		m_eId = tkn.m_eId;
		m_Data = tkn.m_Data;
	}
	void operator=(const token& tkn) {
		if (this == &tkn)
			return;
		m_cName = tkn.m_cName;
		m_eType = tkn.m_eType;
		m_eId = tkn.m_eId;
		m_Data = tkn.m_Data;
	}
	token() : m_eType(tokentype_unknown), m_eId(eTokenID_Unknown) {}
	~token() {}

	char operator[](int pos) {
		if (pos < 0) {
			if (-pos > (int)m_cName.length())
				return '\0';
			return m_cName.c_str()[m_cName.length() + pos];
		}
		if (pos > (int)m_cName.length())
			return '\0';
		return m_cName.c_str()[pos];
	}
	const string& getname(void) const {
		return m_cName;
	}
	operator const string&() const {
		return m_cName;
	}

	ETokenType GetType(void) const {
		return m_eType;
	}

	ETokenID GetID(void) const {
		return m_eId;
	}

	bool operator==(const string& s) const {
		return (m_cName == s) ? true : false;
	}

	bool operator!=(const string& s) const {
		return (m_cName != s) ? true : false;
	}
	bool operator==(ETokenType eType) const {
		return (eType == m_eType) ? true : false;
	}

	bool operator!=(ETokenType eType) const {
		return (eType != m_eType) ? true : false;
	}

	bool operator==(ETokenID eId) const {
		return (m_eId == eId) ? true : false;
	}

	bool operator!=(ETokenID eId) const {
		return (m_eId != eId) ? true : false;
	}

	void SetNumber(number_type eType, double& val) {
		m_Data.Number.eNumType = eType;
		m_Data.Number.NumValue = val;
	}

	bool GetNumber(number_type eType, double& val) {
		if (eTokenType_Number != m_eType)
			return false;
		eType = m_Data.Number.eNumType;
		val = m_Data.Number.NumValue;
		return true;
	}

private:
	string m_cName;
	ETokenType m_eType;
	ETokenID m_eId;

	union TData {
		struct TNumber
		{
			number_type	eNumType;
			double		NumValue;
		} Number;
	} m_Data;
};

inline static bool operator==(const string& s, const token& tkn)
{
	return (tkn == s);
}

inline static bool operator!=(const string& s, const token& tkn)
{
	return (tkn != s);
}

inline static bool operator==(ETokenType eType, const token& tkn)
{
	return (tkn == eType);
}

inline static bool operator!=(ETokenType eType, const token& tkn)
{
	return (tkn != eType);
}

inline static bool operator==(ETokenID eId, const token& tkn)
{
	return (tkn == eId);
}

inline static bool operator!=(ETokenID eId, const token& tkn)
{
	return (tkn != eId);
}

class token_parser
{
	friend class TTokenParserInitializer;
public:
	token_parser(srcfile_stack& cFileStack);
	~token_parser();

	srcfile_stack& get_filestack(void) {
		return m_cFileStack;
	}

	const token& GetToken(void);
	void TokenPushBack(void) {
		set_flags(eTokenParserFlag_TokenPushedBack);
	}
	bool OmitCurrentLine(void) {
		clear_flags(eTokenParserFlag_TokenPushedBack);
		return m_cFileStack.GotoNextLine();
	}

	DeclareFlagOperations();

private:
	bool IsNumber(char c);
	bool CheckAsNumber(const char *token, size_t sz);
	bool ErrorNumber(const char *token, size_t sz);
	bool SetNumber(const char *token, size_t sz, double &num, bool minus, bool is_float);

	bool CheckAsIdentifier(const char *token, size_t sz);
	void AnalyzeUserToken(const char *token, size_t sz);

private:
	enum {
		eTokenParserFlag_TokenPushedBack = 1,
	};

private:
	srcfile_stack&	m_cFileStack;
	token				m_cCurrToken;
	unsigned int		m_Flags;

	struct TReservedIdentifier
	{
		const char *pIdName;
		ETokenType eType;
		ETokenID   eId;
	};

	static void Initialize(void);
	static int compare(const void*, const void*);
	static TReservedIdentifier s_RsvIdList[];

private:
	token_parser(const token_parser&);
	void operator=(const token_parser&);
};

#endif
/* EOF */
