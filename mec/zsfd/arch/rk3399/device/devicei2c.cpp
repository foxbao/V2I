#include "inc/devicei2c.h"

#include <stdio.h>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <sys/timerfd.h>

#define I2C_RDWR  0x0707

char* I2C_DEV_0 = (char*)"/dev/i2c-0";
char* I2C_DEV_1 = (char*)"/dev/i2c-1";

char* get_master_i2c_dev(int i2c_dev) {
	switch (i2c_dev)
	{
	case i2c_dev_0:
		return I2C_DEV_0;
		break;
	case i2c_dev_1:
		return I2C_DEV_1;
		break;
	default:
		return NULL;
		break;
	}
}

int i2c_master_read(int i2c_dev, int addr, std::string &data)
{
	int fd;
	fd = open(get_master_i2c_dev(i2c_dev), O_RDWR);
	struct i2c_rdwr_ioctl_data i2c_data;
	i2c_data.msgs = (struct i2c_msg *)malloc(2*sizeof(struct i2c_msg));
	i2c_data.nmsgs = 2;
	i2c_data.msgs[0].len = 1;
	i2c_data.msgs[0].addr = addr; 
	i2c_data.msgs[0].flags = 0;  
	i2c_data.msgs[0].buf[0] = 0x01;     
	i2c_data.msgs[1].len = 1;
	i2c_data.msgs[1].addr = addr; 
	i2c_data.msgs[1].flags = 1;  
	i2c_data.msgs[1].buf = (unsigned char *)malloc(2);

	ioctl(fd,I2C_RDWR,(unsigned long)&data);

	close(fd);
}

int i2c_master_write(int i2c_dev, int addr, char *data, size_t size)
{
	int fd;
	fd = open(get_master_i2c_dev(i2c_dev), O_RDWR);
	struct i2c_rdwr_ioctl_data i2c_data;
	i2c_data.msgs = (struct i2c_msg *)malloc(2*sizeof(struct i2c_msg));
	i2c_data.nmsgs = 1;   
	i2c_data.msgs[0].len = 2;
	i2c_data.msgs[0].addr = addr;
	i2c_data.msgs[0].flags = 0;  
	i2c_data.msgs[0].buf = (unsigned char *)malloc(2);

	ioctl(fd,I2C_RDWR,(unsigned long)&data);

	close(fd);
}