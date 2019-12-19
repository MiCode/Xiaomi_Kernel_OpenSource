/*
 * Copyright (c) Xiaomi Technologies Co., Ltd. 2019. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * File name: migt.c
 * Descrviption: cpufreq boost function.
 * Author: guchao1@xiaomi.com
 * Version: 2.0
 * Date:  2019/01/29
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "migt: " fmt

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/miscdevice.h>
#include <linux/security.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/uaccess.h>
#include <linux/hrtimer.h>

#define FEED_DOG 1
#define SET_CEILING 2
#define CEILING_RESTORE 3

struct migt {
	int cpu;
	unsigned int migt_min;
	unsigned int boost_freq;
	unsigned int migt_ceiling_max;
	unsigned int ceiling_freq;
};

enum freq_type {
	boost_freq,
	ceiling_freq
};

static bool migt_enabled;
static cpumask_var_t active_cluster_cpus;
static struct hrtimer hrtimer;
static DEFINE_PER_CPU(struct migt, migt_info);
static struct workqueue_struct *migt_wq;
static struct delayed_work migt_work;
static struct work_struct irq_boost_work;
static struct delayed_work migt_rem;
static u64 last_update_time;
static unsigned int high_resolution_enable = 1;
module_param(high_resolution_enable, uint, 0644);
static unsigned int migt_debug;
module_param(migt_debug, uint, 0644);
static unsigned int migt_ms = 50;
module_param(migt_ms, uint, 0644);
static unsigned int migt_thresh = 18;
module_param(migt_thresh, uint, 0644);
static unsigned int over_thresh_count;
module_param(over_thresh_count, uint, 0644);

static int set_migt_freq(const char *buf, const struct kernel_param *kp)
{
	int i, ntokens = 0;
	unsigned int val, cpu;
	const char *cp = buf;
	bool enabled = false;
	enum freq_type type;

	if (strstr(kp->name, "migt_freq"))
		type = boost_freq;
	if (strstr(kp->name, "migt_ceiling_freq"))
		type = ceiling_freq;

	while ((cp = strpbrk(cp + 1, " :")))
		ntokens++;

	if (!ntokens) {
		if (sscanf(buf, "%u\n", &val) != 1)
			return -EINVAL;

		for_each_possible_cpu(i) {
			if (type == boost_freq)
				per_cpu(migt_info, i).boost_freq = val;
			else
				per_cpu(migt_info, i).ceiling_freq = val;
		}
		goto check_enable;
	}

	if (!(ntokens % 2))
		return -EINVAL;

	cp = buf;
	for (i = 0; i < ntokens; i += 2) {
		if (sscanf(cp, "%u:%u", &cpu, &val) != 2)
			return -EINVAL;

		if (cpu >= num_possible_cpus())
			return -EINVAL;

		if (type == boost_freq)
			per_cpu(migt_info, cpu).boost_freq = val;
		else
			per_cpu(migt_info, cpu).ceiling_freq = val;

		cp = strchr(cp, ' ');
		cp++;
	}

check_enable:
	for_each_possible_cpu(i) {
		if (per_cpu(migt_info, i).boost_freq) {
			enabled = true;
			break;
		}
	}
	migt_enabled = enabled;
	return 0;
}

static int get_migt_freq(char *buf, const struct kernel_param *kp)
{
	int cnt = 0, cpu;
	struct migt *s;
	unsigned int freq = 0;
	enum freq_type type;

	if (strstr(kp->name, "migt_freq"))
		type = boost_freq;
	if (strstr(kp->name, "migt_ceiling_freq"))
		type = ceiling_freq;

	for_each_possible_cpu(cpu) {
		s = &per_cpu(migt_info, cpu);
		if (type == boost_freq)
			freq = s->boost_freq;
		else
			freq = s->ceiling_freq;

		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"%d:%u ", cpu, freq);
	}
	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, "\n");
	return cnt;
}

static const struct kernel_param_ops param_ops_migt_freq = {
	.set = set_migt_freq,
	.get = get_migt_freq,
};

module_param_cb(migt_freq, &param_ops_migt_freq, NULL, 0644);
module_param_cb(migt_ceiling_freq, &param_ops_migt_freq, NULL, 0644);

/*
 * Used to override the current policy min to boost cpu freq during app launch/
 * package install or other performance scenaries,
 * The cpufreq framework then does the job
 * of enforcing the new policy.
 */
static int boost_adjust_notify(struct notifier_block *nb, unsigned long val,
				void *data)
{
	struct cpufreq_policy *policy = data;
	unsigned int cpu = policy->cpu;
	struct migt *s = &per_cpu(migt_info, cpu);
	unsigned int min_freq = s->migt_min;

	switch (val) {
	case CPUFREQ_ADJUST:
		if (!min_freq)
			break;

		if (migt_debug) {
			pr_debug("CPU%u policy min before boost: %u kHz\n", cpu, policy->min);
			pr_debug("CPU%u boost min: %u kHz\n", cpu, min_freq);
		}

		cpufreq_verify_within_limits(policy, min_freq, UINT_MAX);
		if (migt_debug)
			pr_debug("CPU%u policy min after boost: %u kHz\n", cpu, policy->min);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block boost_adjust_nb = {
	.notifier_call = boost_adjust_notify,
};

static int ceiling_adjust_notify(struct notifier_block *nb, unsigned long val,
				void *data)
{
	struct cpufreq_policy *policy = data;
	unsigned int cpu = policy->cpu;
	struct migt *s = &per_cpu(migt_info, cpu);
	unsigned int max_freq = s->migt_ceiling_max;

	switch (val) {
	case CPUFREQ_ADJUST:
		if (!max_freq && (max_freq != UINT_MAX))
			break;

		if (migt_debug) {
			pr_debug("CPU%u policy max before boost: %u kHz\n", cpu, policy->max);
			pr_debug("CPU%u boost max: %u kHz\n", cpu, max_freq);
		}

		cpufreq_verify_within_limits(policy, 0, max_freq);
		if (migt_debug)
			pr_debug("CPU%u policy max after boost: %u kHz\n", cpu, policy->max);

		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block ceiling_adjust_nb = {
	.notifier_call = ceiling_adjust_notify,
};

static void update_policy_online(void)
{
	unsigned int i;
	const struct cpumask *cluster_cpus;
	struct cpufreq_policy *policy;

	get_online_cpus();
	for_each_online_cpu(i) {
		cluster_cpus = cpu_coregroup_mask(i);
		cpumask_and(active_cluster_cpus, cluster_cpus, cpu_online_mask);
		if (i != cpumask_first(active_cluster_cpus))
			continue;

		if (migt_debug)
			pr_debug("Updating policy for CPU %d\n", i);

		policy = cpufreq_cpu_get(i);
		if (policy) {
			cpufreq_update_policy(i);
			cpufreq_cpu_put(policy);
		}
	}
	put_online_cpus();
}

static void do_migt_rem(struct work_struct *work)
{
	unsigned int i;
	struct migt *i_migt_info;

	if (migt_debug)
		pr_err("migt boost comming...%d %llu",
				__LINE__, ktime_to_us(ktime_get()));

	for_each_possible_cpu(i) {
		i_migt_info = &per_cpu(migt_info, i);
		i_migt_info->migt_min = 0;
	}

	/* Update policies for all online CPUs */
	update_policy_online();
}

static void do_migt_ceiling_restore(void)
{
	unsigned int i;
	struct migt *i_migt_info;

	for_each_possible_cpu(i) {
		i_migt_info = &per_cpu(migt_info, i);
		i_migt_info->migt_ceiling_max = UINT_MAX;
	}

	/* Update policies for all online CPUs */
	update_policy_online();
}

static void do_migt(struct work_struct *work)
{
	unsigned int i;
	struct migt *i_migt_info;

	cancel_delayed_work_sync(&migt_rem);
	if (migt_debug)
		pr_err("migt boost comming....%d %llu",
				__LINE__, ktime_to_us(ktime_get()));

	for_each_possible_cpu(i) {
		i_migt_info = &per_cpu(migt_info, i);
		i_migt_info->migt_min = i_migt_info->boost_freq;
	}

	/* Update policies for all online CPUs */
	update_policy_online();
	queue_delayed_work(migt_wq, &migt_rem,
		msecs_to_jiffies(migt_ms));
}

static void do_migt_ceiling_limit(void)
{
	unsigned int i;
	struct migt *i_migt_info;

	for_each_possible_cpu(i) {
		i_migt_info = &per_cpu(migt_info, i);
		i_migt_info->migt_ceiling_max = i_migt_info->ceiling_freq;
	}

	/* Update policies for all online CPUs */
	update_policy_online();
}

static enum hrtimer_restart do_boost_work(struct hrtimer *timer)
{
	if (work_pending(&irq_boost_work))
		return HRTIMER_NORESTART;

	queue_work(migt_wq,
			&irq_boost_work);

	return HRTIMER_NORESTART;
}

static void trigger_migt_in_hrtimer(int timeout_ms)
{
	/* If the timer is already running, stop it */
	if (hrtimer_active(&hrtimer))
		hrtimer_cancel(&hrtimer);

	hrtimer_start(&hrtimer, ms_to_ktime(timeout_ms),
			HRTIMER_MODE_REL);
}

static void trigger_migt(void)
{
	u64 now;

	if (!migt_enabled)
		return;

	now = ktime_get_ns();
	if (now - last_update_time >= migt_thresh * NSEC_PER_MSEC)
		over_thresh_count++;

	last_update_time = now;

	if (high_resolution_enable)
		return trigger_migt_in_hrtimer(migt_thresh);

	cancel_delayed_work_sync(&migt_work);

	queue_delayed_work(migt_wq, &migt_work,
		msecs_to_jiffies(migt_thresh));

}

static int migt_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int migt_release(struct inode *ignored, struct file *file)
{
	return 0;
}

static int migt_mmap(struct file *file, struct vm_area_struct *vma)
{
	return 0;
	/*remap_pfn_range(vma, vma->vm_start,
				    page_to_pfn(virt_to_page(migt_zone)),
				    PAGE_SIZE, PAGE_SHARED)) {*/
}

static long migt_ioctl(struct file *fp, unsigned int cmd,
				 unsigned long arg)
{
	if (migt_debug)
		pr_err("migt boost comming. %d %d %llu",
				__LINE__, cmd, ktime_to_us(ktime_get()));

	switch (cmd) {
	case FEED_DOG:
		trigger_migt();
		break;
	case SET_CEILING:
		do_migt_ceiling_limit();
		break;
	case CEILING_RESTORE:
		do_migt_ceiling_restore();
		break;
	}
	return 0;
}

static const struct file_operations migt_fops = {
	.owner = THIS_MODULE,
	.open = migt_open,
	.release = migt_release,
	.mmap = migt_mmap,
	.unlocked_ioctl = migt_ioctl,
};

static struct miscdevice migt_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "migt",
	.fops = &migt_fops,
};

static int migt_init(void)
{
	int cpu, ret;
	struct migt *s;

	if (!alloc_cpumask_var(&active_cluster_cpus, GFP_KERNEL|__GFP_ZERO))
		return -ENOMEM;

	hrtimer_init(&hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer.function = do_boost_work;

	migt_wq = alloc_workqueue("migt_wq", WQ_HIGHPRI, 0);
	if (!migt_wq) {
		free_cpumask_var(active_cluster_cpus);
		return -EFAULT;
	}

	ret = misc_register(&migt_misc);
	if (unlikely(ret)){
		free_cpumask_var(active_cluster_cpus);
		flush_workqueue(migt_wq);
		destroy_workqueue(migt_wq);
		return ret;
	}

	INIT_DELAYED_WORK(&migt_work, do_migt);
	INIT_WORK(&irq_boost_work, do_migt);
	INIT_DELAYED_WORK(&migt_rem, do_migt_rem);
	for_each_possible_cpu(cpu) {
		s = &per_cpu(migt_info, cpu);
		s->cpu = cpu;
		s->migt_ceiling_max = UINT_MAX;
	}

	cpufreq_register_notifier(&boost_adjust_nb, CPUFREQ_POLICY_NOTIFIER);
	cpufreq_register_notifier(&ceiling_adjust_nb, CPUFREQ_POLICY_NOTIFIER);
	return 0;
}
late_initcall(migt_init);
