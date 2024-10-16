#include "inc/ltesender.h"
#include "utils/evloop.h"
#include "utils/timer.h"

using namespace zas::utils;
using namespace zas::lte;
int main()
{
    ltesender tmp;
    tmp.senddata();
    tmp.listendata();
    getchar();
}
