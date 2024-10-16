
#ifndef __CXX_ZAS_HOST_CONTAINER_H__
#define __CXX_ZAS_HOST_CONTAINER_H__
#include <string>
#include "std/zasbsc.h"

namespace zas {
namespace host {

class host
{
public:
	host();
	~host();

public:
	int run(int argc, char *argv[]);
	int start_host_service(void);
	int start_evloop(void);
	int start_service_thread(void);

private:
	std::string _client_name;
	std::string _inst_name;
	std::string _service_pkg;
	std::string _service_name;
	std::string _service_inst;
	std::string _executive;
	std::string _startmode;
	std::string _client_ipv4;
	int _listen_port;
	
private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(host);
};

}}


#endif /*__CXX_ZAS_HOST_CONTAINER_H__*/