#ifndef __CXX_ZAS_RK3399_UART_H__
#define __CXX_ZAS_RK3399_UART_H__

#include <string>

enum uart_device{
	uart_dev_0 = 0,
	uart_dev_1,
	uart_dev_2,
	uart_dev_3,
	uart_dev_4,
	uart_net_0,
	uart_net_1,
	uart_net_2,
	uart_net_3,
	uart_net_4,
	uart_dev_MAX,
};

/*uart struct*/
typedef struct portinfo
{
	portinfo();
	char    prompt;		/*prompt after reciving data*/
	int		baudrate;	/*baudrate*/
	char	databit;	/*data bits, 5, 6, 7, 8*/
	char	debug;		/*debug mode, 0: none, 1: debug*/
	char	echo;		/*echo mode, 0: none, 1: echo*/
	char	fctl;		/*flow control, 0: none, 1: hardware, 
										2: software*/
	char	parity;		/*parity 0: none, 1: odd, 2: even*/
	char	stopbit;	/*stop bits, 1, 2*/
	int reserved;	/*reserved, must be zero*/
} portinfo_t;

typedef portinfo_t* portinfo_tp;
/**
 * get ptty
 * id : uart number 
 * ptty is device
 */
const char *get_ptty(int id);
/**
 * get fd of gpio value file.
 * gpio_idx : index of gpio in system.
 * return: -1 - fail; other - fd
 */
int portopen_dev(int id, portinfo_tp pportinfo);

/**
 * conver baudrate to system baudrate
 * number of baudrate
 */
int convbaud(unsigned long int baudrate);
/**
 * wirte data to uart file
 * id : number of uart
 * data : will to wirte data for uart
 * sz : size of data
 */
int uart_device_write(int fid, int id, portinfo_tp pportinfo, const char* data, size_t sz);
/**
 * read data from uart file
 * id : number of uart
 * data : buffer of read
 * 
 */
int uart_device_read(int fid, int id, portinfo_tp pportinfo, std::string &data, int size, uint32_t timeout);

#endif /* __CXX_ZAS_RK3399_UART_H__*/