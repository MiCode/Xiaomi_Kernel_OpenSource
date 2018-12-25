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

#define pr_fmt(fmt) "[ktch]"fmt

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/input.h>

#include "tchbst.h"
#include "boost_ctrl.h"
#include "mtk_perfmgr_internal.h"



#define MAX_CORE (8)
#define MAX_FREQ (20000000)
#define TARGET_CORE (-1)
#define TARGET_FREQ (1183000)

struct boost {
	spinlock_t touch_lock;
	wait_queue_head_t wq;
	struct task_struct *thread;
	int touch_event;
	atomic_t event;
};

/*--------------------------------------------*/

static struct boost ktchboost;

static int ktch_mgr_enable = 1;
static int ktch_mgr_core = 1;
static int ktch_mgr_freq = 1;
static int ktch_mgr_clstr = 1;

/*--------------------FUNCTION----------------*/
int ktch_get_target_core(void)
{
	return TARGET_CORE;
}

int ktch_get_target_freq(void)
{
	return TARGET_FREQ;
}

void set_freq(int enable, int core, int freq)
{
	struct ppm_limit_data freq_to_set[perfmgr_clusters];
	int i, targetclu;

	targetclu = get_min_clstr_cap();

	for (i = 0 ; i < perfmgr_clusters ; i++) {
		freq_to_set[i].min = -1;
		freq_to_set[i].max = -1;
	}

	if (enable)
		freq_to_set[targetclu].min = freq;

	update_userlimit_cpu_freq(CPU_KIR_PERFTOUCH,
	 perfmgr_clusters, freq_to_set);
}

static int ktchboost_thread(void *ptr)
{
	int event, core, freq;
	unsigned long flags;

	set_user_nice(current, -10);

	while (!kthread_should_stop()) {

		while (!atomic_read(&ktchboost.event))
			wait_event(ktchboost.wq, atomic_read(&ktchboost.event));
		atomic_dec(&ktchboost.event);

		spin_lock_irqsave(&ktchboost.touch_lock, flags);
		event = ktchboost.touch_event;
		core = ktch_mgr_core;
		freq = ktch_mgr_freq;
		spin_unlock_irqrestore(&ktchboost.touch_lock, flags);
		pr_debug("ktchboost_thread\n");
		set_freq(event, core, freq);

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

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = 0;
	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (val > 1)
		return -1;

	spin_lock_irqsave(&ktchboost.touch_lock, flags);
	ktch_mgr_enable = val;
	spin_unlock_irqrestore(&ktchboost.touch_lock, flags);

	return cnt;
}

static int perfmgr_tb_enable_show(struct seq_file *m, void *v)
{
	if (m)
		seq_printf(m, "%d\n", ktch_mgr_enable);
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

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = 0;
	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (val > MAX_CORE)
		return -1;

	spin_lock_irqsave(&ktchboost.touch_lock, flags);
	ktch_mgr_core = val;
	spin_unlock_irqrestore(&ktchboost.touch_lock, flags);

	return cnt;
}

static int perfmgr_tb_core_show(struct seq_file *m, void *v)
{
	if (m)
		seq_printf(m, "%d\n", ktch_mgr_core);
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

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = 0;
	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (val > MAX_FREQ)
		return -1;

	spin_lock_irqsave(&ktchboost.touch_lock, flags);
	ktch_mgr_freq = val;
	spin_unlock_irqrestore(&ktchboost.touch_lock, flags);

	return cnt;
}

static int perfmgr_tb_freq_show(struct seq_file *m, void *v)
{
	if (m)
		seq_printf(m, "%d\n", ktch_mgr_freq);
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
static int perfmgr_tb_clstr_show(struct seq_file *m, void *v)
{
	if (m)
		seq_printf(m, "%d\n", ktch_mgr_clstr);
	return 0;
}

static int perfmgr_tb_clstr_open(struct inode *inode, struct file *file)
{
	return single_open(file, perfmgr_tb_clstr_show, inode->i_private);
}

static const struct file_operations perfmgr_tb_clstr_fops = {
	.open = perfmgr_tb_clstr_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
static void dbs_input_event(struct input_handle *handle, unsigned int type,
			    unsigned int code, int value)
{
	unsigned long flags;

	if (!ktch_mgr_enable)
		return;

	if ((type == EV_KEY) && (code == BTN_TOUCH)) {
		pr_debug("input cb, type:%d, code:%d, value:%d\n",
			 type, code, value);
		spin_lock_irqsave(&ktchboost.touch_lock, flags);
		ktchboost.touch_event = value;
		spin_unlock_irqrestore(&ktchboost.touch_lock, flags);

		atomic_inc(&ktchboost.event);
		wake_up(&ktchboost.wq);
	}
}

static int dbs_input_connect(struct input_handler *handler,
			     struct input_dev *dev,
			     const struct input_device_id *id)
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

int init_ktch(struct proc_dir_entry *parent)
{
	struct proc_dir_entry *ktch_root = NULL;
	struct proc_dir_entry *tbe_dir, *tbc_dir, *tbf_dir, *tbclstr_dir;
	int handle;

	pr_debug("init_ktch_touch\n");

	ktch_mgr_core = ktch_get_target_core();
	ktch_mgr_freq = ktch_get_target_freq();
	ktch_mgr_clstr = perfmgr_clusters;

	/*create kernel touch root file*/
	ktch_root = proc_mkdir("kernel", parent);

	if (!ktch_root)
		pr_debug("ktch_root not create\n");
	/* touch */
	tbe_dir = proc_create("tb_enable", 0644, ktch_root,
		 &perfmgr_tb_enable_fops);
	if (!tbe_dir)
		pr_debug("tbe_dir not create\n");
	tbc_dir = proc_create("tb_core", 0644, ktch_root,
		 &perfmgr_tb_core_fops);
	if (!tbc_dir)
		pr_debug("tbc_dir not create\n");

	tbf_dir = proc_create("tb_freq", 0644, ktch_root,
		 &perfmgr_tb_freq_fops);
	if (!tbf_dir)
		pr_debug("tbf_dir not create\n");
	tbclstr_dir = proc_create("tb_clstr", 0644, ktch_root,
		 &perfmgr_tb_clstr_fops);
	if (!tbclstr_dir)
		pr_debug("tbclstr_dir not create\n");

	spin_lock_init(&ktchboost.touch_lock);
	init_waitqueue_head(&ktchboost.wq);
	atomic_set(&ktchboost.event, 0);
	ktchboost.thread = (struct task_struct *)kthread_run(ktchboost_thread,
						 &ktchboost, "touch_boost");
	if (IS_ERR(ktchboost.thread))
		return -EINVAL;

	handle = input_register_handler(&dbs_input_handler);

	return 0;
}

int ktch_suspend(void)
{
	/*pr_debug(TAG"perfmgr_touch_suspend\n");*/

	set_freq(0, 0, 0);

	return 0;
}
