#ifndef __CXX_ZAS_SYSTEM_SYSSVERT_H__
#define __CXX_ZAS_SYSTEM_SYSSVERT_H__

#include "std/zasbsc.h"
#include "syssvr_pkg_mgr.h"

namespace zas {
namespace syssvr {

class syssvr_mgr
{
public: 
	syssvr_mgr();
	~syssvr_mgr();

public:
	int run(int argc, char *argv[]);
	int syssvr_thread_run(void);
	int syssvr_thread_start(void);

	class syssvr_client_listener: public utils::evloop_listener
	{
	public:
		syssvr_client_listener(syssvr_mgr* base) {
			_base = base;
		}
		void accepted(evlclient client) {
			_base->syssvr_thread_start();
		}
	private:
		syssvr_mgr *_base;
	};
private:
	int start_zrpc_server(void);
	int start_rpcproxy_bridge(void);
	int start_server(void);

private:
	syssvr_pkg_mgr	_syssvr_pkg_mgr;
	syssvr_client_listener* _client_listener;
	bool	_init_started;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(syssvr_mgr);
};


}} // end of namespace zas::system

#endif /*__CXX_ZAS_SYSTEM_SYSSVERT_H__*/
