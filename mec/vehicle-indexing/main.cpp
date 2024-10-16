#include "indexing-service.h"
#include "forward.h"
#include "webcore/sysconfig.h"
#include "webcore/logger.h"


using namespace zas::webcore;
using namespace zas::vehicle_indexing;

int main(int argc, char* argv[])
{
	zas::webcore::load_sysconfig("file:///zassys/sysapp/others/vehicle-indexing/config/sysconfig.json");
	zas::vehicle_indexing::indexing_service service;
	service.run();
	zas::webcore::log.flush();
	return 0;
}
