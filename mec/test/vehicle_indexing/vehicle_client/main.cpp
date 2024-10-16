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
#include "std/zasbsc.h"
#include "utils/uri.h"

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


class client_task {
public:
	client_task()
		: ctx_(1),
			client_socket_(ctx_, ZMQ_DEALER)
	{}

	void start(std::string& vehicle_id, std::string& uri, size_t &count) {
		// set zmq linger & high water
		int linger = 0;
		int sendbuf = 0;
		client_socket_.setsockopt(ZMQ_RCVHWM, &sendbuf, sizeof(int));
		client_socket_.setsockopt(ZMQ_SNDHWM, &sendbuf, sizeof(int));
		client_socket_.setsockopt (ZMQ_LINGER, &linger, sizeof (linger));

		//set client identify
		char identity[10] = {};
		sprintf(identity, "%04X-%04X", within(0x20000), within(0x30000));
		printf("%s\n", identity);
		client_socket_.setsockopt(ZMQ_IDENTITY, identity, strlen(identity));

		client_socket_.connect(uri.c_str());


		std::string header_uri = "ztcp://vehicle-snapshot:8236/update?vid=32efd9ca8&seqid=292403";
		size_t len = header_uri.length() + 1
			+ sizeof(basicinfo_frame_uri)
			+ sizeof(basicinfo_frame_content);
		
		char* pdata = (char*)alloca(len);
		auto* frame_uri = (basicinfo_frame_uri*)pdata;
		frame_uri->uri_length = header_uri.length();
		strcpy(frame_uri->uri, header_uri.c_str());
		auto* frame_content = (basicinfo_frame_content*)(pdata + header_uri.length() + 1 +sizeof(basicinfo_frame_uri));
		frame_content->sequence_id = 1112;
		frame_content->attr = 0;
		frame_content->body_size  = -1;
		frame_content->body_frames = -1;
		// second message

		zmq::message_t messagerec;
		printf("send vehicle is %s\n", vehicle_id.c_str());
		size_t send_cnt = 0;
		size_t recv_cnt = 0;
		int waittime = 0;
		long starttime = gettick_millisecond();

		for (int i = 0; i < count + 1; i++) {
			if (send_cnt < count) {
				zmq::message_t message(pdata, len);
				std::string test_message = "message id :";
				test_message += std::to_string(send_cnt);
				zmq::message_t message1(test_message.c_str(), test_message.length());		
				zmq::message_t emptymsg(0);
				client_socket_.send(emptymsg, ZMQ_SNDMORE);
				client_socket_.send(message, ZMQ_SNDMORE);
				client_socket_.send(message1);
				send_cnt++;
				if (send_cnt == count) {
					i = recv_cnt + 1;
					waittime = -1;
					printf("send %d message, time is %d\n", send_cnt, gettick_millisecond() - starttime);
				}
			}

			try {
				zmq::pollitem_t items[] = {
				{ client_socket_, 0, ZMQ_POLLIN, 0 } };

				int more;	//  Multipart detection
				zmq::poll(items, 1, waittime);
				if (items[0].revents & ZMQ_POLLIN) {
					recv_cnt ++;
					if (recv_cnt % 1000 == 0) {

						printf("recv num %d\n", recv_cnt);
					}
					while(1){
						client_socket_.recv(&messagerec);

							std::string data(static_cast<char*>(messagerec.data()), messagerec.size());
							printf("message is %s\n", data.c_str());

						size_t more_size = sizeof (more);
						client_socket_.getsockopt(ZMQ_RCVMORE, &more, &more_size);
						if (!more) {
							break;      //  Last message part
						}
					}
				}
			}
			catch (std::exception &e) {}	
		}
		printf("revc %d message, time is %d\n", recv_cnt, gettick_millisecond() - starttime);
	}
/*
	void start1(std::string& vehicle_id) {
		// set zmq linger & high water
		int linger = 0;
		int sendbuf = 0;
		client_socket_.setsockopt(ZMQ_RCVHWM, &sendbuf, sizeof(int));
		client_socket_.setsockopt(ZMQ_SNDHWM, &sendbuf, sizeof(int));
		client_socket_.setsockopt (ZMQ_LINGER, &linger, sizeof (linger));

		//set client identify
		char identity[10] = {};
		sprintf(identity, "%04X-%04X", within(0x20000), within(0x20000));
		printf("%s\n", identity);
		client_socket_.setsockopt(ZMQ_IDENTITY, identity, strlen(identity));

		// connect vehicle indexing service
		// client_socket_.connect("tcp://222.71.128.20:30555");
		client_socket_.connect("tcp://10.0.0.101:30555");
		// send message, message struct
		// first message :	empty
		// second message:	vehicle id
		// third message :	testdata
		
		// first message
		zmq::message_t emptymsg(0);
		client_socket_.send(emptymsg, ZMQ_SNDMORE);

		std::string uri = "ztcp://vehicle-snapshot:8236/update?vid=32efd9ca8&seqid=292403";
		size_t len = uri.length() + 1
			+ sizeof(basicinfo_frame_uri)
			+ sizeof(basicinfo_frame_content);
		

		char* pdata = (char*)alloca(len);
		auto* frame_uri = (basicinfo_frame_uri*)pdata;
		frame_uri->uri_length = uri.length();
		strcpy(frame_uri->uri, uri.c_str());
		auto* frame_content = (basicinfo_frame_content*)(pdata + uri.length() + 1 +sizeof(basicinfo_frame_uri));
		frame_content->sequence_id = 1112;
		frame_content->attr = 0;
		frame_content->body_size  = -1;
		frame_content->body_frames = -1;
		// second message
		zmq::message_t message(pdata, len);
		printf("send vehicle is %s\n", vehicle_id.c_str());
		client_socket_.send(message, ZMQ_SNDMORE);
		// third message
		zmq::message_t message1("test hello workd", strlen("test hello workd"));
		client_socket_.send(message1);

		try {
			zmq::pollitem_t items[] = {
			{ client_socket_, 0, ZMQ_POLLIN, 0 } };

			zmq::message_t messagerec;
			int more;	//  Multipart detection
			zmq::poll(items, 1, -1);
			if (items[0].revents & ZMQ_POLLIN) {
				while(1){
					client_socket_.recv(&messagerec);
					std::string data(static_cast<char*>(messagerec.data()), messagerec.size());
					printf("message is %s\n", data.c_str());
					size_t more_size = sizeof (more);
					client_socket_.getsockopt(ZMQ_RCVMORE, &more, &more_size);
					if (!more) {
						break;      //  Last message part
					}
				}
			}
		}
		catch (std::exception &e) {}	
	}*/
private:
	zmq::context_t ctx_;
	zmq::socket_t client_socket_;
};


//  The main thread simply starts several clients and a server, and then
//  waits for the server to finish.

int main(int argc, char* argv[])
{
	std::string cli_name = "default_car";
	//  std::string uri = "tcp://222.71.128.20:30555";
	// std::string uri = "tcp://10.0.0.252:30555";
	std::string uri = "tcp://localhost:5555";
	size_t count = 10000;

	if (argc >= 2) {
		printf("running with client id.\n");
		cli_name = argv[1];
	}
	if (argc >= 3) {
		printf("running with client id.\n");
		uri = argv[2];
	}
	if (argc >= 4) {
		printf("running with client id.\n");
		count = atoi(argv[3]);
	}

	client_task ct1;
	// ct1.start1(cli_name);
	ct1.start(cli_name, uri, count);
	getchar();
	return 0;	
}
