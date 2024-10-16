#include <stdio.h>
#include <string>

#include "utils/mutex.h"
#include "utils/timer.h"
#include "MQTTAsync.h"

#include "mqtt_mgr.h"


namespace zas {
namespace servicebridge {

// #define ADDRESS     "tcp://10.0.0.93:1883"
#define ADDRESS     "tcp://116.236.72.174:65525"

#define QOS         2

using namespace std;
using namespace zas::utils;
static mutex mqcmut;

void connlost(void *context, char *cause)
{
	printf("connlost\n");
	if (!context) return;
	auto* client = reinterpret_cast<mqtt_client*>(context);
	client->connlost(cause);
}

void on_disconnect_failure(void* context, MQTTAsync_failureData* response)
{
	printf("on_disconnect_failure\n");
	if (!context) return;
	auto* client = reinterpret_cast<mqtt_client*>(context);
	client->on_disconnect_failure(response);
}

void on_disconnect(void* context, MQTTAsync_successData* response)
{
	printf("on_disconnect\n");
	if (!context) return;
	auto* client = reinterpret_cast<mqtt_client*>(context);
	client->on_disconnect(response);
}

void on_send_failure(void* context, MQTTAsync_failureData* response)
{
	printf("on_send_failure\n");
	if (!context) return;
	auto* client = reinterpret_cast<mqtt_client*>(context);
	client->on_send_failure(response);
}

void on_send(void* context, MQTTAsync_successData* response)
{
	if (!context) return;
	auto* client = reinterpret_cast<mqtt_client*>(context);
	client->on_send(response);
}

void on_connect_failure(void* context, MQTTAsync_failureData* response)
{
	printf("on_connect_failure\n");
	if (!context) return;
	auto* client = reinterpret_cast<mqtt_client*>(context);
	client->on_connect_failure(response);
}

void on_connect(void* context, MQTTAsync_successData* response)
{
	printf("on_connect\n");
	if (!context) return;
	auto* client = reinterpret_cast<mqtt_client*>(context);
	client->on_connect(response);
}

void on_subscribe(void* context, MQTTAsync_successData* response)
{
	printf("on_subscribe\n");
	if (!context) return;
	auto* client = reinterpret_cast<mqtt_client*>(context);
	client->on_subscribe(response);
}

void on_subscribe_failure(void* context, MQTTAsync_failureData* response)
{
	printf("on_subscribe_failure\n");
	if (!context) return;
	auto* client = reinterpret_cast<mqtt_client*>(context);
	client->on_subscribe_failure(response);
}

int message_arrived(void *context, char *topic_name,
	int topic_len, MQTTAsync_message *message)
{
	if (!context) {
		printf("message arrived context is null\n");
		return -1;
	}
	auto* client = reinterpret_cast<mqtt_client*>(context);
	client->message_arrived(topic_name, topic_len, message);
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topic_name);
    return 1;
}

void mqtt_client_listener::on_disconnect_failure(int errcode,
	const char* err_msg)
{
	printf("on_disconnect_failure \n");
}

void mqtt_client_listener::on_disconnect_success(void)
{
	printf("on_disconnect_success \n");
}

void mqtt_client_listener::on_subscribe_success(void)
{
	printf("on_subscribe_success \n");
}

void mqtt_client_listener::on_subscribe_failure(int errcode,
	const char* err_msg)
{
	printf("on_subscribe_failure \n");
}

void mqtt_client_listener::on_connect_failure(int errcode, const char* err_msg)
{
	printf("on_connect_failure \n");
}

void mqtt_client_listener::on_send_failure(int errcode, const char* err_msg)
{
	printf("on_send_failure \n");
}

void mqtt_client_listener::on_send_success(void)
{
	printf("on_send_success \n");
}

void mqtt_client_listener::on_connect_success(void)
{
	printf("on_connect_success \n");
}

mqtt_client::mqtt_client()
: _client(nullptr), _flags(0), _lnr(nullptr)
, _mqtt_handle_num(0)
{
	listnode_init(_topic_list);
	listnode_init(_mqtt_handle_free_list);
	_topic_tree = nullptr;
	_mqtt_handle_thread = new looper_thread("mqtt_handle_htread");
	char* env = getenv("V2X_VEHICLE_ID");
	if (!env || strlen(env) < 1)
		_mqtt_client_id = "test_car_client";
	else
		_mqtt_client_id = env;
	printf("client id is %s\n", _mqtt_client_id.c_str());
}

mqtt_client::~mqtt_client()
{
	if (_client) {
		MQTTAsync_destroy(&_client);
	}
	_client = nullptr;
	_mqtt_handle_thread->stop();
	_mqtt_handle_thread->release();
	_mqtt_handle_thread = nullptr;
}

int mqtt_client::register_client_listener(mqtt_client_listener *lnr)
{
	_lnr = lnr;
	return 0;
}

mqtt_topic::mqtt_topic(const char* topic)
{
	listnode_init(_listener_list);
	_topic = topic;
}

mqtt_topic::~mqtt_topic()
{
	release_all_listener();
}

int mqtt_topic::add_listener(mqtt_message_listener* lnr)
{
	auto* lnd = find_listener_unlock(lnr);
	if (lnd) return -EEXISTS;
	lnd = new listener_nd;
	lnd->lnr = lnr;
	listnode_add(_listener_list, lnd->ownerlist);
	return 0;
}

int mqtt_topic::remove_listener(mqtt_message_listener* lnr)
{
	auto* lnd = find_listener_unlock(lnr);
	if (!lnd) return -ENOTFOUND;
	listnode_del(lnd->ownerlist);
	delete lnd;
	return 0;
}

int mqtt_topic::release_all_listener(void)
{
	listener_nd *nd;
	while (!listnode_isempty(_listener_list)) {
		nd = LIST_ENTRY(listener_nd, ownerlist, _listener_list.next);
		listnode_del(nd->ownerlist);
		delete nd;
	}
	return 0;
}
int mqtt_topic::message_arrived(void* data, size_t sz)
{
	listnode_t *nd = _listener_list.next;
	for (; nd != &_listener_list; nd = nd->next) {
		auto* lnrnd = LIST_ENTRY(listener_nd, ownerlist, nd);
		lnrnd->lnr->on_recv_msg(data, sz);
	}
	return 0;
}

mqtt_topic::listener_nd* 
mqtt_topic::find_listener_unlock(mqtt_message_listener* lnr)
{
	listnode_t *nd = _listener_list.next;
	for (; nd != &_listener_list; nd= nd->next) {
		auto* lnode = LIST_ENTRY(listener_nd, ownerlist, nd);
		if (lnode->lnr == lnr)
			return lnode;
	}
	return nullptr;
}

int mqtt_client::register_message_listener(const char* topic,
	mqtt_message_listener *lnr)
{
	if (!is_runing()) return -ENOTAVAIL;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	opts.onSuccess = zas::servicebridge::on_subscribe;
	opts.onFailure = zas::servicebridge::on_subscribe_failure;
	opts.context = this;

	int rc;
	if ((rc = MQTTAsync_subscribe(_client, topic, QOS, &opts)) != MQTTASYNC_SUCCESS)
	{
		printf("failed to start subscribe, return code %d\n", rc);
		return -ELOGIC;
	}
	add_topic_listener(topic, lnr);
	return 0;
}

int mqtt_client::send_message(const char* topic, void* data, size_t sz)
{
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	int rc;

	opts.onSuccess = zas::servicebridge::on_send;
	opts.onFailure = zas::servicebridge::on_send_failure;
	opts.context = this;
	pubmsg.payload = data;
	pubmsg.payloadlen = sz;
	pubmsg.qos = QOS;
	pubmsg.retained = 0;
	if ((rc = MQTTAsync_sendMessage(_client, topic, &pubmsg, &opts))
		!= MQTTASYNC_SUCCESS)
	{
		printf("Failed to start sendMessage, return code %d\n", rc);
		return -ELOGIC;
	}
	return 0;
}

mqtt_client* mqtt_client::inst()
{
	static mqtt_client* _inst = nullptr;
	if (_inst) return _inst;

	auto_mutex am(mqcmut);
	if (_inst) return _inst;
	_inst = new mqtt_client();
	assert(nullptr != _inst);
	_inst->init_msqtt_client();
	return _inst;
}

int mqtt_client::init_msqtt_client(void)
{
	printf("init_msqtt_client start\n");
	if (_client) return -EEXISTS;
	_mqtt_handle_thread->start();
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	MQTTAsync_disconnectOptions disc_opts = MQTTAsync_disconnectOptions_initializer;
	MQTTAsync_willOptions will_opts = MQTTAsync_willOptions_initializer;
	int rc;
	int ch;

	if ((rc = MQTTAsync_create(&_client, ADDRESS, _mqtt_client_id.c_str(), MQTTCLIENT_PERSISTENCE_NONE, nullptr))
			!= MQTTASYNC_SUCCESS)
	{
		printf("Failed to create client, return code %d\n", rc);
		return -ELOGIC;
	}

	if ((rc = MQTTAsync_setCallbacks(_client, this,
		zas::servicebridge::connlost, zas::servicebridge::message_arrived,
		nullptr)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to set callbacks, return code %d\n", rc);
		MQTTAsync_destroy(&_client);
		_client = nullptr;
		return -ELOGIC;
	}

	will_opts.message = _mqtt_client_id.c_str();
	will_opts.topicName = "/rpc/will/topic";
	will_opts.qos = 2;
	conn_opts.will = &will_opts;

	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	conn_opts.onSuccess = zas::servicebridge::on_connect;
	conn_opts.onFailure = zas::servicebridge::on_connect_failure;
	conn_opts.context = this;
	if ((rc = MQTTAsync_connect(_client, &conn_opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start connect, return code %d\n", rc);
		MQTTAsync_destroy(&_client);
		_client = nullptr;
		return -ELOGIC;
	}
	printf("init_msqtt_client finished\n");
	return 0;
}

void mqtt_client::connlost(char *cause)
{
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	int rc;
	printf("\nConnection lost\n");
	if (cause)
		printf("     cause: %s\n", cause);

	printf("Reconnecting\n");
	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	if ((rc = MQTTAsync_connect(_client, &conn_opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start connect, return code %d\n", rc);
		_f.finished = 1;
	}
}

void mqtt_client::on_send_failure(MQTTAsync_failureData* response)
{

}

void mqtt_client::on_send(MQTTAsync_successData* response)
{

}

void mqtt_client::on_connect(MQTTAsync_successData* response)
{
	_f.running = 1;
	if (_lnr)
		_lnr->on_connect_success();
}

void mqtt_client::on_disconnect_failure(MQTTAsync_failureData* response)
{
	printf("on_disconnect_failure\n");
}

void mqtt_client::on_disconnect(MQTTAsync_successData* response)
{

}

void mqtt_client::on_subscribe(MQTTAsync_successData* response)
{
	printf("on_subscribe\n");	
}

void mqtt_client::on_subscribe_failure(MQTTAsync_failureData* response)
{
	printf("on_subscribe_failure\n");
}

void mqtt_client::on_connect_failure(MQTTAsync_failureData* response)
{

}

int mqtt_client::message_arrived(char *topic_name,
	int topic_len, MQTTAsync_message *message)
{
	auto* topic_lnr = find_topic(topic_name);
	if (topic_lnr) {
		auto* handler = dequeue_task();
		if (!handler) {
			printf("error: mqtt handle queue is full.\n");
			return -1;
		}
		handler->setdata(topic_lnr, message->payload, message->payloadlen);
		handler->addto(_mqtt_handle_thread);
	} else {
		printf("error not find topic %s\n", topic_name);
	}
	return 0;
}

int mqtt_client::mqtt_topic_compared(avl_node_t* aa, avl_node_t* bb)
{
	mqtt_topic *nda = AVLNODE_ENTRY(mqtt_topic, avlnode, aa);
	mqtt_topic *ndb = AVLNODE_ENTRY(mqtt_topic, avlnode, bb);
	int ret = strcmp(nda->_topic.c_str(), ndb->_topic.c_str());
	if (ret > 0) return 1;
	else if (ret < 0) return -1;
	else return 0;
}

int mqtt_client::add_topic_listener(const char* topic,
	mqtt_message_listener* lnr)
{
	if (!topic) return -EBADPARM;
	auto_mutex am(_mut);
	auto* topic_lnr = find_topic(topic);
	if (!topic_lnr) {
		topic_lnr = new mqtt_topic(topic);
		if (avl_insert(&_topic_tree, &topic_lnr->avlnode,
			mqtt_topic_compared)){
			delete topic_lnr;
			return -ELOGIC;
		}
		listnode_add(_topic_list, topic_lnr->ownerlist);
	}
	auto* tp_lnr = topic_lnr->find_listener_unlock(lnr);
	if (tp_lnr) return -EEXISTS;
	topic_lnr->add_listener(lnr);
	return 0;
}

int mqtt_client::remove_topic_listener(const char* topic,
	mqtt_message_listener* lnr)
{
	auto_mutex am(_mut);
	auto* topic_lnr = find_topic(topic);
	if (!topic_lnr) return -ENOTFOUND;
	return topic_lnr->remove_listener(lnr);	
}

int mqtt_client::remove_topic(const char* topic)
{
	auto_mutex am(_mut);
	auto* topic_lnr = find_topic(topic);
	if (!topic_lnr) return -ENOTFOUND;
	avl_remove(&_topic_tree, &topic_lnr->avlnode);
	listnode_del(topic_lnr->ownerlist);
	delete topic_lnr;
	return 0;
}

int mqtt_client::remove_all_topic(void)
{
	auto_mutex am(_mut);
	mqtt_topic* topic_lnr = nullptr;
	while(!listnode_isempty(_topic_list))
	{
		topic_lnr = LIST_ENTRY(mqtt_topic, ownerlist, _topic_list.next);
		listnode_del(topic_lnr->ownerlist);
		delete topic_lnr;
	}
	_topic_tree = NULL;
	return 0;
}

mqtt_topic* mqtt_client::find_topic(const char* topic)
{
	auto_mutex am(_mut);
	return find_topic_unlock(topic);
}

mqtt_topic* mqtt_client::find_topic_unlock(const char* topic)
{
	std::string tp = topic;
	avl_node_t *tp_nd = avl_find(_topic_tree,
		MAKE_FIND_OBJECT(tp, mqtt_topic, _topic, avlnode),
		mqtt_topic_compared);
	if (!tp_nd) return nullptr;
	return AVLNODE_ENTRY(mqtt_topic, avlnode, tp_nd);
}

mqtt_handle_task* mqtt_client::dequeue_task(void)
{
	auto_mutex am(_handlermut);
	if (listnode_isempty(_mqtt_handle_free_list)) {
		if (_mqtt_handle_num < MQTT_CLIENT_HANDLE_MAX_ITEM){
			_mqtt_handle_num ++;
			return new mqtt_handle_task(this);
		}
		else
			return nullptr;
	}
	auto* handler = list_entry(mqtt_handle_task, _ownerlist, _mqtt_handle_free_list.next);
	listnode_del(handler->_ownerlist);
	return handler;
}

bool mqtt_client::add_to_task_free_list(mqtt_handle_task *task)
{
	auto_mutex am(_handlermut);
	listnode_add(_mqtt_handle_free_list, task->_ownerlist);
	return true;
}

const char* mqtt_client::get_client_id(void)
{
	//if muti client , client id is move to mgr
	return _mqtt_client_id.c_str();
}

mqtt_handle_task::mqtt_handle_task(mqtt_client *client)
: _client_id(client)
, _topic(nullptr), _size(0)
{
	_data.clear();
}

mqtt_handle_task::~mqtt_handle_task()
{
	_client_id = nullptr;
	_size = 0;
}

void mqtt_handle_task::do_action()
{
	if (nullptr == _topic) {
		printf("mqtt_handle_task::do_action no topic\n");
		return;
	}
	_topic->message_arrived((void*)_data.c_str(), _size);
}

void mqtt_handle_task::setdata(mqtt_topic *topic, void* data, size_t sz)
{
	_topic = topic;
	_data.clear();
	_data.append((char*)data, sz);
	_size = sz;
}

void mqtt_handle_task::release()
{
	if (!_client_id) {
		delete this;
		return;
	}
	_topic = nullptr;
	_client_id->add_to_task_free_list(this);
}


}} // end of namespace zas::bridge
/* EOF */