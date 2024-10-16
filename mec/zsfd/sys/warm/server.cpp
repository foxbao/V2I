#include "inc/server.h"
#include "inc/output.h"

namespace zas {
namespace sys {
namespace warm {

// fatal error handler
static const char fatal_errstr[] = fatal_error_str(warm-server);
#define check_fatal_error(expr)	\
do {	\
	if (expr) break;	\
	fprintf(stderr, fatal_errstr, __LINE__, #expr);	\
	exit(100);	\
} while (0)

warm_server::warm_server()
{
	wl_list_init(&output_list);
	init();
}

warm_server::~warm_server() {
	destroy();
}

warm_server* warm_server::inst(void)
{
	static warm_server sinst;
	assert(nullptr != sinst.display);
	return &sinst;
}

void warm_server::init(void)
{
	display = wl_display_create();
	check_fatal_error(nullptr != display);

	backend = wlr_backend_autocreate(display);
	check_fatal_error(nullptr != backend);

	// create the renderer
	renderer = wlr_backend_get_renderer(backend);
	check_fatal_error(nullptr != renderer);
	wlr_renderer_init_wl_display(renderer, display);

	// This creates some hands-off wlroots interfaces
	compositor = wlr_compositor_create(display, renderer);
	check_fatal_error(nullptr != compositor);
	wlr_data_device_manager_create(display);

	// Creates an output layout
	output_layout = wlr_output_layout_create();
	check_fatal_error(nullptr != output_layout);
}

void warm_server::destroy(void)
{
	// remove all remaining outputs
	while (!wl_list_empty(&output_list)) {
		warm_output *output = wl_container_of(output_list.next,
			output, link);
		delete output;
	}
	
	// remove the output signal listener
	wl_list_remove(&_output_sighdr.link);

	if (nullptr != output_layout) {
		wlr_output_layout_destroy(output_layout);
		output_layout = nullptr;
	}
	if (nullptr != backend) {
		wlr_backend_destroy(backend);
		backend = nullptr;
	}
	if (nullptr != display) {
		wl_display_destroy_clients(display);
		wl_display_destroy(display);
		display = nullptr;
	}
}

void warm_server::new_output_wrapper(wl_listener* l, void* d)
{
	auto output = (wlr_output*)d;
	warm_server* svr = wl_container_of(l, svr, _output_sighdr);
	svr->onsig_new_output(output);
}

void warm_server::onsig_new_output(wlr_output* output)
{
	// set mode before using the output, some backends need modesetting
	// (eg. DMS+KMS), just pick the output's preferred mode
	if (!wl_list_empty(&output->modes)) {
		auto mode = wlr_output_preferred_mode(output);
		assert(nullptr != mode);
		warm_logi("using mode: %d x %d %dHz", mode->width, mode->height, mode->refresh);

		wlr_output_set_mode(output, mode);
		wlr_output_enable(output, true);
		if (!wlr_output_commit(output)) {
			return;
		}
	}

	// Allocates and configures the state for this output
	auto wm_outp = new warm_output(output);
	assert(nullptr != wm_outp);
	wm_outp->register_output(this);

	// Adds this to the output layout
	wlr_output_layout_add_auto(output_layout, output);
}

void warm_server::config_sighandlers(void)
{
	assert(nullptr != backend);
	// configure a handler for when new outputs are available on the backend
	_output_sighdr.notify = new_output_wrapper;
	wl_signal_add(&backend->events.new_output, &_output_sighdr);
}

int warm_server::run(void)
{
	config_sighandlers();

	// Add a Unix socket to the Wayland display
	const char *socket = wl_display_add_socket_auto(display);
	if (nullptr == socket) {
		warm_loge("fail to add Unix socket");
		destroy(); return -1;
	}
	socket_name = socket;

	// Start the backend. This will enumerate outputs
	// and inputs, become the DRM, master, etc
	if (!wlr_backend_start(backend)) {
		warm_loge("fail to start backend");
		destroy(); return -2;
	}

	// Set the WAYLAND_DISPLAY environment variable to the socket
	setenv("WAYLAND_DISPLAY", socket, true);
	warm_logi("warm running on WAYLAND_DISPLAY=\"%s\"", socket);

	// start running
	wl_display_run(display);
	return 0;
}

}}} // end of namespace zas::sys::warm
/* EOF */
