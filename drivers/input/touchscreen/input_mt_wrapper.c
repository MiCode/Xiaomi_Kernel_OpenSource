/*
 * Japan Display Inc. INPUT_MT_WRAPPER Device Driver
 *
 * Copyright (C) 2013-2014 Japan Display Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/input/input_mt_wrapper.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

#define DEVICE_NAME   "input_mt_wrapper"

static int input_mt_wrapper_open(struct inode *inode, struct file *filp);
static int input_mt_wrapper_release(struct inode *inode, struct file *filp);
static long input_mt_wrapper_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg);
static long input_mt_wrapper_ioctl_set_coordinate(unsigned long arg);

static const struct file_operations g_input_mt_wrapper_fops = {
	.owner = THIS_MODULE,
	.open = input_mt_wrapper_open,
	.release = input_mt_wrapper_release,
	.unlocked_ioctl = input_mt_wrapper_ioctl,
};

static struct miscdevice g_input_mt_wrapper_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &g_input_mt_wrapper_fops,
};

struct input_dev *g_input_dev;

static int __init input_mt_wrapper_init(void)
{
	struct input_dev *input_dev;
	int error;

	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("Unable to allocated input device\n");
		return  -ENOMEM;
	}

	input_dev->name = "input_mt_wrapper";

	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);

	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
		0, 1, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID,
		0, 9, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
		INPUT_MT_WRAPPER_MIN_X, INPUT_MT_WRAPPER_MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
		INPUT_MT_WRAPPER_MIN_Y, INPUT_MT_WRAPPER_MAX_Y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE,
		INPUT_MT_WRAPPER_MIN_Z, INPUT_MT_WRAPPER_MAX_Z, 0, 0);

	error = input_register_device(input_dev);
	if (error) {
		pr_err("Failed to register %s input device\n", input_dev->name);
		return error;
	}

	error = misc_register(&g_input_mt_wrapper_misc_device);
	if (error) {
		pr_err("Failed to register misc device\n");
		input_unregister_device(input_dev);
		return error;
	}

	g_input_dev = input_dev;

	return 0;
}

static void __exit input_mt_wrapper_exit(void)
{
	input_unregister_device(g_input_dev);
	misc_deregister(&g_input_mt_wrapper_misc_device);
}

module_init(input_mt_wrapper_init);
module_exit(input_mt_wrapper_exit);

MODULE_AUTHOR("Japan Display Inc");
MODULE_DESCRIPTION("Input Multitouch Wrapper.");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("input:input_mt_wrapper");

static int input_mt_wrapper_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int input_mt_wrapper_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static long input_mt_wrapper_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	long ret;

	switch (cmd) {
	case INPUT_MT_WRAPPER_IOCTL_CMD_SET_COORDINATES:
		ret = input_mt_wrapper_ioctl_set_coordinate(arg);
		return ret;
	default:
		pr_err("%s: cmd unkown.\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static long input_mt_wrapper_ioctl_set_coordinate(unsigned long arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;
	struct input_dev *input_dev = g_input_dev;
	struct input_mt_wrapper_ioctl_touch_data data;
	u8 i;
	u8 count = 0;

	if (arg == 0) {
		pr_err("%s: arg == 0.\n", __func__);
		return -EINVAL;
	}

	if (copy_from_user(&data, argp,
			sizeof(struct input_mt_wrapper_ioctl_touch_data))) {
		pr_err("%s: Failed to copy_from_user().\n", __func__);
		return -EFAULT;
	}

	if (data.t_num) {
		for (i = 0; i < INPUT_MT_WRAPPER_MAX_FINGERS; i++) {
			if (data.touch[i].z != 0) {
				input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR,
					1);
				input_report_abs(input_dev, ABS_MT_TRACKING_ID,
					(data.touch[i].t));
				input_report_abs(input_dev, ABS_MT_POSITION_X,
					data.touch[i].x);
				input_report_abs(input_dev, ABS_MT_POSITION_Y,
					data.touch[i].y);
				input_report_abs(input_dev, ABS_MT_PRESSURE,
					data.touch[i].z);
				input_mt_sync(input_dev);
				count++;
			}
		}
	}

	/* SYN_MT_REPORT only if no contact */
	if (!count)
		input_mt_sync(input_dev);

	/* SYN_REPORT */
	input_sync(input_dev);

	return ret;
}

