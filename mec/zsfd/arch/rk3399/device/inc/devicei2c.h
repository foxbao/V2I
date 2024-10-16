#ifndef __CXX_ZAS_RK3399_I2C_H__
#define __CXX_ZAS_RK3399_I2C_H__

#include <string>

enum i2c_device{
	i2c_dev_0 = 0,
	i2c_dev_1 = 1,
};

typedef struct i2c_rdwr_ioctl_data {
	struct i2c_msg  *msgs;  /* pointers to i2c_msgs */
	unsigned int nmsgs;  /* number of i2c_msgs */
} i2c_rdwr_ioctl_data_t ;

typedef i2c_rdwr_ioctl_data_t* i2c_rdwr_ioctl_data_tp;

struct i2c_msg {
	unsigned short addr;  /* slave address  */
	unsigned short flags;
	unsigned short len;  /* msg length  */
	unsigned char *buf;  /* pointer to msg data  */
};

char* get_master_i2c_dev(int i2c_dev);

int i2c_master_read(int i2c_dev, int addr, std::string &data);

int i2c_master_write(int i2c_dev, int addr, char *data, size_t size);

#endif /* __CXX_ZAS_RK3399_I2C_H__*/