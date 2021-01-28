// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#define pr_fmt(fmt) "<SEN_LIST> " fmt

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#include "SCP_sensorHub.h"
#include "sensor_list.h"
#include "SCP_power_monitor.h"
#include "hwmsensor.h"

struct sensorlist_info_t {
	char name[16];
};

enum {
	accel,
	gyro,
	mag,
	als,
	ps,
	baro,
	sar,
	maxhandle,
};

#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
static struct work_struct sensorlist_work;
#endif
static atomic_t first_ready_after_boot;
static struct sensorlist_info_t sensorlist_info[maxhandle];
static DEFINE_SPINLOCK(sensorlist_info_lock);

inline int sensor_to_handle(int sensor)
{
	int handle = -1;

	switch (sensor) {
	case ID_ACCELEROMETER:
		handle = accel;
		break;
	case ID_GYROSCOPE:
		handle = gyro;
		break;
	case ID_MAGNETIC:
		handle = mag;
		break;
	case ID_LIGHT:
		handle = als;
		break;
	case ID_PROXIMITY:
		handle = ps;
		break;
	case ID_PRESSURE:
		handle = baro;
		break;
	case ID_SAR:
		handle = sar;
		break;
	}
	return handle;
}

static inline int handle_to_sensor(int handle)
{
	int sensor = -1;

	switch (handle) {
	case accel:
		sensor = ID_ACCELEROMETER;
		break;
	case gyro:
		sensor = ID_GYROSCOPE;
		break;
	case mag:
		sensor = ID_MAGNETIC;
		break;
	case als:
		sensor = ID_LIGHT;
		break;
	case ps:
		sensor = ID_PROXIMITY;
		break;
	case baro:
		sensor = ID_PRESSURE;
		break;
	case sar:
		sensor = ID_SAR;
		break;
	}
	return sensor;
}

static void init_sensorlist_info(void)
{
	int handle = -1;

	for (handle = accel; handle < maxhandle; ++handle)
		strlcpy(sensorlist_info[handle].name,
			"NULL",
			sizeof(sensorlist_info[handle].name));
}

#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
static void sensorlist_get_deviceinfo(struct work_struct *work)
{
	int err = 0, handle = -1, sensor = -1;
	struct sensorInfo_t devinfo;

	for (handle = accel; handle < maxhandle; ++handle) {
		sensor = handle_to_sensor(handle);
		if (sensor < 0)
			continue;
		memset(&devinfo, 0, sizeof(struct sensorInfo_t));
		err = sensor_set_cmd_to_hub(sensor,
			CUST_ACTION_GET_SENSOR_INFO, &devinfo);
		if (err < 0) {
			pr_err("sensor(%d) not register\n", sensor);
			continue;
		}
		spin_lock(&sensorlist_info_lock);
		strlcpy(sensorlist_info[handle].name,
			devinfo.name,
			sizeof(sensorlist_info[handle].name));
		spin_unlock(&sensorlist_info_lock);
	}
}

static int scp_ready_event(uint8_t event, void *ptr)
{
	pr_err("%s, event:%u\n", __func__, event);
	switch (event) {
	case SENSOR_POWER_UP:
		if (likely(atomic_xchg(&first_ready_after_boot, 1)))
			return 0;
		schedule_work(&sensorlist_work);
		break;
	case SENSOR_POWER_DOWN:
		break;
	}
	return 0;
}

static struct scp_power_monitor scp_ready_notifier = {
	.name = "sensorlist",
	.notifier_call = scp_ready_event,
};
#else
int sensorlist_register_deviceinfo(int sensor,
		struct sensorInfo_NonHub_t *devinfo)
{
	int handle = -1;

	handle = sensor_to_handle(sensor);
	if (handle < 0)
		return -1;
	spin_lock(&sensorlist_info_lock);
	strlcpy(sensorlist_info[handle].name,
		devinfo->name,
		sizeof(sensorlist_info[handle].name));
	spin_unlock(&sensorlist_info_lock);
	return 0;
}
#endif

static int sensorlist_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static ssize_t
sensorlist_read(struct file *file, char __user *buf,
	size_t count, loff_t *ptr)
{
	if (!atomic_read(&first_ready_after_boot))
		return -EINVAL;
	if (count == 0)
		return -EINVAL;
	if (count < sizeof(struct sensorlist_info_t))
		return -EINVAL;
	if (count > maxhandle * sizeof(struct sensorlist_info_t))
		count = maxhandle * sizeof(struct sensorlist_info_t);

	spin_lock(&sensorlist_info_lock);
	if (copy_to_user(buf, sensorlist_info, count)) {
		spin_unlock(&sensorlist_info_lock);
		return -EFAULT;
	}
	spin_unlock(&sensorlist_info_lock);
	return count;
}

static const struct file_operations sensorlist_fops = {
	.owner		= THIS_MODULE,
	.open		= sensorlist_open,
	.read		= sensorlist_read,
};

static struct miscdevice sensorlist_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sensorlist",
	.fops = &sensorlist_fops,
};

static int __init sensorlist_init(void)
{
	int ret = 0;

	init_sensorlist_info();
	ret = misc_register(&sensorlist_miscdev);
	if (ret < 0)
		return -1;
	atomic_set(&first_ready_after_boot, 0);
#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
	INIT_WORK(&sensorlist_work, sensorlist_get_deviceinfo);
	scp_power_monitor_register(&scp_ready_notifier);
#endif
	return 0;
}

static void __exit sensorlist_exit(void)
{

}
module_init(sensorlist_init);
module_exit(sensorlist_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("dynamic sensorlist driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
