#ifndef __CXX_ZAS_SYS_WARM_OUTPUT_H
#define __CXX_ZAS_SYS_WARM_OUTPUT_H

#include <string>
#include "basic.h"

#define static
extern "C" {
	#include <wayland-server-core.h>
	#include <wlr/backend.h>
	#include <wlr/render/wlr_renderer.h>
	#include <wlr/types/wlr_output.h>
}
#undef static

namespace zas {
namespace sys {
namespace warm {
using namespace std;

struct warm_server;


struct warm_server;

struct warm_output
{
	// declaration of members
	wl_list link;
	warm_server* server = nullptr;
	wlr_output *output;
	wl_listener frame_sighdr;
	wl_listener destroy_sighdr;

	// declaration of methods
	warm_output(wlr_output*);
	~warm_output();
	void register_output(warm_server*);

private:
	// signal handler
	void onsig_output_frame(void);
	
private:
	// wrappers
	static void output_frame_wrapper(wl_listener*, void*);
	static void output_remove_wrapper(wl_listener*, void*);

	ZAS_DISABLE_EVIL_CONSTRUCTOR(warm_output);
};

}}} // end of namespace zas::sys::warm
#endif // __CXX_ZAS_SYS_WARM_OUTPUT_H
/* EOF */
