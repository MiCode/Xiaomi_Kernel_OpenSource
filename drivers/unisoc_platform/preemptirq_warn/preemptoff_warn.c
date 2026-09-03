// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "preemptoff_warn: " fmt
#include <linux/debugfs.h>
#include <linux/ftrace.h>
#include <linux/kallsyms.h>
#include <linux/math64.h>
#include <linux/percpu.h>
#include <linux/seq_file.h>
#include <linux/trace.h>
#include <trace/hooks/preemptirq.h>

#include "preemptirq_timing.h"

#define PREEMPTOFF_WARN_VAL_DEF (30 * NSEC_PER_MSEC)

static DEFINE_STATIC_KEY_TRUE(preemptoff_timing_off);
static DEFINE_PER_CPU(struct preemptirq_info, preemptoff_info);
static struct preemptirq_settings preemptoff_settings;
static struct dentry *preemptoff_dir;
noinline void preemptoff_warn(int cpu, u64 preemptoff_t)
{
	u32 off_us, extra_us, start_us;
	int i;
	void *ip;
	char buff[500], *p;
	struct preemptirq_info *info = per_cpu_ptr(&preemptoff_info, cpu);

	/* print time info */
	off_us = do_div(preemptoff_t, NSEC_PER_MSEC) / NSEC_PER_USEC;
	start_us = do_div(info->start_ts, NSEC_PER_SEC) / NSEC_PER_USEC;
	if (!info->extra_time) {
		/* thread <T> disable preempt on core <C> for <D>ms from <F>s */
		pr_warn("C%d T:<%d>%s D:%llu.%03ums F:%llu.%06us\n",
		cpu, info->task->pid, info->task->comm,
		preemptoff_t, off_us, info->start_ts, start_us);
	} else {
		extra_us = do_div(info->extra_time, NSEC_PER_MSEC);
		extra_us /= NSEC_PER_USEC;
		/* <E> means extra time spent in printk and idle, included in <D> */
		pr_warn("C%d T:<%d>%s D:%llu.%03ums F:%llu.%06us E:%llu.%03u ms\n",
			cpu, info->task->pid, info->task->comm,
			preemptoff_t, off_us, info->start_ts, start_us,
			info->extra_time, extra_us);
	}

	/* print thread name and backtrace of disabling */
	p = buff;
	p += sprintf(p, "C%d disabled preempt at:\n", cpu);
	for (i = 0; i < 5; i++) {
		ip = info->callback[i];
		if (!ip)
			break;
		p += sprintf(p, "%pS\n", ip);
	}
	pr_warn("%s", buff);

	/* print thread name and backtrace of enabling */
	p = buff;
	p += sprintf(p, "C%d enabled preempt at:\n", cpu);
	for (i = 0; i < 5; i++) {
		ip = return_address(i+2);
		if (!ip)
			break;
		p += sprintf(p, "%pS\n", ip);
	}
	pr_warn("%s", buff);
#if !defined(CONFIG_FRAME_POINTER) || defined(CONFIG_ARM_UNWIND)
	/*
	 * return_address() may not work on arm, try to show_stack() to get
	 * more information. NOT use dump_stack() here!
	 */
	show_stack(NULL, NULL, KERN_WARNING);
#endif
}

noinline void start_preemptoff_timing(void *a, unsigned long ip, unsigned long parent_ip)
{
	struct preemptirq_info *info;
	int i;

	if (static_branch_unlikely(&preemptoff_timing_off))
		return;

	if (is_idle_task(current) || oops_in_progress)
		return;

	info = this_cpu_ptr(&preemptoff_info);
	/* Return if timing has already started */
	if (info->start_ts)
		return;

	info->start_ts = timing_clock();
	info->task = current;
	info->pid = current->pid;
	info->ncsw = current->nvcsw + current->nivcsw;
	for (i = 0; i < 5; i++)
		info->callback[i] = return_address(i+1);
}

noinline void stop_preemptoff_timing(void *a, unsigned long ip, unsigned long parent_ip)
{
	u64 preemptoff_ns;
	int cpu = smp_processor_id();
	struct preemptirq_info *info = per_cpu_ptr(&preemptoff_info, cpu);

	/* Return if timing has not started */
	if (unlikely(!info->start_ts))
		return;

	if (unlikely(oops_in_progress))
		goto skip;

	/* Skip if return from ilde */
	if (info->pid != current->pid || info->ncsw != current->nvcsw + current->nivcsw)
		goto skip;

	preemptoff_ns = timing_clock() - info->start_ts;

	/* Time in printk should be ignored */
	if (preemptoff_ns - info->extra_time > preemptoff_settings.warn_val)
		preemptoff_warn(cpu, preemptoff_ns);

skip:
	timing_reset(info);
}

#ifdef CONFIG_PREEMPT_WARN
void start_preemptoff_extra_timing(void)
{
	int cpu = smp_processor_id();
	struct preemptirq_info *info = per_cpu_ptr(&preemptoff_info, cpu);

	/* return if timing has not started */
	if (unlikely(!info->start_ts))
		return;

	/* return if extra timing has already started */
	if (info->extra_start_ts)
		return;

	info->extra_start_ts = timing_clock();
}

void stop_preemptoff_extra_timing(void)
{
	int cpu = smp_processor_id();
	struct preemptirq_info *info = per_cpu_ptr(&preemptoff_info, cpu);

	/* return if extra timing has not started */
	if (!info->extra_start_ts)
		return;

	info->extra_time += timing_clock() - info->extra_start_ts;

	info->extra_start_ts = 0;
}
#endif /* CONFIG_PREEMPT_WARN */

/* file "enable" operations */
static ssize_t enable_show(struct file *file, char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	char buf[] = "1\n";

	if (static_branch_unlikely(&preemptoff_timing_off))
		buf[0] = '0';
	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

static ssize_t enable_write(struct file *file, const char __user *ubuf,
			    size_t cnt, loff_t *ppos)
{
	bool val;
	int ret;

	ret = kstrtobool_from_user(ubuf, cnt, &val);
	if (ret)
		return cnt;

	if (val)
		static_branch_disable(&preemptoff_timing_off);
	else
		static_branch_enable(&preemptoff_timing_off);

	return cnt;
}
static const struct file_operations enable_fops = {
	.read =		enable_show,
	.write =        enable_write,
	.llseek =	default_llseek,
};

/* file "warn_val" operations */
static int warn_val_get(void *data, u64 *val)
{
	*val = preemptoff_settings.warn_val;
	return 0;
}
static int warn_val_set(void *data, u64 val)
{
	preemptoff_settings.warn_val = (unsigned int)val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(warn_val_fops, warn_val_get, warn_val_set, "%llu\n");

__init static int creat_preemptoff_timing_fs(void)
{
	preemptoff_dir = debugfs_create_dir("preemptoff_warn", NULL);

	if (preemptoff_dir == NULL)
		return -ENOENT;

	debugfs_create_file("warn_val", 0600, preemptoff_dir, NULL, &warn_val_fops);
	debugfs_create_file("enable", 0600, preemptoff_dir, NULL, &enable_fops);

	return 0;
}

__init static int preemptoff_timing_init(void)
{
	preemptoff_settings.warn_val = PREEMPTOFF_WARN_VAL_DEF;

	if (register_trace_android_rvh_preempt_enable(stop_preemptoff_timing, NULL))
		goto vh_fail;
	if (register_trace_android_rvh_preempt_disable(start_preemptoff_timing, NULL))
		goto vh_fail;

	if (creat_preemptoff_timing_fs())
		goto fs_fail;

	static_branch_disable(&preemptoff_timing_off);
	return 0;

fs_fail:
	pr_err("Failed to create preemptoff_warn nodes\n");
	return -ENOENT;
vh_fail:
	pr_err("Failed to register vendor hooks\n");
	return -EBUSY;
}

static void __exit preemptoff_timing_exit(void)
{
	pr_emerg("This modules should not be removed!\n");
	debugfs_remove(preemptoff_dir);
}

module_init(preemptoff_timing_init);
module_exit(preemptoff_timing_exit);

MODULE_AUTHOR("ben.dai@unisoc.com");
MODULE_LICENSE("GPL");
