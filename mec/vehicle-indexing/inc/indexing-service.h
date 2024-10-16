#ifndef __CXX_INDEXING_SERVICE_H__
#define __CXX_INDEXING_SERVICE_H__

#include "indexing-def.h"

#include <string.h>

#include "std/list.h"
#include "utils/avltree.h"
#include "webcore/webapp.h"

namespace zas {
namespace vehicle_indexing {

using namespace  zas::webcore;

class current_snapshot;
class indexing_rabbitmq;
class indexing_rabbitmq_cb;
class indexing_webapp_cb;
class forward_arbitrate;
class vehicle_mgr;

class indexing_service : public wa_response_factory
{
public:
	indexing_service();
	virtual ~indexing_service();
	int run(void);

public:
	wa_response* create_instance(void);
	void destory_instance(wa_response *reply);

public:
	int oninit(void);
	int onexit(void);
	int on_consuming(char* routingkey, size_t rsz, void* data, size_t dsz);
	void on_consuming_error(int errid);
	int on_request(void* data, size_t sz);

private:
	int init_current_snapshot(void);
	int init_rabbitmq(void);
	int init_forward_arbitrate(void);
	int init_vehicle_mgr(void);

private:
	//current snapshot service
	current_snapshot*	_current_snapshot;

	// connect center cloude
	indexing_rabbitmq*	_consuming_mq;
	indexing_rabbitmq_cb* _rabbitmq_cb;
	indexing_webapp_cb* _webapp_cb;
	forward_arbitrate*	_forward_mgr;

};

}}	//zas::vehicle_indexing

#endif /* __CXX_INDEXING_SERVICE_H__*/