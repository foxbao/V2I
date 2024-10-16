#include "inc/devicegpio.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define GPIO_DEV_PATH "/sys/class/gpio"
#define GPIO_DEV_EXPORT "/sys/class/gpio/export"
#define GPIO_DEV_UNEXPORT "/sys/class/gpio/unexport"

const static int gpio_linux_num[] = {33,45,36,41,42,55,56};
const static char* gpio_pin_num[] = {"gpio_1_a1","gpio_1_a3","gpio_1_a4",
	"gpio_1_b1","gpio_1_b2","gpio_1_c7","gpio_1_d0"};
static const char dir_str[] = "in\0out";  
static const char edge_str[] = "none\0rising\0falling\0both"; 
static const char values_str[] = "01";

int get_gpio_linux_num(int gpio_idx)
{
	if(gpio_idx > get_gpio_count()) {
		printf("The index %d out of bounds.\n", gpio_idx);
		return -1;
	}
	return gpio_linux_num[gpio_idx-1];
}

int open_gpio(int gpio_idx)
{
	int result=0;

	char buffer[4];
	int gpio_linux_number = get_gpio_linux_num(gpio_idx);
	if (gpio_linux_number == -1) {
		printf("The gpio %d does not exist.\n", gpio_idx);
		result = -1;
	}

	int fd = open(GPIO_DEV_EXPORT, O_WRONLY);
	if (fd < 0) {  
		printf("Failed to open export GPIO %d for writing!\n", 
			gpio_linux_number);
		result = -1;
	}
	int len = snprintf(buffer, sizeof(buffer), "%d", gpio_linux_number);
	if (write(fd, buffer, len) < 0) {  
		printf("Failed to export gpio %d!\n", gpio_idx);
		result = -1;
	}
	close(fd);

	return result;
}

int close_gpio(int gpio_idx)
{
	int result=0;

	char buffer[4];
	int gpio_linux_number = get_gpio_linux_num(gpio_idx);
	if (gpio_linux_number == -1) {
		printf("The gpio %d does not exist.\n", gpio_idx);
		result = -1;
	}
	int fd = open(GPIO_DEV_UNEXPORT, O_WRONLY);
	if (fd < 0) {  
		printf("Failed to open export GPIO %d for writing!\n", 
			gpio_linux_number);
		result = -1;
	}
	int len = snprintf(buffer, sizeof(buffer), "%d", gpio_linux_number);
	if (write(fd, buffer, len) < 0) {  
		printf("Failed to export gpio %d!\n", gpio_idx);
		result = -1;
	}
	close(fd);

	return result;
}

int set_gpio_edge(int gpio_idx,int edge)
{
	int result=0;

	char ptr;
	switch(edge){
	case 0:
		ptr = 0;
		break;
	case 1:
		ptr = 5;
		break;
	case 2:
		ptr = 12;
		break;
	case 3:
		ptr = 20;
		break;
	default:
		ptr = 0;
	} 

	char path[64];  
	int gpio_linux_number = get_gpio_linux_num(gpio_idx);
	if (gpio_linux_number == -1) {
		printf("The gpio %d does not exist.\n", gpio_idx);
		result = -1;
	}
	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/edge", 
		gpio_linux_number);
	int fd = open(path, O_WRONLY);
	if (fd < 0) {
		printf("Failed to open gpio direction for writing!\n");  
		result -1;
	}

	if (write(fd, &edge_str[ptr], strlen(&edge_str[ptr])) < 0) {  
		printf("Failed to set direction!\n");
		result -1;
	}

	close(fd);

	return result;
}

int get_gpio_edge(int gpio_idx)
{
	char path[64];  
	int gpio_linux_number = get_gpio_linux_num(gpio_idx);
	if (gpio_linux_number == -1) {
		printf("The gpio %d does not exist.\n", gpio_idx);
		return -1;
	}
	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/edge", 
		gpio_linux_number);
	char value_str[8];
	printf("gpio_dev is %s.\n", path);
	int fd = open(path, O_RDONLY);
	if (fd < 0) {  
		printf("Failed to open gpio direction for reading!\n");  
		return -1;
	}  

	if (read(fd, value_str, 8) < 0) {  
		printf("Failed to read direction!\n");  
		return -1;
	}  
	close(fd);
	if(strcmp(value_str, "rising") == 0) {
		return 1;
	} else if(strcmp(value_str, "falling") == 0) {
		return 2;
	} else if(strcmp(value_str, "both") == 0) {
		return 3;
	} else {
		return 0;
	}
}

int set_gpio_direction(int gpio_idx,int direction)
{
	int result=0;

	char path[64];  
	int gpio_linux_number = get_gpio_linux_num(gpio_idx);
	if (gpio_linux_number == -1) {
		printf("The gpio %d does not exist.\n", gpio_idx);
		result = -1;
	}
	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", 
		gpio_linux_number);  
	int fd = open(path, O_WRONLY);  
	if (fd < 0) {  
		printf("Failed to open gpio direction for writing!\n");  
		result -1;  
	}

	if (write(fd, &dir_str[direction == 1 ? 0 : 3], direction == 1 ? 2 : 3) 
		< 0) {  
		printf("Failed to set direction!\n");  
		result -1;  
	}

	if(direction == 1) {
		set_gpio_edge(gpio_idx, 3);
	} else {
		set_gpio_edge(gpio_idx, 0);
	}

	close(fd);

	return result;
}

int get_gpio_direction(int gpio_idx)
{
	char path[64];  
	int gpio_linux_number = get_gpio_linux_num(gpio_idx);
	if (gpio_linux_number == -1) {
		printf("The gpio %d does not exist.\n", gpio_idx);
		return -1;
	}
	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", 
		gpio_linux_number);
	char value_str[4];
	int fd = open(path, O_RDONLY);
	if (fd < 0) {  
		printf("Failed to open gpio direction for reading!\n");  
		return -1;
	}  

	if (read(fd, value_str, 4) < 0) {  
		printf("Failed to read direction!\n");  
		return -1;
	}  
	close(fd);
	return strcmp(value_str, "in") == 1?1:2;
}

int set_gpio_value(int gpio_idx,int level)
{
	int result=0;

	char lvl = values_str[level == 0 ? 0 : 1];
	char path[64];  

	int gpio_linux_number = get_gpio_linux_num(gpio_idx);
	if (gpio_linux_number == -1) {
		printf("The gpio %d does not exist.\n", gpio_idx);
		result = -1;
	}
	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", 
		gpio_linux_number);

	int fd = open(path, O_WRONLY);
	if (fd < 0) {  
		printf("Failed to open gpio value for writing!\n"); 
		result = -1;  
	}  

	if (write(fd, &lvl, 1)<0) {
		printf("Failed to write value!\n");  
		result = -1;  
	}  
	close(fd);

	return result;
}

int get_gpio_value(int gpio_idx)
{
	char path[64];  
	int gpio_linux_number = get_gpio_linux_num(gpio_idx);
	if (gpio_linux_number == -1) {
		printf("The gpio %d does not exist.\n", gpio_idx);
		return -1;
	}
	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", 
		gpio_linux_number);
	char value_str[2];
	int fd = open(path, O_RDONLY);
	if (fd < 0) {  
		printf("Failed to open gpio value for reading!\n");  
		return -1;
	}  

	if (read(fd, value_str, 2) < 0) {  
		printf("Failed to read value!\n");  
		return -1;
	}
	close(fd);
	return atoi(value_str);
}

int get_gpio_value_fd(int gpio_idx)
{
	char path[64];
	int gpio_linux_number = get_gpio_linux_num(gpio_idx);
	if (gpio_linux_number == -1) {
		printf("The gpio %d does not exist.\n", gpio_idx);
		return -1;
	}
	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", 
		gpio_linux_number);
	char value_str[2];
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		printf("Failed to open gpio value for reading!\n");  
		return -1;
	} 
	return fd;
}

int get_gpio_count()
{
	return sizeof(gpio_linux_num)/sizeof(gpio_linux_num[0]);
}

void print_gpio_info()
{
	for(int i=0; i<sizeof(gpio_linux_num)/sizeof(gpio_linux_num[0]); i++) {
		printf("gpio index is %d,gpio linux number is %d,gpio pin number is %s.\n", (i+1), gpio_linux_num[i], gpio_pin_num[i]);
	}
}