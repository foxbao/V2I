#ifndef __CXX_ZAS_SYS_WARM_SERVER_H
#define __CXX_ZAS_SYS_WARM_SERVER_H

#include <string>
#include "basic.h"

#define static
extern "C" {
	#include <wayland-server-core.h>
	#include <wlr/backend.h>
	#include <wlr/render/wlr_renderer.h>
	#include <wlr/types/wlr_compositor.h>
	#include <wlr/types/wlr_data_device.h>
	#include <wlr/types/wlr_output.h>
	#include <wlr/types/wlr_output_layout.h>
}
#undef static

namespace zas {
namespace sys {
namespace warm {
using namespace std;

struct warm_output;

struct warm_server
{
	// declaration of members
	wl_display *display = nullptr;
	wlr_backend *backend = nullptr;
	wlr_renderer *renderer = nullptr;
	wlr_output_layout *output_layout = nullptr;
	wlr_compositor *compositor = nullptr;

	// wayland socket name
	string socket_name;

	// all output list (possible multiple outpus)
	wl_list output_list;

	// declaration of methods
	warm_server();
	~warm_server();
	static warm_server* inst(void);

	// running the event loop
	int run(void);

private:
	void init(void);
	void destroy(void);
	void config_sighandlers(void);

	// signal handler
	void onsig_new_output(wlr_output* output);

private:
	// signal hander for outputs
	wl_listener _output_sighdr;

private:
	// wrappers
	static void new_output_wrapper(wl_listener*, void*);

	ZAS_DISABLE_EVIL_CONSTRUCTOR(warm_server);
};

}}} // end of namespace zas::sys::warm
#endif // __CXX_ZAS_SYS_WARM_SERVER_H
/* EOF */
