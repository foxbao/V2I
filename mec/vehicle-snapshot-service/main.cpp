#include "snapshot-service.h"
#include "webcore/sysconfig.h"
#include "webcore/logger.h"


using namespace zas::webcore;
using namespace zas::vehicle_snapshot_service;

int main(int argc, char* argv[])
{
	zas::webcore::load_sysconfig("file:///zassys/sysapp/others/vehicle-snapshot-service/config/sysconfig.json");
	zas::vehicle_snapshot_service::snapshot_service service;
	service.run();
	zas::webcore::log.flush();
	return 0;
}
