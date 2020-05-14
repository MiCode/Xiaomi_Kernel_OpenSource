// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/device.h>       /* needed by device_* */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <mt-plat/sync_write.h>
#include "sspm_define.h"
#include "sspm_sysfs.h"
#include "sspm_logger.h"

static wait_queue_head_t logwait;

ssize_t __weak sspm_log_read(char __user *data, size_t len)
{
	return 0;
}

unsigned int __weak sspm_log_poll(void)
{
	return 0;
}

static ssize_t sspm_log_if_read(struct file *file, char __user *data,
	size_t len, loff_t *ppos)
{
	ssize_t ret;

	/* pr_debug("[SSPM] sspm_log_if_read\n"); */

	ret = 0;

	if (access_ok(VERIFY_WRITE, data, len))
		ret = sspm_log_read(data, len);

	return ret;
}

static int sspm_log_if_open(struct inode *inode, struct file *file)
{
	/* pr_debug("[SSPM] sspm_log_if_open\n"); */
	return nonseekable_open(inode, file);
}

static unsigned int sspm_log_if_poll(struct file *file, poll_table *wait)
{
	unsigned int ret = 0;

	/* pr_debug("[SSPM] sspm_log_if_poll\n"); */

	if (!(file->f_mode & FMODE_READ))
		return ret;

	poll_wait(file, &logwait, wait);

	ret = sspm_log_poll();

	return ret;
}

void sspm_log_if_wake(void)
{
	wake_up(&logwait);
}

static const struct file_operations sspm_log_file_ops = {
	.owner = THIS_MODULE,
	.read = sspm_log_if_read,
	.open = sspm_log_if_open,
	.poll = sspm_log_if_poll,
};

static struct miscdevice sspm_log_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sspm",
	.fops = &sspm_log_file_ops
};

int __init sspm_sysfs_init(void)
{
	int ret;

	init_waitqueue_head(&logwait);

	ret = misc_register(&sspm_log_device);

	if (unlikely(ret != 0))
		return ret;

	return 0;
}

int sspm_sysfs_create_file(const struct device_attribute *attr)
{
	return device_create_file(sspm_log_device.this_device, attr);
}

int sspm_sysfs_create_bin_file(const struct bin_attribute *attr)
{
	return device_create_bin_file(sspm_log_device.this_device, attr);
}
