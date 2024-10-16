#include "inc/server.h"
#include "inc/output.h"

namespace zas {
namespace sys {
namespace warm {

// fatal error handler
static const char fatal_errstr[] = fatal_error_str(warm-output);
#define check_fatal_error(expr)	\
do {	\
	if (expr) break;	\
	fprintf(stderr, fatal_errstr, __LINE__, #expr);	\
	exit(100);	\
} while (0)

warm_output::warm_output(wlr_output* outp)
: output(outp) {
	// Sets up a listener for the frame notify event
	frame_sighdr.notify = output_frame_wrapper;
	wl_signal_add(&outp->events.frame, &frame_sighdr);
}

warm_output::~warm_output()
{
	warm_logd("output \"%s\" removed", output->name);
	// remove from the output_list in server
	wl_list_remove(&link);
	// remove signal listeners
	wl_list_remove(&destroy_sighdr.link);
	wl_list_remove(&frame_sighdr.link);
}

void warm_output::output_frame_wrapper(wl_listener* l, void*)
{
	warm_output *output = wl_container_of(l, output, frame_sighdr);
	output->onsig_output_frame();
}

void warm_output::output_remove_wrapper(wl_listener* l, void*)
{
	warm_output *output = wl_container_of(l, output, destroy_sighdr);
	delete output;
}

void warm_output::onsig_output_frame(void)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	// make the OpenGL context current
	if (!wlr_output_attach_render(output, nullptr)) {
		return;
	}

	int width, height;
	wlr_output_effective_resolution(output, &width, &height);

	// Begin the renderer (calls glViewport and some other GL sanity checks)
	assert(nullptr != server->renderer);
	wlr_renderer_begin(server->renderer, width, height);

	float color[4] = {0.0, 0.0, 0.0, 1.0};
	wlr_renderer_clear(server->renderer, color);

#if 0
	/* Each subsequent window we render is rendered on top of the last. Because
	 * our view list is ordered front-to-back, we iterate over it backwards. */
	struct tinywl_view *view;
	wl_list_for_each_reverse(view, &output->server->views, link) {
		if (!view->mapped) {
			/* An unmapped view should not be rendered. */
			continue;
		}
		struct render_data rdata = {
			.output = output->wlr_output,
			.view = view,
			.renderer = renderer,
			.when = &now,
		};
		/* This calls our render_surface function for each surface among the
		 * xdg_surface's toplevel and popups. */
		wlr_xdg_surface_for_each_surface(view->xdg_surface,
				render_surface, &rdata);
	}

	/* Hardware cursors are rendered by the GPU on a separate plane, and can be
	 * moved around without re-rendering what's beneath them - which is more
	 * efficient. However, not all hardware supports hardware cursors. For this
	 * reason, wlroots provides a software fallback, which we ask it to render
	 * here. wlr_cursor handles configuring hardware vs software cursors for you,
	 * and this function is a no-op when hardware cursors are in use. */
	
#endif
	// render the cursur if necessary (no-op when hardware cursors available)
	wlr_output_render_software_cursors(output, nullptr);

	// finish rendering and swap the buffers, showing the final frame on-screen
	wlr_renderer_end(server->renderer);
	wlr_output_commit(output);
	warm_logd("draw");
}

void warm_output::register_output(warm_server* wm_svr)
{
	assert(nullptr != wm_svr);
	server = wm_svr;
	wl_list_insert(&server->output_list, &link);

	destroy_sighdr.notify = output_remove_wrapper;
	wl_signal_add(&output->events.destroy, &destroy_sighdr);
}

}}} // end of namespace zas::sys::warm
/* EOF */
