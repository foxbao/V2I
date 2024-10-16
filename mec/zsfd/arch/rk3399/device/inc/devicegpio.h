#ifndef __CXX_ZAS_RK3399_GPIO_H__
#define __CXX_ZAS_RK3399_GPIO_H__

enum ele_level{
	ele_high = 1,
	ele_low = 0,
};

enum gpio_mode{
	gpio_mode_unknown = 0,
	gpio_mode_input,
	gpio_mode_output,
};

enum gpio_id{
	gpio_unknown = 0,
	gpio_1_a1 = 33,
	gpio_1_a3 = 45,
	gpio_1_a4 = 36,
	gpio_1_b1 = 41,
	gpio_1_b2 = 42,
	gpio_1_c7 = 55,
	gpio_1_d0 = 56
};

/**
 * get gpio linux number by gpio index.
 * gpio_idx : index of gpio in system
 * return -1 - fail, other is number of gpio in linux.
 */
int get_gpio_linux_num(int gpio_idx);
/**
 * open gpio from linux OS.
 * gpio_idx : index of gpio in system
 * return: -1 - fail, other - success
 */
int open_gpio(int gpio_idx);
/**
 * close gpio from linux OS.
 * gpio_idx : index of gpio in system
 * return: -1 - fail, other - success
 */
int close_gpio(int gpio_idx);
/**
 * set gpio direction by index of gpio.
 * gpio_idx : index of gpio in system
 * edge: 0 - none;1 - rising;2 - falling;3 - both 
 * return: -1 - fail, other - success
 */
int set_gpio_edge(int gpio_idx,int edge);
/**
 * set gpio direction by index of gpio.
 * gpio_idx : index of gpio in system
 * edge: 0 - none;1 - rising;2 - falling;3 - both 
 * return: -1 - fail, other - success
 */
int get_gpio_edge(int gpio_idx);
/**
 * set gpio direction by index of gpio.
 * direction: 0 - output;1 - input
 * return: -1 - fail, other - success
 */
int set_gpio_direction(int gpio_idx,int direction);
/**
 * get gpio direction by index of gpio.
 * gpio_idx : index of gpio in system.
 * return: -1 - fail; other - success, 0 - output 1 - input
 */
int get_gpio_direction(int gpio_idx);
/**
 * set gpio level on output mode.
 * gpio_idx : index of gpio in system.
 * level : electricity level 0 - low,1 - high
 * return: -1 - fail, other - success
 */
int set_gpio_value(int gpio_idx,int level);
/**
 * get gpio level.
 * gpio_idx : index of gpio in system.
 * return: -1 - fail; other - success,0 - low,1 - high
 */
int get_gpio_value(int gpio_idx);
/**
 * get fd of gpio value file.
 * gpio_idx : index of gpio in system.
 * return: -1 - fail; other - fd
 */
int get_gpio_value_fd(int gpio_idx);
/**
 * get count of enable gpio.
 * return count of gpio.
 */
int get_gpio_count();
/**
 * print gpio mapping index.
 */
void print_gpio_info();

#endif /* __CXX_ZAS_RK3399_GPIO_H__*/