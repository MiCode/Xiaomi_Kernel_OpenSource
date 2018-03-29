/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <asm/uaccess.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/input.h>

#include <linux/platform_device.h>
#include "perfmgr.h"

/*--------------------------------------------*/

#define SEQ_printf(m, x...)\
	do {\
		if (m)\
			seq_printf(m, x);\
		else\
			pr_emerg(x);\
	} while (0)
#undef TAG
#define TAG "[PERFMGR]"

#define MAX_CORE (8)
#define MAX_FREQ (20000000)

struct touch_boost {
	spinlock_t touch_lock;
	wait_queue_head_t wq;
	struct task_struct *thread;
	int touch_event;
	atomic_t event;
};

/*--------------------------------------------*/

static struct touch_boost tboost;

static int perf_mgr_touch_enable = 1;
static int perf_mgr_touch_core = 1;
static int perf_mgr_touch_freq = 1;

/*--------------------FUNCTION----------------*/

static int tboost_thread(void *ptr)
{
	int event, core, freq;
	unsigned long flags;

	set_user_nice(current, -10);

	while (!kthread_should_stop()) {

		while (!atomic_read(&tboost.event))
			wait_event(tboost.wq, atomic_read(&tboost.event));
		atomic_dec(&tboost.event);

		spin_lock_irqsave(&tboost.touch_lock, flags);
		event = tboost.touch_event;
		core = perf_mgr_touch_core;
		freq = perf_mgr_touch_freq;
		spin_unlock_irqrestore(&tboost.touch_lock, flags);
#ifdef MTK_BOOST_SUPPORT
		perfmgr_boost(event, core, freq);
#endif
	}
	return 0;
}

static ssize_t perfmgr_tb_enable_write(struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	char buf[64];
	unsigned long val;
	int ret;
	unsigned long flags;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = 0;
	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (val > 1)
		return -1;

	spin_lock_irqsave(&tboost.touch_lock, flags);
	perf_mgr_touch_enable = val;
	spin_unlock_irqrestore(&tboost.touch_lock, flags);

	return cnt;
}

static int perfmgr_tb_enable_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "%d\n", perf_mgr_touch_enable);
	return 0;
}

static int perfmgr_tb_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, perfmgr_tb_enable_show, inode->i_private);
}

static const struct file_operations perfmgr_tb_enable_fops = {
	.open = perfmgr_tb_enable_open,
	.write = perfmgr_tb_enable_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static ssize_t perfmgr_tb_core_write(struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	char buf[64];
	unsigned long val;
	int ret;
	unsigned long flags;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = 0;
	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (val > MAX_CORE)
		return -1;

	spin_lock_irqsave(&tboost.touch_lock, flags);
	perf_mgr_touch_core = val;
	spin_unlock_irqrestore(&tboost.touch_lock, flags);

	return cnt;
}

static int perfmgr_tb_core_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "%d\n", perf_mgr_touch_core);
	return 0;
}

static int perfmgr_tb_core_open(struct inode *inode, struct file *file)
{
	return single_open(file, perfmgr_tb_core_show, inode->i_private);
}

static const struct file_operations perfmgr_tb_core_fops = {
	.open = perfmgr_tb_core_open,
	.write = perfmgr_tb_core_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static ssize_t perfmgr_tb_freq_write(struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	char buf[64];
	unsigned long val;
	int ret;
	unsigned long flags;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = 0;
	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (val > MAX_FREQ)
		return -1;

	spin_lock_irqsave(&tboost.touch_lock, flags);
	perf_mgr_touch_freq = val;
	spin_unlock_irqrestore(&tboost.touch_lock, flags);

	return cnt;
}

static int perfmgr_tb_freq_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "%d\n", perf_mgr_touch_freq);
	return 0;
}

static int perfmgr_tb_freq_open(struct inode *inode, struct file *file)
{
	return single_open(file, perfmgr_tb_freq_show, inode->i_private);
}

static const struct file_operations perfmgr_tb_freq_fops = {
	.open = perfmgr_tb_freq_open,
	.write = perfmgr_tb_freq_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void dbs_input_event(struct input_handle *handle, unsigned int type,
			    unsigned int code, int value)
{
	unsigned long flags;

	if (!perf_mgr_touch_enable)
		return;

	if ((type == EV_KEY) && (code == BTN_TOUCH)) {
		pr_debug(TAG"input cb, type:%d, code:%d, value:%d\n", type, code, value);
		spin_lock_irqsave(&tboost.touch_lock, flags);
		tboost.touch_event = value;
		spin_unlock_irqrestore(&tboost.touch_lock, flags);

		atomic_inc(&tboost.event);
		wake_up(&tboost.wq);
	}
}

static int dbs_input_connect(struct input_handler *handler,
			     struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);

	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "perfmgr";

	error = input_register_handle(handle);

	if (error)
		goto err2;

	error = input_open_device(handle);

	if (error)
		goto err1;

	return 0;
 err1:
	input_unregister_handle(handle);
 err2:
	kfree(handle);
	return error;
}

static void dbs_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id dbs_ids[] = {
	{.driver_info = 1},
	{},
};

static struct input_handler dbs_input_handler = {
	.event = dbs_input_event,
	.connect = dbs_input_connect,
	.disconnect = dbs_input_disconnect,
	.name = "cpufreq_ond",
	.id_table = dbs_ids,
};

/*--------------------INIT------------------------*/

int init_perfmgr_touch(void)
{
	struct proc_dir_entry *touch_dir = NULL;
	int handle;

#ifdef MTK_BOOST_SUPPORT
	perf_mgr_touch_core = perfmgr_get_target_core();
	perf_mgr_touch_freq = perfmgr_get_target_freq();
#endif

	touch_dir = proc_mkdir("perfmgr/touch", NULL);

	/* touch */
	proc_create("tb_enable", 0644, touch_dir, &perfmgr_tb_enable_fops);
	proc_create("tb_core", 0644, touch_dir, &perfmgr_tb_core_fops);
	proc_create("tb_freq", 0644, touch_dir, &perfmgr_tb_freq_fops);

	spin_lock_init(&tboost.touch_lock);
	init_waitqueue_head(&tboost.wq);
	atomic_set(&tboost.event, 0);
	tboost.thread = (struct task_struct *)kthread_run(tboost_thread, &tboost, "touch_boost");
	if (IS_ERR(tboost.thread))
		return -EINVAL;

	handle = input_register_handler(&dbs_input_handler);

	return 0;
}

int perfmgr_touch_suspend(void)
{
	/*pr_debug(TAG"perfmgr_touch_suspend\n");*/
#ifdef MTK_BOOST_SUPPORT
	perfmgr_boost(0, 0, 0);
#endif
	return 0;
}

/*MODULE_LICENSE("GPL");*/
/*MODULE_AUTHOR("MTK");*/
/*MODULE_DESCRIPTION("The fliper function");*/
