#include "utils/log.h"
#include "inc/sigw-app.h"

namespace sigw {
using namespace zas::utils;

void evlhandler::connected(evlclient client)
{
	printf("send request\n");
    for (int i = 0; i < 200000; ++i)
    {
        char buf[32];
        sprintf(buf, "val=%d", i);
        int ret = log->write(log_level::debug, "tag", buf);
        if (ret) printf("error:%d, ret=%d\n",i,ret);
    }
}

void evlhandler::disconnected(const char* cliname, const char* instname)
{
}

} // end of namespace sigw
/* EOF */