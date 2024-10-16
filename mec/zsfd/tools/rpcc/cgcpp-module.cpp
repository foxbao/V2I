/** @file inc/cgcpp-module.h
 * implementation of c++ code generation of module for zrpc
 */

#include <assert.h>
#include <string.h>

#include "error.h"
#include "parser.h"
#include "codegencxx.h"
#include "utilities.h"
#include "config.h"

using namespace std;
using namespace utilities;

void module_object_cpp_code_generator::upper(string& s)
{
	int len = s.length();
	for (int i = 0; i < len; ++i) {
		char c = s[i];
		if (c >= 'a' && c <= 'z') {
			c -= 'a' - 'A';
			s[i] = c;
		}
	}
}

module_object_cpp_code_generator::module_object_cpp_code_generator(
	const string& modname, grammar_parser* gpsr, config& cfg)
	: module(modname, gpsr, cfg)
	, _fp_module_h(nullptr)
	, _fp_module_proxy_internal_h(nullptr)
	, _fp_module_protobuf(nullptr)
	, _fp_module_proxy_structs(nullptr)
	, _fp_module_proxy_source(nullptr)
	, _fp_module_skeleton_source(nullptr)
	, _fp_module_source(nullptr)
{
}

module_object_cpp_code_generator::~module_object_cpp_code_generator()
{
	close_files();
	release_all_userdef_array_info();
	release_include_module_list(_protobuf_inc_module_list);
	release_include_module_list(_inc_module_list);
}

void module_object_cpp_code_generator::close_files(void)
{
	if (_fp_module_h) {
		fclose(_fp_module_h);
		_fp_module_h = nullptr;
	}
	if (_fp_module_proxy_internal_h) {
		fclose(_fp_module_proxy_internal_h);
		_fp_module_proxy_internal_h = nullptr;
	}
	if (_fp_module_protobuf) {
		fclose(_fp_module_protobuf);
		_fp_module_protobuf = nullptr;
	}
	if (_fp_module_proxy_structs) {
		fclose(_fp_module_proxy_structs);
		_fp_module_proxy_structs = nullptr;
	}
	if (_fp_module_proxy_source) {
		fclose(_fp_module_proxy_source);
		_fp_module_proxy_source = nullptr;
	}
	if (_fp_module_skeleton_source) {
		fclose(_fp_module_skeleton_source);
		_fp_module_skeleton_source = nullptr;
	}
	if (_fp_module_source) {
		fclose(_fp_module_source);
		_fp_module_source = nullptr;
	}
}

void module_object_cpp_code_generator::closefile(srcfile_type type){

}

module_userdef_array_info* module_object_cpp_code_generator
	::get_userdef_array_info(const char* clsname)
{
	if (!clsname || !*clsname) return nullptr;
	list_item* item = _userdef_array_info_list.getnext();
	for (; item != &_userdef_array_info_list; item = item->getnext()) {
		auto* info = LIST_ENTRY(module_userdef_array_info, ownerlist, item);
		if (!strcmp(info->userdef_class_name.c_str(), clsname)) {
			return info;
		}
	}
	return nullptr;
}

int module_object_cpp_code_generator::
	create_userdef_array_info(const char* clsname)
{
	if (get_userdef_array_info(clsname)) {
		return 1;
	}
	auto* info = new module_userdef_array_info();
	if (nullptr == info) {
		return 2;
	}
	info->userdef_class_name = clsname;
	info->ownerlist.AddTo(_userdef_array_info_list);
	return 0;
}

void module_object_cpp_code_generator::
	release_all_userdef_array_info(void)
{
	while(!_userdef_array_info_list.is_empty()) {
		auto* item = LIST_ENTRY(module_userdef_array_info, ownerlist, \
			_userdef_array_info_list.getnext());
		item->ownerlist.Delete();
		delete item;
	}
}

int module_object_cpp_code_generator::create_files(void)
{
	if (test_flags(grammar_object_flags_code_file_created)) {
		return 0;
	}
	set_flags(grammar_object_flags_code_file_created);

	string descdir(getconfig().GetDescDir());
	descdir += "/";

	// dir: "{dest}/proxy"
	string proxy_dir = descdir + "proxy/";
	if (createdir(proxy_dir.c_str())) {
		return 1;
	}

	// dir: "{dest}/common"
	string common_dir = descdir + "common/";
	if (createdir(common_dir.c_str())) {
		return 2;
	}

	if (create_srcfile_module_header(descdir)) {
		close_files(); return 3;
	}
	if (create_srcfile_module_proxy_internal_header(common_dir)) {
		close_files(); return 4;
	}

	return 0;
}

void module_object_cpp_code_generator::create_srcfile_protobuf(void)
{
	goto next;
error:
	printf("fatal error in creating protobuffer source file "
		"[module:%s].\n", getname().c_str());
	exit(1);

next:

	string descdir(getconfig().GetDescDir());

	// dir: "{dest}/proto"
	string proto_path = descdir + "/proto/";
	if (createdir(proto_path.c_str())) {
		goto error;
	}

	// add the file name
	string filename = getname() + ".pro";
	proto_path += filename;

	FILE *fp = fopen(proto_path.c_str(), "w");
	if (nullptr == fp) goto error;

	// write the begin comments
	write_srcfile_begin_comments(fp, filename);
	::fputs("syntax = \"proto3\";\n\n", fp);

	if (!generate_protobuf_includes(fp)) {
		goto error;
	}
	// write included protobuf
	::fprintf(fp, "package %s_pbf;\n\n", getname().c_str());

	// success
	_fp_module_protobuf = fp;
}

void module_object_cpp_code_generator::create_srcfile_proxy(void)
{
	goto next;
error:
	printf("fatal error in creating proxy source file "
		"[module:%s].\n", getname().c_str());
	exit(1);

next:

	string descdir(getconfig().GetDescDir());

	// dir: "{dest}/proto"
	string proxy_path = descdir + "/proxy/";
	if (createdir(proxy_path.c_str())) {
		goto error;
	}

	// add the file name
	string filename = getname() + "-proxy.cpp";
	proxy_path += filename;

	FILE *fp = fopen(proxy_path.c_str(), "w");
	if (nullptr == fp) goto error;

	// write the begin comments
	::fprintf(fp, "#include \"common/%s-internal.h\"\n\n", getname().c_str());
	::fprintf(fp, 
		"namespace %s {\n"
		"namespace proxy {\n\n", getname().c_str());

	// success
	_fp_module_proxy_source = fp;
}

void module_object_cpp_code_generator::create_srcfile_skeleton(void)
{
	goto next;
error:
	printf("fatal error in creating skeleton source file "
		"[module:%s].\n", getname().c_str());
	exit(1);

next:

	string descdir(getconfig().GetDescDir());

	// dir: "{dest}/proto"
	string skeleton_path = descdir + "/skeleton/";
	if (createdir(skeleton_path.c_str())) {
		goto error;
	}

	// add the file name
	string filename = getname() + "-skeleton.cpp";
	skeleton_path += filename;

	FILE *fp = fopen(skeleton_path.c_str(), "w");
	if (nullptr == fp) goto error;

	// write the begin comments
	::fprintf(fp, "#include \"common/%s-internal.h\"\n\n", getname().c_str());
	::fprintf(fp, 
		"namespace %s {\n"
		"namespace skeleton {\n\n"
		"using namespace zas::mware::rpc;\n\n", getname().c_str());

	// success
	_fp_module_skeleton_source = fp;
}

void module_object_cpp_code_generator::create_srcfile(void)
{
	goto next;
error:
	printf("fatal error in creating source file "
		"[module:%s].\n", getname().c_str());
	exit(1);

next:

	string descdir(getconfig().GetDescDir());

	// dir: "{dest}/proto"
	string skeleton_path = descdir + "/skeleton/";
	if (createdir(skeleton_path.c_str())) {
		goto error;
	}

	// add the file name
	string filename = getname() + ".cpp";
	skeleton_path += filename;

	FILE *fp = fopen(skeleton_path.c_str(), "w");
	if (nullptr == fp) goto error;

	// write the begin comments
	::fprintf(fp, "#include \"%s.h\"\n\n", getname().c_str());
	::fprintf(fp, 
		"namespace %s {\n\n", getname().c_str());

	// success
	_fp_module_source = fp;
}

bool module_object_cpp_code_generator::generate_protobuf_includes(FILE* fp)
{
	auto* item = _protobuf_inc_module_list.getnext();
	for (; item != &_protobuf_inc_module_list; item = item->getnext()) {
		auto* node = LIST_ENTRY(included_module_node, ownerlist, item);
		::fprintf(fp, "import \"%s.pro\";\n", node->mod->getname().c_str());
	}
	if (!_protobuf_inc_module_list.is_empty()) {
		::fputs("\n", fp);
	}
	return true;
}

bool module_object_cpp_code_generator::generate_moudle_includes(FILE* fp)
{
	auto* item = _inc_module_list.getnext();
	for (; item != &_inc_module_list; item = item->getnext()) {
		auto* node = LIST_ENTRY(included_module_node, ownerlist, item);
		::fprintf(fp, "#include \"%s.h\"\n", node->mod->getname().c_str());
	}
	if (!_inc_module_list.is_empty()) {
		::fputs("\n", fp);
	}
	return true;
}

void module_object_cpp_code_generator::create_srcfile_module_proxy_structs(void)
{
	goto next;
error:
	printf("fatal error in creating struct source file "
		"[module:%s].\n", getname().c_str());
	exit(2);

next:

	string descdir(getconfig().GetDescDir());
	string struct_path = descdir + "/proxy/";

	// add the file name
	string filename = getname() + "-structs.cpp";
	struct_path += filename;

	FILE* fp = fopen(struct_path.c_str(), "w");
	if (nullptr == fp) goto error;

	// write the begin comments
	write_srcfile_begin_comments(fp, filename);
	::fprintf(fp, "#include \"mware/rpc/rpcerror.h\"\n"
		"#include \"common/%s-internal.h\"\n\n", getname().c_str());
	::fprintf(fp, "namespace %s {\n\n", getname().c_str());

	// success
	_fp_module_proxy_structs = fp;
}

int module_object_cpp_code_generator::create_srcfile_module_header(string dir)
{
	// if it is the global scope, and the scope is empty
	// means there is nothing declared in the global scope
	// than we will not generate the global header file
	if (test_flags(grammar_object_flags_globalscope)) {
		if (is_empty()) return 0;
	}

	dir = dir + getname() + ".h";
	_fp_module_h = fopen(dir.c_str(), "w");
	if (nullptr == _fp_module_h) return 1;

	write_srcfile_begin_comments(_fp_module_h, getname() + ".h");

	string ifdef_identifier;
	get_header_file_ifdef_identifer(ifdef_identifier, 0);

	// #ifndef ... #define
	::fputs("#ifndef ", _fp_module_h);
	::fputs(ifdef_identifier.c_str(), _fp_module_h);
	::fputs("\n#define ", _fp_module_h);
	::fputs(ifdef_identifier.c_str(), _fp_module_h);
	::fputs("\n\n", _fp_module_h);

	::fputs(
		"#ifndef ZIDL_PREFIX\n"
		"#	ifdef ZIDL_EXPORT\n"
		"#	define ZIDL_PREFIX __attribute__((visibility(\"default\")))\n"
		"#	else\n"
		"#	define ZIDL_PREFIX\n"
		"#	endif\n"
		"#endif\n\n", _fp_module_h);

	::fprintf(_fp_module_h, "#include <stdint.h>\n");
	::fprintf(_fp_module_h, "#include \"mware/rpc/codegen-helper.h\"\n");

	// check if we need to include the global header file
	auto* psr = get_gammar_parser();
	auto* gblmod = psr->get_globalscope();
	if (gblmod && !gblmod->is_empty()) {
		::fprintf(_fp_module_h, "#include \"global.h\"\n");
	}

	generate_moudle_includes(_fp_module_h);

	::fputs("\nusing namespace zas::mware::rpc;\n\n", _fp_module_h);
	return 0;
}

int module_object_cpp_code_generator::create_srcfile_module_proxy_internal_header(string dir)
{
	// global scope have no internal header file
	if (test_flags(grammar_object_flags_globalscope)) {
		return 0;
	}

	dir = dir + getname() + "-internal.h";
	_fp_module_proxy_internal_h = fopen(dir.c_str(), "w");
	if (nullptr == _fp_module_proxy_internal_h) return 1;

	write_srcfile_begin_comments(_fp_module_proxy_internal_h, getname()
		+ "-internal.h");

	// #ifndef ... #define
	string ifdef_identifier;
	get_header_file_ifdef_identifer(ifdef_identifier,
		MODOBJ_CPP_CODEGEN_INTERNAL_HDRFILE);

	::fputs("#ifndef ", _fp_module_proxy_internal_h);
	::fputs(ifdef_identifier.c_str(), _fp_module_proxy_internal_h);
	::fputs("\n#define ", _fp_module_proxy_internal_h);
	::fputs(ifdef_identifier.c_str(), _fp_module_proxy_internal_h);
	::fputs("\n\n", _fp_module_proxy_internal_h);
	return 0;
}

bool module_object_cpp_code_generator::generate_namespace(void)
{
	if (test_flags(grammar_object_flags_globalscope)) {
		return true;
	}

	// if this is not global scope, we generate the "namespace" declaration
	::fprintf(_fp_module_proxy_internal_h, "#include \"%s.h\"\n",
		getname().c_str());
	::fprintf(_fp_module_proxy_internal_h, "#include \"proto/%s.pro.pb.h\"\n\n",
		getname().c_str());
	::fprintf(_fp_module_proxy_internal_h,
		"using namespace zas::mware::rpc;\n\n");
	::fprintf(_fp_module_h, "namespace %s {\n\n", getname().c_str());
	::fprintf(_fp_module_proxy_internal_h, "namespace %s {\n",
		getname().c_str());
	::fprintf(_fp_module_proxy_internal_h, "namespace struct_wrapper {\n\n");
	return true;
}

void module_object_cpp_code_generator::write_srcfile_begin_comments(
	FILE* fp, string filename)
{
	assert(nullptr != fp);
	::fprintf(fp, "/*\n");
	::fprintf(fp, " * Generated by the zrpc compiler (rpcc).  DO NOT EDIT!\n");
	::fprintf(fp, " * @(#)$Id: %s\n", filename.c_str());
	::fprintf(fp, " */\n\n");
}

void module_object_cpp_code_generator::get_header_file_ifdef_identifer(
	string& ret, uint32_t flags)
{
	char buffer[256];
	char* buf = buffer, *end = buf + 256;

	buf += snprintf(buf, end - buf, "__CXX_ZRPCC_AUTOGEN_");

	// upper-case the service name
	string service_name(getconfig().get_service_name());
	upper(service_name);

	bool has_service_name = false;
	if (!service_name.empty()) {
		buf += snprintf(buf, end - buf, "%s_", service_name.c_str());
		has_service_name = true;
	}

	if (!test_flags(grammar_object_flags_globalscope)) {
		string scope_name(getname());
		upper(scope_name);
		buf += snprintf(buf, end - buf, "%s_", scope_name.c_str());
	} else if (!has_service_name) {
		buf += snprintf(buf, end - buf, "GLOBAL_");
	}

	if (flags & MODOBJ_CPP_CODEGEN_INTERNAL_HDRFILE) {
		buf += snprintf(buf, end - buf, "INTERNAL_");
	}
	ret.assign(buffer, buf - buffer);
	ret.append("H__");
}

FILE** module_object_cpp_code_generator::getfile(srcfile_type type)
{
	FILE **ret = nullptr;
	switch (type)
	{
	case srcfile_module_proxy_header:
		ret = &_fp_module_h;
		break;

	case srcfile_module_proxy_internal_header:
		ret = &_fp_module_proxy_internal_h;
		break;

	case srcfile_module_protobuf:
		ret = &_fp_module_protobuf;
		if (!_fp_module_protobuf) {
			create_srcfile_protobuf();
		}
		break;

	case srcfile_module_proxy_structs:
		ret = &_fp_module_proxy_structs;
		if (!_fp_module_proxy_structs) {
			create_srcfile_module_proxy_structs();
		}
		break;
	case scrfile_module_proxy_source:
		ret = &_fp_module_proxy_source;
		if (!_fp_module_proxy_source) {
			create_srcfile_proxy();
		}
		break;
	case scrfile_module_skeleton_source:
		ret = &_fp_module_skeleton_source;
		if (!_fp_module_skeleton_source) {
			create_srcfile_skeleton();
		}
		break;
	case scrfile_module_source:
		ret = &_fp_module_source;
		if (!_fp_module_source) {
			create_srcfile();
		}
		break;
	default: break;
	}
	return ret;
}

bool module_object_cpp_code_generator::generate_typedefs(void)
{
	for (int i = 0; i < HASH_MAX; ++i) {
		list_item &h = _typedef_header[i];
		list_item* next = h.getnext();
		for (; next != &h; next = next->getnext()) {
			auto *obj = LIST_ENTRY(typedef_object, m_OwnerList, next);
			if (!obj->generate_code()) {
				return false;
			}
		}
	}
	fputs(srcfile_module_proxy_header, "\n");
	return true;
}

bool module_object_cpp_code_generator::generate_enums(void)
{
	for (int i = 0; i < HASH_MAX; ++i) {
		list_item &h = _enumdef_header[i];
		list_item* next = h.getnext();
		for (; next != &h; next = next->getnext()) {
			auto *obj = LIST_ENTRY(enumdef_object, m_OwnerList, next);
			if (!obj->generate_code()) {
				return false;
			}
		}
	}
	return true;
}

bool module_object_cpp_code_generator::generate_constitems(void)
{
	for (int i = 0; i < HASH_MAX; ++i) {
		list_item &h = _constitem_header[i];
		list_item* next = h.getnext();
		for (; next != &h; next = next->getnext()) {
			auto *obj = LIST_ENTRY(TConstItemObject, m_OwnerList, next);
			if (!obj->generate_code()) {
				return false;
			}
		}
	}
	return true;
}

bool module_object_cpp_code_generator::generate_userdef_types(void)
{
	for (int i = 0; i < HASH_MAX; ++i) {
		list_item &h = _userdef_type_header[i];
		list_item* next = h.getnext();
		for (; next != &h; next = next->getnext()) {
			auto *obj = LIST_ENTRY(userdef_type_object, m_OwnerList, next);
			if (!obj->generate_code()) {
				return false;
			}
		}
	}
	return true;
}

bool module_object_cpp_code_generator::generate_global_scope_userdef_types(void)
{
	for (int i = 0; i < HASH_MAX; ++i) {
		list_item &h = _userdef_type_header[i];
		list_item* next = h.getnext();
		for (; next != &h; next = next->getnext()) {
			auto *obj = LIST_ENTRY(userdef_type_object, m_OwnerList, next);
			if (!obj->global_scope_generate_code()) {
				return false;
			}
		}
	}
	return true;
}

bool module_object_cpp_code_generator::generate_global_scope_enums(void)
{
	for (int i = 0; i < HASH_MAX; ++i) {
		list_item &h = _enumdef_header[i];
		list_item* next = h.getnext();
		for (; next != &h; next = next->getnext()) {
			auto *obj = LIST_ENTRY(enumdef_object, m_OwnerList, next);
			if (!obj->global_scope_generate_code()) {
				return false;
			}
		}
	}
	return true;
}

bool module_object_cpp_code_generator::generate_interfaces(void)
{
	for (int i = 0; i < HASH_MAX; ++i)
	{
		list_item &h = _interface_header[i];
		list_item* next = h.getnext();
		for (; next != &h; next = next->getnext())
		{
			auto *obj = LIST_ENTRY(interface_object, m_OwnerList, next);
			if (!obj->generate_code())
				return false;
		}
	}
	return true;
}

bool module_object_cpp_code_generator::generate_event(void)
{
	for (int i = 0; i < HASH_MAX; ++i)
	{
		list_item &h = _event_header[i];
		list_item* next = h.getnext();
		for (; next != &h; next = next->getnext())
		{
			TEventObject *obj = LIST_ENTRY(TEventObject, m_OwnerList, next);
			if (!obj->generate_code())
				return false;
		}
	}
	return true;
}

bool module_object_cpp_code_generator::generate_finished(void)
{
	if (_fp_module_proxy_internal_h) {

		string ifdef_identifier;
		get_header_file_ifdef_identifer(ifdef_identifier,
			MODOBJ_CPP_CODEGEN_INTERNAL_HDRFILE);
		::fprintf(_fp_module_proxy_internal_h,
			"}// end of namespace struct_wrapper\n");
		::fprintf(_fp_module_proxy_internal_h,
			"}// end of namespace %s\n",
			 getname().c_str());
		::fprintf(_fp_module_proxy_internal_h,
			"#endif /* %s */\n", ifdef_identifier.c_str());
		::fprintf(_fp_module_proxy_internal_h,
			"/* EOF */");
		get_header_file_ifdef_identifer(ifdef_identifier, 0);
		::fprintf(_fp_module_h,
			"}// end of namespace %s\n",
			 getname().c_str());
		::fprintf(_fp_module_h,
			"#endif /* %s */\n", ifdef_identifier.c_str());
		::fprintf(_fp_module_h,
			"/* EOF */");
		if (_fp_module_proxy_source)
		::fprintf(_fp_module_proxy_source,
			"}} // end of namespace %s::proxy", getname().c_str());
		if (_fp_module_skeleton_source)
		::fprintf(_fp_module_skeleton_source,
			"}} // end of namespace %s::skeleton", getname().c_str());
		if (_fp_module_source)
		::fprintf(_fp_module_source,
			"} // end of namespace %s", getname().c_str());
		if (_fp_module_proxy_structs)
		::fprintf(_fp_module_proxy_structs,
			"} // end of namespace %s", getname().c_str());
	}
	return true;
}

bool module_object_cpp_code_generator::generate_code(void)
{
	if (test_flags(grammar_object_flags_code_generated)) {
		return true;
	}
	set_flags(grammar_object_flags_code_generated);

	if (generate_include_list()) {
		return false;
	}

	if (create_files()) {
		return false;
	}

	// generate the user defined type related contents
	if (!generate_global_scope_userdef_types()) {
		return false;
	}

	if (!generate_namespace()) {
		return false;
	}

	// generate the code for const
	if (!generate_constitems()) {
		return false;
	}

	// generate the code for typedefs
	if (!generate_typedefs()) {
		return false;
	}

	if (!generate_enums()) {
		return false;
	}
	
	// generate the user defined type
	if (!generate_userdef_types()) {
		return false;
	}

	// generate the interfaces
	if (!generate_interfaces())
		return false;

	if (!generate_event())
		return false;

	if (!generate_finished())
		return false;

	return true;
}

bool module_object_cpp_code_generator::SetOutputReplacement(
	const char *org, const char *rep)
{
	return true;
}

bool module_object_cpp_code_generator::file_handler_error(FILE **fp)
{
	assert(nullptr != fp);

	if (nullptr != *fp)
		return true;

	if (!test_flags(grammar_object_flags_code_file_created)) {
		if (create_files()) {
			return false;
		}
		if (nullptr == *fp || !test_flags(grammar_object_flags_code_file_created)) {
			return false;
		}
		return true;
	}
	return false;
}
bool module_object_cpp_code_generator::fputs(const char *filename, 
srcfile_type type, const char *content)
{
	return false;
}
bool module_object_cpp_code_generator::fputs(
	srcfile_type type, const char *content)
{
	FILE** fp = getfile(type);
	if (nullptr == fp) return false;

	if (!file_handler_error(fp) || nullptr == content)
		return false;

	string tmp;
	if (!_replace_original.empty()) {
		size_t pos = 0;
		const char *s = content, *p = s, *e = s + strlen(content);
		while (s + _replace_original.length() <= e) {
			if (!strncmp(s, _replace_original.c_str(), _replace_original.length()))
			{
				tmp.append(p, s);
				tmp.append(_replace_new);
				p = s;
				s += _replace_original.length();
			}
			else s++;
		}
		if (s < e) tmp.append(s);
		content = tmp.c_str();
	}

	::fputs(content, *fp);
	return true;
}

bool module_object_cpp_code_generator::fprintf(const char *filename, 
	srcfile_type type, const char *content, ...)
{
	return false;
}

bool module_object_cpp_code_generator::fprintf(
	srcfile_type type, const char *content, ...)
{
	va_list ap;
	va_start(ap, content);
	bool ret = fprintf_common(type, content, ap);
	va_end(ap);
	return ret;
}

bool module_object_cpp_code_generator::fprintf_common(
	srcfile_type type, const char* content, va_list ap)
{
	FILE** fp = getfile(type);
	if (nullptr == fp) return false;

	if (!file_handler_error(fp) || nullptr == content) {
		return false;
	}

	string tmp;
	if (!_replace_original.empty())
	{
		size_t pos = 0;
		const char *s = content, *p = s, *e = s + strlen(content);
		while (s + _replace_original.length() <= e)
		{
			if (!strncmp(s, _replace_original.c_str(), _replace_original.length()))
			{
				tmp.append(p, s);
				tmp.append(_replace_new);
				p = s;
				s += _replace_original.length();
			}
			else s++;
		}
		if (s < e) tmp.append(s);
		content = tmp.c_str();
	}

	vfprintf(*fp, content, ap);
	return true;
}

void module_object_cpp_code_generator::
	release_include_module_list(list_item& lst)
{
	while (!lst.is_empty()) {
		auto* node = LIST_ENTRY(included_module_node, ownerlist, \
		lst.getnext());
		node->ownerlist.Delete();
		delete node;
	}
}

bool module_object_cpp_code_generator::
	is_module_included(list_item& lst, module* mod)
{
	assert(nullptr != mod);
	auto* item = lst.getnext();
	for (; item != &lst; item = item->getnext()) {
		auto* node = LIST_ENTRY(included_module_node, ownerlist, item);
		if (node->mod == mod) return true;
	}
	return false;
}

void module_object_cpp_code_generator::
	set_module_included(list_item& lst, module* mod)
{
	assert(nullptr != mod);
	if (is_module_included(lst, mod)) {
		return;
	}

	auto* node = new included_module_node;
	assert(nullptr != node);
	node->mod = mod;
	node->ownerlist.AddTo(lst);
}

int module_object_cpp_code_generator::
	generate_interface_include_list(list_item& lst)
{
	for (int i = 0; i < HASH_MAX; ++i)
	{
		list_item& h = InterfaceHeader()[i];
		list_item *item = h.getnext();
		for (; item != &h; item = item->getnext())
		{
			auto *ifobj = LIST_ENTRY(interface_object, m_OwnerList, item);
			module **modobj = ifobj->GetDependentModule(), **mdel = modobj;
			if (NULL == modobj) continue;

			for (; *modobj; modobj++)
			{
				module* mobj = *modobj;
				if (NULL == mobj || mobj == this
					|| is_module_included(lst, mobj)) {
					continue;
				}
				set_module_included(lst, mobj);
			}
			delete [] mdel;
		}
	}
	return true;
}

int module_object_cpp_code_generator
::generate_userdeftype_include_list(list_item& lst)
{
	for (int i = 0; i < HASH_MAX; ++i)
	{
		list_item& h = UserDefTypeItemHeader()[i];
		list_item *item = h.getnext();
		for (; item != &h; item = item->getnext())
		{
			auto *ifobj = LIST_ENTRY(userdef_type_object, m_OwnerList, item);
			module **modobj = ifobj->GetDependentModule(), **mdel = modobj;
			if (NULL == modobj) continue;

			for (; *modobj; modobj++)
			{
				module* mobj = *modobj;
				if (NULL == mobj || mobj == this
					|| is_module_included(lst, mobj)) {
					continue;
				}
				set_module_included(lst, mobj);
			}
			delete [] mdel;
		}
	}
	return true;
}

int module_object_cpp_code_generator
::generate_event_include_list(list_item& lst)
{
	for (int i = 0; i < HASH_MAX; ++i)
	{
		list_item& h = EventHeader()[i];
		list_item *item = h.getnext();
		for (; item != &h; item = item->getnext())
		{
			auto *ifobj = LIST_ENTRY(TEventObject, m_OwnerList, item);
			module **modobj = ifobj->GetDependentModule(), **mdel = modobj;
			if (NULL == modobj) continue;

			for (; *modobj; modobj++)
			{
				module* mobj = *modobj;
				if (NULL == mobj || mobj == this
					|| is_module_included(lst, mobj)) {
					continue;
				}
				set_module_included(lst, mobj);
			}
			delete [] mdel;
		}
	}
	return true;
}


int module_object_cpp_code_generator::generate_include_list(void)
{
	if (!generate_interface_include_list(
		_protobuf_inc_module_list)) {
		return 1;
	}

	if (!generate_userdeftype_include_list(
		_protobuf_inc_module_list)) {
		return 2;
	}
	if (!generate_interface_include_list(
		_inc_module_list)) {
		return 3;
	}

	if (!generate_userdeftype_include_list(
		_inc_module_list)) {
		return 4;
	}

	if (!generate_event_include_list(_inc_module_list)) {
		return 5;
	}

//GenerateConstItemIncludeFiles
//GenerateTypedefIncludeFiles

	return 0;
}
