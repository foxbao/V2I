#include "webcore/sysconfig.h"
#include "webcore/logger.h"

using namespace zas::webcore;

int main(int argc, char* argv[])
{
	load_sysconfig("file:///home/jimview/mec/test/webcore/sysconfig.json");
	log.init();
	for (int i = 0; i < 10000; ++i) {
		log.i("test", "this is a %u test", i);
	}
	log.flush();
	log.finialize();

	getchar();
	return 0;
}