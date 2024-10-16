#include "kafka-consumer.h"
#include <sys/time.h>
#include "librdkafka/rdkafkacpp.h"

#include "webcore/logger.h"
#include "webcore/sysconfig.h"
#include "webcore/webapp.h"
#include "service-worker.h"
#include "utils/json.h"
#include "utils/uri.h"

namespace zas {
namespace vehicle_snapshot_service {

using namespace zas::utils;
using namespace zas::webcore;

static volatile sig_atomic_t consumer_run = 1;
static int partition_cnt         = 0;
static int verbosity             = 0;
static long msg_cnt              = 0;
static int64_t msg_bytes         = 0;

class kafka_consumer_event : public RdKafka::EventCb
{
public:
	void event_cb(RdKafka::Event &event) {
		switch (event.type()) {
		case RdKafka::Event::EVENT_ERROR:
			if (event.fatal()) {
				log.e(SNAPSHOT_SNAPSHOT_TAG, "FATAL ");
				consumer_run = 0;
			}
			log.e(SNAPSHOT_SNAPSHOT_TAG, "ERROR (%s ): %s\n", RdKafka::err2str(event.err()).c_str(), event.str().c_str());
		break;

		case RdKafka::Event::EVENT_STATS:
			log.e(SNAPSHOT_SNAPSHOT_TAG, "\"STATS\": %s\n", event.str().c_str());
			break;

	    case RdKafka::Event::EVENT_LOG:
			log.e(SNAPSHOT_SNAPSHOT_TAG, "LOG-%i-%s: %s\n", event.severity(),
				event.fac().c_str(), event.str().c_str());
			break;

		case RdKafka::Event::EVENT_THROTTLE:
			log.e(SNAPSHOT_SNAPSHOT_TAG, "THROTTLED: %dms by %s, id %d\n", event.throttle_time(), event.broker_name().c_str(), (int)event.broker_id());;
			break;

		default:
			log.e(SNAPSHOT_SNAPSHOT_TAG, "EVENT %d (%s): %s\n", event.type(), RdKafka::err2str(event.err()).c_str(), event.str().c_str());
			break;
		}
	}
};

class light_rebalance_cb : public RdKafka::RebalanceCb
{
private:
	static void part_list_print(const std::vector<RdKafka::TopicPartition *> &partitions)
	{
		for (unsigned int i = 0; i < partitions.size(); i++) {
		  log.e(SNAPSHOT_SNAPSHOT_TAG, "%s[%s], ", partitions[i]->topic(), partitions[i]->partition());
		}
		log.e(SNAPSHOT_SNAPSHOT_TAG, "\n");
	}

public:
	void rebalance_cb(RdKafka::KafkaConsumer *consumer,
			RdKafka::ErrorCode err,
			std::vector<RdKafka::TopicPartition *> &partitions) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, "RebalanceCb: %s: ",RdKafka::err2str(err).c_str());
		part_list_print(partitions);

		RdKafka::Error *error      = NULL;
		RdKafka::ErrorCode ret_err = RdKafka::ERR_NO_ERROR;

		if (err == RdKafka::ERR__ASSIGN_PARTITIONS) {
			if (consumer->rebalance_protocol() == "COOPERATIVE") {
				error = consumer->incremental_assign(partitions);
			}
			else {
				ret_err = consumer->assign(partitions);
			}
			partition_cnt += (int)partitions.size();
		} else {
			if (consumer->rebalance_protocol() == "COOPERATIVE") {
				error = consumer->incremental_unassign(partitions);
				partition_cnt -= (int)partitions.size();
			} else {
				ret_err       = consumer->unassign();
				partition_cnt = 0;
			}
		}

		if (error) {
			log.e(SNAPSHOT_SNAPSHOT_TAG, "incremental assign failed: %s\n",error->str());
			delete error;
		}
		else if (ret_err) {
			log.e(SNAPSHOT_SNAPSHOT_TAG, "assign failed: %s\n", RdKafka::err2str(ret_err));
		}
	}
};


void msg_consume(RdKafka::Message *message, void *opaque) {
	switch (message->err()) {
	case RdKafka::ERR__TIMED_OUT:
		log.e(SNAPSHOT_SNAPSHOT_TAG, "msg_consume time out\n");
	break;

	case RdKafka::ERR_NO_ERROR:
		/* Real message */
		msg_cnt++;
		msg_bytes += message->len();
		if (verbosity >= 3) {
			log.d(SNAPSHOT_SNAPSHOT_TAG, "Read msg at offset %ld\n", message->offset());
		}
		RdKafka::MessageTimestamp ts;
		ts = message->timestamp();
		if (verbosity >= 2 
			&& ts.type != RdKafka::MessageTimestamp::MSG_TIMESTAMP_NOT_AVAILABLE) {
			std::string tsname = "?";
			if (ts.type == RdKafka::MessageTimestamp::MSG_TIMESTAMP_CREATE_TIME) {
				tsname = "create time";
			}
			else if (ts.type == RdKafka::MessageTimestamp::MSG_TIMESTAMP_LOG_APPEND_TIME) {
				tsname = "log append time";
			}
			log.d(SNAPSHOT_SNAPSHOT_TAG, "Timestamp: %s %ld\n", tsname.c_str(), ts.timestamp);
		}
		if (verbosity >= 2 && message->key()) {
		  log.d(SNAPSHOT_SNAPSHOT_TAG, "Key: %s\n", message->key()->c_str());
		}
		if (verbosity >= 1) {
			log.d(SNAPSHOT_SNAPSHOT_TAG, "%.*s\n", static_cast<int>(message->len()), static_cast<const char *>(message->payload()));
		}
		vss_light_mgr::inst()->update_light_state(static_cast<const char *>(message->payload()));
	break;

	case RdKafka::ERR__PARTITION_EOF:
		log.e(SNAPSHOT_SNAPSHOT_TAG, "msg_consume ERR__PARTITION_EOF\n");
	break;

	case RdKafka::ERR__UNKNOWN_TOPIC:
	case RdKafka::ERR__UNKNOWN_PARTITION:
		log.e(SNAPSHOT_SNAPSHOT_TAG, "Consume failed: %s\n", message->errstr());
		consumer_run = 0;
	break;

	default:
		/* Errors */
		log.e(SNAPSHOT_SNAPSHOT_TAG, "Consume failed: %s\n", message->errstr().c_str());
		consumer_run = 0;
	}
}

static void sigterm(int sig) {
  consumer_run = 0;
}

vss_kafka_consumer::vss_kafka_consumer()
{
	_topics.clear();
	load_consumerkafka_config();
	vss_light_mgr::inst();
}

vss_kafka_consumer::~vss_kafka_consumer()
{

}

int vss_kafka_consumer::run(void)
{
	std::string errstr;
	/*
	* Create configuration objects
	*/
	RdKafka::Conf *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
	light_rebalance_cb light_rebalance_cb;
	printf("kafka running\n");
	conf->set("security.protocol", "SASL_PLAINTEXT", errstr);
    conf->set("sasl.mechanism", "SCRAM-SHA-256", errstr);
    conf->set("sasl.username", "vehicle", errstr);
    conf->set("sasl.password", "vehicle@789", errstr);
	if (conf->set("rebalance_cb", &light_rebalance_cb, errstr) != RdKafka::Conf::CONF_OK) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, "rebalance failed: %s\n", errstr.c_str());
	}
	if (conf->set("enable.partition.eof", "true", errstr) != RdKafka::Conf::CONF_OK) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, "eof failed: %s\n", errstr.c_str());
	}
	if (conf->set("group.id", _group_id.c_str(), errstr) != RdKafka::Conf::CONF_OK) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, "group failed: %s\n", errstr.c_str());
	}
	/*
	* Set configuration properties
	*/
	if (conf->set("metadata.broker.list", _brokers, errstr) != RdKafka::Conf::CONF_OK) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, "broker failed: %s\n", errstr.c_str());
	}
	kafka_consumer_event co_event_cb;
	if (conf->set("event_cb", &co_event_cb, errstr) != RdKafka::Conf::CONF_OK) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, "event_cb failed: %s\n", errstr.c_str());
	}

	signal(SIGINT, sigterm);
	signal(SIGTERM, sigterm);

	/*
	* Create consumer using accumulated global configuration.
	*/
	RdKafka::KafkaConsumer *consumer = 
		RdKafka::KafkaConsumer::create(conf, errstr);
	if (!consumer) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, "Failed to create consumer: %s\n", errstr.c_str());
		return 0;
	}
	delete conf;

	log.d(SNAPSHOT_SNAPSHOT_TAG, "Created consumer %s", consumer->name());

	/*
	* Subscribe to topics
	*/
	RdKafka::ErrorCode err = consumer->subscribe(_topics);
	if (err) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, "Failed to subscribe to %lu, topics: %s\n", _topics.size(), RdKafka::err2str(err).c_str());
		return 0;
	}

	/*
	* Consume messages
	*/
	while (consumer_run) {
		RdKafka::Message *msg = consumer->consume(1000);
		msg_consume(msg, NULL);
		delete msg;
	}

	/*
	* Stop consumer
	*/
	consumer->close();
	delete consumer;

	log.e(SNAPSHOT_SNAPSHOT_TAG, "Consumed %ld, messages(%ld bytes)\n", msg_cnt, msg_bytes);

	/*
	* Wait for RdKafka to decommission.
	* This is not strictly needed (with check outq_len() above), but
	* allows RdKafka to clean up all its resources before the application
	* exits so that memory profilers such as valgrind wont complain about
	* memory leaks.
	*/
	RdKafka::wait_destroyed(5000);

	return 0;
}

int vss_kafka_consumer::consumer_stop(void)
{
	consumer_run = 0;
	return 0;
}

int vss_kafka_consumer::load_consumerkafka_config(void)
{
	std::string path = "kafka.consumer.broker";
	int ret = 0;
	_brokers.clear();
	_brokers = get_sysconfig(path.c_str(), _brokers.c_str(), &ret);
	if (ret) {
		log.e(SNAPSHOT_SNAPSHOT_TAG,
			"no found kafka consumer brokers\n");
		return -ENOTFOUND;
	}
	path = "kafka.consumer.group_id";
	_group_id = "mec.snapshot";
	_group_id = get_sysconfig(path.c_str(), _group_id.c_str(), &ret);
	if (ret) {
		log.e(SNAPSHOT_SNAPSHOT_TAG,
			"no found kafka consumer group id\n");
		return -ENOTFOUND;
	}

	path = "kafka.consumer.topic";
	std::string topic;
	topic = get_sysconfig(path.c_str(), topic.c_str(), &ret);
	if (ret) {
		log.e(SNAPSHOT_SNAPSHOT_TAG,
			"no found kafka consumer topic\n");
		return -ENOTFOUND;
	}
	_topics.clear();
	_topics.push_back(topic);
	log.d(SNAPSHOT_SNAPSHOT_TAG, "kafka consumer load config finished\n");
	return 0;
}

vss_light_mgr* vss_light_mgr::inst()
{
	static vss_light_mgr* _inst = NULL;
	if (_inst) return _inst;
	_inst = new vss_light_mgr();
	assert(NULL != _inst);
	return _inst;
}

vss_light_mgr::vss_light_mgr()
{
	_light_juncs.clear();
	_lights.clear();
	_junc_lights.clear();
}

vss_light_mgr::~vss_light_mgr()
{
	_light_juncs.clear();
	_lights.clear();
	_junc_lights.clear();
}

int vss_light_mgr::add_light(int junc_id, int light_id)
{
	if (light_id < 0) {
		return -EBADPARM;
	}
	auto_mutex am(_light_mut);
	if (_junc_lights.count(junc_id) <= 0) {
		_junc_lights[junc_id] = std::make_shared<junc_lights>();
	}
	_junc_lights[junc_id]->update = true;

	if (_lights.count(light_id) <=0) {
		_lights[light_id] = std::make_shared<light_info>();
		_lights[light_id]->id = light_id;
		_lights[light_id]->timestamp = 0;
		_lights[light_id]->state = 1;
		_lights[light_id]->enttime = 0;
	}
	_junc_lights[junc_id]->lights[light_id] = _lights[light_id];
	_light_juncs[light_id].insert(junc_id);
	return 0;
}

int vss_light_mgr::remove_light(int junc_id, int light_id)
{
	if (light_id < 0) {
		return -EBADPARM;
	}
	auto_mutex am(_light_mut);
	if (_junc_lights.count(junc_id) <= 0) {
		return -ENOTFOUND;
	}
	if (_junc_lights[junc_id]->lights.count(light_id) <= 0) {
		return -ENOTFOUND;
	}
	_junc_lights[junc_id]->lights.erase(light_id);

	if (_light_juncs[light_id].count(junc_id) >= 0) {
		_light_juncs[light_id].erase(junc_id);
	}
	if (_light_juncs[light_id].size() == 0) {
		_light_juncs.erase(light_id);
		_lights.erase(light_id);
	}
	return 0;
}

int vss_light_mgr::get_light_state(int junc_id, std::map<int, int> *lights)
{
	if (nullptr == lights) {
		return -EBADPARM;
	}
	auto_mutex am(_light_mut);
	if (_junc_lights.count(junc_id) <= 0) {
		return -ENOTFOUND;
	}
	if (!_junc_lights[junc_id]->update) {
		return -ENOTAVAIL;
	}
	for (auto const &lt : _junc_lights[junc_id]->lights) {
		(*lights)[lt.first] = lt.second->state;
	}
	_junc_lights[junc_id]->update = false;
	return 0;
}

int vss_light_mgr::update_light_state(const char *light)
{
	if (nullptr == light) {
		return -EBADPARM;
	}

	auto& light_info = json::parse(light);
	if (light_info.is_null()) {
		return -EBADPARM;
	}

	auto& jid = light_info.get("lightId");
	if (!jid.is_number()) {
		light_info.release();
		return -EBADPARM;
	}
	int id = jid.to_number();
	auto_mutex am(_light_mut);
	if (_lights.count(id) <= 0) {
		light_info.release();
		return -ENOTALLOWED;
	}
	auto& jtimestamp = light_info.get("timeStamp");
	auto& jentime = light_info.get("likelyEndTime");
	auto& jstate = light_info.get("lightState");
	if(!jtimestamp.is_number() || !jstate.is_number()) {
		light_info.release();
		return -EBADPARM;
	}
	int64_t timestamp = jtimestamp.to_number();
	int endtime = -1;
	if (jentime.is_number()) {
		endtime = jentime.to_number();
	}
	int state = jstate.to_number();
	light_info.release();

	_lights[id]->enttime = endtime;
	_lights[id]->timestamp = timestamp;
	if (_lights[id]->state == state) {
		return 0;
	}
	_lights[id]->state = state;

	// for test
	// if (_lights.count(21) > 0) {
	// 	_lights[21]->state = 1;
	// }
	// if (_lights.count(22) > 0) {
	// 	_lights[22]->state = 1;
	// }
	// if (_lights.count(23) > 0) {
	// 	_lights[23]->state = 1;
	// }
	// if (_lights.count(25) > 0) {
	// 	_lights[25]->state = 1;
	// }
	// printf("light id %d, state %d\n", id, state);
	if (_light_juncs.count(id) <= 0) {
		_lights.erase(id);
	}
	for (const int& juncid : _light_juncs[id]) {
		_junc_lights[juncid]->update = true;
	}
	return 0;
}

}}	//zas::vehicle_snapshot_service




