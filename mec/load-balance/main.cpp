#include "load-balance-service.h"
#include "forward.h"
#include "webcore/sysconfig.h"
#include "webcore/logger.h"


using namespace zas::webcore;
using namespace zas::load_balance;

int main(int argc, char* argv[])
{
	zas::webcore::load_sysconfig("file:///zassys/sysapp/others/load-balance/config/sysconfig.json");
	zas::load_balance::lbservice service;
	service.run();
	zas::webcore::log.flush();
	return 0;
}
