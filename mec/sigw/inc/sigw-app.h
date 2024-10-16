#ifndef __CXX_SIGW_APP_H__
#define __CXX_SIGW_APP_H__

#include "utils/evloop.h"

namespace sigw {
using namespace zas::utils;

class evlhandler : public evloop_listener
{
public:

    /**
     * @brief handle connection when sigw connected to server
     * @param client
     */
	void connected(evlclient client);

    /**
     * @brief handle disconnection when sigw disconnected from server
     * @param cliname 
     * @param instname 
     */
	void disconnected(const char* cliname, const char* instname);
};

} // end of namespace sigw
#endif // __CXX_SIGW_APP_H__
/* EOF */
