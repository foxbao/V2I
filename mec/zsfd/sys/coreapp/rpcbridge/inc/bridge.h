
#ifndef __CXX_ZAS_HOST_CONTAINER_H__
#define __CXX_ZAS_HOST_CONTAINER_H__
#include <string>
#include "std/zasbsc.h"

namespace zas {
namespace servicebridge {

class proxy_birdge
{
public:
	proxy_birdge();
	~proxy_birdge();

public:
	int run(int argc, char *argv[]);
	int start_host_service(void);
	int start_service_thread(void);
	int start_evloop();

private:
	std::string _client_name;
	std::string _inst_name;

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(proxy_birdge);
};

}}


#endif /*__CXX_ZAS_HOST_CONTAINER_H__*/