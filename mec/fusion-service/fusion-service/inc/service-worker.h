
#ifndef __CXX_FUSION_SERVICE_WORKER_H__
#define __CXX_FUSION_SERVICE_WORKER_H__

#include "fusion-service-def.h"
#include "webcore/webapp-backend-zmq.h"

#include <string.h>

#include "std/list.h"
#include "utils/avltree.h"
#include "utils/mutex.h"
#include "utils/uri.h"

namespace zas {
namespace fusion_service {

using namespace zas::webcore;
using namespace zas::utils;

// internal communication function
class service_worker : public zmq_backend_worker
{
public:
	service_worker(zmq::context_t* context, uint32_t timeout);
	virtual ~service_worker();
	webapp_backend_callback* set_callback(webapp_backend_callback *cb);
	int handle_zmq_msg(zmq_backend_socket* wa_sock);

private:
	zmq::context_t*		_zmq_context;
	int				_force_exit;
	uint32_t		_timeout;
	webapp_backend_callback* _cb;
};

class service_backend : public webapp_zmq_backend
{
public:
	service_backend();
	virtual ~service_backend();
	static service_backend* inst();

public:
	zmq_backend_worker* create_backend_worker(
		zmq::context_t* zmq_context);
	webapp_backend_callback* set_callback(
			void* context, webapp_backend_callback *cb);
	timermgr* get_timermgr(void);
};

}}

#endif /* __CXX_SNAPSHOT_SERVICE_WORKER_H__*/