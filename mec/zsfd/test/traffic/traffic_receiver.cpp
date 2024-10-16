
#include "inc/traffic_receiver.h"

#include <string>
#include <arpa/inet.h>

#include "utils/thread.h"
#include "utils/http.h"
#include "utils/timer.h"

#include "inc/rmsocket.h"

namespace zas {
namespace traffic {

using namespace zas::utils;


class singnal_receive_thread : public thread
{
public:
	singnal_receive_thread(traffic_receiver* receiver)
	: _receiver(receiver) {}

	~singnal_receive_thread() {

	}

	int run(void) {
		_receiver->receive_traffic_singnal();
	}
	
private:
	traffic_receiver* _receiver;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(singnal_receive_thread);
};

class singnal_handle : public looper_task
{
public:
	singnal_handle(){}
	virtual ~singnal_handle(){}
	void do_action(void);
	std::string 	data;
	
	
private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(singnal_handle);
};

void singnal_handle::do_action(void)
{
	http shttp;
	shttp.open("POST", "http://118.178.179.38:8080/signal/upload");
	shttp.set_request_header("Content-Type", "application/json");
	// printf("senddata: %s\n", data.c_str());
	int ret = shttp.send(data.c_str());
	int status = shttp.status();
	if (ret || (status != 200)) {
		printf("http send error %d, status %d\n", ret, status);
		return;
	}
	if (shttp.response_type() != response_type_json) {
		printf("http receive error %d\n", shttp.response_type());
		return;
	}

	jsonobject& resp = json::parse(shttp.response_text());
	std::string data ;
	resp.serialize(data, false);
	resp.release();
	// printf("singnal return data %s\n", data.c_str());
}

traffic_receiver::traffic_receiver()
: _buff(NULL)
{
	_buff = create_fifobuffer();
	_task_thread.start();
}

traffic_receiver::~traffic_receiver()
{
	if (_buff)
		delete _buff;
	_buff = NULL;
	_task_thread.stop();
}

int traffic_receiver::start_traffic_receiver(void)
{
	auto *receiver = new singnal_receive_thread(this);
	receiver->start();
	receiver->release();
}

int traffic_receiver::receive_traffic_singnal(void)
{
	rmsocket rec_sock;
	rec_sock.init_udp();
	rec_sock.bind(8000);
	uint16_t buffer[256];
	// auto* sock_addr = rec_sock.get_addr();
	// if (!sock_addr) return -ENOTAVAIL;
	printf("traffic_receiver starting\n");
	// sockaddr_in sock_addr;
	// socklen_t sock_len = sizeof(sock_addr);
	// memset(&sock_addr, 0, sock_len);
	sockaddr_in sock_addr;
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(8000);
    sock_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	socklen_t sock_len = sizeof(sock_addr);
	int ret = 0;
	unsigned int len = 256;
	_buff->drain();
	while(1) {
		ret = rec_sock.recvfrom((char*)buffer, len, 
			(sockaddr*)&sock_addr, &sock_len);
		if (ret < 0) {
			printf("recvfrom error \n");
			exit(1);
		}

		_buff->append((void*)buffer, ret);

		if (move_to_package_start()) continue;

		if (check_traffice_info()) {
			continue;
		}
	} 
	return 0;
}

#define SINGNAL_HEADER_SIZE	(7)
#define SINGNAL_HEADER_MAGIC1	(0XFA)
#define SINGNAL_HEADER_MAGIC2	(0XFB)

size_t traffic_receiver::move_to_package_start()
{
	size_t bufsz = _buff->getsize();
	if (bufsz < 7)
		return 1;
	uint8_t magic[2];
	size_t n = _buff->peekdata(0, magic, sizeof(uint8_t)*2);
	if (n != sizeof(uint8_t)*2) return 2;
	if (magic[0] == SINGNAL_HEADER_MAGIC1 && magic[1] == SINGNAL_HEADER_MAGIC2)
		return 0;

	const size_t chksz = 512;
	uint8_t buf[chksz];
	size_t start = 0;

	_buff->seek(0, seek_set);
	while (bufsz)
	{
		size_t nread = (chksz > bufsz) ? bufsz : chksz;
		if (nread < sizeof(uint16_t)) return 2;
		n = _buff->read(buf, nread);
		assert(n == nread);

		for (size_t i = 0, end = nread - (sizeof(uint16_t) - 1); i < end; ++i)
		{
			if (buf[i] == SINGNAL_HEADER_MAGIC1 
				&& buf[i + 1] == SINGNAL_HEADER_MAGIC2) {
				_buff->discard(start + i);
				return 0;
			}
		}
		start += nread;
		bufsz -= nread;
	}
	// there is no package found in the fifobuffer
	_buff->drain();
	return 3;
}

int traffic_receiver::check_traffice_info()
{
	size_t bufsz = _buff->getsize();
	if (bufsz < 7) return 1;

	uint8_t version = 0;
	size_t n = _buff->peekdata(2, &version, sizeof(uint8_t));

	if (n != sizeof(uint8_t)) {
		printf("checkinfo buffer read error\n");
		_buff->drain();
		return -ELOGIC;
	}

	if (version > 1) {
		printf("version not support %d\n", version);
		_buff->drain();
		return -ENOTSUPPORT;
	}
	uint8_t len = 0;
	n = _buff->peekdata(4, &len, sizeof(uint8_t));
	
	uint8_t data_size = len + 6;
	if (bufsz < data_size) return 2;

	if (data_size > 144) {
		_buff->drain();
		return 3;
	}

	uint8_t tmp;
	_buff->peekdata(0, _buffer, data_size);

	for (int i = 3; i < data_size - 1; i++) {
		version = version ^ _buffer[i];
	}
	if (version != _buffer[data_size - 1]) {
		printf("singnal check code error\n");
		_buff->drain();
		return -ENOTAVAIL;
	}

	long timestamp = *((uint32_t*)(&_buffer[6]));
	timestamp *= 1000;
	timestamp += _buffer[10] * 10; 
	jsonobject& result = json::new_object();
	result.new_number("timestamp", timestamp);
	jsonobject& tralights = result.new_array("lightData");

	for (int i = 11; i < (data_size - 3); ) {
		jsonobject& lightdata = tralights.new_item();
		lightdata.new_number("lightNum", _buffer[i]);
		lightdata.new_number("prediction", _buffer[i + 1]);
		if (0x00 != _buffer[i + 1]) {
			i += 2;
			continue;
		}

		lightdata.new_number("redCycle",
			_buffer[i + 2] * 10);
		lightdata.new_number("yellowCycle",
			_buffer[i + 3] * 10);
		lightdata.new_number("greenCycle",
			_buffer[i + 4] * 10);
		lightdata.new_number("currentColor", _buffer[i + 5]);
		lightdata.new_number("leftTime",
			_buffer[i + 6] * 10);
		i += 7;
	}
	singnal_handle *handle = new singnal_handle();
	result.serialize(handle->data, false);
	handle->addto(&_task_thread);
	result.release();
	_buff->discard(data_size);
	return 0;
}


}};	//zas::traffic
