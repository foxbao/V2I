/** @file render.cpp
 * implementation of render process
 */

#include "utils/thread.h"
#include "uicore/region.h"

#include "inc/render.h"
#include "inc/context.h"

namespace zas {
namespace uicore {

using namespace zas::utils;

// todo: remove
int UICORE_EXPORT do_render(void)
{
	irect_t cr;
	if (gblctx_merge_view_changes(cr))
		return -1;

	return 0;
}

class render_thread : public thread
{
public:
	render_thread()
	: thread() {
		setattr(thread_attr_detached);
	}

	int run(void)
	{
		return 0;
	}
};

// the global render thread
static render_thread rthd_;

}} // end of namespace zas::uicore
/* EOF */
