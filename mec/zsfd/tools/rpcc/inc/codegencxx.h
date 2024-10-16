/** @file inc/codegencxx.h
 * definition of c++ code generation for zrpc
 */

#ifndef __CXX_RPCC_CODEGENCPP_H__
#define __CXX_RPCC_CODEGENCPP_H__

#include <stdint.h>
#include <string>
#include <stdio.h>
#include <stdarg.h>
#include "gencommon.h"

using namespace std;

class gammer_parser;

#define zrpc_downcast(base, derive, ptr)	\
	(derive*)(((size_t)(ptr)) - (size_t)((base*)((derive*)0)))

class module_object_cpp_code_generator : public module
{
	enum {
		MODOBJ_CPP_CODEGEN_INTERNAL_HDRFILE = 1,
	};
public:
	module_object_cpp_code_generator(const string& modname,
		grammar_parser* gpsr, config& cfg);
	~module_object_cpp_code_generator();

	bool generate_code(void);
	bool SetOutputReplacement(const char *org, const char *rep);
	bool fputs(srcfile_type type, const char *content);
	bool fprintf(srcfile_type type, const char *content, ...);
	bool fputs(const char* filename, srcfile_type type, const char *content);
	bool fprintf(const char* filename, srcfile_type type, 
		const char *content, ...);

	module_userdef_array_info* get_userdef_array_info(const char* clsname);
	int create_userdef_array_info(const char* clsname);

private:
	void upper(string& s);

	int create_files(void);
	void close_files(void);
	void closefile(srcfile_type type);
	void create_srcfile_protobuf(void);
	void create_srcfile_module_proxy_structs(void);
	int create_srcfile_module_header(string dir);
	int create_srcfile_module_proxy_internal_header(string dir);
	void create_srcfile(void);
	void create_srcfile_proxy(void);
	void create_srcfile_skeleton(void);

	void write_srcfile_begin_comments(FILE* fp, string filename);
	void get_header_file_ifdef_identifer(string& ret, uint32_t flags);

	// file operations
	bool fprintf_common(srcfile_type type, const char* content, va_list ap);
	FILE** getfile(srcfile_type type);
	bool file_handler_error(FILE **fp);

	// generators
	bool generate_namespace(void);
	bool generate_typedefs(void);
	bool generate_userdef_types(void);
	bool generate_global_scope_userdef_types(void);
	bool generate_global_scope_enums(void);
	bool generate_interfaces(void);
	bool generate_protobuf_includes(FILE* fp);
	bool generate_moudle_includes(FILE* fp);
	bool generate_event(void);
	bool generate_enums(void);
	bool generate_constitems(void);
	bool generate_finished(void);

	// include line generators
	int generate_interface_include_list(list_item&);
	int generate_userdeftype_include_list(list_item&);
	int generate_enum_include_list(list_item& lst);
	int generate_event_include_list(list_item& lst);
	int generate_include_list(void);

	// release methods
	void release_all_userdef_array_info(void);
	void release_include_module_list(list_item& lst);
	
	bool is_module_included(list_item& lst, module* mod);
	void set_module_included(list_item& lst, module* mod);

private:
	// files
	FILE* _fp_module_h;
	FILE* _fp_module_proxy_internal_h;
	FILE* _fp_module_protobuf;
	FILE* _fp_module_proxy_structs;
	FILE* _fp_module_proxy_source;
	FILE* _fp_module_skeleton_source;
	FILE* _fp_module_source;

	string _replace_original;
	string _replace_new;

	list_item _userdef_array_info_list;
	list_item _protobuf_inc_module_list;
	list_item _inc_module_list;
};

class typedef_object_cpp_code_generator : public typedef_object
{
public:
	typedef_object_cpp_code_generator(module* mod,
		const string& name, basic_objtype type);
	typedef_object_cpp_code_generator(module* mod,
		const string& name, typedef_object *pRefType);
	typedef_object_cpp_code_generator(module* mod,
		const string& name, userdef_type_object *pRefType);
	typedef_object_cpp_code_generator(module* mod,
		const string& name, enumdef_object* enum_def);
	typedef_object_cpp_code_generator(module* mod,
		const string& name, interface_object* ifobj);
	~typedef_object_cpp_code_generator();

	bool generate_code(void);
};

class constitem_object_cpp_code_generator : public TConstItemObject
{
public:
	constitem_object_cpp_code_generator(module* mod,
		const string& name, number_type type, double& value);
	~constitem_object_cpp_code_generator();

	bool generate_code(void);
};

class enumdef_object_cpp_code_generator : public enumdef_object
{
public:
	enumdef_object_cpp_code_generator(module *mod,
		const string name);
	~enumdef_object_cpp_code_generator();

	
	bool generate_code(void);
};

class userdeftype_object_cpp_code_generator : public userdef_type_object
{
public:
	userdeftype_object_cpp_code_generator(module* mod,
		const string& name);
	~userdeftype_object_cpp_code_generator();

	bool generate_code(void);
	bool global_scope_generate_code(void);

private:
	int generate_global_scope_header_file_code(void);
	int generate_variable_class(TVariableObject *obj);
	int generate_variable_class_constructor(TVariableObject *obj, bool param);
	int generate_protobuf_variable_class(TVariableObject* obj);
	int generate_variable_class_boolean(TVariableObject *obj);
	int generate_variable_class_int(TVariableObject *obj,
		basic_objtype type);
	int generate_variable_class_enum(TVariableObject *obj,
		basic_objtype type);
	int generate_variable_class_userdefined(TVariableObject *obj,
		basic_objtype type);
	int generate_variable_class_interface(TVariableObject *obj,
		basic_objtype type);
	int generate_variable_class_string(TVariableObject *obj,
		basic_objtype type);
	int generate_variable_class_stream(TVariableObject *obj,
		basic_objtype type);
	int generate_variable_class_constructor_userdefined(TVariableObject *obj,
		basic_objtype type, bool param);
	int generate_class_definition(void);
	int generate_class_dependency(void);

	int generate_generic_array(TVariableObject* var);

private:
	int _protobuf_count;
};

class interface_object_cpp_code_generator : public interface_object
{
public:
	interface_object_cpp_code_generator(module* mod, const string& name);
	~interface_object_cpp_code_generator();

	bool generate_code(void);

	//curret no used
	bool generate_proxy_code(void);
	bool generate_skeleton_code(void);

private:
	int generate_method_protobuf_structs(void);
	int generate_single_method_protobuf(TInterfaceNode* node);
	int generate_interface_protobuf_include_files(void);

	int generate_header_code(void);
	int generate_source_code(void);
	int generate_proxy_header(void);
	int generate_proxy_observable_header(void);
	int generate_proxy_header_method(TInterfaceNode* node);
	int generate_skeleton_header(void);
	int generate_skeleton_observable_header(void);
	int generate_skeleton_header_method(TInterfaceNode* node);

	int generate_proxy_source(void);
	int generate_observable_proxy_source(void);
	int generate_skeleton_source(void);
	int generate_observable_skeleton_source(void);
	int generate_interface_source(void);

private:
	// generate source proxy method
	int generate_source_method_header(srcfile_type id, TInterfaceNode *node);
	int method_has_in_out_param(TInterfaceNode *node,
		bool &has_in, bool &has_out);
	bool interface_has_constructor();
	int generate_source_method_body_header(srcfile_type id, TInterfaceNode *node);
	int generate_source_method_body_inparam(module* cmod,
		TVariableObject* var, string& ret);
	int generate_source_method_body_outparam(module* cmod,
		TVariableObject* var, string& ret);
	int generate_source_method_body_ret_param(module* cmod,
		TInterfaceNode* node, string& ret);
	int generate_source_method_body_return_default(module* cmod,
		TInterfaceNode* node, string& ret, bool proxy, bool observable);

	int generate_attribute_header(TInterfaceNode* node, bool proxy);
	int generate_attribute_body(TInterfaceNode* node, bool proxy);
	int generate_attribute_body_ret_param(module* cmod,
		TInterfaceNode* node, string& ret);
	int generate_attribute_body_end(module* cmod, TInterfaceNode* node, string &ret, bool get);
	int generate_attribute_body_default_ret(module* cmod, TInterfaceNode* node, string &ret);
	int generate_source_constructor_end(module* cmod, TInterfaceNode* node, string &ret);
	
	int generate_method_header_param(TInterfaceNode* node, string &ret);

	//generate source proxy observable
	int generate_source_method_emtpy(module* cmod,
		TInterfaceNode* node, srcfile_type id, bool proxy,
		bool bobservable, bool bcomments);
	int generate_source_call_mehtod(srcfile_type id, module* cmod, TInterfaceNode* node, 
		bool obersvable);
	int generate_source_call_mehtod_end(module* cmod, TInterfaceNode* node, string &ret);
	int generate_source_call_method_var(module* cmod,
		TVariableObject* node, string &ret);
	int generate_source_call_method_end_var(module* cmod,
		TVariableObject* node, string &ret);

	int get_variable_full_name(module* cmod, TVariableObject* var, string& ret,
		bool proxy, bool observable, bool param, bool out_type, 
		bool ref = false, bool declaration = false, bool breturn = false);

	int method_retval_variable_name(module* cmod, TInterfaceNode* node,
		string& ret, bool proxy, bool observable = false, 
		bool ref = false, bool declaration = false);
	int method_attribute_variable_name(module* cmod, TInterfaceNode* node,
		string& ret, bool proxy, bool observable = false, 
		bool ref = false, bool declaration = false, bool param = false);

	int get_variable_return_default_value(module* cmod, TInterfaceNode* node,
		string& ret, bool proxy, bool observable);

	int generate_uid(const char* name, std::string& uid);
};

class event_object_cpp_code_generator : public TEventObject
{
public:
	event_object_cpp_code_generator(module *mod,
		const string name);
	~event_object_cpp_code_generator();

	bool generate_code(void);

private:
	int generate_event_header(void);
	int generate_event_header_skeleton(void);
	int generate_event_source(void);
	int generate_event_source_skeleton(void);
	int generate_event_protobuf(void);


	bool has_param(void);
	int generate_source_call_method_var(module* cmod,
		TVariableObject* var, string& ret);
	int generate_source_method_body_inparam(module* cmod,
		TVariableObject* var, string& ret);
	int variable_full_name(	module* cmod, TVariableObject* var,
		string& ret, bool skeleton = false, bool defined = false);
};

#endif /* __CXX_RPCC_CODEGENCPP_H__ */
/* EOF */
