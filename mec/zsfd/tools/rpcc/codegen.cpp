#include <assert.h>
#include "error.h"
#include "codegen.h"

typedef_object* typedef_object::create_instance(module* mod,
		const string& name, basic_objtype eType)
{
	typedef_object *ret = NULL;
	assert(NULL != mod);
	if (codegen_lang_type_cpp == mod->getconfig().GetCodeGenType())
		ret = new typedef_object_cpp_code_generator(mod, name, eType);
	else if (codegen_lang_type_java == mod->getconfig().GetCodeGenType())
		ret = new typedef_object_java_code_generator(mod, name, eType);
	
	if (NULL == ret) Panic_OutOfMemory();
	return ret;
}

typedef_object* typedef_object::create_instance(module* mod,
		const string& name, typedef_object *pRefType)
{
	typedef_object *ret = NULL;
	assert(NULL != mod);
	if (codegen_lang_type_cpp == mod->getconfig().GetCodeGenType())
		ret = new typedef_object_cpp_code_generator(mod, name, pRefType);
	else if (codegen_lang_type_java == mod->getconfig().GetCodeGenType())
		ret = new typedef_object_java_code_generator(mod, name, pRefType);

	if (NULL == ret) Panic_OutOfMemory();
	return ret;
}

typedef_object* typedef_object::create_instance(module* mod,
		const string& name, userdef_type_object *pRefType)
{
	assert(NULL != mod);
	typedef_object *ret = NULL;
	if (codegen_lang_type_cpp == mod->getconfig().GetCodeGenType())
		ret = new typedef_object_cpp_code_generator(mod, name, pRefType);
	else if (codegen_lang_type_java == mod->getconfig().GetCodeGenType())
		ret = new typedef_object_java_code_generator(mod, name, pRefType);

	if (NULL == ret) Panic_OutOfMemory();
	return ret;
}

typedef_object* typedef_object::create_instance(module* mod,
		const string& name, enumdef_object *pEnumDef)
{
	assert(NULL != mod);
	typedef_object *ret = NULL;
	if (codegen_lang_type_cpp == mod->getconfig().GetCodeGenType())
		ret = new typedef_object_cpp_code_generator(mod, name, pEnumDef);
	else if (codegen_lang_type_java == mod->getconfig().GetCodeGenType())
		ret = new typedef_object_java_code_generator(mod, name, pEnumDef);

	if (NULL == ret) Panic_OutOfMemory();
	return ret;
}

typedef_object* typedef_object::create_instance(module* mod,
		const string& name, interface_object *pIFObject)
{
	assert(NULL != mod);
	typedef_object *ret = NULL;
	if (codegen_lang_type_cpp == mod->getconfig().GetCodeGenType())
		ret = new typedef_object_cpp_code_generator(mod, name, pIFObject);
	else if (codegen_lang_type_java == mod->getconfig().GetCodeGenType())
		ret = new typedef_object_java_code_generator(mod, name, pIFObject);

	if (NULL == ret) Panic_OutOfMemory();
	return ret;
}

userdef_type_object* userdef_type_object::create_instance(module* mod,
		const string& name)
{
	assert(NULL != mod);
	userdef_type_object *ret = NULL;
	if (codegen_lang_type_cpp == mod->getconfig().GetCodeGenType())
		ret = new userdeftype_object_cpp_code_generator(mod, name);
	else if (codegen_lang_type_java == mod->getconfig().GetCodeGenType())
		ret = new userdeftype_object_java_code_generator(mod, name);

	if (NULL == ret) Panic_OutOfMemory();
	return ret;
}

TConstItemObject* TConstItemObject::create_instance(module* mod,
		const string& name, number_type eType, double& value)
{
	assert(NULL != mod);
	TConstItemObject *ret = NULL;
	if (codegen_lang_type_cpp == mod->getconfig().GetCodeGenType())
		ret = new constitem_object_cpp_code_generator(mod, name, eType, value);
	else if (codegen_lang_type_java == mod->getconfig().GetCodeGenType())
		ret = new constitem_object_java_code_generator(mod, name, eType, value);

	if (NULL == ret) Panic_OutOfMemory();
	return ret;
}

module* module::create_instance(const string& modulename,
	grammar_parser* gpsr, config& cfg)
{
	module *ret = NULL;
	if (codegen_lang_type_cpp == cfg.GetCodeGenType()) {
		ret = new module_object_cpp_code_generator(
			modulename, gpsr, cfg);
	} else if (codegen_lang_type_java == cfg.GetCodeGenType()) {
		ret = new module_object_java_code_generator(
			modulename, gpsr, cfg);
	}

	if (NULL == ret) Panic_OutOfMemory();
	return ret;
}

interface_object* interface_object::create_instance(module* mod,
		const string& name)
{
	assert(NULL != mod);
	interface_object *ret = NULL;
	if (codegen_lang_type_cpp == mod->getconfig().GetCodeGenType())
		ret = new interface_object_cpp_code_generator(mod, name);
	else if (codegen_lang_type_java == mod->getconfig().GetCodeGenType())
		ret = new interface_object_java_code_generator(mod, name);

	if (NULL == ret) Panic_OutOfMemory();
	return ret;
}

enumdef_object* enumdef_object::create_instance(module* mod, const string& name)
{
	assert(NULL != mod);
	enumdef_object *ret = NULL;
	if (codegen_lang_type_cpp == mod->getconfig().GetCodeGenType())
		ret = new enumdef_object_cpp_code_generator(mod, name);
	else if (codegen_lang_type_java == mod->getconfig().GetCodeGenType())
		ret = new enumdef_object_java_code_generator(mod, name);

	if (NULL == ret) Panic_OutOfMemory();
	return ret;
}

TEventObject* TEventObject::create_instance(module* mod, const string& name)
{
	assert(NULL != mod);
	TEventObject *ret = NULL;
	if (codegen_lang_type_cpp == mod->getconfig().GetCodeGenType())
		ret = new event_object_cpp_code_generator(mod, name);
	else if (codegen_lang_type_java == mod->getconfig().GetCodeGenType())
		ret = new event_object_java_code_generator(mod, name);

	if (NULL == ret) Panic_OutOfMemory();
	return ret;
}
