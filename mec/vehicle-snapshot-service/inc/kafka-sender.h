#ifndef __CXX_SNAPSHOT_KAFKA_TO_CENTER_H__
#define __CXX_SNAPSHOT_KAFKA_TO_CENTER_H__

#include <string.h>
#include "snapshot-service-def.h"
#include "service-worker.h"
#include "std/list.h"
#include "utils/avltree.h"
#include "utils/timer.h"
#include "utils/uri.h"

namespace vss {
	class vehicle_snapshot;
};
namespace RdKafka {
	class Producer;
};

namespace zas {
namespace vehicle_snapshot_service {

using namespace zas::utils;

class kafka_delivery_report;

class vss_kafka_sender : kafka_data_callback
{
public:
	vss_kafka_sender();
	virtual ~vss_kafka_sender();
	int on_kafka_data_send(const char* data, size_t sz);

public:
	int init();

private:
	int load_kafka_config(void);
	int init_kafka(void);
	int send_kafka_to_center(std::string &topic, void* data, size_t sz);

private:
	bool _inited;
	int _port;
	std::string _kafka_addr;
	std::string _junciton_topic;
	std::string _vehicle_topic;
	std::string _count_topic;
	RdKafka::Producer* _producer;
	kafka_delivery_report* _delivery_cb;
};

}}	//zas::vehicle_snapshot_service

#endif /* __CXX_SNAPSHOT_KAFKA_TO_CENTER_H__*/