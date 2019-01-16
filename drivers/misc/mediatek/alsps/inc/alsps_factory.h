#ifndef __ALSPS_FACTORY_H__
#define __ALSPS_FACTORY_H__

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>

#include <cust_alsps.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <linux/hwmsen_helper.h>
#include <linux/batch.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#include <alsps.h>

extern struct alsps_context *alsps_context_obj;

#define SETCALI 1
#define CLRCALI 2
#define GETCALI 3

#define GET_TH_HIGH 	1
#define GET_TH_LOW		2
#define SET_TH			3
#define GET_TH_RESULT 	4

int alsps_factory_device_init(void);

#endif

