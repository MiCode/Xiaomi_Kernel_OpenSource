/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "sprd-gnss-pm: " fmt

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/major.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/string.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include "../gnss_common_ctl/gnss_common.h"

#define GNSS_DATA_MAX_LEN	16

struct sprd_gnss {
	u32 chip_en;
	bool gnss_flag_sleep;
	bool gnss_flag_resume;
	char gnss_status[16];
	wait_queue_head_t gnss_sleep_wait;
};

static struct sprd_gnss gnss_dev;
static int gnss_delay_cancel;

static int gnss_pmnotify_ctl_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int gnss_pmnotify_ctl_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static int gnss_pm_notify(struct notifier_block *nb,
			  unsigned long event, void *dummy)
{
	int i = 10;

	if (event == PM_SUSPEND_PREPARE) {
		gnss_dev.gnss_flag_sleep = true;
		wake_up_interruptible(&gnss_dev.gnss_sleep_wait);
		gnss_delay_cancel = 0;
		while (gnss_delay_ctl() && (i--) &&
				(gnss_delay_cancel != 1)) {
			mdelay(2);
		}
	} else
		gnss_dev.gnss_flag_sleep = false;
	pr_info("%s event:%ld\n", __func__, event);

	return NOTIFY_OK;
}

static struct notifier_block gnss_pm_notifier = {
	.notifier_call = gnss_pm_notify,
};

static unsigned int gnss_pmnotify_ctl_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(filp, &gnss_dev.gnss_sleep_wait, wait);
	if (gnss_dev.gnss_flag_sleep == true)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static ssize_t gnss_pmnotify_ctl_read(struct file *filp,
			  char __user *buf, size_t count, loff_t *pos)
{
	gnss_delay_cancel = 1;

	return (gnss_dev.gnss_flag_sleep == true) ? 1:0;
}

static const struct file_operations gnss_pmnotify_ctl_fops = {
	.owner = THIS_MODULE,
	.open = gnss_pmnotify_ctl_open,
	.release = gnss_pmnotify_ctl_release,
	.read = gnss_pmnotify_ctl_read,
	.poll = gnss_pmnotify_ctl_poll,
};

static struct miscdevice gnss_pmnotify_ctl_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gnss_pmnotify_ctl",
	.fops = &gnss_pmnotify_ctl_fops,
};
#ifdef GNSS_SINGLE_MODULE
int gnss_pnt_ctl_init(void)
{
	int err = 0;

	pr_info("%s entry single ko\n", __func__);
	err = misc_register(&gnss_pmnotify_ctl_device);
	if (err)
		pr_err("gnss_pmnotify_ctl_device add failed!!!\n");

	err = register_pm_notifier(&gnss_pm_notifier);
	if (err)
		pr_err("gnss_pm_notifier add failed!!!\n");
	init_waitqueue_head(&gnss_dev.gnss_sleep_wait);

	return err;
}

void gnss_pnt_ctl_cleanup(void)
{
	misc_deregister(&gnss_pmnotify_ctl_device);
	unregister_pm_notifier(&gnss_pm_notifier);
}
#else
static int __init gnss_pmnotify_ctl_init(void)
{
	int err = 0;

	pr_info("%s entry multiple ko\n", __func__);
	err = misc_register(&gnss_pmnotify_ctl_device);
	if (err)
		pr_err("gnss_pmnotify_ctl_device add failed!!!\n");

	err = register_pm_notifier(&gnss_pm_notifier);
	if (err)
		pr_err("gnss_pm_notifier add failed!!!\n");
	init_waitqueue_head(&gnss_dev.gnss_sleep_wait);

	return err;
}

static void __exit gnss_pmnotify_ctl_cleanup(void)
{
	misc_deregister(&gnss_pmnotify_ctl_device);
	unregister_pm_notifier(&gnss_pm_notifier);
}

module_init(gnss_pmnotify_ctl_init);
module_exit(gnss_pmnotify_ctl_cleanup);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("sprd gnss pmnotify ctl driver");
#endif
