
#ifndef __CXX_ZAS_RPC_MANAGER_H__
#define __CXX_ZAS_RPC_MANAGER_H__

#include "std/smtptr.h"
#include "utils/coroutine.h"
#include "utils/evloop.h"
#include "utils/mutex.h"
#include "utils/uri.h"
#include "mware/rpc/codegen-helper.h"

namespace google {
namespace protobuf {
class Message;
}}

namespace zas {
namespace mware {
namespace rpc {

zas_interface MWARE_EXPORT service
{
	/**
	 * @brief it will be called once, when service created
	 * @param  cor              
	 */
	virtual void on_create(utils::coroutine* cor) = 0;

	/**
	 * @brief it will be called when startservice called
	 * @param  cor             
	 */
	virtual void on_start(utils::coroutine* cor) = 0;

	/**
	 * @brief it will be called when current service distoryd
	 * @param  cor              
	 */
	virtual void on_destroy(utils::coroutine* cor) = 0;

	/**
	 * @brief it will be called when client connect to services
	 * @param  cor             
	 * @param  cli              connected client
	 * @return int 
	 */
	virtual int on_bind(utils::coroutine* cor, utils::evlclient cli) = 0;

	/**
	 * @brief it will be called when client disconnect to services
	 * @param  cor             
	 * @param  cli              disconnected client
	 * @return int 
	 */
	virtual void on_unbind(utils::coroutine* cor, utils::evlclient cli) = 0;

	/**
	 * @brief regist all method of current service
	 * @return int 			0:success
	 */
	virtual int register_all(void) = 0;
};

typedef void* rpcmethod_handler;

struct rpcproxy_static_data
{
	void* inst_id_tree;
	void* host_client;
	const char* package_name;
	const char* service_name;
	const char* inst_name;
	const char* class_name; 
	utils::mutex list_mut;
	utils::mutex access_mut;
};

class MWARE_EXPORT rpcproxy
{
public:
	rpcproxy() = delete;
	~rpcproxy() = delete;
	int addref();
	int release();

public:
	/**
	 * @brief proxy_data instance is vaild
	 * @return true vaild
	 * @return false 
	 */
	bool is_vaild(void);

	/**
	 * @brief get proxy_base instance id
	 * @return void* 	instanceid
	 */
	void* get_instid(void);

	/**
	 * @brief get proxy_base class name
	 * @return const char* 
	 */
	const char* get_class_name(void);

	/**
	 * @brief invoke service method
	 * @param  uuid             method uuid
	 * @param  inparam           inparam
	 * @param  inout_param       inoutparam
	 * @param  method_id         method id by user
	 * @param  method_name      method name
	 * 
	 * methodid is used for java
	 * method_name is used by traceinfo
	 */

	// jimview: new: for reference
	static rpcproxy* invoke(void* instid, uint128_t& uuid,
		google::protobuf::Message* inparam,
		google::protobuf::Message* inout_param,
		rpcproxy_static_data *data,
		uint methodid = 0,  const char *method_name = NULL);

	/**
	 * @brief get the singleton instance of proxy_base
	 * @param  clsname          singleton class name
	 * @return true 			success
	 * @return false 			failure
	 */
	static rpcproxy* get_singleton_instance(rpcproxy_static_data *data);

	/**
	 * @brief get instance of class & inst_name
	 * if current proxy_data has instance, new instance
	 * instead of current instance
	 * @param  clsname          instance class name
	 * @param  inst_name        instance name
	 * @param  param            create instance param
	 * @return true 
	 * @return false 
	 */
	static rpcproxy* get_instance(google::protobuf::Message* inparam, 
		rpcproxy_static_data *data);
	
	/**
	 * @brief find or create rpcproxy by instid
	 * @param  instid           rpcproxy instid
	 * @param  data             rpcproxy data
	 * @return rpcproxy* 
	 * rpcproxy* must addref before use
	 */
	static rpcproxy* from_instid(void* instid, 
		rpcproxy_static_data *data);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(rpcproxy);
};


class MWARE_EXPORT rpcinterface_object
{
public:
	rpcinterface_object() = delete;
	~rpcinterface_object() = delete;
	int addref();
	int release();

	/**
	 * @brief add class method
	 * @param  uuid             mehtod uuid
	 * @param  inparam          mehtod inparam
	 * @param  inout_param      methond inoutparam
	 * @param  hdr              method function
	 * @return int 				0:success
	 */
	int add_method(const uint128_t& uuid, pbf_wrapper inparam,
		pbf_wrapper inout_param, rpcmethod_handler hdr);
	void set_singleton(bool singleton);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(rpcinterface_object);
};

typedef zas::smtptr<rpcinterface_object> rpcinterface;

//global datapool
class MWARE_EXPORT rpcevent
{
public:
	rpcevent();
	~rpcevent();

	virtual void on_triggered(pbf_wrapper event);
	void force_trigger_last_event(const char *event_name);

protected:

	void start_listen(const char *event_name, pbf_wrapper event_data);
	void end_listen(const char *event_name);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(rpcevent);
};

void MWARE_EXPORT trigger_event(const char *event_name, google::protobuf::Message* data);


/**
 * @brief rpc service object
 */
class MWARE_EXPORT rpchost_object
{
public:
	rpchost_object() = delete;
	~rpchost_object() = delete;
	int addref();
	int release();

	/**
	 * @brief register class interface
	 * @param  clsname          class name
	 * @param  factory          class create & desotry function
	 * @param  flags            is singleton. 1: singleton
	 * @return rpcinterface  	interface object
	 */
	rpcinterface register_interface(const char* clsname,
		void* factory, pbf_wrapper param, uint32_t flags = 0);

	/**
	 * @brief get the interface by class name
	 * @param  clsname          class name
	 * @return rpcinterface 	
	 */
	rpcinterface get_interface(const char *clsname);

	int add_instid(const char* clsname, void* inst_id);
	int release(void* inst_id);

		
	/**
	 * @brief keep instance is not relase,
	 * when first release after call this function.
	 * @param  instid          object inst id
	 * @return int  		0:success
	 */
	int keep_instance(void* instid);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(rpchost_object);
};
typedef zas::smtptr<rpchost_object> rpchost;

struct rpcobservable_data
{
	std::string pkg_name;
	std::string inst_name;
};

/**
 * @brief for rpc observable fuciton
 */
class MWARE_EXPORT rpcobservable
{
public:
	rpcobservable() = delete;
	~rpcobservable() = delete;

	/**
	 * @brief get rpc observable object
	 * @return rpcobservable* 
	 */
	static rpcobservable* inst();

	/**
	 * @brief register class of observable type
	 * @param  clsname          class name
	 * @return rpcinterface 	interface object
	 */
	rpcinterface register_observable_interface(const char* clsname);

	/**
	 * @brief get the observable interface object of class
	 * @param  clsname          My Param doc
	 * @return rpcinterface 	interface object
	 */
	rpcinterface get_observable_interface(const char *clsname);

	/**
	 * @brief get the observable data 
	 * @return rpcobservable_data* 
	 */
	rpcobservable_data* get_observable_data(void);

	/**
	 * @brief add class instance of observable type
	 * @param  clsname          class name 
	 * @param  inst             class instance point
	 * @return int 				0:success
	 */
	int add_observable_instance(const char *clsname, void *inst);

	/**
	 * @brief release observable instance
	 * it must be called before inst delete
	 * @param  inst             class instance point
	 */
	void release_observable_instance(void *inst);

	/**
	 * @brief add observable proxy class instance
	 * @param  cliname          client name of evlclient
	 * @param  inst_name        inst name of evlclient
	 * @param  inst             observable class instance point
	 * @return int 				0:success
	 */
	int add_client_instance(const char *cliname, const char *inst_name,
		void *inst);

	/**
	 * @brief release client by inst
	 * @param  inst             observable class instance point
	 */
	void release_client_instance(void *inst);


	/**
	 * @brief invoke observable method
	 * @param  inst             observable class instance point
	 * @param  clsname          class name of method
	 * @param  uuid             method uuid
	 * @param  inparam            input param
	 * @param  inout_param           output param
	 * @param  methodid         method id
	 * @param  method_name      method name
	 */

	// jimview: todo
	void invoke_observable_method(void* inst_id, const char* clsname,
		const uint128_t& uuid,
		google::protobuf::Message* inparam,
		google::protobuf::Message* inout_param,
		uint32_t methodid = 0, const char* method_name = nullptr);

	void invoke_observable_method(void* inst_id, const char* clsname,
		const uint128_t& uuid, void* input, size_t insize,
		std::string &inout, size_t &inoutsize, uint32_t methodid = 0,
		const char* method_name = nullptr);

	void invoke_observable_method(void* inst_id, const char* cli_name,
		const char* inst_name, const char* clsname,
		const uint128_t& uuid, void* input, size_t insize,
		std::string &inout, size_t &inoutsize, uint32_t methodid= 0,
		const char* method_name = nullptr);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(rpcobservable);
};

/**
 * @brief rpc server, it must be call in syssvr
 * other process can't call this function
 */
class MWARE_EXPORT rpcserver_object
{
public:
	rpcserver_object() = delete;
	~rpcserver_object() = delete;
	int addref();
	int release();

	/**
	 * @brief load services config file
	 * @param  file            config gile of services
	 * @return int 			0:success
	 */
	int load_config_file(zas::utils::uri& file);
	
	/**
	 * @brief run rpc server.
	 * evloop must be run, when run server
	 * @return int 		0:success
	 */
	int run();

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(rpcserver_object);
};

typedef zas::smtptr<rpcserver_object> rpcserver;



zas_interface MWARE_EXPORT  bridge_service
{
	virtual int on_recvdata(utils::evlclient cli, void* inst_id,
		uint32_t seqid, std::string &classname, uint128_t* method_uuid,
		uint8_t flag, void* indata, size_t insize,
		void* inoutdata, size_t inoutsize) = 0;
	virtual int on_eventdata(std::string &eventid, uint8_t action) = 0;
};

/**
 * @brief rpc service object
 */
class MWARE_EXPORT rpcbridge_object
{
public:
	rpcbridge_object() = delete;
	~rpcbridge_object() = delete;
	int addref();
	int release();

	int senddata(utils::evlclient evl, void* inst_id, uint32_t seqid,
		uint32_t result, uint8_t flag, void* data, size_t sz);
	int sendevent(std::string &eventid, void* data, size_t sz);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(rpcbridge_object);
};
typedef zas::smtptr<rpcbridge_object> rpcbridge;

/**
 * @brief bridge service create function of service'dll
 */
typedef bridge_service* (*create_bridge_service)(const char* pkg_name,
	const char* service_name, const char* inst_name);
typedef void (*destory_bridge_service)(bridge_service* svc);


/**
 * @brief service create function of service'dll
 */
typedef service* (*create_service)(void);
typedef void (*destory_service)(service* svc);

class MWARE_EXPORT rpcmgr
{
public:
	rpcmgr() = delete;
	~rpcmgr() = delete;

	static rpcmgr* inst();

	/**
	 * @brief load service
	 * @param  service_name     service name
	 * @param  inst_name        service instance name
	 * @param  executive        dll of service
	 * @return rpc_host 
	 */
	rpchost load_service(const char* service_name, const char* inst_name,
		const char* executive, const char* startmode);

	/**
	 * @brief get loaded service 
	 * @param  service_name     service name
	 * @param  inst_name        service instance name
	 * @return rpc_host 
	 */
	rpchost get_service(const char* service_name, 
		const char* inst_name);

	/**
	 * @brief get rpc server
	 * @return zrpc_server 
	 */
	rpcserver get_server(void);

	/**
	 * @brief start service, on_start will be called
	 * @param  pkgname          service package name
	 * @param  service_name     service name
	 * @param  inst_name        service instance name
	 * @return int 				0: success
	 */
	int start_service(const char* pkgname, const char* service_name,
		const char* inst_name = nullptr);

	/**
	 * @brief stop current client in service, on_bind will be called
	 * @param  pkgname          service package name
	 * @param  service_name     service name
	 * @param  inst_name        service instance name
	 * @return int 				0: success
	 */
	int stop_service(const char* pkgname, const char* service_name,
		const char* inst_name);

	/**
	 * @brief terminate service of service_name&inst_name
	 * @param  pkgname          service package name
	 * @param  service_name     service name
	 * @param  inst_name        service instance name
	 * @return int 				0: success
	 */
	int terminate_service(const char* pkgname, const char* service_name,
		const char* inst_name);

	/**
	 * @brief register loading function of service's implementation.
	 * @param  service_name    	service name
	 * @param  inst_name    	service instance
	 * @param  create_svc	    create function of service's implementation
	 * @param  destory_svc      destory function of service's implementation
	 * @return int 
	 */
	int register_service(const char* service_name, const char* inst_name,
		create_service create_svc, destory_service destory_svc);
	/**
	 * @brief get service's implementation.
	 * @param  service_name    	service name
	 * @param  inst_name    	service instance
	 * @return int 
	 */
	service* get_service_impl(const char* service_name, const char* inst_name);

	/**
	 * @brief load bridge service
	 * @param  pkg_name         service package
	 * @param  service_name     service name
	 * @return rpcbridge 
	 */
	rpcbridge load_bridge(const char* pkg_name, const char* service_name,
		const char* inst_name);

	rpcbridge get_bridge(const char* pkg_name,
		const char* service_name, const char* inst_name);

	/**
	 * @brief 
	 * @param  create           My Param doc
	 * @param  destory          My Param doc
	 * @return int 
	 */
	int register_bridge(create_bridge_service create,
		destory_bridge_service destory);
};

}}} // end of namespace zas::mware::rpc
#endif  /*__CXX_ZAS_RPC_MANAGER_H__*/