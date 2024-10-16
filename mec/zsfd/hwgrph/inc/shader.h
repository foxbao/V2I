/** @file shader.h
 * declaration of the shader structurs
 */

#ifndef __CXX_ZAS_HWGRPH_SHADER_H__
#define __CXX_ZAS_HWGRPH_SHADER_H__

#include <string>
#include <GLES2/gl2.h>

#include "std/list.h"
#include "hwgrph/hwgrph.h"

namespace zas {
namespace hwgrph {

enum export_varible
{
	glslvar_unknown = -1,
	glslvar_u_modelview,
	glslvar_a_position,
	glslvar_u_fillcolor,
	glslvar_a_srctexcord,
	glslvar_u_srctex,
	glslvar_u_count,
	glslvar_u_offset,
	glslvar_u_weight,

	// end of definition
	glslvar_max,
};

#define GLSB_MAGIC	"glsbfile"

struct glsb_header
{
	char magic[8];
	uint32_t version;
	uint32_t size;
	uint32_t start;
	GLenum format;
	char program_name[1];
} PACKED;

class shader_mgr;
class ctx_shadermgr;

class shader
{
	friend class shader_mgr;
	friend class ctx_shadermgr;
public:
	shader(void* ctx, const char* name,
		uint32_t version,
		const char* vert_src,
		const char* frag_src,
		const char* vert_vartab);
	~shader();

	GLint get_attrib(const char* name);
	GLint get_uniform(const char* name);

	int activate(void);
	bool iscurrent(void);

public:
	GLint get_varible(int idx)
	{
		if (idx >= glslvar_max) {
			return -1;
		}
		return _export_var[idx];
	}

private:
	int load_program(const char* name, uint32_t version,
		const char* vert_src, const char* frag_src);
	void generate_program(const char* vert_src, const char* frag_src);
	int load_program_from_file(const char* filename, uint32_t version);
	int save_program_to_file(const char* filename, uint32_t version);
	GLuint create_shader(const char *source, GLenum shader_type);
	void check_bind_varible(std::string& var);

private:
	listnode_t _ownerlist;
	std::string _name;
	const char* _vert_vartab;
	union {
		uint32_t _flags;
		struct {
			uint32_t ready : 1;
		} _f;
	};
	void* _ctx;
	GLuint _vert, _frag;
	GLuint _program;
	GLint _export_var[glslvar_max];
	ctx_shadermgr* _manager;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(shader);
};

struct ctx_shadermgr
{
	friend class shader;
	friend class shader_mgr;
public:
	ctx_shadermgr(void* ctx, shader_mgr* mgr);
	~ctx_shadermgr();

	int register_shader_unlocked(shader* shr);
	int remove_shader_unlocked(shader* shr);
	shader* getshader_unlocked(const char* name);
	int setactive_unlocked(shader* sdr);

private:
	listnode_t _ownerlist;
	void* _ctx;
	shader_mgr* _sdrmgr;
	unsigned long int _thdid;
	listnode_t _shader_list;
	shader* _active_shader;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(ctx_shadermgr);
};

class shader_mgr
{
	friend class shader;
	friend class ctx_shadermgr;
public:
	shader_mgr();
	~shader_mgr();
	static shader_mgr* inst(void);

public:
	int register_shader(shader* shr);
	int remove_shader(shader* shr);

	shader* getshader(void* ctx, const char* name);

public:
	shader* current(void* ctx);

private:

	shader* getshader_unlocked(void* ctx, const char* name) {
		ctx_shadermgr* m = find_ctxmgr_unlocked(ctx);
		if (NULL == m) return NULL;
		return m->getshader_unlocked(name);
	}

	int setactive_unlocked(shader* sdr) {
		if (!sdr->_manager) return -10;
		return sdr->_manager->setactive_unlocked(sdr);
	}

	ctx_shadermgr* find_ctxmgr_unlocked(void* ctx) {
		listnode_t* nd = _ctxmgr_list.next;
		for (; nd != &_ctxmgr_list; nd = nd->next) {
			ctx_shadermgr* m = list_entry(ctx_shadermgr, _ownerlist, nd);
			if (m->_ctx == ctx) return m;
		}
		return NULL;
	}

private:
	listnode_t _ctxmgr_list;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(shader_mgr);
};

}} // end of namespace zas::hwgrph
#endif // __CXX_ZAS_HWGRPH_SHADER_H__
/* EOF */
