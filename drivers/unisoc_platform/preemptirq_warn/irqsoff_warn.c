// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "irqsoff_warn: " fmt
#include <linux/debugfs.h>
#include <linux/ftrace.h>
#include <linux/kallsyms.h>
#include <linux/math64.h>
#include <linux/percpu.h>
#include <linux/seq_file.h>
#include <linux/trace.h>
#include <trace/hooks/preemptirq.h>

#include "preemptirq_timing.h"

#define IRQSOFF_WARN_VAL_DEF (30 * NSEC_PER_MSEC)

static DEFINE_STATIC_KEY_TRUE(irqsoff_timing_off);
static DEFINE_STATIC_KEY_FALSE(not_trace_printk);
static DEFINE_PER_CPU(struct preemptirq_info, irqsoff_info);
static struct preemptirq_settings irqsoff_settings;
static int log_ctrl = 1;
static struct dentry *irqsoff_dir;
/*
 * WARN: this function will be called when IRQ is disabled, so we should
 * print log as less as possible.
 */
noinline void irqsoff_warn(int cpu, u64 irqsoff_t)
{
	u32 off_us, extra_us, start_us;
	int i;
	void *ip;
	char buff[500], *p;
	struct preemptirq_info *info = per_cpu_ptr(&irqsoff_info, cpu);

	/* print time info */
	off_us = do_div(irqsoff_t, NSEC_PER_MSEC) / NSEC_PER_USEC;
	start_us = do_div(info->start_ts, NSEC_PER_SEC) / NSEC_PER_USEC;
	if (!info->extra_time) {
		/* thread <T> disable IRQ on core <C> for <D>ms from <F>s */
		pr_warn("C%d T:<%d>%s D:%llu.%03ums F:%llu.%06us\n",
			cpu, info->task->pid, info->task->comm,
			irqsoff_t, off_us, info->start_ts, start_us);
	} else {
		extra_us = do_div(info->extra_time, NSEC_PER_MSEC);
		extra_us /= NSEC_PER_USEC;
		/* <E> means extra time spent in printk, included in <D> */
		pr_warn("C%d T:<%d>%s D:%llu.%03ums F:%llu.%06us E:%llu.%03ums\n",
			cpu, info->task->pid, info->task->comm,
			irqsoff_t, off_us, info->start_ts, start_us,
			info->extra_time, extra_us);
	}

	if (log_ctrl < 0)
		return;

	/* print thread name and backtrace of disabling */
	p = buff;
	p += sprintf(p, "C%d disabled IRQ at:\n", cpu);
	for (i = 0; i < 5; i++) {
		ip = info->callback[i];
		if (!ip)
			break;
		p += sprintf(p, "%pS\n", ip);
	}
	pr_warn("%s", buff);

	/* print thread name and backtrace of enabling */
	p = buff;
	p += sprintf(p, "C%d enabled IRQ at:\n", cpu);
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

noinline void start_irqsoff_timing(void *a, unsigned long ip, unsigned long parent_ip)
{
	struct preemptirq_info *info;
	int i;

	if (static_branch_unlikely(&irqsoff_timing_off))
		return;

	if (is_idle_task(current) || oops_in_progress)
		return;

	info = this_cpu_ptr(&irqsoff_info);
	/* return if timing has already started */
	if (info->start_ts)
		return;

	info->start_ts = timing_clock();
	info->task = current;
	for (i = 0; i < 5; i++)
		info->callback[i] = return_address(i+1);
}

noinline void stop_irqsoff_timing(void *a, unsigned long ip, unsigned long parent_ip)
{
	u64 irqsoff_ns, irqsoff_real;
	int cpu = smp_processor_id();
	struct preemptirq_info *info = per_cpu_ptr(&irqsoff_info, cpu);

	/* return if timing has not started */
	if (unlikely(!info->start_ts))
		return;

	if (unlikely(oops_in_progress))
		goto skip;

	irqsoff_ns = timing_clock() - info->start_ts;
	irqsoff_real = irqsoff_ns;

	/* If we don’t care about the time in printk */
	if (static_branch_unlikely(&not_trace_printk))
		irqsoff_real -= info->extra_time;

	if (irqsoff_real > irqsoff_settings.warn_val)
		irqsoff_warn(cpu, irqsoff_ns);

skip:
	timing_reset(info);
}

#ifdef CONFIG_IRQSOFF_WARN
void start_irqsoff_extra_timing(void)
{
	int cpu = smp_processor_id();
	struct preemptirq_info *info = per_cpu_ptr(&irqsoff_info, cpu);

	/* return if timing has not started */
	if (unlikely(!info->start_ts))
		return;

	/* return if extra timing has already started */
	if (info->extra_start_ts)
		return;

	info->extra_start_ts = timing_clock();
}

void stop_irqsoff_extra_timing(void)
{
	u64 extra_deta;
	int cpu = smp_processor_id();
	struct preemptirq_info *info = per_cpu_ptr(&irqsoff_info, cpu);

	/* return if extra timing has not started */
	if (!info->extra_start_ts)
		return;

	extra_deta = timing_clock() - info->extra_start_ts;
	info->extra_time += extra_deta;

	/*
	 * too many logs waiting to be pushed to the console, discard some
	 * non-essential information to prevent vicious circle in
	 * bad latency->more log->worse latency->...
	 */
	if (extra_deta > irqsoff_settings.warn_val)
		log_ctrl--;

	else if (extra_deta < irqsoff_settings.warn_val / 3)
		log_ctrl = 1;

	info->extra_start_ts = 0;
}
#endif /* CONFIG_IRQSOFF_WARN */

/* file "enable" operations */
static ssize_t enable_show(struct file *file, char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	char buf[] = "1\n";

	if (static_branch_unlikely(&irqsoff_timing_off))
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
		static_branch_disable(&irqsoff_timing_off);
	else
		static_branch_enable(&irqsoff_timing_off);

	return cnt;
}

static const struct file_operations enable_fops = {
	.read =		enable_show,
	.write =        enable_write,
	.llseek =	default_llseek,
};

/* file "trace_printk" operations */
static ssize_t trace_printk_show(struct file *file, char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	char buf[] = "1\n";

	if (static_branch_unlikely(&not_trace_printk))
		buf[0] = '0';
	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

static ssize_t trace_printk_write(struct file *file, const char __user *ubuf,
			    size_t cnt, loff_t *ppos)
{
	bool val;
	int ret;

	ret = kstrtobool_from_user(ubuf, cnt, &val);
	if (ret)
		return cnt;

	if (val)
		static_branch_disable(&not_trace_printk);
	else
		static_branch_enable(&not_trace_printk);

	return cnt;
}

static const struct file_operations trace_printk_fops = {
	.read =		trace_printk_show,
	.write =        trace_printk_write,
	.llseek =	default_llseek,
};

/* file "warn_val" operations */
static int warn_val_get(void *data, u64 *val)
{
	*val = irqsoff_settings.warn_val;
	return 0;
}

static int warn_val_set(void *data, u64 val)
{
	irqsoff_settings.warn_val = (unsigned int)val;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(warn_val_fops, warn_val_get, warn_val_set, "%llu\n");

static int __init creat_irqsoff_timing_fs(void)
{
	irqsoff_dir = debugfs_create_dir("irqsoff_warn", NULL);

	if (irqsoff_dir == NULL)
		return -EFAULT;

	debugfs_create_file("warn_val", 0600, irqsoff_dir, NULL, &warn_val_fops);
	debugfs_create_file("enable", 0600, irqsoff_dir, NULL, &enable_fops);
	debugfs_create_file("trace_printk", 0600, irqsoff_dir, NULL, &trace_printk_fops);

	return 0;
}

static int __init irqsoff_timing_init(void)
{
	irqsoff_settings.warn_val = IRQSOFF_WARN_VAL_DEF;

	if (register_trace_android_rvh_irqs_enable(stop_irqsoff_timing, NULL))
		goto vh_fail;
	if (register_trace_android_rvh_irqs_disable(start_irqsoff_timing, NULL))
		goto vh_fail;

	if (creat_irqsoff_timing_fs())
		goto fs_fail;

	static_branch_disable(&irqsoff_timing_off);
	pr_info("Initialized\n");
	return 0;

fs_fail:
	pr_err("Failed to create irqsoff_warn nodes\n");
	return -ENOENT;
vh_fail:
	pr_err("Failed to register vendor hooks\n");
	return -EBUSY;
}

static void __exit irqsoff_timing_exit(void)
{
	pr_emerg("This modules should not be removed!\n");
	debugfs_remove(irqsoff_dir);
}

module_init(irqsoff_timing_init);
module_exit(irqsoff_timing_exit);

MODULE_AUTHOR("ben.dai@unisoc.com");
MODULE_LICENSE("GPL");
