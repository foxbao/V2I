#include "fusion-service.h"
#include "webcore/sysconfig.h"
#include "webcore/logger.h"


using namespace zas::webcore;
using namespace zas::fusion_service;

int main(int argc, char* argv[])
{
	zas::webcore::load_sysconfig("file:///zassys/sysapp/others/fusion-service/config/sysconfig.json");
	zas::fusion_service::device_fusion_service service;
	service.run();
	zas::webcore::log.flush();
	return 0;
}
