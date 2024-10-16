#include "webcore/webapp.h"
#include "webcore/webapp-backend-zmq.h"
#include "webcore/sysconfig.h"
#include "utils/thread.h"
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "utils/timer.h"
using namespace zas::utils;
using namespace zas::webcore;

class webapptest_wa_request : public wa_request
{
public:
	webapptest_wa_request(const char* uri)
	:wa_request(uri){}
	~webapptest_wa_request(){}
	int on_reply(void* context, void* data, size_t sz);
};

webapptest_wa_request * req = nullptr;
class webapptest_wa_response : public wa_response
{
public:
	webapptest_wa_response(){}
	~webapptest_wa_response(){};
	int on_request(void* data, size_t sz)
	{
		printf("server on_request: \n");
		auto* buf = (char*)data;
		for (int i = 0; i < sz; ++i) {
			putchar(buf[i]);
		}
		printf("\n");
		// reply((void*)"return hello", strlen("return hello"));
		if (!req) {
			req = new webapptest_wa_request("ztcp://localhost:5555/update?vid=32efd9ca8&seqid=292403");
			req->set_response(true);
		}
		msleep(200);
		std::string tmp = "server hello request";
		req->send((uint8_t*)tmp.c_str(), tmp.length());
	}
};

int webapptest_wa_request::on_reply(void* context, void* data, size_t sz)
{
	printf("server on_reply: \n");
	auto* buf = (char*)data;
	for (int i = 0; i < sz; ++i) {
		putchar(buf[i]);
	}
	printf("\n");
	webapptest_wa_response *rpy = new webapptest_wa_response();
	std::string tmp = "server return hello request";
	rpy->response((void*)tmp.c_str(), tmp.length());
}

class webapptest_thread : public thread
{
public:
	webapptest_thread(){}

	~webapptest_thread() {

	}

	int run(void) {
		printf("thread is run\n");
		getchar();
	}
	
private:

	ZAS_DISABLE_EVIL_CONSTRUCTOR(webapptest_thread);
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

class webapptest_callback : public webapp_callback
{
public:
	webapptest_callback(){}
	~webapptest_callback(){}
	int oninit(void) {
		printf("-----------oninit\n");
	}
	int onexit(void) {
		printf("----------onexit\n");
	}
};

int main()
{
	load_sysconfig("file://~/zsfd/test/webcore/sysconfig-webserver.json");
	auto* wa = webapp::inst();
	webapp_zmq_backend backend;
	wa->set_backend(&backend);
	webapptest_callback callback;
	wa->set_app_callback(&callback);
	webapptest_factory factory;
	wa->set_response_factory(&factory);
	wa->run("testservice");
	auto* thread = new webapptest_thread();
	thread->start();
	thread->release();
	getchar();
	getchar();
	if (req) {
		delete req;
	}

	return 0;
}
