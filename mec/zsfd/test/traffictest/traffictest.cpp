#include "inc/traffic_sender.h"
#include "utils/evloop.h"
#include "utils/timer.h"

using namespace zas::utils;
using namespace zas::traffic;
int main()
{
    traffic_sender tmp;
    tmp.senddata();
    getchar();
}
