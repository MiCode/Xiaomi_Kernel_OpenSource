#ifndef _TOUCH_PALM_SENSOR_H
#define _TOUCH_PALM_SENSOR_H

#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>

struct palm_sensor_device {
	struct miscdevice 	misc_dev;
	wait_queue_head_t 	wait_queue;
	struct mutex 	mutex;
	struct work_struct palm_work;
	/*struct completion update_completion;*/
	int (* palmsensor_switch)(bool on);
	bool palmsensor_onoff;
	bool status_changed;
	bool open_status;
	int report_value;
};

struct palm_sensor_data {
	struct palm_sensor_device *device;
	const char *name;
};

#define PALM_SENSOR_SWITCH 0x1

struct palm_sensor_device *palmsensor_dev_get(int minor);
int palmsensor_register_switch(int (*cb)(bool on));
void palmsensor_update_data(int data);

#endif

