// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#define pr_fmt(fmt) "[usrtch]"fmt

#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <mt-plat/fpsgo_common.h>


#include "tchbst.h"
#include "fstb.h"
#include "mtk_perfmgr_internal.h"

static void notify_touch_up_timeout(struct work_struct *work);
static DECLARE_WORK(mt_touch_timeout_work, (void *) notify_touch_up_timeout);

static struct workqueue_struct *wq;
static struct mutex notify_lock;
static struct hrtimer hrt1;

static int usrtch_dbg;
static int  touch_boost_value;
static int touch_boost_opp; /* boost freq of touch boost */
static struct cpu_ctrl_data *target_freq, *reset_freq;
static int touch_boost_duration;
static int prev_boost_pid;
static long long active_time;
static int usrtch_debug;
static int touch_event;/*touch down:1 */

void switch_usrtch(int enable)
{
	mutex_lock(&notify_lock);
	usrtch_dbg = !enable;
	mutex_unlock(&notify_lock);
}

void switch_init_opp(int boost_opp)
{
	int i;

	touch_boost_opp = boost_opp;
	for (i = 0; i < perfmgr_clusters; i++)
		target_freq[i].min =
			perfmgr_cpufreq_get_freq_by_idx(i, touch_boost_opp);
}

void switch_init_duration(int duration)
{
	touch_boost_duration = duration;
}
void switch_active_time(int duration)
{
	active_time = duration;
}

/*--------------------TIMER------------------------*/
static void enable_touch_boost_timer(void)
{
	ktime_t ktime;

	ktime = ktime_set(0, touch_boost_duration);
	hrtimer_start(&hrt1, ktime, HRTIMER_MODE_REL);
	if (usrtch_debug)
		pr_debug("touch_boost_duration:\t %d\n", touch_boost_duration);

}

static void disable_touch_boost_timer(void)
{
	hrtimer_cancel(&hrt1);
}


static enum hrtimer_restart mt_touch_timeout(struct hrtimer *timer)
{
	if (wq)
		queue_work(wq, &mt_touch_timeout_work);

	return HRTIMER_NORESTART;
}

/*--------------------FRAME HINT OP------------------------*/
static int notify_touch(int action)
{
	int ret = 0;
	int isact = 0;
	/* lock is mandatory*/
	WARN_ON(!mutex_is_locked(&notify_lock));
	isact = is_fstb_active(active_time);

	prev_boost_pid = current->pid;
	fpsgo_systrace_c_fbt((pid_t)prev_boost_pid, isact, "isact");

	if (is_fstb_active(active_time) || usrtch_dbg)
		return ret;

	/*action 1: touch down 2: touch up*/
	if (action == 1) {
		disable_touch_boost_timer();
		enable_touch_boost_timer();

		/* boost */
#ifdef CONFIG_MTK_SCHED_EXTENSION
		update_eas_uclamp_min(EAS_KIR_TOUCH,
				CGROUP_TA, touch_boost_value);
#endif
#ifdef CONFIG_MTK_PPM
		update_userlimit_cpu_freq(CPU_KIR_TOUCH,
				perfmgr_clusters, target_freq);
#endif
		if (usrtch_debug)
			pr_debug("touch down\n");
		fpsgo_systrace_c_fbt((pid_t)prev_boost_pid, 1, "touch");
		touch_event = 1;
	}



	return ret;
}
void switch_init_boost(int boost_value)
{
	touch_boost_value = boost_value;
}
static void notify_touch_up_timeout(struct work_struct *work)
{
	mutex_lock(&notify_lock);

#ifdef CONFIG_MTK_SCHED_EXTENSION
	update_eas_uclamp_min(EAS_KIR_TOUCH, CGROUP_TA, 0);
#endif
#ifdef CONFIG_MTK_PPM
	update_userlimit_cpu_freq(CPU_KIR_TOUCH, perfmgr_clusters, reset_freq);
#endif
	fpsgo_systrace_c_fbt((pid_t)prev_boost_pid, 0, "touch");
	touch_event = 2;
	if (usrtch_debug)
		pr_debug("touch timeout\n");

	mutex_unlock(&notify_lock);
}

/*--------------------DEV OP------------------------*/
static ssize_t device_write(struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	char buf[32], cmd[32];
	int arg;

	arg = 0;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = '\0';

	if (sscanf(buf, "%31s %d", cmd, &arg) != 2)
		return -EFAULT;

	if (strncmp(cmd, "enable", 6) == 0)
		switch_usrtch(arg);
	else if (strncmp(cmd, "init", 4) == 0)
		switch_init_boost(arg);
	else if (strncmp(cmd, "touch_opp", 9) == 0) {
		if (arg >= 0 && arg <= 15)
			switch_init_opp(arg);
	} else if (strncmp(cmd, "duration", 8) == 0) {
		switch_init_duration(arg);
	} else if (strncmp(cmd, "active_time", 11) == 0) {
		if (arg > 0)
			switch_active_time(arg);
	}
	return cnt;
}

static int device_show(struct seq_file *m, void *v)
{
	seq_puts(m, "-----------------------------------------------------\n");
	seq_printf(m, "enable:\t%d\n", !usrtch_dbg);
	seq_printf(m, "init:\t%d\n", touch_boost_value);
	seq_printf(m, "touch_opp:\t%d\n", touch_boost_opp);
	seq_printf(m, "duration:\t%d\n", touch_boost_duration);
	seq_printf(m, "active_time:\t%d\n", (int)active_time);
	seq_printf(m, "touch_event:\t%d\n", touch_event);
	seq_puts(m, "-----------------------------------------------------\n");
	return 0;
}

static int device_open(struct inode *inode, struct file *file)
{
	return single_open(file, device_show, inode->i_private);
}

long usrtch_ioctl(unsigned int cmd, unsigned long arg)
{
	ssize_t ret = 0;

	mutex_lock(&notify_lock);

	switch (cmd) {
		/*receive touch info*/
	case FPSGO_TOUCH:
		ret = notify_touch(arg);
		break;

	default:
		pr_debug("non-game unknown cmd %u\n", cmd);
		ret = -1;
		goto ret_ioctl;
	}

ret_ioctl:
	mutex_unlock(&notify_lock);
	return ret >= 0 ? ret : 0;
}

static const struct file_operations Fops = {
	.open = device_open,
	.write = device_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
/*--------------------usrdebug OP------------------------*/
static ssize_t mt_usrdebug_write(struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	char buf[32];
	int arg, ret, val;

	arg = 0;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = '\0';
	ret = kstrtoint(buf, 10, &val);

	if (ret < 0) {
		pr_debug("ddr_write ret < 0\n");
		return ret;
	}

	usrtch_debug = val;
	return cnt;
}

static int mt_usrdebug_show(struct seq_file *m, void *v)
{
	seq_printf(m, "usrdebug\t%d\n", usrtch_debug);
	return 0;
}

static int mt_usrdebug_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_usrdebug_show, inode->i_private);
}
static const struct file_operations fop = {
	.open = mt_usrdebug_open,
	.write = mt_usrdebug_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
/*--------------------INIT------------------------*/
int init_utch(struct proc_dir_entry *parent)
{
	struct proc_dir_entry *usrtch, *usrdebug, *usrtch_root;
	int i;
	int ret_val = 0;

	pr_debug("Start to init usrtch  driver\n");

	/*create usr touch root procfs*/
	usrtch_root = proc_mkdir("user", parent);

	touch_boost_value = TOUCH_BOOST_EAS;

	touch_event = 2;
	touch_boost_opp = TOUCH_BOOST_OPP;
	touch_boost_duration = TOUCH_TIMEOUT_NSEC;
	active_time = TOUCH_FSTB_ACTIVE_US;

	target_freq = kcalloc(perfmgr_clusters,
			sizeof(struct cpu_ctrl_data), GFP_KERNEL);
	reset_freq = kcalloc(perfmgr_clusters,
			sizeof(struct cpu_ctrl_data), GFP_KERNEL);

	for (i = 0; i < perfmgr_clusters; i++) {
		target_freq[i].min =
			perfmgr_cpufreq_get_freq_by_idx(i, touch_boost_opp);
		target_freq[i].max = reset_freq[i].min = reset_freq[i].max = -1;
	}
	mutex_init(&notify_lock);

	wq = create_singlethread_workqueue("mt_usrtch__work");
	if (!wq) {
		pr_debug("work create fail\n");
		return -ENOMEM;
	}

	hrtimer_init(&hrt1, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrt1.function = &mt_touch_timeout;

	usrtch = proc_create("usrtch", 0664, usrtch_root, &Fops);
	if (!usrtch) {
		ret_val = -ENOMEM;
		goto out_chrdev;
	}
	usrdebug = proc_create("usrdebug", 0664, usrtch_root, &fop);
	if (!usrdebug) {
		ret_val = -ENOMEM;
		goto out_chrdev;
	}
	pr_debug("init usrtch  driver done\n");

	return 0;

out_chrdev:
	destroy_workqueue(wq);
	return ret_val;
}
