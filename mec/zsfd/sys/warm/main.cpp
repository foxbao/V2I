#include <stdio.h>

#include "inc/server.h"

namespace zas {
namespace sys {
namespace warm {

int main(int argc, char* argv[])
{
	wlr_log_init(WLR_DEBUG, NULL);
	auto svr = warm_server::inst();
	return svr->run();
}

}}} // end of namespace zas::sys::warm

int main(int argc, char* argv[])
{
	return zas::sys::warm::main(argc, argv);
}

/* EOF */
