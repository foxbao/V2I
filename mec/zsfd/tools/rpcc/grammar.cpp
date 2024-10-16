#include <assert.h>
#include "error.h"
#include "grammar.h"

TGrammarObject::TGrammarObject(module* pModule,
	const string& name)
: m_cName(name)
, m_pModule(pModule)
, m_Flags(0)
{
}

TGrammarObject::TGrammarObject(module* pModule,
	const string& name, grammar_parser* gpsr)
: m_cName(name)
, m_pModule(pModule)
, m_Flags(0)
, _gammar_parser(gpsr)
{
}

TGrammarObject::~TGrammarObject()
{
}

void TGrammarObject::Add(grammer_object_header& header)
{
	unsigned int hash = Str2ID(getname().c_str());
	m_OwnerList.AddTo(header[hash & (HASH_MAX - 1)]);
}

list_item* TGrammarObject::FindObjectByName(grammer_object_header& header, const string& name)
{
	unsigned int hash = Str2ID(name.c_str());
	list_item& h = header[hash & (HASH_MAX - 1)];
	list_item* pItem = h.getnext();
	for (; pItem != &h; pItem = pItem->getnext())
	{
		TGrammarObject* obj = LIST_ENTRY(TGrammarObject, m_OwnerList, pItem);
		if (obj->m_cName == name) return pItem;
	}
	return NULL;
}

bool TGrammarObject::global_scope_generate_code(void)
{
	return true;
}

string TGrammarObject::get_fullname(module *pCurr)
{
	assert(NULL != pCurr);
	module *pMyModule = GetModule();
	assert(NULL != pMyModule);

	string prefix;

	if (pMyModule == pCurr)
		return getname();
	else if (pMyModule->test_flags(grammar_object_flags_globalscope))
		prefix = "::";
	else prefix = pMyModule->getname() + "::";

	return prefix + getname();
}

unsigned int Str2ID(const char* name)
{
	unsigned int s, i;
	for (s = i = 0; name[i]; i++)
		s = (s + (i + 1) * name[i]) % 0x8000000bu * 0xffffffefu;
	return s ^ 0x12345678;
}

TConstItemObject::TConstItemObject(module* pModule, const string& name,
	number_type eType, double& value)
: TGrammarObject(pModule, name)
, m_pRefType(NULL)
, m_ConstType(eType)
, m_Value(value)
{
}

TConstItemObject::~TConstItemObject()
{
	m_OwnerList.Delete();
}

string TConstItemObject::get_fullname(module *pCurr)
{
	string original = TGrammarObject::get_fullname(pCurr);
	if (original.c_str()[0] == ':' && !TConstItemObject::Find(pCurr, getname()))
		return getname();
	return original;
}

module* TConstItemObject::GetDependentModule(void)
{
	if (m_pRefType)
		return m_pRefType->GetModule();
	return NULL;
}

bool TConstItemObject::generate_related_object_code(void)
{
	if (m_pRefType)
	{
		if (!m_pRefType->generate_code())
			return false;
	}
	return true;
}

void ReleaseAllConstItemObject(grammer_object_header& header)
{
	for (int i = 0; i < HASH_MAX; ++i)
	{
		list_item &h = header[i];
		while (!h.is_empty())
		{
			list_item *item = h.getnext();
			TConstItemObject *obj = LIST_ENTRY(TConstItemObject, m_OwnerList, item);
			delete obj;
		}
	}
}

TConstItemObject* TConstItemObject::Find(module* pModule, const string& name)
{
	assert(NULL != pModule);
	list_item *pItem = FindObjectByName(pModule->ConstItemHeader(), name);
	if (!pItem) return NULL;

	return LIST_ENTRY(TConstItemObject, m_OwnerList, pItem);
}

typedef_object::typedef_object(module* pModule, const string& name, basic_objtype eType)
: TGrammarObject(pModule, name)
, _basic_type(eType)
, m_pRefType(NULL)
, m_pEnumDef(NULL)
, m_pIFObject(NULL)
, m_pUserDefType(NULL)
{
	assert(IsBasicObjectType(_basic_type));
}

typedef_object::typedef_object(module* pModule, const string& name, typedef_object *pRefType)
: TGrammarObject(pModule, name)
, _basic_type(eBasicObjType_Typedefed)
, m_pRefType(pRefType)
, m_pEnumDef(NULL)
, m_pIFObject(NULL)
, m_pUserDefType(NULL)
{
	assert(NULL != pRefType);
}

typedef_object::typedef_object(module* pModule, const string& name, userdef_type_object *pRefType)
: TGrammarObject(pModule, name)
, _basic_type(eBasicObjType_UserDefined)
, m_pRefType(NULL)
, m_pEnumDef(NULL)
, m_pIFObject(NULL)
, m_pUserDefType(pRefType)
{
	assert(NULL != pRefType);
}

typedef_object::typedef_object(module* pModule, const string& name, enumdef_object *pEnumDef)
: TGrammarObject(pModule, name)
, _basic_type(eBasicObjType_Enum)
, m_pRefType(NULL)
, m_pEnumDef(pEnumDef)
, m_pIFObject(NULL)
, m_pUserDefType(NULL)
{
}

typedef_object::typedef_object(module* pModule, const string& name, interface_object* pIFObject)
: TGrammarObject(pModule, name)
, _basic_type(eBasicObjType_Interface)
, m_pRefType(NULL)
, m_pEnumDef(NULL)
, m_pIFObject(pIFObject)
, m_pUserDefType(NULL)
{
}

typedef_object::~typedef_object()
{
}

string typedef_object::get_fullname(module *pCurr)
{
	string original = TGrammarObject::get_fullname(pCurr);
	if (original.c_str()[0] == ':' && !typedef_object::Find(pCurr, getname()))
		return getname();
	return original;
}

module* typedef_object::GetDependentModule(void)
{
	if (eBasicObjType_Typedefed == _basic_type)
	{
		assert(NULL != m_pRefType);
		return m_pRefType->GetModule();
	}
	else if (eBasicObjType_UserDefined == _basic_type)
	{
		assert(NULL != m_pUserDefType);
		return m_pUserDefType->GetModule();
	}
	else if (eBasicObjType_Enum == _basic_type)
	{
		assert(NULL != m_pEnumDef);
		return m_pEnumDef->GetModule();
	}
	else if (eBasicObjType_Interface == _basic_type)
	{
		assert(NULL != m_pIFObject);
		return m_pIFObject->GetModule();
	}
	else return NULL;
}

typedef_object* typedef_object::Find(module* pModule, const string& name)
{
	assert(NULL != pModule);
	list_item *pItem = FindObjectByName(pModule->TypedefHeader(), name);
	if (!pItem) return NULL;

	return LIST_ENTRY(typedef_object, m_OwnerList, pItem);
}

userdef_type_object* typedef_object::GetOriginalType(basic_objtype& eType,
			TArrayTypeObject& ArrayType, enumdef_object** ppEnumDef, interface_object** ppIFObj)
{
	assert(eBasicObjType_Unknown != _basic_type);

	ArrayType = m_ArrayType;
	if (eBasicObjType_UserDefined == _basic_type)
	{
		eType = _basic_type;
		if (ppEnumDef) *ppEnumDef = NULL;
		assert(NULL != m_pUserDefType);
		return m_pUserDefType;
	}
	else if (eBasicObjType_Typedefed == _basic_type)
	{
		// it is a typedef type
		assert(NULL != m_pRefType);
		userdef_type_object *pUserDefObj = m_pRefType->GetOriginalType(eType, ArrayType, ppEnumDef, ppIFObj);

		// the compiler doesn't support 2-dimension array
		// check and block that approach
		if (is_array(m_ArrayType) && is_array(ArrayType))
		{
			eType = eBasicObjType_Unknown;
			if (ppEnumDef) *ppEnumDef = NULL;
			return NULL;
		}

		ArrayType = (is_array(m_ArrayType)) ? m_ArrayType : ArrayType;
		return NULL;
	}
	else if (eBasicObjType_Enum == _basic_type)
	{
		eType = _basic_type;
		assert(NULL != m_pEnumDef);
		if (ppEnumDef) *ppEnumDef = m_pEnumDef;
		return NULL;
	}
	else if (eBasicObjType_Interface == _basic_type)
	{
		eType = _basic_type;
		assert(NULL != m_pIFObject);
		if (ppIFObj) *ppIFObj = m_pIFObject;
		return NULL;
	}
	else
	{
		// it is a basic type
		eType = _basic_type;
		return NULL;
	}
}

bool typedef_object::generate_related_object_code(void)
{
	if (eBasicObjType_Typedefed == _basic_type)
	{
		if (NULL == m_pRefType)
			return false;
		return m_pRefType->generate_code();
	}
	else if (eBasicObjType_UserDefined == _basic_type)
	{
		if (NULL == m_pUserDefType)
			return false;
		return m_pUserDefType->generate_code();
	}
	else if (eBasicObjType_Enum == _basic_type)
	{
		if (NULL == m_pEnumDef)
			return false;
		return m_pEnumDef->generate_code();
	}
	else if (eBasicObjType_Interface == _basic_type)
	{
		if (NULL == m_pIFObject)
			return false;
		return m_pIFObject->generate_code();
	}
	else if (!IsBasicObjectType(_basic_type))
		return false;
	else return true;
}

module::module(const string& modulename,
	grammar_parser* gpsr, config& cfg)
	: TGrammarObject(NULL, modulename, gpsr)
	, m_cConfig(cfg)
{
}

module::~module()
{
	ReleaseAllConstItemObject(ConstItemHeader());
	m_OwnerList.Delete();
}

void ReleaseAllModuleObject(grammer_object_header& header)
{
	for (int i = 0; i < HASH_MAX; ++i)
	{
		list_item &h = header[i];
		while (!h.is_empty())
		{
			list_item *item = h.getnext();
			module *obj = LIST_ENTRY(module, m_OwnerList, item);
			delete obj;
		}
	}
}

TEnumNode* module::Find(const string& cEnumNodeName)
{
	unsigned int hash = Str2ID(cEnumNodeName.c_str());
	list_item& h = m_cEnumNodeHeader[hash & (HASH_MAX - 1)];
	list_item* pItem = h.getnext();
	for (; pItem != &h; pItem = pItem->getnext())
	{
		TEnumNode* obj = LIST_ENTRY(TEnumNode, m_cOverallOwnerList, pItem);
		if (obj->m_cEnumNodeName == cEnumNodeName) return obj;
	}
	return NULL;
}

module* module::Find(grammer_object_header& header, const string& name)
{
	list_item *pItem = FindObjectByName(header, name);
	if (!pItem) return NULL;

	return LIST_ENTRY(module, m_OwnerList, pItem);
}

grammer_object_header& module::TypedefHeader(void)
{
	return _typedef_header;
}

grammer_object_header& module::ConstItemHeader(void)
{
	return _constitem_header;
}

grammer_object_header& module::UserDefTypeItemHeader(void)
{
	return _userdef_type_header;
}

grammer_object_header& module::InterfaceHeader(void)
{
	return _interface_header;
}

grammer_object_header& module::EventHeader(void)
{
	return _event_header;
}

grammer_object_header& module::EnumDefHeader(void)
{
	return _enumdef_header;
}

grammer_object_header& module::EnumNodeHeader(void)
{
	return m_cEnumNodeHeader;
}

bool module::is_empty(void)
{
	if (!is_empty(_event_header)) {
		return false;
	}
	if (!is_empty(_enumdef_header)) {
		return false;
	}
	if (!is_empty(_typedef_header)) {
		return false;
	}
	if (!is_empty(_interface_header)) {
		return false;
	}
	if (!is_empty(_constitem_header)) {
		return false;
	}
	if (!is_empty(_userdef_type_header)) {
		return false;
	}
	if (!is_empty(m_cEnumNodeHeader)) {
		return false;
	}
	return true;
}

userdef_type_object::userdef_type_object(module* pModule, const string& name)
: TGrammarObject(pModule, name)
{
}

string userdef_type_object::get_fullname(module *pCurr)
{
	string original = TGrammarObject::get_fullname(pCurr);
	if (original.c_str()[0] == ':' && !userdef_type_object::Find(pCurr, getname()))
		return getname();
	return original;
}

#define _AddModule(m)	\
do {	\
	if (NULL == ret)	\
	{	\
		ret = new module* [total];	\
		assert(NULL != ret);	\
		for (int i = 0; i < total; i++) ret[i] = NULL;	\
		ret[count++] = (m);	\
	}	\
	else	\
	{	\
		module** tmp1 = ret;	\
		for (; *tmp1 && *tmp1 != (m); tmp1++);	\
		if (!*tmp1)	\
		{	\
			if (count >= total)	\
			{	\
				module** tmp = new module *[total * 2];	\
				assert(NULL != tmp && NULL != ret);	\
				for (int i = 0; i < total; ++i) tmp[i] = ret[i];	\
				for (int i = total; i < total * 2; ++i) tmp[i] = NULL;	\
				delete [] ret;	\
				ret = tmp; total *= 2;	\
			}	\
			ret[count++] = (m);	\
		}	\
	}	\
} while (0)


module** userdef_type_object::GetDependentModule(void)
{
	int count = 0, total = 8;
	module **ret = NULL;

	list_item *pItem = m_VariableList.getnext();
	for (; pItem != &m_VariableList; pItem = pItem->getnext())
	{
		TVariableObject *pVar = LIST_ENTRY(TVariableObject, m_OwnerList, pItem);

		//check every parameters
		module *pModuleObj = pVar->GetModule();
		if (NULL == pModuleObj) continue;
		_AddModule(pModuleObj);
	}
	return ret;
}

bool userdef_type_object::generate_related_object_code(void)
{
	list_item *pItem = m_VariableList.getnext();
	for (; pItem != &m_VariableList; pItem = pItem->getnext())
	{
		TVariableObject *pVar = LIST_ENTRY(TVariableObject, m_OwnerList, pItem);
		if (!pVar->generate_related_object_code())
			return false;
	}
	return true;
}

userdef_type_object* userdef_type_object::Find(module* pModule, const string& name)
{
	assert(NULL != pModule);
	list_item *pItem = FindObjectByName(pModule->UserDefTypeItemHeader(), name);
	if (!pItem) return NULL;

	return LIST_ENTRY(userdef_type_object, m_OwnerList, pItem);
}

enumdef_object* enumdef_object::Find(module *pModule, const string& name)
{
	assert(NULL != pModule);
	list_item *pItem = FindObjectByName(pModule->EnumDefHeader(), name);
	if (!pItem) return NULL;

	return LIST_ENTRY(enumdef_object, m_OwnerList, pItem);
}

void enumdef_object::AddNode(TEnumNode *pNode)
{
	assert(NULL != pNode);
	module *pModule = GetModule();
	assert(NULL != pModule);

	pNode->m_cEnumOwnerList.AddTo(m_cEnumNodeList);

	unsigned int hash = Str2ID(pNode->m_cEnumNodeName.c_str());
	grammer_object_header& header = pModule->EnumNodeHeader();
	pNode->m_cOverallOwnerList.AddTo(header[hash & (HASH_MAX - 1)]);
}

interface_object::interface_object(module* pModule, const string& name)
: TGrammarObject(pModule, name)
, m_InterfaceFlags(0)
{
}

interface_object* interface_object::Find(module* pModule, const string& name)
{
	assert(NULL != pModule);
	list_item *pItem = FindObjectByName(pModule->InterfaceHeader(), name);
	if (!pItem) return NULL;

	return LIST_ENTRY(interface_object, m_OwnerList, pItem);
}

string interface_object::get_fullname(module *pCurr)
{
	string original = TGrammarObject::get_fullname(pCurr);

	// interface will never appear in a global scope
	// so we'll never encounter something like: "::SomeInterface"
	// make sure this is not happen
	assert(original.c_str()[0] != ':');

	// change "ns::IF" to "ns::Proxy::IF"
	string::size_type pos = original.find("::");
	if (pos != string::npos)
		original.insert(pos + 2, "Proxy::");
	else original = "Proxy::" + original;
	return original;
}

module** interface_object::GetDependentModule(void)
{
	int count = 0, total = 8;
	module **ret = NULL;

	list_item *pItem = NodeList().getnext();
	for (; pItem != &NodeList(); pItem = pItem->getnext())
	{
		TInterfaceNode *pIFNode = LIST_ENTRY(TInterfaceNode, m_OwnerList, pItem);

		// if it is an attribute, just check the variable type
		// if it is an interface, check the return type
		basic_objtype eRetType = (eIFNodeType_Attribute == pIFNode->GetType())
			? pIFNode->m_Data.attribute._basic_type : pIFNode->m_Data.method.m_eRetValType;

		if (eBasicObjType_Typedefed == eRetType)
		{
			module *pModuleObj = pIFNode->m_Data.method.m_pRetValTypedefType->GetModule();
			if (NULL == pModuleObj) continue;
			_AddModule(pModuleObj);
		}
		else if (eBasicObjType_UserDefined == eRetType)
		{
			module *pModuleObj = pIFNode->m_Data.method.m_pRetValUserDefType->GetModule();
			if (NULL == pModuleObj) continue;
			_AddModule(pModuleObj);
		}
		else if (eBasicObjType_Enum == eRetType)
		{
			module *pModuleObj = pIFNode->m_Data.method.m_pRetValEnumDef->GetModule();
			if (NULL == pModuleObj) continue;
			_AddModule(pModuleObj);
		}
		else if (eBasicObjType_Interface == eRetType)
		{
			module *pModuleObj = pIFNode->m_Data.method.m_pRetValIFObject->GetModule();
			if (NULL == pModuleObj) continue;
			_AddModule(pModuleObj);
		}
		// todo: other types

		// then, check every parameters (only for interface definition)
		if (eIFNodeType_Attribute == pIFNode->GetType())
			continue;

		list_item *pNext = pIFNode->m_VariableList.getnext();
		for (; pNext != &(pIFNode->m_VariableList); pNext = pNext->getnext())
		{
			TVariableObject *pVar = LIST_ENTRY(TVariableObject, m_OwnerList, pNext);
			module *pModuleObj = pVar->GetModule();
			if (NULL == pModuleObj) continue;
			_AddModule(pModuleObj);
		}
	}
	return ret;
}

bool interface_object::generate_proxy_code(void)
{
	return true;
}

bool interface_object::generate_skeleton_code(void)
{
	return true;
}

bool interface_object::generate_related_object_code(void)
{
	list_item *pItem = NodeList().getnext();
	for (; pItem != &NodeList(); pItem = pItem->getnext())
	{
		TInterfaceNode *pIFNode = LIST_ENTRY(TInterfaceNode, m_OwnerList, pItem);

		// if it is an attribute, only check the variable type
		// if it is an interface, check the return type
		basic_objtype eRetType = (eIFNodeType_Attribute == pIFNode->GetType())
			? pIFNode->m_Data.attribute._basic_type : pIFNode->m_Data.method.m_eRetValType;
		if (eBasicObjType_Typedefed == eRetType)
		{
			assert(NULL != pIFNode->m_Data.method.m_pRetValTypedefType);
			if (!pIFNode->m_Data.method.m_pRetValTypedefType->generate_code())
				return false;
		}
		else if (eBasicObjType_UserDefined == eRetType)
		{
			assert(NULL != pIFNode->m_Data.method.m_pRetValUserDefType);
			if (!pIFNode->m_Data.method.m_pRetValUserDefType->generate_code())
				return false;
		}
		else if (eBasicObjType_Enum == eRetType)
		{
			assert(NULL != pIFNode->m_Data.method.m_pRetValEnumDef);
			if (!pIFNode->m_Data.method.m_pRetValEnumDef->generate_code())
				return false;
		}
		else if (eBasicObjType_Interface == eRetType)
		{
			assert(NULL != pIFNode->m_Data.method.m_pRetValIFObject);
			if (!pIFNode->m_Data.method.m_pRetValIFObject->generate_code())
				return false;
		}
		// todo: other types

		// then, check every parameters(only for interface definition)
		if (eIFNodeType_Attribute == pIFNode->GetType())
			continue;

		list_item *pNext = pIFNode->m_VariableList.getnext();
		for (; pNext != &(pIFNode->m_VariableList); pNext = pNext->getnext())
		{
			TVariableObject *pVar = LIST_ENTRY(TVariableObject, m_OwnerList, pNext);
			if (!pVar->generate_related_object_code())
				return false;
		}
	}
	return true;
}

TVariableObject::TVariableObject()
: _basic_type(eBasicObjType_Unknown), m_Flags(0)
, m_pIFType(NULL), m_pRefType(NULL), m_pEnumDefType(NULL), m_pUserDefType(NULL)
{
}

TVariableObject::TVariableObject(basic_objtype eType, const string& name, TArrayTypeObject& ArrayType)
: m_ArrayType(ArrayType), _basic_type(eType), m_cName(name), m_Flags(0)
, m_pIFType(NULL), m_pRefType(NULL), m_pEnumDefType(NULL), m_pUserDefType(NULL)
{
	assert(IsBasicObjectType(eType));
}

TVariableObject::TVariableObject(typedef_object *obj, const string& name, TArrayTypeObject& ArrayType)
: m_ArrayType(ArrayType), _basic_type(eBasicObjType_Typedefed), m_cName(name), m_Flags(0)
, m_pIFType(NULL), m_pRefType(obj), m_pEnumDefType(NULL), m_pUserDefType(NULL)
{
	assert(NULL != obj);
}

TVariableObject::TVariableObject(enumdef_object *obj, const string& name, TArrayTypeObject& ArrayType)
: m_ArrayType(ArrayType), _basic_type(eBasicObjType_Enum), m_cName(name), m_Flags(0)
, m_pIFType(NULL), m_pRefType(NULL), m_pEnumDefType(obj), m_pUserDefType(NULL)
{
}

TVariableObject::TVariableObject(userdef_type_object *obj, const string& name, TArrayTypeObject& ArrayType)
: m_ArrayType(ArrayType), _basic_type(eBasicObjType_UserDefined), m_cName(name), m_Flags(0)
, m_pIFType(NULL), m_pRefType(NULL), m_pEnumDefType(NULL), m_pUserDefType(obj)
{
}

TVariableObject::TVariableObject(interface_object *obj, const string& name, TArrayTypeObject& eArrayType)
: m_ArrayType(eArrayType), _basic_type(eBasicObjType_Interface), m_cName(name), m_Flags(0)
, m_pIFType(obj), m_pRefType(NULL), m_pEnumDefType(NULL), m_pUserDefType(NULL)
{
}

TVariableObject::~TVariableObject()
{
}

module* TVariableObject::GetModule(void)
{
	if (eBasicObjType_Typedefed == _basic_type)
	{
		assert(NULL != m_pRefType);
		return m_pRefType->GetModule();
	}
	else if (eBasicObjType_Enum == _basic_type)
	{
		assert(NULL != m_pEnumDefType);
		return m_pEnumDefType->GetModule();
	}
	else if (eBasicObjType_UserDefined == _basic_type)
	{
		assert(NULL != m_pUserDefType);
		return m_pUserDefType->GetModule();
	}
	else if (eBasicObjType_Interface == _basic_type)
	{
		assert(NULL != m_pIFType);
		return m_pIFType->GetModule();
	}
	return NULL;
}

bool TVariableObject::generate_related_object_code(void)
{
	if (eBasicObjType_Typedefed == _basic_type)
	{
		assert(NULL != m_pRefType);
		return m_pRefType->generate_code();
	}
	else if (eBasicObjType_Enum == _basic_type)
	{
		assert(NULL != m_pEnumDefType);
		return m_pEnumDefType->generate_code();
	}
	else if (eBasicObjType_UserDefined == _basic_type)
	{
		assert(NULL != m_pUserDefType);
		return m_pUserDefType->generate_code();
	}
	else if (eBasicObjType_Interface == _basic_type)
	{
		assert(NULL != m_pIFType);
		return m_pIFType->generate_code();
	}
	return true;
}

TInterfaceNode::TInterfaceNode(interface_object* pIFObj, EIFNodeType eType)
: m_pIFObject(pIFObj)
, m_eType(eType)
, m_NodeID(0)
{
	if (eIFNodeType_Method == eType)
	{
		m_Data.method.m_pRetValEnumDef = NULL;
		m_Data.method.m_pRetValIFObject = NULL;
		m_Data.method.m_pRetValTypedefType = NULL;
		m_Data.method.m_pRetValUserDefType = NULL;
		m_Data.method.m_eRetValType = eBasicObjType_Unknown;
	}
	else if (eIFNodeType_Attribute == eType)
	{
		m_Data.attribute._basic_type = eBasicObjType_Unknown;
		m_Data.attribute.m_pIFObject = NULL;
		m_Data.attribute.m_pTypedefType = NULL;
		m_Data.attribute.m_pEnumDef = NULL;
		m_Data.attribute.m_pUserDefType = NULL;
	}
}

TInterfaceNode::~TInterfaceNode()
{
}

enumdef_object::enumdef_object(module* pModule, const string& name)
: TGrammarObject(pModule, name)
{
}

string enumdef_object::get_fullname(module *pCurr)
{
	string original = TGrammarObject::get_fullname(pCurr);
	if (original.c_str()[0] == ':' && !enumdef_object::Find(pCurr, getname()))
		return getname();
	return original;
}

string enumdef_object::GetDefault(module *pCurr)
{
	assert(NULL != pCurr);

	if (m_cEnumNodeList.is_empty())
	{
		string ret("(");
		ret.append(get_fullname(pCurr));
		ret.append(")0");
		return ret;
	}
	string val = LIST_ENTRY(TEnumNode, m_cEnumOwnerList, m_cEnumNodeList.getnext())->m_cEnumNodeName;
	if (GetModule() != pCurr)
	{
		if (GetModule()->test_flags(grammar_object_flags_globalscope))
			val = "::" + val;
		else val = GetModule()->getname() + "::" + val;
	}
	return val;
}

TEventObject::TEventObject(module* pModule, const string& name)
: TGrammarObject(pModule, name)
{
}

TEventObject* TEventObject::Find(module* pModule, const string& name)
{
	assert(NULL != pModule);
	list_item *pItem = FindObjectByName(pModule->EventHeader(), name);
	if (!pItem) return NULL;

	return LIST_ENTRY(TEventObject, m_OwnerList, pItem);
}

string TEventObject::get_fullname(module *pCurr)
{
	string original = TGrammarObject::get_fullname(pCurr);

	// event will never appear in a global scope
	// so we'll never encounter something like: "::SomeEvent"
	// make sure this is not happen
	assert(original.c_str()[0] != ':');

	// change "ns::IF" to "ns::Proxy::IF"
	string::size_type pos = original.find("::");
	if (pos != string::npos)
		original.insert(pos + 2, "Proxy::");
	else original = "Proxy::" + original;
	return original;
}

bool TEventObject::generate_related_object_code(void)
{
	list_item *pItem = NodeList().getnext();
	for (; pItem != &NodeList(); pItem = pItem->getnext())
	{
		TVariableObject *pVarObject = LIST_ENTRY(TVariableObject, m_OwnerList, pItem);
		if (!pVarObject->generate_related_object_code())
			return false;
	}
	return true;
}

module** TEventObject::GetDependentModule(void)
{
	int count = 0, total = 8;
	module **ret = NULL;

	list_item *pItem = NodeList().getnext();
	for (; pItem != &NodeList(); pItem = pItem->getnext())
	{
		TVariableObject *pVar = LIST_ENTRY(TVariableObject, m_OwnerList, pItem);
		module *pModuleObj = pVar->GetModule();
		if (NULL == pModuleObj) continue;
		_AddModule(pModuleObj);
	}
	return ret;
}
