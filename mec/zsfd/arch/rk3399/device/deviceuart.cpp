#include "inc/deviceuart.h"

#include <stdio.h>
#include <fcntl.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <sys/times.h>
#include <sys/types.h>
#include <termios.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdarg.h>

#include "utils/timer.h"
#include "utils/dir.h"

#define TTY_USB_PATH "/dev/ttyUSB"
#define TTY_NET_PATH "/dev/RG500U"
#define TTY_DEVICE_COUNT_MAX (15)

#define TIMEOUT_SEC(buflen,baud) (buflen*20/baud+2)
#define TIMEOUT_USEC 0

using namespace zas::utils;

static const int buff_size = 1024;

portinfo::portinfo()
{
	prompt = '0';	
	baudrate = 9600;
	databit = '8';
	debug = '0';
	echo = '0';
	fctl = '2';
	parity = 'N';
	stopbit = '1';
	reserved = 0;
}

const char* find_dev_ptty(int index, const char* rp)
{
	std::string root_path = rp;
	int dev_pos = 0;
	for (int i = 0; i < TTY_DEVICE_COUNT_MAX; i++) {
		std::string ttyfile = root_path + std::to_string(i);
		printf("checkt dev %s, %d\n", ttyfile.c_str(), dev_pos);
		if (fileexists(ttyfile.c_str())) {
			dev_pos ++;
			if (dev_pos == (index + 1)) {
				return ttyfile.c_str();
			}
		}
	}
	return nullptr;
}

const char *get_ptty(int id)
{
	printf("tty value is %d.\n", id);
	if (id < uart_dev_0 || id >= uart_dev_MAX) {
		printf("get uart is not exist\n");
		return nullptr;
	}

	if (id < uart_net_0) {
		return find_dev_ptty(id, TTY_USB_PATH);
	} else if ( id < uart_dev_MAX) {
		return find_dev_ptty(id - uart_net_0, TTY_NET_PATH);
	}

	return nullptr;
}

int portopen(int id) {
	int fdcom;
	const char* pttyname = get_ptty(id);
	if (nullptr == pttyname) {
		return -ENOTAVAIL;
	}
	std::string ptty = pttyname;
	printf("ptty is %s.\n", ptty.c_str());
	fdcom = open(ptty.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);

	return (fdcom);
}

int convbaud(unsigned long int baudrate) {
	switch(baudrate)
	{
		case 2400:
		return B2400;
		case 4800:
		return B4800;
		case 9600:
		return B9600;
		case 19200:
		return B19200;
		case 38400:
		return B38400;
		case 57600:
		return B57600;
		case 115200:
		return B115200;
		default:
		return B9600;
	}
}

int portset(int fdcom, const portinfo_tp pportinfo) {
	struct termios termios_old, termios_new;
	int  baudrate, tmp;
	char databit, stopbit, parity, fctl;

	bzero(&termios_old, sizeof(termios_old));
	bzero(&termios_new, sizeof(termios_new));
	cfmakeraw(&termios_new);
	tcgetattr(fdcom, &termios_old); //get the serial port attributions

	//baudrates
	baudrate = convbaud(pportinfo -> baudrate);
	cfsetispeed(&termios_new, baudrate);  
	cfsetospeed(&termios_new, baudrate);  
	termios_new.c_cflag |= CLOCAL;   
	termios_new.c_cflag |= CREAD;   

	// flow control
	fctl = pportinfo-> fctl;
	switch(fctl)
	{
		case '0':
			termios_new.c_cflag &= ~CRTSCTS;  //no flow control
			break;
		case '1':
			termios_new.c_cflag |= CRTSCTS;   //hardware flow control
			break;
		case '2':
			termios_new.c_iflag |= IXON | IXOFF |IXANY; //software flow control
			break;
	}

	//data bits
	termios_new.c_cflag &= ~CSIZE;  
	databit = pportinfo -> databit;
	switch(databit)
	{
		case '5':
			termios_new.c_cflag |= CS5;
		case '6':
			termios_new.c_cflag |= CS6;
		case '7':
			termios_new.c_cflag |= CS7;
		default:
			termios_new.c_cflag |= CS8;
	}

	//parity check
	parity = pportinfo -> parity;
	switch(parity)
	{
		case '0':
			termios_new.c_cflag &= ~PARENB;  //no parity check
			break;
		case '1':
			termios_new.c_cflag |= PARENB;  //odd check
			termios_new.c_cflag &= ~PARODD;
			break;
		case '2':
			termios_new.c_cflag |= PARENB;  //even check
			termios_new.c_cflag |= PARODD;
			break;
	}

	//stop bits
	stopbit = pportinfo -> stopbit;
	if(stopbit == '2') {
		termios_new.c_cflag |= CSTOPB; //2 stop bits
	} else {
		termios_new.c_cflag &= ~CSTOPB; //1 stop bits
	}

	//other attributions default
	termios_new.c_oflag &= ~OPOST;   

	termios_new.c_cc[VMIN]  = 1;   
	termios_new.c_cc[VTIME] = 1;   //unit: (1/10)second

	tcflush(fdcom, TCIFLUSH);  
	tmp = tcsetattr(fdcom, TCSANOW, &termios_new); 
	tcgetattr(fdcom, &termios_old);
	return(tmp);
}

int portsend(int fdcom, const char *data, int datalen) {
	int len = 0;
	len = write(fdcom, data, datalen); 
	printf("write date is %s\n", data);
	printf("wirete sizeis %d, %d\n", len, datalen);
	if (len == datalen) {
		return (len);
	} else {
		// tcflush(fdcom, TCOFLUSH);
		return len;
	}
}

int uart_device_write(int fid, int id, portinfo_tp pportinfo, const char* data, size_t sz)
{
	int fdcom = fid;
	if (fid < 0) {
		fdcom = portopen(id);
		if(fdcom < 0) {
			printf("Error: open serial port error./n");
			return -1;
		}
		portset(fdcom, pportinfo);
	}


	int SendLen = portsend(fdcom, data, sz);
	if(SendLen <= 0){
		printf("Error: send failed.\n");
		return -2;
	}

	if (fdcom != fid) {
		close(fdcom);
	}
	return 0;
}

int uart_device_read(int fid, int id, portinfo_tp pportinfo, std::string &data, int size, uint32_t timeout)
{
	int data_len = 0;
	char recvbuf[buff_size];
	int read_data = 0;
	uint32_t read_len = 0;
	bool time_no_limit = false;
	data.clear();
	int fdcom = fid;
	if (fid < 0) {
		printf("read open uart  %d\n", id);
		fdcom = portopen(id);
		if(fdcom < 0) {
			printf("Error: open serial port error./n");
			return -1;
		}
		portset(fdcom, pportinfo);
	}
	long endtime = gettick_millisecond() + timeout;
	if (0 == time) {
		time_no_limit = true;
	}

	if (size > 0) {
		read_len = size;
	}

	do {
		if (read_len == 0) {
			read_data = buff_size;
		} else {
			read_data = read_len > buff_size ? buff_size : read_len;
		}
		read_data = read(fdcom, recvbuf, read_data);
		if (-1 == read_data) {
			msleep(10);
		} else {
			data.append(recvbuf, read_data);
			if (read_len > read_data) {
				read_len -= read_data;
			} else {
				read_len = 0;
			}
			data_len += read_data;
		}
	} while((read_len > 0 || (size < 0 || data_len == 0))
		&& (gettick_millisecond() < endtime || time_no_limit));
	if (fdcom != fid) {
		close(fdcom);
	}
	return data_len;
}

int portopen_dev(int id, portinfo_tp pportinfo) {
	int fdcom = portopen(id);
	if (-1 == fdcom) {
		printf("uart device open error\n");
		return fdcom;
	}
	portset(fdcom, pportinfo);
	return (fdcom);
}