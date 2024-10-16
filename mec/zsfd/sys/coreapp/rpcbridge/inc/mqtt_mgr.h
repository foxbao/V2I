
#ifndef __CXX_SYSTEM_MQTT_MGR_H__
#define __CXX_SYSTEM_MQTT_MGR_H__

#include "std/zasbsc.h"
#include "std/list.h"
#include "utils/avltree.h"
#include "utils/mutex.h"
#include "utils/thread.h"
#include "MQTTAsync.h"

namespace zas {
namespace servicebridge {
#define SUBSCRIB_TOPIC_PRIFEX "/p2p/"
#define MQTT_CLIENT_HANDLE_MAX_ITEM	(20)
using namespace zas::utils;

class mqtt_client;
class mqtt_topic;

class mqtt_handle_task : public looper_task
{
	friend class mqtt_client;
public:
	mqtt_handle_task(mqtt_client *client);
	~mqtt_handle_task();

	virtual void do_action(void);
	virtual void release(void);

	void setdata(mqtt_topic* topic, void* data, size_t sz);

private:
	std::string 	_data;
	mqtt_topic*		_topic;
	mqtt_client*	_client_id;
	size_t				_size;
	listnode_t 		_ownerlist;
};

class mqtt_client_listener
{
public:
	virtual void on_disconnect_failure(int errcode, const char* err_msg);
	virtual void on_disconnect_success(void);
	virtual void on_subscribe_failure(int errcode, const char* err_msg);
	virtual void on_subscribe_success(void);
	virtual void on_connect_failure(int errcode, const char* err_msg);
	virtual void on_connect_success(void);
	virtual void on_send_failure(int errcode, const char* err_msg);
	virtual void on_send_success(void);
};

class mqtt_message_listener
{
public:
	virtual int on_recv_msg(void* data, size_t sz) = 0;
};

class mqtt_topic
{
friend class mqtt_client;
public:
	mqtt_topic(const char* topic);
	~mqtt_topic();

public:
	int add_listener(mqtt_message_listener* lnr);
	int remove_listener(mqtt_message_listener* lnr);
	int release_all_listener(void);
	int message_arrived(void* data, size_t sz);

private:
	typedef struct 
	{
		listnode_t ownerlist;
		mqtt_message_listener* lnr;
	}listener_nd;
	listener_nd* find_listener_unlock(mqtt_message_listener* lnr);
private:
	listnode_t ownerlist;
	avl_node_t avlnode;
	std::string _topic;
	listnode_t _listener_list;
};

class mqtt_client
{
public:
	static mqtt_client* inst();
	int register_client_listener(mqtt_client_listener *lnr);
	int register_message_listener(const char* topic,
		mqtt_message_listener *lnr);
	int send_message(const char* topic, void* data, size_t sz);
	bool add_to_task_free_list(mqtt_handle_task* task);
	const char* get_client_id(void);

public:
	void connlost(char *cause);
	void on_disconnect_failure(MQTTAsync_failureData* response);
	void on_disconnect(MQTTAsync_successData* response);
	void on_send_failure(MQTTAsync_failureData* response);
	void on_send(MQTTAsync_successData* response);
	void on_connect_failure(MQTTAsync_failureData* response);
	void on_connect(MQTTAsync_successData* response);
	void on_subscribe(MQTTAsync_successData* response);
	void on_subscribe_failure(MQTTAsync_failureData* response);
	int message_arrived(char *topic_name, int topic_len,
		MQTTAsync_message *message);

	bool is_runing(void) {
		return (1 == _f.running);
	}

private:
	mqtt_handle_task* dequeue_task(void);

private:
	mqtt_client();
	~mqtt_client();
	int init_msqtt_client(void);
	int add_topic_listener(const char* topic, mqtt_message_listener* lnr);
	int remove_topic_listener(const char* topic, mqtt_message_listener* lnr);
	int remove_topic(const char* topic);
	int remove_all_topic(void);
	mqtt_topic* find_topic(const char* topic);
	mqtt_topic* find_topic_unlock(const char* topic);
	union {
		uint32_t _flags;
		struct
		{
			uint32_t starting : 1;
			uint32_t running : 1;
			uint32_t finished : 1;
		}_f;
	};
	static int mqtt_topic_compared(avl_node_t* aa, avl_node_t* bb);
private:
	MQTTAsync _client;
	listnode_t	_topic_list;
	avl_node_t* _topic_tree;
	mqtt_client_listener* _lnr;
	looper_thread*	_mqtt_handle_thread;
	listnode_t	_mqtt_handle_free_list;
	int			_mqtt_handle_num;

	std::string 	_mqtt_client_id;

	mutex _mut;
	mutex _handlermut;
};

}};

#endif

