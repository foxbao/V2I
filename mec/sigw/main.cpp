#include <stdio.h>
#include "inc/sigw-app.h"

namespace sigw {
using namespace zas::utils;

int run_evloop(void)
{
	evloop *evl = evloop::inst();
	evl->setrole(evloop_role_client);

	// set the name of launch as "zas.system.sysd"
	evl->updateinfo(evlcli_info_client_name, "zas-sigw")
		->updateinfo(evlcli_info_instance_name, "1")
		->updateinfo(evlcli_info_commit);

	evlhandler apphdr;
	if (evl->add_listener("sigw-evlhandler", &apphdr)) {
		return 1;
	}

    // start running evloop
    return evl->start(false);
}

} // end of namespace sigw

using namespace sigw;

int main(int argc, char* argv[])
{
    return run_evloop();
}
