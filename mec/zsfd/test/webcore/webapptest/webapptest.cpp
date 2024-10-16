#include "webcore/webapp.h"
#include "webcore/webapp-backend-zmq.h"
#include "utils/thread.h"
#include "utils/timer.h"
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "webcore/sysconfig.h"
using namespace zas::utils;
using namespace zas::webcore;

class webapptest_wa_request;
__thread webapptest_wa_request *reqz = nullptr;
__thread webapptest_wa_request *reqh = nullptr;
__thread webapptest_wa_request *reqh1 = nullptr;

int _total_reqh1_cnt = 0;
int _reqnum = 0;

class webapptest_wa_response : public wa_response
{
public:
	webapptest_wa_response(){}
	~webapptest_wa_response(){};
	int on_request(void* data, size_t sz)
	{
		printf("on_request is: \n");
		auto* buf = (char*)data;
		for (int i = 0; i < sz; ++i) {
			putchar(buf[i]);
		}
		printf("\n");
		std::string tmp = "return server hello request";
		response((uint8_t*)tmp.c_str(), tmp.length());
	}
};

class webapptest_factory : public wa_response_factory
{
public:
	wa_response* create_instance(void)
	{
		return new webapptest_wa_response();
	}

	void destory_instance(wa_response *response)
	{
		delete response;
	}
};

class webapptest_wa_request : public wa_request
{
public:
	webapptest_wa_request(const char* uri)
	:wa_request(uri) {}
	~webapptest_wa_request() {}
	int on_reply(void* context, void* data, size_t sz)
	{
//		printf("on_reply[%lu]: %lu\n", sz, get_escaped_ticks());
		printf("%lu ", get_escaped_ticks());fflush(stdout);
		auto* buf = (char*)data;
		for (int i = 0; i < sz; ++i) {
			putchar(buf[i]);
		}
		printf("\n");
		if (reqz) {
			delete reqz;
			reqz = nullptr;			
		}

	}

	void on_finish(void* context, int code)
	{
//		printf("wa_request finished code %d\n", code);
		_total_reqh1_cnt++;
		if (reqh && _total_reqh1_cnt > 100) {
			if (0 == _reqnum) {
				if (reqh) {
					delete reqh;
				}
				reqh = nullptr;
				_reqnum = 1;
			} else {
				if (reqh1) {
					delete reqh1;
				}
				reqh1 = nullptr;
				_reqnum = 0;
			}
			printf("destory reqh\n");
			_total_reqh1_cnt = 0;
		}
	}
};

class webapptest_thread : public thread
{
public:
	webapptest_thread(){}

	~webapptest_thread() {

	}

	int run(void) {
		printf("thread is run\n");
		msleep(1000);
		// webapptest_wa_request req("ztcp://localhost:5556/update?vid=32efd9ca8&seqid=292403");
		// req.send((uint8_t*)"hello world", strlen("hello world"));
		getchar();
	}
	
private:

	ZAS_DISABLE_EVIL_CONSTRUCTOR(webapptest_thread);
};

class webapptest_timer : public timer
{
public:
	webapptest_timer(timermgr* mgr, uint32_t interval)
	: timer(mgr, interval) {
	}

	void on_timer(void) {
#if 0
		if (!reqz) {
			reqz = new webapptest_wa_request("ztcp://localhost:5556/update");
			// auto* reqbase = reinterpret_cast<wa_request*>(req);
			reqz->set_response(true);
			// reqz->add_parameter("vin", "12345678");
			// reqz->add_parameter("test", "987676");
			// std::string tmp = "hello firset request";
			// reqz->send((uint8_t*)tmp.c_str(), tmp.length());
		} else {
			reqz->reset_parameter();
			reqz->add_parameter("vin", "teste2");
			reqz->add_parameter("test", "test3");
			std::string tmp = "hello continue request";
			reqz->send((uint8_t*)tmp.c_str(), tmp.length());
		}
		start();
#else
		if (!reqh) {
			printf("create reqh\n");
			reqh = new webapptest_wa_request("http://10.0.0.125:30080/account/v1/system/login");
		}
		std::string data("{ \"password\": \"123456\", \"phone\": \"13812341234\", \"vin\": \"LSGZG5373GS176598\"}");
		reqh->send((uint8_t*)data.c_str(), data.length(), "POST");
			printf("create reqh finished\n");
		// if (!reqh1) {
		// 	printf("create reqh\n");
		// 	reqh1 = new webapptest_wa_request("http://www.baidu.com");
		// }
		// reqh1->send(nullptr, 0);
		// auto* req = new webapptest_wa_request("http://www.baidu.com");
		// 	printf("create %lu\n", (size_t)req);
		// req->send(nullptr, 0);
		start();
#endif
	}
};


class webapptest_callback : public webapp_callback
{
public:
	webapptest_callback(){}
	~webapptest_callback(){}
	int oninit(void) {
		printf("-----------oninit\n");
		timermgr* tmrmgr = webapp::get_timermgr();
		if (!tmrmgr) {
			printf("-----------oninit, timer mgr is null\n");
			return 0;
		}
		auto* webtmr = new webapptest_timer(tmrmgr, 100);
		webtmr->start();
	}
	int onexit(void) {
		printf("----------onexit\n");
	}
};

int main()
{
	load_sysconfig("file://~/zsfd/test/webcore/sysconfig-webcore.json");
	auto* wa = webapp::inst();
	webapp_zmq_backend backend;
	wa->set_backend(&backend);
	webapptest_callback callback;
	wa->set_app_callback(&callback);
	webapptest_factory factory;
	wa->set_response_factory(&factory);
	// auto* thread = new webapptest_thread();
	// thread->start();
	// thread->release();
	wa->run(nullptr);
	getchar();
	msleep(100);
	return 0;
}
