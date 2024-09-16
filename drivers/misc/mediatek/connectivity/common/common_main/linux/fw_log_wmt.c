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
#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
#include "fw_log_wmt.h"
#include "connsys_debug_utility.h"
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/poll.h>
#include "wmt_lib.h"

#define DRIVER_NAME "fw_log_wmt"
#define WMT_FW_LOG_IOC_MAGIC         0xfc
#define WMT_FW_LOG_IOCTL_ON_OFF      _IOW(WMT_FW_LOG_IOC_MAGIC, 0, int)
#define WMT_FW_LOG_IOCTL_SET_LEVEL   _IOW(WMT_FW_LOG_IOC_MAGIC, 1, int)

static dev_t gDevId;
static struct cdev gLogCdev;
static struct class *fw_log_wmt_class;
static struct device *fw_log_wmt_dev;
static wait_queue_head_t wq;

static int fw_log_wmt_open(struct inode *inode, struct file *file)
{
	pr_info("%s\n", __func__);
	return 0;
}

static int fw_log_wmt_close(struct inode *inode, struct file *file)
{
	pr_info("%s\n", __func__);
	return 0;
}

static ssize_t fw_log_wmt_read(struct file *filp, char __user *buf,
	size_t count, loff_t *f_pos)
{
	ssize_t size = 0;

	pr_debug("%s\n", __func__);
	size = connsys_log_read_to_user(CONNLOG_TYPE_MCU, buf, count);
	return size;
}

static ssize_t fw_log_wmt_write(struct file *filp, const char __user *buf,
	size_t count, loff_t *f_pos)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static unsigned int fw_log_wmt_poll(struct file *filp, poll_table *wait)
{
	pr_debug("%s\n", __func__);

	poll_wait(filp, &wq, wait);
	if (connsys_log_get_buf_size(CONNLOG_TYPE_MCU) > 0)
		return POLLIN | POLLRDNORM;

	return 0;
}

static long fw_log_wmt_unlocked_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	long ret = 0;

	switch (cmd) {
	case WMT_FW_LOG_IOCTL_ON_OFF:
		pr_debug("ioctl: WMT_FW_LOG_IOCTL_ON_OFF(%lu)", arg);
		if (arg == 0 || arg == 1)
			wmt_lib_fw_log_ctrl(WMT_FWLOG_MCU, (unsigned char)arg, 0xFF);
		break;
	case WMT_FW_LOG_IOCTL_SET_LEVEL:
		pr_debug("ioctl: WMT_FW_LOG_IOCTL_SET_LEVEL(%lu)", arg);
		if (arg <= 4)
			wmt_lib_fw_log_ctrl(WMT_FWLOG_MCU, 0xFF, (unsigned char)arg);
		break;
	default:
		/*no action*/
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long fw_log_wmt_compat_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	return fw_log_wmt_unlocked_ioctl(filp, cmd, arg);
}
#endif

const struct file_operations gLogFops = {
	.open = fw_log_wmt_open,
	.release = fw_log_wmt_close,
	.read = fw_log_wmt_read,
	.write = fw_log_wmt_write,
	.poll = fw_log_wmt_poll,
	.unlocked_ioctl = fw_log_wmt_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = fw_log_wmt_compat_ioctl,
#endif
};

static void fw_log_wmt_event_cb(void)
{
	wake_up_interruptible(&wq);
}

int fw_log_wmt_init(void)
{
	int cdevErr = -1;
	int ret = -1;

	ret = alloc_chrdev_region(&gDevId, 0, 1, DRIVER_NAME);
	if (ret)
		pr_err("fail to alloc_chrdev_region\n");

	cdev_init(&gLogCdev, &gLogFops);
	gLogCdev.owner = THIS_MODULE;

	cdevErr = cdev_add(&gLogCdev, gDevId, 1);
	if (cdevErr) {
		pr_err("cdev_add() fails (%d)\n", cdevErr);
		goto error;
	}

	fw_log_wmt_class = class_create(THIS_MODULE, DRIVER_NAME);
	if (IS_ERR(fw_log_wmt_class)) {
		pr_err("class_create fail\n");
		goto error;
	}

	fw_log_wmt_dev = device_create(fw_log_wmt_class, NULL, gDevId, NULL,
		DRIVER_NAME);
	if (IS_ERR(fw_log_wmt_dev)) {
		pr_err("device_create fail\n");
		goto error;
	}

	init_waitqueue_head(&wq);
	ret = connsys_log_init(CONNLOG_TYPE_MCU);
	if (ret)
		pr_err("fail to connsys_log_init\n");

	ret = connsys_log_register_event_cb(CONNLOG_TYPE_MCU,
		fw_log_wmt_event_cb);
	if (ret)
		pr_err("fail to connsys_log_register_event_cb\n");

	return 0;

error:
	if (!(IS_ERR(fw_log_wmt_dev)))
		device_destroy(fw_log_wmt_class, gDevId);
	if (!(IS_ERR(fw_log_wmt_class))) {
		class_destroy(fw_log_wmt_class);
		fw_log_wmt_class = NULL;
	}

	if (cdevErr == 0)
		cdev_del(&gLogCdev);
	if (ret == 0)
		unregister_chrdev_region(gDevId, 1);
	pr_err("fw_log_wmt_init fail\n");
	return -1;
}
EXPORT_SYMBOL(fw_log_wmt_init);

void fw_log_wmt_deinit(void)
{
	connsys_log_deinit(CONNLOG_TYPE_MCU);
	if (fw_log_wmt_dev) {
		device_destroy(fw_log_wmt_class, gDevId);
		fw_log_wmt_dev = NULL;
	}

	if (fw_log_wmt_class) {
		class_destroy(fw_log_wmt_class);
		fw_log_wmt_class = NULL;
	}

	cdev_del(&gLogCdev);
	unregister_chrdev_region(gDevId, 1);
	pr_warn("fw_log_wmt_driver_deinit done\n");
}
EXPORT_SYMBOL(fw_log_wmt_deinit);
#endif
