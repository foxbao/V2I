//  Asynchronous client-to-server (DEALER to ROUTER)
//
//  While this example runs in a single process, that is to make
//  it easier to start and stop the example. Each task has its own
//  context and conceptually acts as a separate process.

#include <vector>
#include <thread>
#include <memory>
#include <functional>

#include "zmsg.h"
#include "webcore/webapp-def.h"
#include "indexing-def.h"

#include "webcore/logger.h"

using namespace zas::vehicle_indexing;
using namespace zas::webcore;

//  This is our client task class.
//  It connects to the server, and then sends a request once per second
//  It collects responses as they arrive, and it prints them out. We will
//  run several client tasks in parallel, each with a different random ID.
//  Attention! -- this random work well only on linux.
long gettick_millisecond(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

#define CLIENT_SEND_MAX_MESSAGE (1000000)

class service_task {
public:
	service_task()
		: ctx_(1),
			service_socket_(ctx_, ZMQ_DEALER)
	{}

	void start(std::string& svr_uri) {
		// set zmq linger & high water
		int linger = 0;
		int sendbuf = 0;
		service_socket_.setsockopt(ZMQ_RCVHWM, &sendbuf, sizeof(int));
		service_socket_.setsockopt(ZMQ_SNDHWM, &sendbuf, sizeof(int));
		service_socket_.setsockopt (ZMQ_LINGER, &linger, sizeof (linger));

		//set client identify
		char identity[10] = {};
		sprintf(identity, "%04X-%04X", within(0x20000), within(0x20000));
		printf("%s\n", identity);
		service_socket_.setsockopt(ZMQ_IDENTITY, identity, strlen(identity));

		// connect vehicle indexing service
		service_socket_.connect(svr_uri.c_str());

		// send message, message struct
		// first message :	empty
		// second emssage : INITIALIZE
		// third message:	vehicle id
		// fourth message :	vehilce count
		
		// first message
		zmq::message_t emptymsg(0);
		service_socket_.send(emptymsg, ZMQ_SNDMORE);

		// second message
		zmq::message_t message1("INITIALIZE", strlen("INITIALIZE"));
		service_socket_.send(message1, ZMQ_SNDMORE);

		// third message
		zmq::message_t message(svr_uri.c_str(), svr_uri.length());
		printf("send vehicle is %s\n", svr_uri.c_str());
		service_socket_.send(message, ZMQ_SNDMORE);

		// fourth message
		int vehicle_cnt = 2;
		std::string veh_cnt = std::to_string(vehicle_cnt);
		zmq::message_t msgcnt(veh_cnt.c_str(), veh_cnt.length());
		service_socket_.send(msgcnt);

		int recv_cnt = 0;
		zmq::pollitem_t items[] = {
		{ service_socket_, 0, ZMQ_POLLIN, 0 } };
		while (1) {
			try {
				zmq::poll(items, 1, -1);
				if (items[0].revents & ZMQ_POLLIN) {
					zmsg* msg = new zmsg(service_socket_);
					msg->dump();
					if (msg->parts() < 2) {
						printf("recv message error\\n");
						continue;
					}
					std::string testempty = msg->get_part(0);
					if (testempty.length() == 0) {
						printf("result from indexing is %s\n", msg->get_part(1).c_str());
						continue;
					}
					std::string type = msg->get_part(0);
					std::string retrunstr = msg->get_part(msg->parts() - 1);
					if (!type.compare("MASTER")) {
						// for client
						// std::string senddata = svr_uri;
						// senddata += retrunstr;
						// msg->body_set((char*)(senddata.c_str()), senddata.length());
						//for digtwins
						std::string data = msg->body(0);
						char* pdata = (char*)data.c_str();
						auto* hdr = (basicinfo_frame_uri*)pdata;
						printf("uri is [%u]: %s\n", hdr->uri_length, hdr->uri);
						auto* content = (basicinfo_frame_content*)(pdata + hdr->uri_length + sizeof(basicinfo_frame_uri));
						printf("content is %d, %d, %d, %d, %d\n", content->sequence_id, content->timeout,
						content->a.pattern,
						content->body_frames, content->body_size);
						content->a.type = msgtype_reply;
						msg->body_set((char*)(data.c_str()), data.length());
						msg->push_back((char*)("server reply"), strlen("server reply"));
						msg->send(service_socket_);
					} else {
						printf("message type is SALVE\n");
						std::string data = msg->body(0);
						char* pdata = (char*)data.c_str();
						auto* hdr = (basicinfo_frame_uri*)pdata;
						auto* content = (basicinfo_frame_content*)(pdata + hdr->uri_length + 1 + sizeof(basicinfo_frame_uri));
						printf("content is %d, %d, %d, %d, %d\n", content->sequence_id, content->timeout, content->attr,
						content->body_frames, content->body_size);
					}
					delete msg;
				}
			}
			catch (std::exception &e) {}
		}
	}
private:
	zmq::context_t ctx_;
	zmq::socket_t service_socket_;
};


//  The main thread simply starts several clients and a server, and then
//  waits for the server to finish.

int main(int argc, char* argv[])
{
	std::string svr_uri = "tcp://localhost:5556";
	if (argc >= 2) {
		printf("running with client uri.\n");
		svr_uri = argv[1];
	}
	service_task ct1;
	ct1.start(svr_uri);
	getchar();
	return 0;	
}
