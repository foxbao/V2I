/** @file inc/cgjava-module.h
 * implementation of java code generation of module for zrpc
 */

#include <assert.h>
#include <string.h>

#include "error.h"
#include "parser.h"
#include "codegenjava.h"
#include "utilities.h"
#include "config.h"

using namespace std;
using namespace utilities;

void module_object_java_code_generator::upper(string& s)
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

module_object_java_code_generator::module_object_java_code_generator(
	const string& modname, grammar_parser* gpsr, config& cfg)
	: module(modname, gpsr, cfg)
	, _fp_module_h(nullptr)
	, _fp_module_proxy_internal_h(nullptr)
	, _fp_module_protobuf(nullptr)
	, _fp_module_proxy_structs(nullptr)
	, _fp_module_proxy_source(nullptr)
	, _fp_module_skeleton_source(nullptr)
	, _fp_module_source(nullptr)
	, _fp_module_source_virtual(nullptr)
	, _fp_module_enum_source(nullptr)
	, _fp_module_observer_source(nullptr)
{
}

module_object_java_code_generator::~module_object_java_code_generator()
{
	close_files();
	release_all_userdef_array_info();
	release_include_module_list(_protobuf_inc_module_list);
}

void module_object_java_code_generator::close_files(void)
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
	if (_fp_module_source_virtual) {
		fclose(_fp_module_source_virtual);
		_fp_module_source_virtual = nullptr;
	}
	if(_fp_module_enum_source) {
		fclose(_fp_module_enum_source);
		_fp_module_enum_source = nullptr;
	}
	if(_fp_module_final_source) {
		::fputs("}\n", _fp_module_final_source);
		fclose(_fp_module_final_source);
		_fp_module_final_source = nullptr;
	}
	if(_fp_module_event_source) {
		::fputs("}\n", _fp_module_event_source);
		fclose(_fp_module_event_source);
		_fp_module_event_source = nullptr;
	}
	if(_fp_module_observer_source) {
		fclose(_fp_module_observer_source);
		_fp_module_observer_source = nullptr;
	}
	if(_fp_module_observer_interface_source) {
		fclose(_fp_module_observer_interface_source);
		_fp_module_observer_interface_source = nullptr;
	}
}

void module_object_java_code_generator::closefile(srcfile_type type){
	switch (type)
	{
	case scrfile_module_proxy_source:
		fclose(_fp_module_protobuf);
		_fp_module_protobuf = nullptr;
		break;
	case scrfile_module_source:
		fclose(_fp_module_source);
		_fp_module_source = nullptr;
		break;
	case scrfile_module_source_virtual:
		fclose(_fp_module_source_virtual);
		_fp_module_source_virtual = nullptr;
		break;
	case scrfile_module_enum_source:
		fclose(_fp_module_enum_source);
		_fp_module_enum_source = nullptr;
		break;
	case scrfile_module_observer_source:
		fclose(_fp_module_observer_source);
		_fp_module_observer_source = nullptr;
		break;
	case scrfile_module_observer_interface_source:
		fclose(_fp_module_observer_interface_source);
		_fp_module_observer_interface_source = nullptr;
		break;
	default:
		break;
	}
}

module_userdef_array_info* module_object_java_code_generator
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

int module_object_java_code_generator::
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

void module_object_java_code_generator::
	release_all_userdef_array_info(void)
{
	while(!_userdef_array_info_list.is_empty()) {
		auto* item = LIST_ENTRY(module_userdef_array_info, ownerlist, \
			_userdef_array_info_list.getnext());
		item->ownerlist.Delete();
		delete item;
	}
}

int module_object_java_code_generator::create_files(void)
{
	if (test_flags(grammar_object_flags_code_file_created)) {
		return 0;
	}
	set_flags(grammar_object_flags_code_file_created);

	string descdir(getconfig().GetDescDir());
	descdir += "/src/main/java/com/civip/rpc/" + getname()+"/";

	// dir: "{dest}/entity"
	string entity_dir = descdir + "entity/";
	if (createdir(entity_dir.c_str())) {
		return 1;
	}

	// dir: "{dest}/service"
	string service_dir = descdir + "service/";
	if (createdir(service_dir.c_str())) {
		return 2;
	}
	return 0;
}

void module_object_java_code_generator::create_srcfile_protobuf(void)
{
	goto next;
error:
	printf("fatal error in creating protobuffer source file "
		"[module:%s].\n", getname().c_str());
	exit(1);

next:

	string descdir(getconfig().GetDescDir());

	// dir: "{dest}/proto"
	string proto_path = descdir + "/src/main/proto/";
	if (createdir(proto_path.c_str())) {
		goto error;
	}

	// add the file name
	string filename = getname() + ".proto";
	proto_path += filename;

	FILE *fp = fopen(proto_path.c_str(), "w");
	if (nullptr == fp) goto error;

	// write the begin comments
	write_srcfile_begin_comments(fp, filename);
	::fputs("syntax = \"proto3\";\n\n", fp);

	if (!generate_protobuf_includes(fp)) {
		goto error;
	}
	::fprintf(fp,"option java_package = \"com.civip.rpc.%s.entity\";\n\n", 
		getname().c_str());
	::fprintf(fp,"option java_outer_classname = \"%s\";\n\n", 
		getname().c_str());
	::fputs("option optimize_for = CODE_SIZE;\n\n", fp);
	// write included protobuf
	::fprintf(fp, "package %s_pbf;\n\n", getname().c_str());

	// success
	_fp_module_protobuf = fp;
}

void module_object_java_code_generator::create_srcfile_proxy(void)
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
	::fprintf(fp, "#include \"%s-internal.h\"\n\n", getname().c_str());
	::fprintf(fp, 
		"namespace %s {\n"
		"namespace proxy {\n\n", getname().c_str());

	// success
	_fp_module_proxy_source = fp;
}

void module_object_java_code_generator::create_srcfile_skeleton(void)
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
	::fprintf(fp, "#include \"%s-internal.h\"\n\n", getname().c_str());
	::fprintf(fp, 
		"namespace %s {\n"
		"namespace skeleton {\n\n"
		"using namespace zas::mware::rpc;\n\n", getname().c_str());

	// success
	_fp_module_skeleton_source = fp;
}

void module_object_java_code_generator::create_final_srcfile(
	const char* filename){
	goto next;
error:
	printf("fatal error in creating source file "
		"[module:%s].\n", getname().c_str());
	exit(1);

next:

	string descdir(getconfig().GetDescDir());

	// dir: "{dest}/proto"
	string skeleton_path = descdir + "/src/main/java/com/civip/rpc/"
		 + getname()+"/entity/";
	if (createdir(skeleton_path.c_str())) {
		goto error;
	}

	// add the file name
	// string filename = getname() + ".java";
	skeleton_path += +"I" + getname() + "Const";
	skeleton_path += ".java";
	FILE *fp = fopen(skeleton_path.c_str(), "w");
	if (nullptr == fp) goto error;

	// write the begin comments
	::fprintf(fp, "package com.civip.rpc.%s.entity;\n\n", getname().c_str());
	::fprintf(fp, "public interface I%sConst { \n\n", getname().c_str());
	// success
	_fp_module_final_source = fp;
}

void module_object_java_code_generator::create_event_srcfile(
	const char* filename){
	goto next;
error:
	printf("fatal error in creating source file "
		"[module:%s].\n", getname().c_str());
	exit(1);

next:

	string descdir(getconfig().GetDescDir());

	// dir: "{dest}/proto"
	string skeleton_path = descdir + "/src/main/java/com/civip/rpc/"
		 + getname()+"/service/";
	if (createdir(skeleton_path.c_str())) {
		goto error;
	}

	// add the file name
	// string filename = getname() + ".java";
	skeleton_path += getname()+"Event";
	skeleton_path += ".java";
	FILE *fp = fopen(skeleton_path.c_str(), "w");
	if (nullptr == fp) goto error;

	// write the begin comments
	::fprintf(fp, "package com.civip.rpc.%s.service;\n\n", getname().c_str());
	::fputs("import com.civip.rpc.core.anno.*;\n", fp);
	::fputs("import com.civip.rpc.core.anno.enumm.*;\n", fp);
	::fputs("import com.civip.rpc.core.type.*;\n", fp);
	::fputs("import org.springframework.stereotype.Component;\n\n", fp);

	string content = "import com.civip.rpc.%s.entity.*;\n\n";
	content += "@Component\n";
	content += "@RPC_service(pkg = \"%s\","; 
	content += "name = \"%s\")\n";
	content += "public class %s {\n\n";

	::fprintf(fp, content.c_str(),getname().c_str(), 
	getconfig().get_package_name().c_str(), 
	getconfig().get_service_name().c_str(), (getname()+"Event").c_str());

	// success
	_fp_module_event_source = fp;
}

void module_object_java_code_generator::create_observer_srcfile(
	const char* filename)
{
	goto next;
error:
	printf("fatal error in creating source file "
		"[module:%s].\n", getname().c_str());
	exit(1);

next:

	string descdir(getconfig().GetDescDir());

	// dir: "{dest}/proto"
	string skeleton_path = descdir + "/src/main/java/com/civip/rpc/"
		 + getname()+"/service/";
	skeleton_path += filename;
	skeleton_path += ".java";
	FILE *fp = fopen(skeleton_path.c_str(), "w");
	if (nullptr == fp) goto error;

	// write the begin comments
	::fprintf(fp, "package com.civip.rpc.%s.service;\n\n", getname().c_str());
	::fputs("import com.civip.rpc.core.anno.*;\n", fp);
	::fputs("import com.civip.rpc.core.anno.enumm.*;\n", fp);
	::fputs("import com.civip.rpc.core.type.*;\n", fp);

	_fp_module_observer_source = fp;
}

void module_object_java_code_generator::create_observer_interface_srcfile(
	const char* filename)
{
	goto next;
error:
	printf("fatal error in creating source file "
		"[module:%s].\n", getname().c_str());
	exit(1);

next:

	string descdir(getconfig().GetDescDir());

	// dir: "{dest}/proto"
	string skeleton_path = descdir + "/src/main/java/com/civip/rpc/"
		 + getname()+"/service/";
	skeleton_path += filename;
	skeleton_path += ".java";
	FILE *fp = fopen(skeleton_path.c_str(), "w");
	if (nullptr == fp) goto error;

	// write the begin comments
	::fprintf(fp, "package com.civip.rpc.%s.service;\n\n", getname().c_str());
	::fputs("import com.civip.rpc.core.anno.*;\n", fp);
	::fputs("import com.civip.rpc.core.anno.enumm.*;\n", fp);
	::fputs("import com.civip.rpc.core.type.*;\n", fp);

	_fp_module_observer_interface_source = fp;
}

void module_object_java_code_generator::create_enum_srcfile(const char* filename)
{
	goto next;
error:
	printf("fatal error in creating source file "
		"[module:%s].\n", getname().c_str());
	exit(1);

next:

	string descdir(getconfig().GetDescDir());

	// dir: "{dest}/proto"
	string skeleton_path = descdir + "/src/main/java/com/civip/rpc/"
		 + getname()+"/entity/";
	if (createdir(skeleton_path.c_str())) {
		goto error;
	}

	// add the file name
	// string filename = getname() + ".java";
	skeleton_path += filename;
	skeleton_path += ".java";
	FILE *fp = fopen(skeleton_path.c_str(), "w");
	if (nullptr == fp) goto error;

	// write the begin comments
	::fprintf(fp, "package com.civip.rpc.%s.entity;\n\n", getname().c_str());

	// success
	_fp_module_enum_source = fp;
}

void module_object_java_code_generator::create_srcfile(const char* filename)
{
	goto next;
error:
	printf("fatal error in creating source file "
		"[module:%s].\n", getname().c_str());
	exit(1);

next:

	string descdir(getconfig().GetDescDir());

	// dir: "{dest}/proto"
	string skeleton_path = descdir + "/src/main/java/com/civip/rpc/"
		 + getname()+"/service/";
	if (createdir(skeleton_path.c_str())) {
		goto error;
	}

	// add the file name
	// string filename = getname() + ".java";
	skeleton_path += filename;
	skeleton_path += ".java";
	FILE *fp = fopen(skeleton_path.c_str(), "w");
	if (nullptr == fp) goto error;

	// write the begin comments
	::fprintf(fp, "package com.civip.rpc.%s.service;\n\n", getname().c_str());
	::fputs("import com.civip.rpc.core.anno.*;\n", fp);
	::fputs("import com.civip.rpc.core.anno.enumm.*;\n", fp);
	::fputs("import com.civip.rpc.core.type.*;\n", fp);
	::fputs("import com.civip.rpc.client.service.RPCService;\n", fp);
	::fputs("import org.springframework.stereotype.Component;\n\n", fp);

	// success
	_fp_module_source = fp;
}

void module_object_java_code_generator::create_virtual_srcfile(const char* filename)
{
	goto next;
error:
	printf("fatal error in creating source file "
		"[module:%s].\n", getname().c_str());
	exit(1);

next:

	string descdir(getconfig().GetDescDir());

	// dir: "{dest}/proto"
	string skeleton_path = descdir + "/src/main/java/com/civip/rpc/"
		 + getname()+"/service/";
	if (createdir(skeleton_path.c_str())) {
		goto error;
	}

	// add the file name
	// string filename = getname() + ".java";
	skeleton_path += filename;
	skeleton_path += ".java";
	FILE *fp = fopen(skeleton_path.c_str(), "w");
	if (nullptr == fp) goto error;

	// write the begin comments
	::fprintf(fp, "package com.civip.rpc.%s.service;\n\n", getname().c_str());

	// success
	_fp_module_source_virtual = fp;
}

bool module_object_java_code_generator::generate_protobuf_includes(FILE* fp)
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

void module_object_java_code_generator::create_srcfile_module_proxy_structs(void)
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
	string filename = getname() + "-structs.java";
	struct_path += filename;

	FILE* fp = fopen(struct_path.c_str(), "w");
	if (nullptr == fp) goto error;

	// write the begin comments
	write_srcfile_begin_comments(fp, filename);
	::fprintf(fp, "#include \"mware/rpc/rpcerror.java\"\n"
		"#include \"%s-internal.java\"\n\n", getname().c_str());
	::fprintf(fp, "namespace %s {\n\n", getname().c_str());

	// success
	_fp_module_proxy_structs = fp;
}

int module_object_java_code_generator::create_srcfile_module_header(string dir)
{
	// if it is the global scope, and the scope is empty
	// means there is nothing declared in the global scope
	// than we will not generate the global header file
	if (test_flags(grammar_object_flags_globalscope)) {
		if (is_empty()) return 0;
	}

	dir = dir + getname() + ".java";
	_fp_module_h = fopen(dir.c_str(), "w");
	if (nullptr == _fp_module_h) return 1;

	write_srcfile_begin_comments(_fp_module_h, getname() + ".java");

	string ifdef_identifier;
	get_header_file_ifdef_identifer(ifdef_identifier, 0);

	// #ifndef ... #define
	::fputs("#ifndef ", _fp_module_h);
	::fputs(ifdef_identifier.c_str(), _fp_module_h);
	::fputs("\n#define ", _fp_module_h);
	::fputs(ifdef_identifier.c_str(), _fp_module_h);
	::fputs("\n\n", _fp_module_h);

	::fprintf(_fp_module_h, "#include <stdint.h>\n");
	::fprintf(_fp_module_h, "#include \"mware/rpc/codegen-helper.h\"\n");

	// check if we need to include the global header file
	auto* psr = get_gammar_parser();
	auto* gblmod = psr->get_globalscope();
	if (gblmod && !gblmod->is_empty()) {
		::fprintf(_fp_module_h, "#include \"global.h\"\n");
	}
	::fputs("\nusing namespace zas::mware::rpc;\n\n", _fp_module_h);
	return 0;
}

int module_object_java_code_generator::create_srcfile_module_proxy_internal_header(string dir)
{
	// global scope have no internal header file
	if (test_flags(grammar_object_flags_globalscope)) {
		return 0;
	}

	dir = dir + getname() + "-internal.java";
	_fp_module_proxy_internal_h = fopen(dir.c_str(), "w");
	if (nullptr == _fp_module_proxy_internal_h) return 1;

	write_srcfile_begin_comments(_fp_module_proxy_internal_h, getname()
		+ "-internal.java");

	// #ifndef ... #define
	string ifdef_identifier;
	get_header_file_ifdef_identifer(ifdef_identifier,
		MODOBJ_JAVA_CODEGEN_INTERNAL_HDRFILE);

	::fputs("#ifndef ", _fp_module_proxy_internal_h);
	::fputs(ifdef_identifier.c_str(), _fp_module_proxy_internal_h);
	::fputs("\n#define ", _fp_module_proxy_internal_h);
	::fputs(ifdef_identifier.c_str(), _fp_module_proxy_internal_h);
	::fputs("\n\n", _fp_module_proxy_internal_h);
	return 0;
}

void module_object_java_code_generator::write_srcfile_begin_comments(
	FILE* fp, string filename)
{
	assert(nullptr != fp);
	::fprintf(fp, "/*\n");
	::fprintf(fp, " * Generated by the zrpc compiler (rpcc).  DO NOT EDIT!\n");
	::fprintf(fp, " * @(#)$Id: %s\n", filename.c_str());
	::fprintf(fp, " */\n\n");
}

void module_object_java_code_generator::get_header_file_ifdef_identifer(
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

	if (flags & MODOBJ_JAVA_CODEGEN_INTERNAL_HDRFILE) {
		buf += snprintf(buf, end - buf, "INTERNAL_");
	}
	ret.assign(buffer, buf - buffer);
	ret.append("H__");
}

FILE** module_object_java_code_generator::getfile(const char* filename,
	srcfile_type type)
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
			create_srcfile(filename);
		}
		break;
	case scrfile_module_source_virtual:
		ret = &_fp_module_source_virtual;
		if (!_fp_module_source_virtual) {
			create_virtual_srcfile(filename);
		}
		break;
	case scrfile_module_enum_source:
		ret = &_fp_module_enum_source;
		if (!_fp_module_enum_source) {
			create_enum_srcfile(filename);
		}
		break;
	case scrfile_module_event_source:
		ret = &_fp_module_event_source;
		if(!_fp_module_event_source){
			create_event_srcfile(filename);
		}
		break;
	case scrfile_module_final_source:
		ret = &_fp_module_final_source;
		if(!_fp_module_final_source) {
			create_final_srcfile(filename);
		}
		break;
	case scrfile_module_observer_source:
		ret = &_fp_module_observer_source;
		if(!_fp_module_observer_source){
			create_observer_srcfile(filename);
		}
		break;
	case scrfile_module_observer_interface_source:
		ret = &_fp_module_observer_interface_source;
		if(!_fp_module_observer_interface_source) {
			create_observer_interface_srcfile(filename);
		}
	default: break;
	}
	return ret;
}

bool module_object_java_code_generator::generate_typedefs(void)
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
	fputs(nullptr,srcfile_module_proxy_header, "\n");
	return true;
}

bool module_object_java_code_generator::generate_userdef_types(void)
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

bool module_object_java_code_generator::generate_interfaces(void)
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

bool module_object_java_code_generator::generate_constitems(void)
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

bool module_object_java_code_generator::generate_event(void)
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

bool module_object_java_code_generator::generate_finished(void)
{
	if (_fp_module_proxy_internal_h) {

		string ifdef_identifier;
		get_header_file_ifdef_identifer(ifdef_identifier,
			MODOBJ_JAVA_CODEGEN_INTERNAL_HDRFILE);
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
		::fprintf(_fp_module_proxy_source,
			"}} // end of namespace %s::proxy", getname().c_str());
		::fprintf(_fp_module_skeleton_source,
			"}} // end of namespace %s::skeleton", getname().c_str());
		::fprintf(_fp_module_source,
			"}} // end of namespace %s", getname().c_str());
	}
	return true;
}

bool module_object_java_code_generator::generate_code(void)
{
	if(test_flags(grammar_object_flags_globalscope)){
		return true;
	}
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
	// if (!generate_global_scope_userdef_types()) {
	// 	return false;
	// }

	// if (!generate_namespace()) {
	// 	return false;
	// }

	// generate the code for typedefs
	// if (!generate_typedefs()) {
	// 	return false;
	// }
#if 0
	
	if (generate_enums()) {
		return false;
	}
#endif
	// generate the code for const
	if (!generate_constitems()) {
		return false;
	}
	// generate the user defined type
	if (!generate_userdef_types()) {
		return false;
	}

	// // generate the interfaces
	if (!generate_interfaces())
		return false;

	if (!generate_event())
		return false;

	if (!generate_finished())
		return false;

	return true;
}

bool module_object_java_code_generator::SetOutputReplacement(
	const char *org, const char *rep)
{
	return true;
}

bool module_object_java_code_generator::file_handler_error(FILE **fp)
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

bool module_object_java_code_generator::fputs( 
	srcfile_type type, const char *content)
{
	return false;
}

bool module_object_java_code_generator::fputs(const char *filename, 
	srcfile_type type, const char *content)
{
	FILE** fp = getfile(filename, type);
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

bool module_object_java_code_generator::fprintf(
	srcfile_type type, const char *content, ...)
{
	return false;
}

bool module_object_java_code_generator::fprintf(const char *filename, 
	srcfile_type type, const char *content, ...)
{
	va_list ap;
	va_start(ap, content);
	bool ret = fprintf_common(filename, type, content, ap);
	va_end(ap);
	return ret;
}

bool module_object_java_code_generator::fprintf_common(const char *filename, 
	srcfile_type type, const char* content, va_list ap)
{
	FILE** fp = getfile(filename, type);
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

void module_object_java_code_generator::
	release_include_module_list(list_item& lst)
{
	while (!lst.is_empty()) {
		auto* node = LIST_ENTRY(included_module_node, ownerlist, \
		lst.getnext());
		node->ownerlist.Delete();
		delete node;
	}
}

bool module_object_java_code_generator::
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

void module_object_java_code_generator::
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

int module_object_java_code_generator::
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

int module_object_java_code_generator::generate_include_list(void)
{
/*
	if (!GenerateTypedefIncludeFiles())
		return false;

	if (!GenerateConstItemIncludeFiles())
		return false;
*/
	if (!generate_interface_include_list(
		_protobuf_inc_module_list)) {
		return 1;
	}
/*
	if (!GenerateEventIncludeFiles())
		return false;

	if (!GenerateUserDefTypeIncludeFiles())
		return false;
*/
	return 0;
}
