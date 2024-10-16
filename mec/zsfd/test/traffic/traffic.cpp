#include "inc/traffic_receiver.h"
#include "utils/evloop.h"
#include "utils/timer.h"

using namespace zas::utils;
using namespace zas::traffic;
int main()
{
	traffic_receiver rec;
	rec.start_traffic_receiver();
	getchar();
}
