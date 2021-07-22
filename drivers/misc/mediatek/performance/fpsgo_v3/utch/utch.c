// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[usrtch]"fmt

#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include <linux/sort.h>

#include "utch.h"
#include <mt-plat/fpsgo_common.h>
#include "fstb.h"

#define DEBUG_LOG	0

/* variable definition */
static void notify_touch_up_timeout(struct work_struct *work);
static DECLARE_WORK(mt_touch_timeout_work, (void *) notify_touch_up_timeout);

static struct workqueue_struct *wq;
static struct mutex notify_lock;
static struct hrtimer hrt1;

static int usrtch_dbg;
static int touch_boost_opp; /* boost freq of touch boost */
static int *cluster_opp, *opp_count;
static unsigned int **opp_tbl;
static int *target_freq;
static int touch_boost_duration;
static long long active_time;
static int time_to_last_touch;
static int deboost_when_render;
static int usrtch_debug;
static int touch_event;/*touch down:1 */
static ktime_t last_touch_time;

struct cpufreq_policy **tchbst_policy;
struct freq_qos_request *tchbst_rq;

static int policy_num;

int powerhal_tid;


/* local function */
static noinline int tracing_mark_write(const char *buf)
{
	trace_printk(buf);
	return 0;
}

static void __utch_systrace(int val, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;
	char buf2[256];

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		log[255] = '\0';

	len = snprintf(buf2, sizeof(buf2), "C|%d|%s|%d\n", powerhal_tid, log, val);
	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf2[255] = '\0';
	tracing_mark_write(buf2);
}

static int cmp_uint(const void *a, const void *b)
{
	return *(unsigned int *)b - *(unsigned int *)a;
}

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
	for (i = 0; i < policy_num; i++)
		if (touch_boost_opp < opp_count[i])
			target_freq[i] = opp_tbl[i][touch_boost_opp];
}

void switch_cluster_opp(int id, int boost_opp)
{
	if (id < 0 || id >= policy_num || boost_opp < -2)
		return;

	cluster_opp[id] = boost_opp;

	if (boost_opp == -2) /* don't boost */
		target_freq[id] = 0;
	else if (boost_opp == -1) /* use touch_boost_opp */
		target_freq[id] = opp_tbl[id][touch_boost_opp];
	else /* use boost_opp */
		target_freq[id] = opp_tbl[id][boost_opp];
}

void switch_init_duration(int duration)
{
	touch_boost_duration = duration;
}

void switch_active_time(int duration)
{
	active_time = duration;
}

void switch_time_to_last_touch(int duration)
{
	time_to_last_touch = duration;
}

void switch_deboost_when_render(int enable)
{
	deboost_when_render = !!enable;
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
int notify_touch(int action)
{
	int ret = 0, i;
	int isact = 0;
	ktime_t now, delta;

	if (!deboost_when_render && action == 3)
		return ret;

	if (action != 3) {
		now = ktime_get();
		delta = ktime_sub(now, last_touch_time);
		last_touch_time = now;

		/* lock is mandatory*/
		WARN_ON(!mutex_is_locked(&notify_lock));
#if defined(CONFIG_MTK_FPSGO_V3)
		isact = is_fstb_active(active_time);
		__utch_systrace(isact, "utch isact");
#endif
		if ((isact && ktime_to_ms(delta) < time_to_last_touch) ||
				usrtch_dbg)
			return ret;
	}

	/*action 1: touch down 2: touch up*/
	/* -> 3: fpsgo active*/
	if (action == 1) {
		disable_touch_boost_timer();
		enable_touch_boost_timer();

		/* boost */
		for (i = 0; i < policy_num; i++)
			freq_qos_update_request(&(tchbst_rq[i]), target_freq[i]);

		if (usrtch_debug)
			pr_debug("touch down\n");

		__utch_systrace(1, "utch touch");

		touch_event = 1;
	} else if (touch_event == 1 && action == 3) {
		disable_touch_boost_timer();
		for (i = 0; i < policy_num; i++)
			freq_qos_update_request(&(tchbst_rq[i]), 0);

		__utch_systrace(3, "utch touch");

		touch_event = 2;
		if (usrtch_debug)
			pr_debug("touch timeout\n");
	}

	return ret;
}


static void notify_touch_up_timeout(struct work_struct *work)
{
	int i;

	mutex_lock(&notify_lock);

	for (i = 0; i < policy_num; i++)
		freq_qos_update_request(&(tchbst_rq[i]), 0);

	__utch_systrace(0, "utch touch");

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
	int arg1, arg2;

	arg1 = 0;
	arg2 = -1;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = '\0';

	if (sscanf(buf, "%31s %d %d", cmd, &arg1, &arg2) < 2)
		return -EFAULT;

	if (strncmp(cmd, "enable", 6) == 0)
		switch_usrtch(arg1);
	else if (strncmp(cmd, "touch_opp", 9) == 0) {
		if (arg1 >= 0 && arg1 <= 15)
			switch_init_opp(arg1);
	} else if (strncmp(cmd, "cluster_opp", 11) == 0) {
		if (arg1 >= 0 && arg1 < policy_num && arg2 >= -2 && arg2 <= 15)
			switch_cluster_opp(arg1, arg2);
	} else if (strncmp(cmd, "duration", 8) == 0) {
		switch_init_duration(arg1);
	} else if (strncmp(cmd, "active_time", 11) == 0) {
		if (arg1 >= 0)
			switch_active_time(arg1);
	} else if (strncmp(cmd, "time_to_last_touch", 18) == 0) {
		if (arg1 >= 0)
			switch_time_to_last_touch(arg1);
	} else if (strncmp(cmd, "deboost_when_render", 19) == 0) {
		if (arg1 >= 0)
			switch_deboost_when_render(arg1);
	}
	return cnt;
}

static int device_show(struct seq_file *m, void *v)
{
	int i;

	seq_puts(m, "-----------------------------------------------------\n");
	seq_printf(m, "enable:\t%d\n", !usrtch_dbg);
	seq_printf(m, "touch_opp:\t%d\n", touch_boost_opp);

	for (i = 0; i < policy_num; i++)
		seq_printf(m, "cluster_opp[%d]:\t%d\n",
		i, cluster_opp[i]);

	seq_printf(m, "duration(ns):\t%d\n", touch_boost_duration);
	seq_printf(m, "active_time(us):\t%d\n", (int)active_time);
	seq_printf(m, "time_to_last_touch(ms):\t%d\n", time_to_last_touch);
	seq_printf(m, "deboost_when_render:\t%d\n", deboost_when_render);
	seq_printf(m, "touch_event:\t%d\n", touch_event);
	seq_puts(m, "-----------------------------------------------------\n");
	return 0;
}

static int device_open(struct inode *inode, struct file *file)
{
	return single_open(file, device_show, inode->i_private);
}

int usrtch_ioctl(unsigned long arg)
{
	int ret;

	if (unlikely(powerhal_tid == 0))
		powerhal_tid = current->pid;

	mutex_lock(&notify_lock);
	ret = notify_touch(arg);
	mutex_unlock(&notify_lock);

	return ret >= 0 ? ret : 0;
}

static const struct proc_ops Fops = {
	.proc_open = device_open,
	.proc_write = device_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
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
static const struct proc_ops fop = {
	.proc_open = mt_usrdebug_open,
	.proc_write = mt_usrdebug_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

/*--------------------INIT------------------------*/
static int usrtch_init(struct proc_dir_entry *parent)
{
	struct proc_dir_entry *usrtch, *usrdebug, *usrtch_root, *tch_root;
	int i;
	int ret_val = 0;

	pr_debug("Start to init usrtch  driver\n");

	/*create usr touch root procfs*/
	tch_root = proc_mkdir("tchbst", parent);
	usrtch_root = proc_mkdir("user", tch_root);

	touch_event = 2;
	touch_boost_opp = TOUCH_BOOST_OPP;
	touch_boost_duration = TOUCH_TIMEOUT_NSEC;
	active_time = TOUCH_FSTB_ACTIVE_US;
	time_to_last_touch = TOUCH_TIME_TO_LAST_TOUCH_MS;
	last_touch_time = ktime_get();

	pr_info("%s, policy_num:%d", __func__, policy_num);

	target_freq = kcalloc(policy_num, sizeof(int), GFP_KERNEL);
	cluster_opp = kcalloc(policy_num, sizeof(int), GFP_KERNEL);

	for (i = 0; i < policy_num; i++) {
		target_freq[i] = opp_tbl[i][touch_boost_opp];
		cluster_opp[i] = -1; /* depend on touch_boost_opp */
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


void exit_utch_mod(void)
{
	int i;

	for (i = 0; i < policy_num; i++) {
		freq_qos_remove_request(&(tchbst_rq[i]));
		if (opp_tbl)
			kfree(opp_tbl[i]);
	}

	kfree(tchbst_policy);
	kfree(tchbst_rq);
	kfree(opp_count);
	kfree(opp_tbl);
}

int init_utch_mod(void)
{
	int cpu;
	int num = 0, count;
	struct cpufreq_policy *policy;
	struct cpufreq_frequency_table *pos;

	/* query policy number */
	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);

		if (policy) {
			pr_info("%s, policy[%d]: first:%d, min:%d, max:%d",
				__func__, num, cpu, policy->min, policy->max);

			num++;
			cpu = cpumask_last(policy->related_cpus);
			cpufreq_cpu_put(policy);
		}
	}

	policy_num = num;

	if (policy_num == 0) {
		pr_info("%s, no policy", __func__);
		return 0;
	}

	/* register callback */
	usrtch_ioctl_fp = usrtch_ioctl;

	tchbst_policy = kcalloc(policy_num,	sizeof(struct cpufreq_policy *), GFP_KERNEL);
	tchbst_rq = kcalloc(policy_num,	sizeof(struct freq_qos_request), GFP_KERNEL);
	opp_count = kcalloc(policy_num,	sizeof(int), GFP_KERNEL);
	opp_tbl = kcalloc(policy_num,	sizeof(int *), GFP_KERNEL);

	num = 0;
	for_each_possible_cpu(cpu) {
		if (num >= policy_num)
			break;

		policy = cpufreq_cpu_get(cpu);

		if (!policy)
			continue;

		tchbst_policy[num] = policy;
#if DEBUG_LOG
		pr_info("%s, policy[%d]: first:%d, sort:%d",
			__func__, num, cpu, (int)policy->freq_table_sorted);
#endif

		/* calc opp count */
		count = 0;
		cpufreq_for_each_entry(pos, policy->freq_table) {
#if DEBUG_LOG
			pr_info("%s, [%d]:%d", __func__, count, pos->frequency);
#endif
			count++;
		}
		opp_count[num] = count;
		pr_info("%s, policy[%d]: opp_count:%d", __func__, num, opp_count[num]);

		opp_tbl[num] = kcalloc(count, sizeof(int), GFP_KERNEL);
		count = 0;
		cpufreq_for_each_entry(pos, policy->freq_table) {
			opp_tbl[num][count] = pos->frequency;
			count++;
		}

		sort(opp_tbl[num], opp_count[num], sizeof(unsigned int), cmp_uint, NULL);
#if DEBUG_LOG
		for (i = 0; i < opp_count[num]; i++) {
			pr_info("%s, policy[%d]: opp[%d]:%d",
				__func__, num, i, opp_tbl[num][i]);
		}
#endif
		/* freq QoS */
		freq_qos_add_request(&policy->constraints, &(tchbst_rq[num]), FREQ_QOS_MIN, 0);

		num++;
		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}

	/* init procfs */
	if (perfmgr_root)
		usrtch_init(perfmgr_root);

	return 0;
}

