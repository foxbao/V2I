
#include "inc/traffic_sender.h"
#include "inc/rmsocket.h"

#include <arpa/inet.h>
#include <error.h>
#include <errno.h>
#include <time.h>

#include "utils/thread.h"
#include "utils/timer.h"

namespace zas {
namespace traffic {

using namespace zas::utils;

traffic_sender::traffic_sender()
: _size(0), _lefttime(30), _status(0), _timestamp(NULL), _timestampms(NULL)
{
	uint16_t* start = (uint16_t*)_buffer;
	_buffer[0] = 0XFA;
	_buffer[1] = 0XFB;
	_buffer[2] = 1; 
	_buffer[3] = 0x03;
	_buffer[5] = 0;
	_timestamp = (uint32_t*)&(_buffer[6]);
	_timestampms = &(_buffer[10]);
}

traffic_sender::~traffic_sender()
{

}

int traffic_sender::senddata(void)
{
	rmsocket rec_sock;
	rec_sock.init_udp();
	printf("traffic_sender starting\n");
	sockaddr_in sock_addr;
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(8000);
    sock_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	socklen_t sock_len = sizeof(sock_addr);
	int ret = 0;
	unsigned int len = 256;
	while(1) {
		refreshdata();	
		printf("send data, size %d\n", _size);
		uint16_t *tmp = (uint16_t*)_buffer;
		ret = rec_sock.sendto((char*)_buffer, _size, 
			(sockaddr*)&sock_addr, &sock_len);

		if (ret != _size) {
			printf("send error error \n");
		}
		msleep(1000);
	} 
}

int traffice_start[12] =	{11, 22, 33, 44,
							 55, 66, 77, 88, 
							 99, 110, 121, 132};
int traffice_num[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

int traffice_time[4] = {30, 5, 15, 5};
uint8_t light_status[9] = {0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,0xFF};

static int tra_time = 0;
static int tra_diret = 0;

int traffic_sender::refreshdata()
{
	if (0 == _lefttime) {
		tra_time++;
		if (tra_time == 4)
			tra_diret = (++tra_diret) % 2;
		tra_time = tra_time % 4;
		_lefttime = traffice_time[tra_time];
	}
	*_timestamp = time(NULL);
	*_timestampms = (gettick_millisecond() % 1000) / 10;
	printf("timestamp %ld, ms %ld\n", *_timestamp, *_timestampms);
	_size = 11;
	for (int i = 0; i < 12; i++) {
		if (0 == traffice_num[i])
			break;
		_buffer[traffice_start[i]] = traffice_num[i];
		_buffer[traffice_start[i] + 1] = 0;
		_buffer[traffice_start[i] + 2] = 300 / 256;
		_buffer[traffice_start[i] + 3] = 300 % 256;
		_buffer[traffice_start[i] + 4] = 50 / 256;
		_buffer[traffice_start[i] + 5] = 50 % 256;
		_buffer[traffice_start[i] + 6] = 1100 / 256;
		_buffer[traffice_start[i] + 7] = 1100 % 256;
		int light_color = 1;
		if (tra_diret == 0 && (i % 6 < 3)) {
			if (0 == tra_time && (i % 3 > 0)) {
				light_color = 3;
			} else if (1 == tra_time && (i % 3 > 0))
				light_color = 2;
			else if (2 == tra_time && (i % 3 == 0))
				light_color = 3;
			else if (3 == tra_time && (i % 3 == 0))
				light_color = 2;
		}
		else if (tra_diret == 1 && (i % 6 > 2)) {
			if (0 == tra_time && (i % 3 > 0)) {
				light_color = 3;
			} else if (1 == tra_time && (i % 3 > 0))
				light_color = 2;
			else if (2 == tra_time && (i % 3 == 0))
				light_color = 3;
			else if (3 == tra_time && (i % 3 == 0))
				light_color = 2;
		}
		_buffer[traffice_start[i] + 8] = light_color;
		_buffer[traffice_start[i] + 9] = _lefttime * 10 / 256;
		_buffer[traffice_start[i] + 10] = _lefttime * 10 % 256;
		_size += 11;
	}
	_buffer[4] = _size - 5;

	uint8_t checksum = _buffer[2];
	for (int i = 3; i < _size; i++) {
		checksum = checksum ^ _buffer[i];
	}
	_buffer[_size] = checksum;
	_size += 1;
	_lefttime--;
}

}};	//zas::traffic
