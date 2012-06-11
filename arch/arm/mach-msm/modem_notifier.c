/* Copyright (c) 2008-2010, 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*
 * Modem Restart Notifier -- Provides notification
 *			     of modem restart events.
 */

#include <linux/notifier.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/workqueue.h>

#include "modem_notifier.h"

#define DEBUG

static struct srcu_notifier_head modem_notifier_list;
static struct workqueue_struct *modem_notifier_wq;

static void notify_work_smsm_init(struct work_struct *work)
{
	modem_notify(0, MODEM_NOTIFIER_SMSM_INIT);
}
static DECLARE_WORK(modem_notifier_smsm_init_work, &notify_work_smsm_init);

void modem_queue_smsm_init_notify(void)
{
	int ret;

	ret = queue_work(modem_notifier_wq, &modem_notifier_smsm_init_work);

	if (!ret)
		printk(KERN_ERR "%s\n", __func__);
}
EXPORT_SYMBOL(modem_queue_smsm_init_notify);

static void notify_work_start_reset(struct work_struct *work)
{
	modem_notify(0, MODEM_NOTIFIER_START_RESET);
}
static DECLARE_WORK(modem_notifier_start_reset_work, &notify_work_start_reset);

void modem_queue_start_reset_notify(void)
{
	int ret;

	ret = queue_work(modem_notifier_wq, &modem_notifier_start_reset_work);

	if (!ret)
		printk(KERN_ERR "%s\n", __func__);
}
EXPORT_SYMBOL(modem_queue_start_reset_notify);

static void notify_work_end_reset(struct work_struct *work)
{
	modem_notify(0, MODEM_NOTIFIER_END_RESET);
}
static DECLARE_WORK(modem_notifier_end_reset_work, &notify_work_end_reset);

void modem_queue_end_reset_notify(void)
{
	int ret;

	ret = queue_work(modem_notifier_wq, &modem_notifier_end_reset_work);

	if (!ret)
		printk(KERN_ERR "%s\n", __func__);
}
EXPORT_SYMBOL(modem_queue_end_reset_notify);

int modem_register_notifier(struct notifier_block *nb)
{
	int ret;

	ret = srcu_notifier_chain_register(
		&modem_notifier_list, nb);

	return ret;
}
EXPORT_SYMBOL(modem_register_notifier);

int modem_unregister_notifier(struct notifier_block *nb)
{
	int ret;

	ret = srcu_notifier_chain_unregister(
		&modem_notifier_list, nb);

	return ret;
}
EXPORT_SYMBOL(modem_unregister_notifier);

void modem_notify(void *data, unsigned int state)
{
	srcu_notifier_call_chain(&modem_notifier_list, state, data);
}
EXPORT_SYMBOL(modem_notify);

#if defined(CONFIG_DEBUG_FS)
static int debug_reset_start(const char __user *buf, int count)
{
	modem_queue_start_reset_notify();
	return 0;
}

static int debug_reset_end(const char __user *buf, int count)
{
	modem_queue_end_reset_notify();
	return 0;
}

static ssize_t debug_write(struct file *file, const char __user *buf,
			   size_t count, loff_t *ppos)
{
	int (*fling)(const char __user *buf, int max) = file->private_data;
	fling(buf, count);
	return count;
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static const struct file_operations debug_ops = {
	.write = debug_write,
	.open = debug_open,
};

static void debug_create(const char *name, mode_t mode,
			 struct dentry *dent,
			 int (*fling)(const char __user *buf, int max))
{
	debugfs_create_file(name, mode, dent, fling, &debug_ops);
}

static void modem_notifier_debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("modem_notifier", 0);
	if (IS_ERR(dent))
		return;

	debug_create("reset_start", 0444, dent, debug_reset_start);
	debug_create("reset_end", 0444, dent, debug_reset_end);
}
#else
static void modem_notifier_debugfs_init(void) {}
#endif

#if defined(DEBUG)
static int modem_notifier_test_call(struct notifier_block *this,
				  unsigned long code,
				  void *_cmd)
{
	switch (code) {
	case MODEM_NOTIFIER_START_RESET:
		printk(KERN_ERR "Notify: start reset\n");
		break;
	case MODEM_NOTIFIER_END_RESET:
		printk(KERN_ERR "Notify: end reset\n");
		break;
	case MODEM_NOTIFIER_SMSM_INIT:
		printk(KERN_ERR "Notify: smsm init\n");
		break;
	default:
		printk(KERN_ERR "Notify: general\n");
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block nb = {
	.notifier_call = modem_notifier_test_call,
};

static void register_test_notifier(void)
{
	modem_register_notifier(&nb);
}
#endif

int __init msm_init_modem_notifier_list(void)
{
	static bool registered;

	if (registered)
		return 0;

	registered = true;

	srcu_init_notifier_head(&modem_notifier_list);
	modem_notifier_debugfs_init();
#if defined(DEBUG)
	register_test_notifier();
#endif

	/* Create the workqueue */
	modem_notifier_wq = create_singlethread_workqueue("modem_notifier");
	if (!modem_notifier_wq) {
		srcu_cleanup_notifier_head(&modem_notifier_list);
		return -ENOMEM;
	}

	return 0;
}
module_init(msm_init_modem_notifier_list);
