/** @file defglsl.cpp
 * implementation of default glsl programs
 */

#include "inc/shader.h"

namespace zas {
namespace hwgrph {

/*
 * Start of the implmentation of "solidfill-shader"
 */
static const uint32_t solidfill_shader_version = 0x00000001;

static const char *solidfill_vertex_shader =
	"uniform mat4 u_modelview;"									"\n"
	"attribute vec4 a_position;"								"\n"
	"void main()"												"\n"
	"{"															"\n"
	"	gl_Position = u_modelview * a_position;"				"\n"
	"};"														"\n";

static const char *solidfill_fragment_shader =
	"precision mediump float;"									"\n"
	"uniform vec4 u_fillcolor;"									"\n"
	"void main()"												"\n"
	"{"															"\n"
	"	gl_FragColor = u_fillcolor;"							"\n"
	"}"															"\n";

static const char *solidfill_vertex_shader_varibles =
	"u_modelview a_position u_fillcolor";

/*
 * Start of the implmentation of "default-texshader"
 */
static const uint32_t texdefault_shader_version = 0x00000001;

static const char *texdefault_vertex_shader =
	"attribute vec4 a_position;"								"\n"
	"attribute vec2 a_srctexcord;"								"\n"
	"varying vec2 v_texcord;"									"\n"
	"void main()"												"\n"
	"{"															"\n"
	"	gl_Position = a_position;"								"\n"
	"	v_texcord = a_srctexcord;"								"\n"
	"};"														"\n";

static const char *texdefault_fragment_shader =
	"precision mediump float;"									"\n"
	"varying vec2 v_texcord;"									"\n"
	"uniform sampler2D u_srctex;"								"\n"
	"void main()"												"\n"
	"{"															"\n"
	"	gl_FragColor = texture2D(u_srctex, v_texcord);"			"\n"
	"}"															"\n";

static const char *texdefault_vertex_shader_varibles =
	"a_position a_srctexcord u_srctex";

/*
 * Start of the implmentation of Gauss blur pass 1
 */
static const uint32_t gaussblur_pass1_shader_version = 0x00000001;

static const char *gaussblur_pass1_fragment_shader =
	"precision mediump float;"																	"\n"
	"varying vec2 v_texcord;"																	"\n"
	"uniform sampler2D u_srctex;"																"\n"
	"uniform int u_count;"																		"\n"
	"uniform float u_offset[15];"																"\n"
	"uniform float u_weight[15];"																"\n"
	"void main()"																				"\n"
	"{"																							"\n"
	"	vec4 color = texture2D(u_srctex, v_texcord) * u_weight[0];"								"\n"
	"	for (int i = 1; i < u_count; ++i) {"													"\n"
	"		color += texture2D(u_srctex, v_texcord + vec2(u_offset[i], 0.0)) * u_weight[i];"	"\n"
	"		color += texture2D(u_srctex, v_texcord - vec2(u_offset[i], 0.0)) * u_weight[i];"	"\n"
	"	}"																						"\n"
	"	gl_FragColor = color;"																	"\n"
	"}"																							"\n";

/*
 * Start of the implmentation of Gauss blur pass 2
 */
static const uint32_t gaussblur_pass2_shader_version = 0x00000001;

static const char *gaussblur_pass2_fragment_shader =
	"precision mediump float;"																	"\n"
	"varying vec2 v_texcord;"																	"\n"
	"uniform sampler2D u_srctex;"																"\n"
	"uniform int u_count;"																		"\n"
	"uniform float u_offset[15];"																"\n"
	"uniform float u_weight[15];"																"\n"
	"void main()"																				"\n"
	"{"																							"\n"
	"	vec4 color = texture2D(u_srctex, v_texcord) * u_weight[0];"								"\n"
	"	for (int i = 1; i < u_count; ++i) {"													"\n"
	"		color += texture2D(u_srctex, v_texcord + vec2(0.0, u_offset[i])) * u_weight[i];"	"\n"
	"		color += texture2D(u_srctex, v_texcord - vec2(0.0, u_offset[i])) * u_weight[i];"	"\n"
	"	}"																						"\n"
	"	gl_FragColor = color;"																	"\n"
	"}"																							"\n";

static const char *gaussblur_shader_varibles =
	"a_position a_srctexcord u_srctex u_count u_offset u_weight";

/*
 * Start of the implementation of shadow drawing
 */
static const uint32_t shadowtex_shader_version = 0x00000001;

static const char *shadowtex_fragment_shader =
	"precision mediump float;"															"\n"
	"varying vec2 v_texcord;"															"\n"
	"uniform sampler2D u_srctex;"														"\n"
	"uniform vec4 u_fillcolor;"															"\n"
	"void main()"																		"\n"
	"{"																					"\n"
	"	vec4 color = texture2D(u_srctex, v_texcord);"									"\n"
	"	gl_FragColor = vec4(u_fillcolor.xyz, u_fillcolor.a * color.x);"					"\n"
	"}"																					"\n";

static const char *shadowtex_shader_varibles =
	"a_position a_srctexcord u_srctex u_fillcolor";

int glsl_load(void)
{
	shader* sdr;
	sdr = new shader(NULL, // global shader
		"solidfill-shader",
		solidfill_shader_version,
		solidfill_vertex_shader,
		solidfill_fragment_shader,
		solidfill_vertex_shader_varibles);

	assert(NULL != sdr);
	shader_mgr::inst()->register_shader(sdr);

	sdr = new shader(NULL, // global shader
		"default-texshader",
		texdefault_shader_version,
		texdefault_vertex_shader,
		texdefault_fragment_shader,
		texdefault_vertex_shader_varibles);

	assert(NULL != sdr);
	shader_mgr::inst()->register_shader(sdr);

	sdr = new shader(NULL, // global shader
		"shadow-gaussblur-pass1",
		gaussblur_pass1_shader_version,
		texdefault_vertex_shader,
		gaussblur_pass1_fragment_shader,
		gaussblur_shader_varibles);
	
	assert(NULL != sdr);
	shader_mgr::inst()->register_shader(sdr);

	sdr = new shader(NULL, // global shader
		"shadow-gaussblur-pass2",
		gaussblur_pass2_shader_version,
		texdefault_vertex_shader,
		gaussblur_pass2_fragment_shader,
		gaussblur_shader_varibles);
	
	assert(NULL != sdr);
	shader_mgr::inst()->register_shader(sdr);

	sdr = new shader(NULL, // global shader
		"shadow-texshader",
		texdefault_shader_version,
		texdefault_vertex_shader,
		shadowtex_fragment_shader,
		shadowtex_shader_varibles);
	
	assert(NULL != sdr);
	shader_mgr::inst()->register_shader(sdr);
	return 0;
}

}} // end of namepsace zas::hwgrph
/* EOF */

