/** @file rect.cpp
 * implementation of shader management
 */

#include <string>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include "utils/mutex.h"
#include "utils/thread.h"
#include "utils/dir.h"

#include "inc/shader.h"
#include "inc/grphgbl.h"

using namespace std;
using namespace zas::utils;

namespace zas {
namespace hwgrph {

// global mutex for shader operations
mutex _mut;

shader::shader(void* ctx, const char* name,
	uint32_t version,
	const char* vert_src,
	const char* frag_src,
	const char* vert_vartab)
: _flags(0)
, _name(name)
, _ctx(ctx)
, _vert(0), _frag(0)
, _program(0)
, _manager(NULL)
, _vert_vartab(vert_vartab)
{
	assert(name && *name);
	assert(vert_src && *vert_src);
	assert(frag_src && *frag_src);
	listnode_init(_ownerlist);
	for (int i = 0; i < glslvar_max; ++i)
		_export_var[i] = -1;

	load_program(name, version, vert_src, frag_src);
	_f.ready = 1;
}

shader::~shader()
{
	glDeleteProgram(_program);
	glDeleteShader(_frag);
	glDeleteShader(_vert);

	if (_manager) {
		auto_mutex am(_mut);
		_manager->remove_shader_unlocked(this);
	}
}

GLint shader::get_attrib(const char* name)
{
	if (!name || !*name)
		return -2;
	return glGetAttribLocation(_program, name);
}

GLint shader::get_uniform(const char* name)
{
	if (!name || !*name)
		return -2;
	return glGetUniformLocation(_program, name);
}

int shader::activate(void)
{
	if (!_f.ready) return -1;
	assert(NULL != _manager);

	// check if this is activated
	auto_mutex am(_mut);
	if (iscurrent()) {
		return 0;
	}

	glUseProgram(_program);
	_manager->setactive_unlocked(this);
	return 1;
}

static const char* glsl_expvar_search_tbl[] =
{
	"u_modelview",
	"a_position",
	"u_fillcolor",
	"a_srctexcord",
	"u_srctex",
	"u_count",
	"u_offset",
	"u_weight",
};

static int glsl_expvar_search_index(const char* name)
{
	for (int i = 0; i < glslvar_max; ++i) {
		const char* tgr = glsl_expvar_search_tbl[i];
		if (!strcmp(tgr, name)) return i;
	}
	return -1;
}

void shader::check_bind_varible(std::string& var)
{
	char c = var[0];
	int i = glsl_expvar_search_index(var.c_str());
	if (i < 0) goto error;

	if (c == 'a') {
		GLint idx = get_attrib(var.c_str());
		if (idx < 0) goto error;
		_export_var[i] = idx;
	}
	else if (c == 'u') {
		GLint idx = get_uniform(var.c_str());
		if (idx < 0) goto error;
		_export_var[i] = idx;
	}

	return; error:
	printf("glsl '%s': unrecognized glsl varible: %s\n",
		_name.c_str(), var.c_str());
	exit(9980);
}

void shader::generate_program(const char* vert_src, const char* frag_src)
{
	_frag = create_shader(frag_src, GL_FRAGMENT_SHADER);
	_vert = create_shader(vert_src, GL_VERTEX_SHADER);

	_program = glCreateProgram();
	glAttachShader(_program, _frag);
	glAttachShader(_program, _vert);
	glLinkProgram(_program);

	GLint status;
	glGetProgramiv(_program, GL_LINK_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetProgramInfoLog(_program, 1000, &len, log);
		fprintf(stderr, "Error: linking:\n%.*s\n", len, log);
		exit(9989);
	}
}

int shader::load_program_from_file(const char* name, uint32_t version)
{
	int ret = 0, len;
	glsb_header header;
	char* glsl_name;
	void* tmpbuf = NULL;

	string filename(name);
	filename += ".glsb";

	FILE *fp = fopen(filename.c_str(), "rb");
	if (NULL == fp) return -1;

	if (sizeof(header) != fread(&header, 1, sizeof(header), fp)) {
		ret = -2; goto error;
	}
	if (memcmp(header.magic, GLSB_MAGIC, sizeof(header.magic))) {
		ret = -3; goto error;
	}
	if (header.version != version) {
		ret = -4; goto error;
	}

	len = header.start - offsetof(glsb_header, program_name);
	glsl_name = reinterpret_cast<char*>(alloca(len));
	fseek(fp, offsetof(glsb_header, program_name), SEEK_SET);
	if (len != fread(glsl_name, 1, len, fp) || strcmp(glsl_name, name)) {
		ret = -5; goto error;
	}

	if (!header.size) {
		ret = -6; goto error;
	}

	tmpbuf = (void*)malloc(header.size);
	if (!tmpbuf) { ret = -7; goto error; }
	fseek(fp, header.start, SEEK_SET);
	if (header.size != fread(tmpbuf, 1, header.size, fp)) {
		ret = -8; goto error;
	}

	_program = glCreateProgram();
	gblconfig::inst()->glProgramBinary(_program, header.format,
		tmpbuf, header.size);
	GLint status;
	glGetProgramiv(_program, GL_LINK_STATUS, &status);
	if (!status) {
		ret = -9; goto error;
	}

	printf("[loaded] %s\n", filename.c_str());
	if (tmpbuf) free(tmpbuf);
	fclose(fp);
	return ret;

error:
	if (tmpbuf) free(tmpbuf);
	fclose(fp);
	deletefile(filename.c_str());
	return ret;
}

int shader::save_program_to_file(const char* name, uint32_t version)
{
	GLint length = 0;
	glGetProgramiv(_program, GL_PROGRAM_BINARY_LENGTH_OES, &length);
	if (!length) return -1;

	void* tmpbuf = (void*)malloc(length);
	if (NULL == tmpbuf) return -2;

	GLenum format = 0;
	gblconfig::inst()->glGetProgramBinary(_program, length, NULL, &format, tmpbuf);

	uint32_t start = sizeof(glsb_header) + strlen(name);
	glsb_header *header = reinterpret_cast<glsb_header*>(alloca(start));
	memcpy(header->magic, GLSB_MAGIC, sizeof(header->magic));
	header->version = version;
	header->size = length;
	header->start = start;
	header->format = format;
	strcpy(header->program_name, name);
	
	string filename(name);
	filename += ".glsb";
	FILE* fp = fopen(filename.c_str(), "wb");
	if (NULL == fp) {
		free(tmpbuf); return -1;
	}

	fwrite(header, 1, start, fp);
	fwrite(tmpbuf, 1, length, fp);
	free(tmpbuf);
	fclose(fp);
	return 0;
}

int shader::load_program(const char* name, uint32_t version,
	const char* vert_src, const char* frag_src)
{
	if (!name || !vert_src || !frag_src)
		return -1;

	if (gblconfig::inst()->binary_shader_supported())
	{		
		if (load_program_from_file(name, version)) {
			generate_program(vert_src, frag_src);
			save_program_to_file(name, version);
		}
	}
	else generate_program(vert_src, frag_src);

	// bind all varibles
	string var;
	const char* s = NULL, *e;
	for (e = _vert_vartab ;; ++e)
	{
		if (*e && *e != ' ' && *e != '\t'
			&& *e != '\r' && *e != '\n') {
			if (!s) s = e;	
		}
		else {
			if (s) {
				var.assign(s, e - s);

				// var is the token of variable
				check_bind_varible(var);
				s = NULL;
			}
			if (!*e) break;
		}
	}
	return 0;
}

GLuint shader::create_shader(const char *source, GLenum shader_type)
{
	GLuint shader;
	GLint status;

	shader = glCreateShader(shader_type);
	assert(shader != 0);

	glShaderSource(shader, 1, (const char **) &source, NULL);
	glCompileShader(shader);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetShaderInfoLog(shader, 1000, &len, log);
		fprintf(stderr, "Error: compiling %s: %.*s\n",
			shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
			len, log);
		exit(9990);
	}
	return shader;
}

ctx_shadermgr::ctx_shadermgr(void* ctx, shader_mgr* mgr)
: _ctx(NULL)
, _sdrmgr(mgr)
, _thdid(0)
, _active_shader(NULL) {
	listnode_init(_shader_list);
}

ctx_shadermgr::~ctx_shadermgr()
{
	// remove all shaders
	while (!listnode_isempty(_shader_list))
	{
		shader* shr = list_entry(shader, \
			_ownerlist, _shader_list.next);
		delete shr;
	}
}

shader* ctx_shadermgr::getshader_unlocked(const char* name)
{
	shader* ret;
	for (listnode_t* nd = _shader_list.next;
		nd != &_shader_list; nd = nd->next) {
		ret = list_entry(shader, _ownerlist, nd);
		if (ret->_name == name) {
			return ret;
		}
	}
	return NULL;
}

int ctx_shadermgr::setactive_unlocked(shader* sdr)
{
	if (NULL == sdr) {
		if (_ctx) _thdid = 0;
		_active_shader = NULL;
		return 0;
	}

	if (sdr->_manager != this)
		return -1;
	unsigned long int thdid = gettid();
	if (_ctx) {
		if (_thdid != thdid)
			return -2;
		_active_shader = sdr;
	}
	else {
		_thdid = thdid;
		_active_shader = sdr;
	}
	return 0;
}

shader_mgr::shader_mgr()
{
	listnode_init(_ctxmgr_list);

	// add the global context shader manager
	ctx_shadermgr* gblmgr = new ctx_shadermgr(0, this);
	listnode_add(_ctxmgr_list, gblmgr->_ownerlist);
}

shader_mgr::~shader_mgr()
{
	auto_mutex am(_mut);
	while (!listnode_isempty(_ctxmgr_list))
	{
		ctx_shadermgr* m = list_entry(ctx_shadermgr, \
			_ownerlist, _ctxmgr_list.next);
		listnode_del(m->_ownerlist);
		delete m;
	}
}

shader_mgr* shader_mgr::inst(void)
{
	static shader_mgr* _inst = NULL;
	if (NULL == _inst) {
		auto_mutex am(_mut);
		if (_inst) return _inst;
		_inst = new shader_mgr();
		assert(NULL != _inst);
	}
	return _inst;
}

shader* shader_mgr::current(void* ctx)
{
	auto_mutex am(_mut);
	ctx_shadermgr* m = find_ctxmgr_unlocked(ctx);
	if (NULL == m) return NULL;
	return m->_active_shader;
}

int shader_mgr::register_shader(shader* shr)
{
	auto_mutex am(_mut);
	if (shr->_manager) return -1;
	ctx_shadermgr* m = find_ctxmgr_unlocked(shr->_ctx);
	if (NULL == m) return -2;
	return m->register_shader_unlocked(shr);
}

int ctx_shadermgr::register_shader_unlocked(shader* shr)
{
	if (NULL == shr) return -EBADPARM;

	// check if there is an existed shader
	// with the same name
	if (getshader_unlocked(shr->_name.c_str())) {
		return -EEXISTS;
	}

	// add the shader
	listnode_add(_shader_list, shr->_ownerlist);
	shr->_manager = this;
	return 0;
}

int shader_mgr::remove_shader(shader* shr)
{
	auto_mutex am(_mut);
	if (!shr->_manager) return -1;
	return shr->_manager->remove_shader_unlocked(shr);
}

int ctx_shadermgr::remove_shader_unlocked(shader* shr)
{
	if (NULL == shr) return -EBADPARM;
	if (!listnode_isempty(shr->_ownerlist)) {
		listnode_del(shr->_ownerlist);
	}
	if (_active_shader == shr) {
		_active_shader = NULL;
		if (!_ctx) _thdid = 0;
	}
	shr->_manager = NULL;
	return 0;
}

bool shader::iscurrent(void)
{
	unsigned long int thd = gettid();
	if (NULL == _manager) {
		return false;
	}
	return (_manager->_active_shader == this
		&& _manager->_thdid == thd)
		? true : false;
}

shader* shader_mgr::getshader(void* ctx, const char* name)
{
	auto_mutex am(_mut);
	return getshader_unlocked(ctx, name);
}

}} // end of namespace zas::hwgrph
/* EOF */
