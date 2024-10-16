#include "kafka-sender.h"
#include "proto/snapshot_package.pb.h"
#include "proto/vehicle_snapshot.pb.h"
#include "proto/center_kafka_data.pb.h"
#include "webcore/logger.h"
#include "webcore/sysconfig.h"
#include "webcore/webapp.h"
#include "service-worker.h"
#include "utils/uri.h"
#include <sys/time.h>
#include "librdkafka/rdkafkacpp.h"

namespace zas {
namespace vehicle_snapshot_service {

using namespace zas::utils;
using namespace zas::webcore;
using namespace vss;
using namespace jos;


class kafka_delivery_report : public RdKafka::DeliveryReportCb {
 public:
	void dr_cb(RdKafka::Message &message) {
		/* If message.err() is non-zero the message delivery failed permanently
			* for the message. */
		if (message.err()) {
			log.e(SNAPSHOT_SNAPSHOT_TAG, 
				"%s msg delivery failed\n", message.errstr().c_str());
		} else {
			// log.e(SNAPSHOT_SNAPSHOT_TAG, 
			// 	"%s msgdelivered to topic \n"
			// 	"[ %d ] at offset %ld\n", 
			// 	message.topic_name().c_str(),
			// 	message.partition(),
			// 	message.offset());
		}
	}
};

vss_kafka_sender::vss_kafka_sender()
: _inited(false)
, _port(0)
, _producer(nullptr)
, _delivery_cb(nullptr)
{
	_junciton_topic = "junctioninformation";
	_vehicle_topic = "vehicleinformation";
	_count_topic = "junctioncount";
	// _junciton_topic = "test";
	// _vehicle_topic = "test";
}

vss_kafka_sender::~vss_kafka_sender()
{
	if (_producer) {
		log.d(SNAPSHOT_SNAPSHOT_TAG,
			"flushing final messages...\n");
		_producer->flush(10 * 1000 /* wait for max 10 seconds */);
		if (_producer->outq_len() > 0) {
			log.d(SNAPSHOT_SNAPSHOT_TAG,
			"%d message(s) were not delivered\n",
			_producer->outq_len());
		}
		delete _producer;
		_producer = nullptr;
	}
}

static int kafka_handle_cnt = 0;
static uint64_t tmpusetime1 = 0;
int vss_kafka_sender::on_kafka_data_send(const char* data, size_t sz)
{
	center_kafka_data kafka_data;
	kafka_data.ParseFromArray(data, sz);
	size_t jos_sz = kafka_data.junction_size();
	size_t vss_sz = kafka_data.snapshot_size();
	size_t cnt_sz = kafka_data.count_size();
	for (size_t i = 0; i < vss_sz; i++) {
		auto vss_snapshot = kafka_data.snapshot(i);
		// auto vss_item = vss_snapshot.package();
		// timeval tv;
		// tv.tv_sec = vss_item.timestamp_sec();
		// tv.tv_usec = vss_item.timestamp_usec();
		// tm t;
		// localtime_r(&tv.tv_sec, &t);
		// printf("data time %d/%02d/%02d %02d:%02d:%02d:%06ld\n",
		// 	t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
		// 	t.tm_hour, t.tm_min, t.tm_sec,
		// 	tv.tv_usec);
		// printf("snapshot distance %f m, speed %f km/h\n",
		// 	vss_item.distance(), vss_item.vehicle_speed());
		// printf("snapshot gps latitude %.8f, longtitude %.8f, heading %f\n",
		// 	vss_item.gpsinfo().latitude(),
		// 	vss_item.gpsinfo().longtitude(), 
		// 	vss_item.gpsinfo().heading());
		// printf("snapshot acc_pedal_pos  %f, brake_pedal %f, steering_wheel %f\n",
		// 	vss_item.acc_pedal_pos_percentage(),
		// 	vss_item.brake_pedal_pos_percentage(),
		// 	vss_item.steering_wheel_angle()
		// 	);
		// printf("snapshot vehicle_state %d, flags %d, shift_lever_position %d\n",
		// 	vss_item.vehicle_state(),
		// 	vss_item.flags(),
		// 	vss_item.shift_lever_position()
		// 	);
		std::string veh_data;
		vss_snapshot.SerializeToString(&veh_data);
		send_kafka_to_center(_vehicle_topic, 
			(void*)veh_data.c_str(), veh_data.length());
	}
	timeval tv;
	gettimeofday(&tv, nullptr);
	uint64_t times = tv.tv_sec * 1000 + tv.tv_usec / 1000;

	for (size_t i = 0; i < jos_sz; i++) {
		auto* jos_item = kafka_data.mutable_junction(i);
		// printf("[%ld] junciton id %s, time %lu, target sz %d, lat %lf, lon %lf\n",
		// 	times - jos_item->timestamp(),
		// 	jos_item->junction_id().c_str(), jos_item->timestamp(), jos_item->targets_size(),
		// 	jos_item->lat(),
		// 	jos_item->lon());

		// for (size_t j = 0; j < jos_item->targets_size(); j++) {
		// 	auto target_item = jos_item->targets(j);
		// 	printf("target id [%d], lat %lf, lon %lf, heading %f, speed %f\n",
		// 		target_item.id(), target_item.lat(), target_item.lon(),
		// 		target_item.hdg(), target_item.speed());
		// }

		// for (size_t j = 0; j < jos_item->devices_size(); j++) {
		// 	auto target_item = jos_item->devices(j);
		// 	printf("[%ld] junciton target %s, time %lu\n",
		// 		times -jos_item->timestamp(),
		// 		target_item.id().c_str(), target_item.timestamp());
		// }
		auto* timeinfo = jos_item->mutable_timeinfo();

		// 时间延时设置，移动到snapshot接收到融合后数据位置
		// timeval tv;
		// gettimeofday(&tv, nullptr);
		// timeinfo->set_esend_timestamp_sec(tv.tv_sec);
		// timeinfo->set_esend_timestamp_usec(tv.tv_usec);
		// tm t;
		// localtime_r(&tv.tv_sec, &t);
		// printf("send data time %d/%02d/%02d %02d:%02d:%02d:%06ld\n",
		// 	t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
		// 	t.tm_hour, t.tm_min, t.tm_sec,
		// 	tv.tv_usec);
		// tv.tv_sec = timeinfo->erecv_timestamp_sec();
		// tv.tv_usec = timeinfo->erecv_timestamp_usec();
		// localtime_r(&tv.tv_sec, &t);
		// printf("recv data time %d/%02d/%02d %02d:%02d:%02d:%06ld\n",
		// 	t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
		// 	t.tm_hour, t.tm_min, t.tm_sec,
		// 	tv.tv_usec);
		// tv.tv_sec = timeinfo->collection_timestamp_sec();
		// tv.tv_usec = timeinfo->collection_timestamp_usec();
		// localtime_r(&tv.tv_sec, &t);
		// printf("collection data time %d/%02d/%02d %02d:%02d:%02d:%06ld\n",
		// 	t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
		// 	t.tm_hour, t.tm_min, t.tm_sec,
		// 	tv.tv_usec);
		// tv.tv_sec = timeinfo->timestamp_sec();
		// tv.tv_usec = timeinfo->timestamp_usec();
		// localtime_r(&tv.tv_sec, &t);
		// printf("transfer data time %d/%02d/%02d %02d:%02d:%02d:%06ld\n",
		// 	t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
		// 	t.tm_hour, t.tm_min, t.tm_sec,
		// 	tv.tv_usec);
		std::string veh_data;
		jos_item->SerializeToString(&veh_data);
		send_kafka_to_center(_junciton_topic, 
			(void*)veh_data.c_str(), veh_data.length());
	}

	for (size_t i = 0; i < cnt_sz; i++) {
		auto& cnt_item = kafka_data.count(i);
		if (cnt_item.invs_size() > 0) {
			for (int in_index = 0; in_index < cnt_item.invs_size(); in_index++) {
				auto& ininfo = cnt_item.invs(in_index);
				printf("[%u]incomming name %s, id %s, type %d, id %d, interval %u, vid %u, speed %lf\n", cnt_item.junction_id(), ininfo.name().c_str(), ininfo.approachid().c_str(), (int)ininfo.type(), ininfo.laneid(), ininfo.time_interval(), ininfo.veh().id(), ininfo.veh().speed());
			}
		}
		if (cnt_item.outvs_size() > 0) {
			for (int out_index = 0; out_index < cnt_item.outvs_size(); out_index++) {
				auto& outinfo = cnt_item.outvs(out_index);
				printf("outgoing name %s, id %s, outid %s, type %d, inlaneid %d, outlaneid %d, vid %u, speed %lf\n", outinfo.name().c_str(), outinfo.outgoingid().c_str(), outinfo.incommingid().c_str(), (int)outinfo.type(), outinfo.incomming_laneid(), outinfo.outgoing_laneid(), outinfo.veh().id(), outinfo.veh().speed());
			}
		}
		std::string cnt_data;
		cnt_item.SerializeToString(&cnt_data);

		timeval tv;
		gettimeofday(&tv, nullptr);
		uint64_t time1 = tv.tv_sec * 1000 * 1000 + tv.tv_usec;
		send_kafka_to_center(_count_topic, 
			(void*)cnt_data.c_str(), cnt_data.length());
		gettimeofday(&tv, nullptr);
		uint64_t time2 = tv.tv_sec * 1000 * 1000 + tv.tv_usec;
		if (kafka_handle_cnt < 2000) {
			kafka_handle_cnt ++;
			tmpusetime1 += (time2 - time1);
		}
		else {
			char timebuf[256];
			tm t;
			memset(timebuf, 0, 256);
  			localtime_r(&tv.tv_sec, &t);
			snprintf(timebuf, 255, "%d/%02d/%02d %02d:%02d:%02d:%06ld",
			t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
			t.tm_hour, t.tm_min, t.tm_sec,
			tv.tv_usec);
			printf("[%s]kafka send 10000 use time %lu\n", timebuf, tmpusetime1);
			tmpusetime1 = 0;
			kafka_handle_cnt = 0;
		}
	}
	return 0;
}

int vss_kafka_sender::init(void)
{
	if (_inited) {
		return -EEXISTS;
	}
	service_backend::inst()->create_kafka_receiver("kafka_center");
	service_backend::inst()->set_kafka_callback(this);
	load_kafka_config();
	init_kafka();
	_inited = true;

	return 0;
}

int vss_kafka_sender::load_kafka_config(void)
{
	std::string path = "kafka.producer.addr";
	int ret = 0;
	_kafka_addr = get_sysconfig(path.c_str(), _kafka_addr.c_str(), &ret);
	if (ret) {
		log.e(SNAPSHOT_SNAPSHOT_TAG,
			"no found kafka addr\n");
		return -ENOTFOUND;
	}
	path = "kafka.producer.port";
	_port = get_sysconfig(path.c_str(), _port, &ret);
	if (ret) {
		log.e(SNAPSHOT_SNAPSHOT_TAG,
			"no found kafka port\n");
		return -ENOTFOUND;
	}
	// path = "kafka.producer.topic";
	// _topic = get_sysconfig(path.c_str(), _topic.c_str(), &ret);
	// if (ret) {
	// 	log.e(SNAPSHOT_SNAPSHOT_TAG,
	// 		"no found kafka topic\n");
	// 	return -ENOTFOUND;
	// }
	log.d(SNAPSHOT_SNAPSHOT_TAG, "kafka load config finished\n");
	return 0;
}

int vss_kafka_sender::init_kafka(void)
{
	RdKafka::Conf *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
	std::string brokers;
	std::string errstr;	
	brokers = _kafka_addr;
	brokers += ":";
	brokers += std::to_string(_port);
	log.d(SNAPSHOT_SNAPSHOT_TAG, "kafka brokers %s\n", brokers.c_str());
	_delivery_cb = new kafka_delivery_report;
	if (conf->set("bootstrap.servers", brokers, errstr) !=
		RdKafka::Conf::CONF_OK) {
		log.e(SNAPSHOT_SNAPSHOT_TAG,
			"set callback erro: %s\n", errstr.c_str());
		return -ELOGIC;
	}

	if (conf->set("dr_cb", _delivery_cb, errstr) != RdKafka::Conf::CONF_OK) {
		log.e(SNAPSHOT_SNAPSHOT_TAG,
			"set callback erro: %s\n", errstr.c_str());
		return -ELOGIC;
	}

	_producer = RdKafka::Producer::create(conf, errstr);
	if (!_producer) {
		log.e(SNAPSHOT_SNAPSHOT_TAG,
			"create produce erro: %s\n", errstr.c_str());
		return -ELOGIC;
	}
	log.d(SNAPSHOT_SNAPSHOT_TAG, "kafka init finished\n");
	delete conf;
	return 0;
}

int vss_kafka_sender::send_kafka_to_center(std::string &topic,
	void* data, size_t sz)
{
	if (!_producer) {
		return -ENOTAVAIL;
	}
	// printf("send to kafka topic %s, data sz %lu\n", topic.c_str(), sz);
	if (!data || 0 == sz) { return -EBADPARM; }
	RdKafka::ErrorCode err = _producer->produce(
		topic,
		RdKafka::Topic::PARTITION_UA,
		RdKafka::Producer::RK_MSG_COPY /* Copy payload */,
		data, sz,
		NULL, 0,
		0,
		NULL,
		NULL);

	if (err != RdKafka::ERR_NO_ERROR) {
		log.e(SNAPSHOT_SNAPSHOT_TAG,
			"%s failed to produce to topic : %s, %d\n",
			topic.c_str(), RdKafka::err2str(err), err);

		if (err == RdKafka::ERR__QUEUE_FULL) {
			//TODO
		}

    } else {
		// log.d(SNAPSHOT_SNAPSHOT_TAG,	
		// 	"enqueued message (%d bytes) for topic %s\n",
		// 	sz, topic.c_str());
	}
	_producer->poll(0);
	return 0;
}

}}	//zas::vehicle_snapshot_service




