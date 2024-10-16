
#include "inc/ltesender.h"
#include "inc/rmsocket.h"

#include <arpa/inet.h>
#include <error.h>
#include <errno.h>
#include <time.h>

#include "utils/thread.h"
#include "utils/timer.h"

namespace zas {
namespace lte {

using namespace zas::utils;

ltesender::ltesender()
{

}

ltesender::~ltesender()
{

}

int ltesender::senddata(void)
{
	rmsocket letsock;
	letsock.init_tcpip();
	printf("ltesender starting\n");
	letsock.connect("192.168.1.12", 5051);
	letsock.send("AT+AUTH=admin123,admin123", sizeof("AT+AUTH=admin123,admin123") - 1);
	char buf[256] = {0};
	letsock.recv(buf, 255);
	printf("recv %s\n", buf);	
	printf("finished\n");
	getchar();
	letsock.send("AT^DRPS?", sizeof("AT^DRPS?") - 1);
	letsock.recv(buf, 255);
	printf("recv %s\n", buf);
	letsock.recv(buf, 255);
	printf("recv %s\n", buf);
	printf("finished\n");
	getchar();
	letsock.send("AT^OPSO?", sizeof("AT^OPSO?") - 1);
	letsock.recv(buf, 255);
	printf("recv %s\n", buf);
	printf("finished\n");
	getchar();
	letsock.send("AT+CFUN?", sizeof("AT+CFUN?") - 1);
	letsock.recv(buf, 255);
	printf("recv %s\n", buf);
	letsock.recv(buf, 255);
	printf("recv %s\n", buf);
	printf("finished\n");

	getchar();
	letsock.send("AT^DACS?", sizeof("AT^DACS?") - 1);
	letsock.recv(buf, 255);
	printf("recv %s\n", buf);
	letsock.recv(buf, 255);
	printf("recv %s\n", buf);
	printf("finished\n");

	getchar();
	letsock.send("AT^DRPC?", sizeof("AT^DRPC?") - 1);
	letsock.recv(buf, 255);
	printf("recv %s\n", buf);
	letsock.recv(buf, 255);
	printf("recv %s\n", buf);
	printf("finished\n");


	getchar();
	letsock.send("AT^DSSMTP?", sizeof("AT^DSSMTP?") - 1);
	letsock.recv(buf, 255);
	printf("recv %s\n", buf);
	letsock.recv(buf, 255);
	printf("recv %s\n", buf);
	printf("finished\n");
}

int ltesender::listendata(void)
{
	rmsocket letsock;
	letsock.init_tcpip();
	printf("ltesender starting\n");
	letsock.connect("192.168.1.12", 5052);
	char buf[256] = {0};
	sockaddr_in *addr = letsock.get_addr();
	socklen_t addrLen = sizeof(*addr);

	while(1) {
		letsock.recvfrom(buf, 255, (struct sockaddr*)addr, &addrLen);
		printf("recv %s\n", buf);	
		printf("finished\n");
	}

}

}};	//zas::lte
