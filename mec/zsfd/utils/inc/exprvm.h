/** @file exprvm_token.h
 * definition of all objects for express virtual machine
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_EXPRVM

#ifndef __CXX_ZAS_UTILS_EXPRVM_H__
#define __CXX_ZAS_UTILS_EXPRVM_H__

#include <string>
#include "utils/uri.h"
#include "exprvm_expr.h"

namespace zas {
namespace utils {
namespace exprvm {

using namespace zas::utils;

enum scopeobject_type
{
	sot_unknown = 0,
	sot_variable,
	sot_register,
};

class scope;

#define _downcast(clazz, base, ptr)	\
	((clazz*)(((size_t)(ptr)) + (((size_t)(ptr)) - ((size_t)((base*)(ptr))))))
#define scope_object_downcast(clazz, ptr) _downcast(clazz, scope_object, ptr)

class scope_object
{
	friend class scope;
public:
	scope_object(int type);
	virtual ~scope_object();

	virtual const char* getname(void) = 0;

	scopeobject_type gettype(void) {
		return (scopeobject_type)_f.id;
	}

private:
	union {
		uint32_t _flags;
		struct {
			uint32_t id : 8;
		} _f;
	};
	listnode_t _ownerlist;
	scope* _scope;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(scope_object);
};

enum scope_type
{
	// types shall less than 16
	st_unknown = 0,
	st_namespace,
	st_function,
	st_ifelse,
	st_while,
	st_dowhile,
	st_for,
	st_subscope,
};

class scope
{
public:
	scope();
	~scope(void);

	void release_tree(void);

	static scope* create_function_scope(bool entry);

	scope_object* findobject(const char* name);
	scope* findscope(const char* name);

public:
	scope* addchild(scope* child) {
		if (!child) return NULL;
		listnode_add(_children, child->_sibling);
		child->_parents = this;
		return child;
	}

	int addobject(scope_object* obj) {
		if (NULL == obj) return -1;
		obj->_scope = this;
		listnode_add(_objects, obj->_ownerlist);
	}

	scope* parents(void) {
		return _parents;
	}

	scope_type gettype(void) {
		return (scope_type)_f.type;
	}

	scope_object* recurse_findobject(const char* name) {
		if (!name || !*name) return NULL;
		return do_recurse_findobject(this, name);
	}

private:
	scope_object* do_recurse_findobject(scope* s, const char* name)
	{
		if (!s) return NULL;
		scope_object* ret = s->findobject(name);
		if (ret) return ret;
		listnode_t *node = s->_children.next;
		for (; node != &_children; node = node->next) {
			scope* ns = list_entry(scope, _sibling, node);
			ret = ns->findobject(name);
			if (ret) return ret;
		}
		return NULL;
	}

private:
	listnode_t _children;
	listnode_t _sibling;
	scope* _parents;
	listnode_t _objects;
	
	std::string _name;
	union {
		uint32_t _flags;
		struct {
			// scope type
			uint32_t type : 4;

			// indicate if it is the main function (as is the entry of the program)
			// if entry = ture, the pre-requisite is function = true
			uint32_t entry : 1;
		} _f;
	};
	ZAS_DISABLE_EVIL_CONSTRUCTOR(scope);
};

struct variable : public scope_object
{
	variable() : scope_object(sot_variable),
	flags(0), array_sz(0), array_datasz(0) {}

	const char* getname(void) {
		return name.c_str();
	}

	listnode_t ownerlist;
	string name;
	numtype type;
	union {
		uint32_t flags;
		struct {
			uint32_t is_const : 1;
			uint32_t is_unsigned : 1;
			uint32_t is_array : 1;
		} f;
	};
	uint32_t array_sz;
	uint32_t array_datasz;
	union {
		long as_int;
		unsigned long as_uint;
		double as_float;
	} value;
};

class parser
{
public:
	parser(tokenparser& tknparser);
	~parser();
	int parse(void);

public:
	
	scope* getglobal(void) {
		assert(NULL != _global);
		return _global;
	}

	scope* getscope(void) {
		assert(NULL != _cur_scope);
		return _cur_scope;
	}

private:
	int parse_token_user(token& tkn);
	int parse_function(token& tkn);
	int parse_entry_function(void);
	int parse_entry_function_variable();

	enum vardef_flags {
		setval_allowed = 1,
		setval_must_const = 2,
	};

	variable* parse_variable_declaration(uint32_t flags,
		const char* statname);
	uint32_t to_exprflags(uint32_t flags);

private:
	scope* set_current(scope* s) {
		assert(NULL != s);
		_cur_scope = s;
	}

	scope* push_scope(scope* s) {
		if (!_cur_scope) return NULL;
		if (!_cur_scope->addchild(s)) return NULL;
		_cur_scope = s;
		return _cur_scope;
	}

	scope* pop_scope(bool bdel = false)
	{
		if (!_cur_scope) return NULL;
		if (_cur_scope == _global) return NULL;
		scope* ret = _cur_scope;
		_cur_scope = _cur_scope->parents();
		assert(NULL != _cur_scope);

		if (bdel) {
			delete ret;
			return NULL;
		}
		else return ret;
	}

private:
	tokenparser &_token_parser;
	errorinfo _errinfo;
	expr _expr_parser;

	scope* _global;
	scope* _cur_scope;
	union {
		uint32_t _flags;
		struct {
			uint32_t simple_mode : 1;
		} _f;
	};
	ZAS_DISABLE_EVIL_CONSTRUCTOR(parser);
};

}}} // end of namespace zas::utils::exprvm
#endif // __CXX_ZAS_UTILS_EXPRVM_H__
#endif // UTILS_ENABLE_FBLOCK_EXPRVM
/* EOF */
