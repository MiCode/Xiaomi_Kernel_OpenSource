// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Unisoc, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>

#define TUI_MINOR 187
#define USER_DEBUG_CMD 1000

struct tui_status {
	int tui_notify;
	int tui_state;
	struct wait_queue_head tui_wq;
};

enum {
	TUI_CANCEL_NOTIFY = 1,
	TUI_NORMAL_EXIT_NOTIFY = 2,
};

static struct tui_status *s_ts;

bool is_in_tui(void)
{
	return s_ts ? s_ts->tui_state > 0 : false;
}
EXPORT_SYMBOL(is_in_tui);

void notify_cancel_tui(void)
{
	if (s_ts) {
		s_ts->tui_notify = TUI_CANCEL_NOTIFY;
		wake_up_interruptible(&s_ts->tui_wq);
	}
}
EXPORT_SYMBOL(notify_cancel_tui);

static int tui_open(struct inode *inode, struct file *file)
{
	struct tui_status *ts = NULL;

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (!ts) {
		s_ts = NULL;
		return -ENOMEM;
	}
	init_waitqueue_head(&ts->tui_wq);
	file->private_data = ts;
	s_ts = ts;
	return 0;
}

static int tui_release(struct inode *inode, struct file *file)
{
	struct tui_status *ts = file->private_data;

	kfree(ts);
	s_ts = NULL;
	return 0;
}

// tui_read: get notify
static ssize_t tui_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	struct tui_status *ts = file->private_data;

	if (count != sizeof(int)) {
		pr_err("%s: output buf err, expect(%ld) actual(%ld)\n",
				__func__, sizeof(int), count);
		return -EINVAL;
	}

	if (copy_to_user(user_buf, &ts->tui_notify, sizeof(int)))
		return -EFAULT;

	ts->tui_notify = -1;
	pr_info("tui read notify done.");
	return sizeof(int);
}

// tui_write: set state
static ssize_t tui_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	int input_data = 0;
	struct tui_status *ts = file->private_data;

	if (count != sizeof(int)) {
		pr_err("%s: input buf err, expect(%ld) actual(%ld)\n",
				__func__, sizeof(int), count);
		return -EINVAL;
	}

	if (copy_from_user(&input_data, user_buf, sizeof(int)))
		return -EFAULT;

	if (input_data < USER_DEBUG_CMD)
		s_ts->tui_state = input_data;
	else { // user debug cmd
		if (is_in_tui())
			notify_cancel_tui();
		else
			pr_info("not in tui state, ignore this debug cmd.");
	}

	if (!input_data) { // exit tui normally, wake up poll
		s_ts->tui_notify = TUI_NORMAL_EXIT_NOTIFY;
		wake_up_interruptible(&ts->tui_wq);
	}
	return sizeof(int);
}

static __poll_t tui_poll(struct file *file, poll_table *wait)
{
	__poll_t mask = 0;
	struct tui_status *ts = file->private_data;

	poll_wait(file, &ts->tui_wq, wait);
	if (ts->tui_notify > 0) {
		mask |= POLLIN;
		pr_info("tui wake up pollin");
	}
	return mask;
}

static const struct file_operations tui_fops = {
	.owner = THIS_MODULE,
	.open = tui_open,
	.read = tui_read,
	.write = tui_write,
	.poll = tui_poll,
	.release = tui_release,
};

static struct miscdevice tui_dev = {
	.minor = TUI_MINOR,
	.name = "tui_dev",
	.fops = &tui_fops,
};

static int trusty_tui_probe(struct platform_device *pdev)
{
	int result = 0;

	result = misc_register(&tui_dev);
	if (result)
		pr_err("can't register miscdev minor=%d (%d)",
		    TUI_MINOR, result);

	return result;
}

static int trusty_tui_remove(struct platform_device *pdev)
{
	misc_deregister(&tui_dev);
	return 0;
}

static const struct of_device_id trusty_tui_of_match[] = {
	{ .compatible = "sprd,trusty-tui-v1", },
	{},
};
MODULE_DEVICE_TABLE(of, trusty_tui_of_match);

struct platform_driver trusty_tui_driver = {
	.probe = trusty_tui_probe,
	.remove = trusty_tui_remove,
	.driver = {
		.name = "sprd-trusty-tui",
		.owner = THIS_MODULE,
		.of_match_table = trusty_tui_of_match,
	}
};

module_platform_driver(trusty_tui_driver);

MODULE_DESCRIPTION("Sprd trusty tui driver");
MODULE_LICENSE("GPL v2");
