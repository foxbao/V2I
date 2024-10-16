#include "utils/evloop.h"
#include <stdio.h>

using namespace zas::utils;

int main()
{
	evloop *evl = evloop::inst();
	evl->setrole(evloop_role_client);

	evl->updateinfo(evlcli_info_client_name, "zas.system")
		->updateinfo(evlcli_info_instance_name, "sysd2")
		->updateinfo(evlcli_info_server_ipv4, "127.0.0.1")
		->updateinfo(evlcli_info_server_port, 5556)
		->updateinfo(evlcli_info_listen_port, 5555)
		->updateinfo(evlcli_info_commit);

	// start the system server
	return evl->start(false);
}