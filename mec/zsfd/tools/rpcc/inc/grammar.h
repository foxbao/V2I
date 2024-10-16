#ifndef __CXX_RPCC_GRAMMAR_H__
#define __CXX_RPCC_GRAMMAR_H__

#include <stdio.h>
#include <string>
#include "rpcc.h"
#include "list.h"
#include "config.h"

using namespace std;

#define HASH_MAX	(16)
typedef list_item grammer_object_header[HASH_MAX];

unsigned int Str2ID(const char* name);

struct TArrayTypeObject
{
	EArrayType eArrayType;
	unsigned int ArraySize;
	TArrayTypeObject() : eArrayType(eArrayType_NotArray)
		, ArraySize(0) {}
	TArrayTypeObject(const TArrayTypeObject& c) {
		if (this != &c) {
			eArrayType = c.eArrayType;
			ArraySize = c.ArraySize;
		}
	}
};

class module;
class grammar_parser;
class typedef_object;
class userdef_type_object;

enum ETGrammarObjectFlags
{
	grammar_object_flags_code_generated = 1,
	grammar_object_flags_code_file_created = 2,
	eGrammarObjectFlag_RelatedCodeGenerted = 4,
	grammar_object_flags_globalscope = 8,
	grammer_object_flags_userdef_type_pb_generated = 16,
};

class TGrammarObject
{
public:
	TGrammarObject(module* pModule, const string& cName);
	TGrammarObject(module* pModule, const string& cName,
		grammar_parser* gpsr);
	virtual ~TGrammarObject();

public:
	void Add(grammer_object_header& header);
	
	module* GetModule(void) {
		return m_pModule;
	}

	const string& getname(void) {
		return m_cName;
	}

	grammar_parser* get_gammar_parser(void) {
		return _gammar_parser;
	}

	virtual bool generate_code(void) = 0;
	virtual bool global_scope_generate_code(void);
	virtual string get_fullname(module *pCurr);

	static list_item* FindObjectByName(grammer_object_header& header, const string& cName);

	DeclareFlagOperations();

public:
	list_item	m_OwnerList;

protected:
	string		m_cName;
	grammar_parser* _gammar_parser;
	module* m_pModule;
	unsigned int m_Flags;
};

class TEnumNode
{
public:
	TEnumNode(const string& cEnumNodeName, int value)
		: m_cEnumNodeName(cEnumNodeName), m_Value(value) {}
	~TEnumNode();

	int GetValue(void) {
		return m_Value;
	}

public:
	list_item	m_cEnumOwnerList;
	list_item	m_cOverallOwnerList;
	string		m_cEnumNodeName;
	int			m_Value;
};

class enumdef_object : public TGrammarObject
{
public:
	enumdef_object(module* pModule, const string& cName);
	static enumdef_object* Find(module *pModule, const string& cName);
	static enumdef_object* create_instance(module* pModule, const string& cName);

	void AddNode(TEnumNode *pNode);
	string get_fullname(module *pCurr);
    virtual string GetDefault(module *pCurr);

protected:
	list_item m_cEnumNodeList;
};

class userdef_type_object : public TGrammarObject
{
public:
	userdef_type_object(module* pModule, const string& cName);

	list_item& VariableListHeader(void) {
		return m_VariableList;
	}

	static userdef_type_object* Find(module* pModule, const string& cName);
	static userdef_type_object* create_instance(module* pModule, const string& cName);

	bool generate_related_object_code(void);
	module** GetDependentModule(void);
	string get_fullname(module *pCurr);

protected:
	list_item	m_VariableList;
};

class interface_object;

class TInterfaceNode
{
public:
	TInterfaceNode(interface_object* pIFObj, EIFNodeType eType);
	~TInterfaceNode();

	void AddTo(list_item& h) {
		m_OwnerList.AddTo(h);
	}

	EIFNodeType GetType(void) {
		return m_eType;
	}

	interface_object* GetInterfaceObject(void) {
		return m_pIFObject;
	}

	void SetNodeID(unsigned int id) {
		m_NodeID = id;
	}

	unsigned int GetNodeID(void) {
		return m_NodeID;
	}

	string& getname(void) {
		return m_cName;
	}

	list_item m_OwnerList;
	list_item m_VariableList;
	interface_object* m_pIFObject;
	EIFNodeType m_eType;
	string m_cName;
	unsigned int m_NodeID;

	enum EMethodFlags
	{
		eMethodFlag_Virtual = 1,
		eMethodFlag_Constructor = 2,
	};

	union TData {
		struct TMethodData
		{
			basic_objtype m_eRetValType;
			interface_object* m_pRetValIFObject;
			typedef_object* m_pRetValTypedefType;
			enumdef_object* m_pRetValEnumDef;
			userdef_type_object* m_pRetValUserDefType;
			unsigned int m_Flags;

			void set_flags(unsigned int f) {
				m_Flags |= f;
			}

			void clear_flags(unsigned int f) {
				m_Flags &= ~f;
			}

			bool test_flags(unsigned int f) {
				return ((m_Flags & f) == f) ? true : false;
			}
		} method;
		struct TAttributeData
		{
			basic_objtype _basic_type;
			interface_object* m_pIFObject;
			typedef_object* m_pTypedefType;
			enumdef_object* m_pEnumDef;
			userdef_type_object* m_pUserDefType;
		} attribute;
	} m_Data;
};

class interface_object : public TGrammarObject
{
public:
	interface_object(module* pModule, const string& cName);
	static interface_object* Find(module* pModule, const string& cName);
	static interface_object* create_instance(module* pModule, const string& cName);

	list_item& NodeList(void) {
		return m_NodeList;
	}

	bool generate_related_object_code(void);
	module** GetDependentModule(void);
	string get_fullname(module *pCurr);

	virtual bool generate_proxy_code(void);
	virtual bool generate_skeleton_code(void);

public:

	enum EInterfaceFlags
	{
		eInterfaceFlag_Singleton = 1,
		eInterfaceFlag_Observable = 2,
	};

	void SetInterfaceFlags(unsigned int f) {
		m_InterfaceFlags |= f;
	}

	void ClearInterfaceFlags(unsigned int f) {
		m_InterfaceFlags &= ~f;
	}

	bool TestInterfaceFlags(unsigned int f) {
		return ((m_InterfaceFlags & f) == f) ? true : false;
	}

private:
	list_item m_NodeList;
	unsigned int m_InterfaceFlags;
};

class TEventObject : public TGrammarObject
{
public:
	TEventObject(module* pModule, const string& cName);
	static TEventObject* Find(module* pModule, const string& cName);
	static TEventObject* create_instance(module* pModule, const string& cName);

	list_item& NodeList(void) {
		return m_NodeList;
	}

	bool generate_related_object_code(void);
	module** GetDependentModule(void);
	string get_fullname(module *pCurr);

private:
	list_item m_NodeList;
};

class typedef_object : public TGrammarObject
{
public:
	typedef_object(module* pModule, const string& cName, basic_objtype eType);
	typedef_object(module* pModule, const string& cName, typedef_object *pRefType);
	typedef_object(module* pModule, const string& cName, userdef_type_object *pRefType);
	typedef_object(module* pModule, const string& cName, enumdef_object *pEnumDef);
	typedef_object(module* pModule, const string& cName, interface_object* pIFObject);
	~typedef_object();

	static typedef_object* create_instance(module* pModule,
		const string& cName, basic_objtype eType);

	static typedef_object* create_instance(module* pModule,
		const string& cName, typedef_object *pRefType);

	static typedef_object* create_instance(module* pModule,
		const string& cName, userdef_type_object *pRefType);

	static typedef_object* create_instance(module* pModule,
		const string& cName, enumdef_object *pEnumDef);

	static typedef_object* create_instance(module* pModule,
		const string& cName, interface_object *pIFObject);

public:
	userdef_type_object* GetOriginalType(basic_objtype& eType,
		TArrayTypeObject& eArrayType, enumdef_object** ppEnumDef, interface_object **pIFObj);
	bool generate_related_object_code(void);

	void SetArrayType(TArrayTypeObject& obj) {
		m_ArrayType = obj;
	}

	TArrayTypeObject& GetArrayType(void) {
		return m_ArrayType;
	}

	typedef_object* GetRefType(void) {
		return m_pRefType;
	}


	module* GetDependentModule(void);
	string get_fullname(module *pCurr);

public:
	static typedef_object* Find(module* pModule, const string& cName);

protected:
	TArrayTypeObject m_ArrayType;
	basic_objtype _basic_type;

	typedef_object* m_pRefType;
	enumdef_object* m_pEnumDef;
	interface_object* m_pIFObject;
	userdef_type_object *m_pUserDefType;
};

class TConstItemObject : public TGrammarObject
{
public:
	TConstItemObject(module* pModule, const string& cName,
		number_type eType, double& value);
	~TConstItemObject();

	static TConstItemObject* create_instance(module* pModule,
		const string& cName, number_type eType, double& value);

	void SetRefType(typedef_object* obj) {
		m_pRefType = obj;
	}

	typedef_object* GetRefType(void) {
		return m_pRefType;
	}

	void GetValue(double& val) {
		val = m_Value;
	}

	number_type GetConstType(void) {
		return m_ConstType;
	}

	bool generate_related_object_code(void);
	module* GetDependentModule(void);
	string get_fullname(module *pCurr);

public:
	static TConstItemObject* Find(module* pModule, const string& cName);

protected:
	typedef_object*	m_pRefType;
	number_type		m_ConstType;
	double			m_Value;
};

enum EVariableObjectFlags
{
	varobj_flag_method_param_in = 1,
	varobj_flag_method_param_out = 2,
};

class TVariableObject
{
public:
	TVariableObject();
	TVariableObject(basic_objtype eType, const string& cName, TArrayTypeObject& eArrayType);
	TVariableObject(typedef_object *obj, const string& cName, TArrayTypeObject& eArrayType);
	TVariableObject(enumdef_object *obj, const string& cName, TArrayTypeObject& eArrayType);
	TVariableObject(userdef_type_object *obj, const string& cName, TArrayTypeObject& eArrayType);
	TVariableObject(interface_object *obj, const string& cName, TArrayTypeObject& eArrayType);
	~TVariableObject();

	void SetName(const string& cName) {
		m_cName = cName;
	}

	void SetObject(basic_objtype eType) {
		_basic_type = eType;
	}

	void SetObject(typedef_object *obj) {
		if (obj) _basic_type = eBasicObjType_Typedefed;
		m_pRefType = obj;
	}

	void SetObject(enumdef_object *obj) {
		if (obj) _basic_type = eBasicObjType_Enum;
		m_pEnumDefType = obj;
	}

	void SetObject(userdef_type_object *obj) {
		if (obj) _basic_type = eBasicObjType_UserDefined;
		m_pUserDefType = obj;
	}

	void SetObject(interface_object *obj) {
		if (obj) _basic_type = eBasicObjType_Interface;
		m_pIFType = obj;
	}
	
	void AddTo(list_item& item) {
		m_OwnerList.AddTo(item);
	}

	string& getname(void) {
		return m_cName;
	}

	module* GetModule(void);
	bool generate_related_object_code(void);
	bool IsIdentifierUsedInCurrentStruct(const string& cName, EIdentifierType& eType);
	DeclareFlagOperations();

	list_item			m_OwnerList;
	TArrayTypeObject	m_ArrayType;
	basic_objtype	_basic_type;
	string				m_cName;
	unsigned int		m_Flags;

	interface_object*	m_pIFType;
	typedef_object*		m_pRefType;
	enumdef_object*		m_pEnumDefType;
	userdef_type_object*	m_pUserDefType;
};

class module : public TGrammarObject
{
public:
	module(const string& modname,
		grammar_parser* gpsr, config& cfg);
	~module();

	static module* create_instance(const string& modname,
		grammar_parser* gpsr, config& cfg);

public:
	config& getconfig(void) {
		return m_cConfig;
	}

public:
	static module* Find(grammer_object_header& header, const string& cName);
	grammer_object_header& TypedefHeader(void);
	grammer_object_header& ConstItemHeader(void);
	grammer_object_header& UserDefTypeItemHeader(void);
	grammer_object_header& InterfaceHeader(void);
	grammer_object_header& EventHeader(void);
	grammer_object_header& EnumDefHeader(void);
	grammer_object_header& EnumNodeHeader(void);

	TEnumNode* Find(const string& cEnumNodeName);

	virtual bool SetOutputReplacement(const char *org, const char *rep) = 0;
	virtual bool fputs(srcfile_type eType, const char *content) = 0;
	virtual bool fputs(const char* filename, srcfile_type eType, 
		const char *content) = 0;
	virtual bool fprintf(srcfile_type eType, const char *content, ...) = 0;
	virtual bool fprintf(const char* filename, srcfile_type eType, 
		const char *content, ...) = 0;
	virtual void closefile(srcfile_type type) = 0;
	bool is_empty(void);

private:
	bool is_empty(grammer_object_header& hdr) {
		for (int i = 0; i < HASH_MAX; ++i) {
			if (!hdr[i].is_empty()) return false;
		}
		return true;
	}

protected:
	config&				m_cConfig;
	grammer_object_header	_event_header;
	grammer_object_header	_enumdef_header;
	grammer_object_header	_typedef_header;
	grammer_object_header	_interface_header;
	grammer_object_header	_constitem_header;
	grammer_object_header	_userdef_type_header;

	grammer_object_header	m_cEnumNodeHeader;
};

void ReleaseAllModuleObject(grammer_object_header& header);

#endif
