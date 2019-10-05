/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#define pr_fmt(fmt) "<sensorlist> " fmt

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>

#include "sensor_list.h"
#include "hf_sensor_type.h"

enum sensorlist {
	accel,
	gyro,
	mag,
	als,
	ps,
	baro,
	sar,
	maxhandle,
};

static struct sensorlist_info_t sensorlist_info[maxhandle];
static struct mag_libinfo_t mag_libinfo;
static DEFINE_SPINLOCK(sensorlist_info_lock);

int sensorlist_find_sensor(int sensor)
{
	int handle = -1;

	switch (sensor) {
	case SENSOR_TYPE_ACCELEROMETER:
		handle = accel;
		break;
	case SENSOR_TYPE_GYROSCOPE:
		handle = gyro;
		break;
	case SENSOR_TYPE_MAGNETIC_FIELD:
		handle = mag;
		break;
	case SENSOR_TYPE_LIGHT:
		handle = als;
		break;
	case SENSOR_TYPE_PROXIMITY:
		handle = ps;
		break;
	case SENSOR_TYPE_PRESSURE:
		handle = baro;
		break;
	case SENSOR_TYPE_SAR:
		handle = sar;
		break;
	}
	return handle;
}

static void init_sensorlist_info(void)
{
	int handle = -1;

	for (handle = accel; handle < maxhandle; ++handle)
		strlcpy(sensorlist_info[handle].name, "NULL",
			sizeof(sensorlist_info[handle].name));
}

int sensorlist_register_devinfo(int sensor,
		struct sensorlist_info_t *devinfo)
{
	int handle = -1;

	handle = sensorlist_find_sensor(sensor);
	if (handle < 0)
		return -1;
	pr_notice("name(%s) type(%d) registered\n", devinfo->name, sensor);
	spin_lock(&sensorlist_info_lock);
	strlcpy(sensorlist_info[handle].name, devinfo->name,
			sizeof(sensorlist_info[handle].name));
	spin_unlock(&sensorlist_info_lock);
	return 0;
}

int sensorlist_register_maginfo(struct mag_libinfo_t *maginfo)
{
	spin_lock(&sensorlist_info_lock);
	memcpy(&mag_libinfo, maginfo, sizeof(struct mag_libinfo_t));
	spin_unlock(&sensorlist_info_lock);
	return 0;
}

static int sensorlist_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static ssize_t
sensorlist_read(struct file *file, char __user *buf,
		size_t count, loff_t *ptr)
{
	struct sensorlist_info_t temp[maxhandle];

	if (count == 0)
		return -EINVAL;
	if (count < sizeof(struct sensorlist_info_t))
		return -EINVAL;
	if (count > maxhandle * sizeof(struct sensorlist_info_t))
		count = maxhandle * sizeof(struct sensorlist_info_t);

	memset(temp, 0, sizeof(temp));
	spin_lock(&sensorlist_info_lock);
	memcpy(temp, sensorlist_info, sizeof(temp));
	spin_unlock(&sensorlist_info_lock);
	if (copy_to_user(buf, temp, count))
		return -EFAULT;
	return count;
}

static long sensorlist_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	unsigned int size = _IOC_SIZE(cmd);
	void __user *ubuf = (void __user *)arg;
	struct mag_libinfo_t temp;

	if (size != sizeof(struct mag_libinfo_t))
		return -EINVAL;

	switch (cmd) {
	case SENSOR_LIST_GET_MAG_LIB_INFO:
		memset(&temp, 0, sizeof(struct mag_libinfo_t));
		spin_lock(&sensorlist_info_lock);
		memcpy(&temp, &mag_libinfo, sizeof(struct mag_libinfo_t));
		spin_unlock(&sensorlist_info_lock);
		if (copy_to_user(ubuf, &temp, sizeof(struct mag_libinfo_t)))
			return -EFAULT;
		break;
	}
	return 0;
}

static const struct file_operations sensorlist_fops = {
	.owner          = THIS_MODULE,
	.open           = sensorlist_open,
	.read           = sensorlist_read,
	.unlocked_ioctl = sensorlist_ioctl,
	.compat_ioctl   = sensorlist_ioctl,
};

static struct miscdevice sensorlist_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sensorlist",
	.fops = &sensorlist_fops,
};

static int __init sensorlist_init(void)
{
	init_sensorlist_info();
	if (misc_register(&sensorlist_miscdev) < 0)
		return -1;
	return 0;
}

subsys_initcall(sensorlist_init);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("dynamic sensorlist driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
