#ifndef __CXX_SNAPSHOT_SERVICE_H__
#define __CXX_SNAPSHOT_SERVICE_H__

#include <string.h>

#include "std/list.h"
#include "utils/avltree.h"
#include "webcore/webapp.h"
#include "utils/mutex.h"

namespace zas {
namespace vehicle_snapshot_service {

using namespace  zas::webcore;
using namespace  zas::utils;

class snapshot_webapp_cb;
class vss_kafka_sender;
class snapshot_rabbitmq;
class snapshot_rabbitmq_cb;
class snapshot_update_timer;

class snapshot_service : public wa_response_factory
{
public:
	snapshot_service();
	virtual ~snapshot_service();
	int run(void);

public:
	wa_response* create_instance(void);
	void destory_instance(wa_response *reply);

public:
	int oninit(void);
	int onexit(void);
	int on_request(void* data, size_t sz);
	int on_consuming(char* routingkey, size_t rsz, void* data, size_t dsz);
	void on_consuming_error(int errid);
	int on_timer(void);

private:
	int get_urlname_from_uri(uri &url, std::string &url_name);
	int init_rabbitmq(void);
	int send_error_reason(uri &info, int reason);

private:
	union {
		struct {
			uint32_t init_snapshot : 1;
			uint32_t init_kafka_center : 1;
		} _f;
		uint32_t _flags;
	};
	

private:
	snapshot_webapp_cb* _webapp_cb;
	vss_kafka_sender* _vss_kafka_to_center;

	snapshot_rabbitmq*	_consuming_mq;
	snapshot_rabbitmq_cb* _rabbitmq_cb;
	snapshot_update_timer* _update_timer;
	mutex	_mut;
};

}}	//zas::vehicle_snapshot_service


#endif /* __CXX_INDEXING_SERVICE_H__*/