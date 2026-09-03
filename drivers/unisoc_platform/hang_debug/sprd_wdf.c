// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * it's hardware watchdog feeder code for Phoenix II
 */

#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/percpu.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/smp.h>
#include <linux/delay.h>
#include <linux/smpboot.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <uapi/linux/sched/types.h>
#include <linux/watchdog.h>
#include <linux/seq_file.h>
#include <linux/panic_notifier.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include "aphang.h"

#if IS_ENABLED(CONFIG_SPRD_WATCHDOG_FIQ)
#include <linux/sprd_wdt_fiq.h>
#endif

#undef pr_fmt
#define pr_fmt(fmt) "sprd_wdf: " fmt
#define INV_CPUS 256

static DEFINE_PER_CPU(struct task_struct *, hang_debug_task_store);
unsigned int cpu_feed_mask;
unsigned int cpu_feed_bitmap;
EXPORT_SYMBOL(cpu_feed_mask);
EXPORT_SYMBOL(cpu_feed_bitmap);
static DEFINE_MUTEX(wdf_mutex);
/**
 * choose hrtimer here due to hrtimer was handled in hard interrupt context
 * while timer was handled in soft interrupt context
 */
static DEFINE_PER_CPU(struct hrtimer, sprd_wdt_hrtimer);
/* 1: which cpu need to feed, 0: cpu doesn't need to feed */
static DEFINE_PER_CPU(int, g_enable);
struct watchdog_device *wdd;
/* set default watchdog time */
static u32 g_interval = CONFIG_SPRD_WDF_FEED_TIME_MS;
static u32 g_timeout = CONFIG_SPRD_WDT_TIMEOUT_MS;
static u32 g_pretimeout = CONFIG_SPRD_WDT_PRETIMEOUT_MS;
static int wdt_disable;

static DEFINE_SPINLOCK(thread_lock);
static struct task_struct *pthread;
wait_queue_head_t waitq;
static unsigned int thread_cpu = INV_CPUS;
static unsigned int thread_state;

static enum hrtimer_restart sprd_wdt_timer_func(struct hrtimer *hrtimer)
{
	/**
	 * hrtimer_cancel will be called in disable wdt context, however,
	 * check wdt_disable here to bail out early. mutex shouldn't be
	 * added here to avoid dead-lock due to mutex would have been held
	 * before hrtimer_cancel.
	 */
	if (wdt_disable) {
		pr_debug("hrtimer func: wdt_disable\n");
		return HRTIMER_NORESTART;
	}

	__this_cpu_write(g_enable, 1);
	wake_up_process(__this_cpu_read(hang_debug_task_store));
	hrtimer_forward_now(hrtimer, ms_to_ktime(g_interval));
	return HRTIMER_RESTART;
}

static void sprd_wdf_hrtimer_enable(unsigned int cpu)
{
	struct hrtimer *hrtimer = this_cpu_ptr(&sprd_wdt_hrtimer);

	hrtimer_init(hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer->function = sprd_wdt_timer_func;
	hrtimer_start(hrtimer, ms_to_ktime(g_interval),
		      HRTIMER_MODE_REL_PINNED);
}

static void hang_debug_create(unsigned int cpu)
{
	struct task_struct *hang_debug_t;
	struct sched_param param = {.sched_priority = (MAX_RT_PRIO - 1)};

	hang_debug_t = per_cpu(hang_debug_task_store, cpu);
	sched_setscheduler(hang_debug_t, SCHED_FIFO, &param);

	per_cpu(g_enable, cpu) = 0;
}

static int hang_debug_should_run(unsigned int cpu)
{
	return per_cpu(g_enable, cpu);
}

static void hang_debug_park(unsigned int cpu)
{
	struct hrtimer *hrtimer = this_cpu_ptr(&sprd_wdt_hrtimer);

	mutex_lock(&wdf_mutex);
	cpu_feed_mask &= (~(1U << cpu));
	cpu_feed_bitmap = 0;
	pr_debug("offline cpu = %u\n", cpu);

	if (wdt_disable)
		goto out;
#if IS_ENABLED(CONFIG_SPRD_WATCHDOG_FIQ)
	if (wdd->ops->start)
		wdd->ops->start(wdd);
#endif
	hrtimer_cancel(hrtimer);
out:
	mutex_unlock(&wdf_mutex);
}

static void hang_debug_unpark(unsigned int cpu)
{

	mutex_lock(&wdf_mutex);
	cpu_feed_mask |= (1U << cpu);
	cpu_feed_bitmap = 0;
	pr_debug("online cpu = %u\n", cpu);

	if (wdt_disable)
		goto out;
#if IS_ENABLED(CONFIG_SPRD_WATCHDOG_FIQ)
	if (wdd->ops->start)
		wdd->ops->start(wdd);
#endif
	hrtimer_start(this_cpu_ptr(&sprd_wdt_hrtimer),
			ms_to_ktime(g_interval),
			HRTIMER_MODE_REL_PINNED);
out:
	mutex_unlock(&wdf_mutex);
}

static void hang_debug_task(unsigned int cpu)
{

	mutex_lock(&wdf_mutex);
	if (wdt_disable)
		goto out;

	cpu_feed_bitmap |= (1U << cpu);
	if (cpu_feed_mask == cpu_feed_bitmap) {
		pr_debug("feed wdt cpu_feed_bitmap = 0x%08x\n", cpu_feed_bitmap);
		cpu_feed_bitmap = 0;
#if IS_ENABLED(CONFIG_SPRD_WATCHDOG_FIQ)
		if (wdd->ops->start)
			wdd->ops->start(wdd);
#endif
	} else
		pr_debug("cpu_feed_bitmap = 0x%08x\n", cpu_feed_bitmap);

	__this_cpu_write(g_enable, 0);
out:
	mutex_unlock(&wdf_mutex);
}

static struct smp_hotplug_thread hang_debug_threads = {
	.store			= &hang_debug_task_store,
	.thread_should_run	= hang_debug_should_run,
	.create			= hang_debug_create,
	.thread_fn		= hang_debug_task,
	.thread_comm	= "hang_debug/%u",
	.setup			= sprd_wdf_hrtimer_enable,
	.park			= hang_debug_park,
	.unpark			= hang_debug_unpark,
};

static void smp_hrtimer_start(void *data)
{
	int cpu = smp_processor_id();

	pr_debug("hrtimer restart on cpu%d\n", cpu);
	hrtimer_start(this_cpu_ptr(&sprd_wdt_hrtimer),
			ms_to_ktime(g_interval),
			HRTIMER_MODE_REL_PINNED);
}

static int hang_debug_proc_read(struct seq_file *s, void *v)
{
	int cpu;

	seq_printf(s, "WDF: interval=%u pretimeout=%u timeout=%u bitmap=0x%08x/0x%08x\n",
		       g_interval, g_pretimeout, g_timeout,
		       cpu_feed_bitmap, cpu_feed_mask);

	for_each_online_cpu(cpu) {
		seq_printf(s, "[cpu%d] g_enable = %d\n", cpu, per_cpu(g_enable, cpu));
	}

	return 0;
}

static int hang_debug_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, hang_debug_proc_read, NULL);
}

static int timeout_interval_valid(u32 timeout, u32 pretimeout, u32 interval)
{
	if (timeout < 100 || pretimeout < 100 || interval < 50) {
		pr_err("The value entered2 is too small.\n");
		return -EINVAL;
	}
	if (timeout < pretimeout || (timeout - pretimeout) < interval || timeout < interval) {
		pr_err("The value entered is not standard.\n");
		return -EINVAL;
	}
	return 0;
}

static ssize_t hang_debug_proc_write(struct file *file, const char *buf,
	size_t count, loff_t *data)
{
	u32 timeout;
	u32 pretimeout;
	u32 interval;
	int cpu;
	char input[40] = {0};

	if (copy_from_user(input, buf, min(count, sizeof(input))))
		return -EINVAL;
	if (sscanf(input, "%u %u %u", &timeout, &pretimeout, &interval) < 3)
		return -EINVAL;

	if (!timeout_interval_valid(timeout, pretimeout, interval)) {
		mutex_lock(&wdf_mutex);
		cpu_feed_bitmap = 0;

		/* cancel timer on each cpu */
		for_each_online_cpu(cpu)
			hrtimer_cancel(per_cpu_ptr(&sprd_wdt_hrtimer, cpu));

		g_timeout = timeout;
		g_pretimeout = pretimeout;
		g_interval = interval;

		pr_notice("timeout = %u pretimeout = %u interval = %u\n",
				g_timeout, g_pretimeout, g_interval);

#if IS_ENABLED(CONFIG_SPRD_WATCHDOG_FIQ)
		/* set new timeout and touch watchdog
		 * If timeout less-than 40000ms, priority setting pretime. vice versa
		 * notice: timeout maximum value is 60000ms
		 */
		if (g_timeout < 40000) {
			if (wdd->ops->set_pretimeout)
				wdd->ops->set_pretimeout(wdd, g_pretimeout);
			if (wdd->ops->set_timeout)
				wdd->ops->set_timeout(wdd, g_timeout);
		} else {
			if (wdd->ops->set_timeout)
				wdd->ops->set_timeout(wdd, g_timeout);
			if (wdd->ops->set_pretimeout)
				wdd->ops->set_pretimeout(wdd, g_pretimeout);
		}
		if (wdd->ops->start)
			wdd->ops->start(wdd);
#endif
		/* restart timer on each cpu */
		for_each_online_cpu(cpu)
			smp_call_function_single(cpu, smp_hrtimer_start,
					NULL, true);
		mutex_unlock(&wdf_mutex);
	}

	return count;
}

static const struct proc_ops hang_debug_proc_fops = {
	.proc_flags = PROC_ENTRY_PERMANENT,
	.proc_open = hang_debug_proc_open,
	.proc_read = seq_read,
	.proc_write = hang_debug_proc_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int wdt_disable_proc_read(struct seq_file *s, void *v)
{
	seq_printf(s, "%d\n", wdt_disable);
	return 0;
}

static int wdt_disable_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, wdt_disable_proc_read, NULL);
}

void sprd_wdf_wdt_disable(int disable)
{
	int cpu;

	mutex_lock(&wdf_mutex);
	if (disable && !wdt_disable) {
		wdt_disable = 1;
		cpu_feed_bitmap = 0;
		for_each_online_cpu(cpu) {
			/* cancel timer on each cpu */
			hrtimer_cancel(per_cpu_ptr(&sprd_wdt_hrtimer, cpu));
			per_cpu(g_enable, cpu) = 0;
		}
#if IS_ENABLED(CONFIG_SPRD_WATCHDOG_FIQ)
		if (wdd->ops->stop)
			wdd->ops->stop(wdd);
#endif
	} else if (!disable && wdt_disable) {
		wdt_disable = 0;
#if IS_ENABLED(CONFIG_SPRD_WATCHDOG_FIQ)
		if (wdd->ops->set_timeout)
			wdd->ops->set_timeout(wdd, (u32)g_timeout);
		if (wdd->ops->set_pretimeout)
			wdd->ops->set_pretimeout(wdd, (u32)g_pretimeout);
		if (wdd->ops->start)
			wdd->ops->start(wdd);
#endif
		/* restart timer on each cpu */
		for_each_online_cpu(cpu)
			smp_call_function_single(cpu, smp_hrtimer_start,
					NULL, true);
	}
	mutex_unlock(&wdf_mutex);
}

EXPORT_SYMBOL(sprd_wdf_wdt_disable);

static ssize_t wdt_disable_proc_write(struct file *file, const char *buf,
	size_t count, loff_t *data)
{
	unsigned long long disable;
	int err;

	err = kstrtoull_from_user(buf, count, 0, &disable);
	if (err)
		return -EINVAL;

	pr_notice("watchdog disable = %d\n", (int)disable);
	sprd_wdf_wdt_disable((int)disable);

	return count;
}

static const struct proc_ops wdt_disable_proc_fops = {
	.proc_flags = PROC_ENTRY_PERMANENT,
	.proc_open = wdt_disable_proc_open,
	.proc_read = seq_read,
	.proc_write = wdt_disable_proc_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int loop_thread_func(void *data)
{
	unsigned long flags;

	while (!kthread_should_stop()) {
		wait_event_interruptible(waitq,
			thread_state == 1 || kthread_should_stop());

		if (kthread_should_stop())
			break;

		spin_lock_irqsave(&thread_lock, flags);
		while (1)
			cpu_relax();
		spin_unlock_irqrestore(&thread_lock, flags);

		/* impossible to come here */
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}
	return 0;
}

static ssize_t thread_read(struct file *file, char __user *user_buf,
                                 size_t count, loff_t *ppos)
{
	char buf[5];
	size_t len;

	len = snprintf(buf, sizeof(buf), "%u\n", thread_cpu);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t thread_write(struct file *file,
			    const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	unsigned long input;

	if (*ppos < 0)
		return -EINVAL;

	if (count == 0)
		return 0;

	 if (*ppos != 0)
		return 0;

	ret = kstrtoul_from_user(user_buf, count, 10, &input);
	if (ret)
		return -EINVAL;

	if (input == thread_cpu)
		return count;

	if (input > nr_cpu_ids)
		return -EINVAL;

	/* stop bounded thread on previous cpu*/
	if (thread_cpu != INV_CPUS && pthread) {
		kthread_stop(pthread);
		pthread = NULL;
	}

	thread_cpu = input;

	if (!pthread && cpu_online(thread_cpu)) {
		pthread = kthread_create_on_node(loop_thread_func, NULL,
					cpu_to_node(thread_cpu),
					"loop_thread/%d", thread_cpu);
		if (IS_ERR(pthread)) {
			pr_err("%s: Create thread on %d cpu fail\n",
				__func__, thread_cpu);
			return PTR_ERR(pthread);
		}
		/* bind thread to the cpu */
		kthread_bind(pthread, thread_cpu);
		wake_up_process(pthread);
	}
	return count;
}

static ssize_t thread_trigger_read(struct file *file, char __user *user_buf,
			     size_t count, loff_t *ppos)
{
	char buf[5];
	size_t len;

	len = snprintf(buf, sizeof(buf), "%u\n", thread_state);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t thread_trigger_write(struct file *file,
				const char __user *user_buf, size_t count,
				loff_t *ppos)
{
	int ret;
	unsigned long input;

	if (*ppos < 0)
		return -EINVAL;

	if (count == 0)
		return 0;

	if (*ppos != 0)
		return 0;

	ret = kstrtoul_from_user(user_buf, count, 10, &input);
	if (ret)
		return -EINVAL;

	if (input == thread_state)
		return count;

	if (input > 2)
		return -EINVAL;

	thread_state = input;
	if (input == 1)
	wake_up_interruptible(&waitq);

	return count;
}

static const struct proc_ops thread_fops = {
	.proc_open      = simple_open,
	.proc_read      = thread_read,
	.proc_write     = thread_write,
	.proc_lseek     = default_llseek,
};

static const struct proc_ops thread_trigger_fops = {
	.proc_open      = simple_open,
	.proc_read      = thread_trigger_read,
	.proc_write     = thread_trigger_write,
	.proc_lseek     = default_llseek,
};

#if IS_ENABLED(CONFIG_SPRD_HANG_TRIGGER)
static int __init send_ipi_init(void)
{
	int ret = 0;

	if (!proc_create("thread_cpu", 0660, NULL, &thread_fops)) {
		ret = -ENOMEM;
		goto err;
	}

	if (!proc_create("thread_trigger", 0660, NULL, &thread_trigger_fops)) {
		ret = -ENOMEM;
		goto err_trigger;
	}

	init_waitqueue_head(&waitq);

	return ret;

err_trigger:
	remove_proc_entry("thread_cpu", NULL);
err:
	return ret;
}

static void __exit send_ipi_exit(void)
{
	remove_proc_entry("thread_cpu", NULL);
	remove_proc_entry("thread_trigger", NULL);
	if (pthread)
		kthread_stop(pthread);
}
#else
static int send_ipi_init(void)
{
        return 0;
}
static void send_ipi_exit(void) {}
#endif

static int wdf_panic_event(struct notifier_block *self,
					unsigned long val, void *reason)
{
	sprd_wdf_wdt_disable(1);
	return NOTIFY_DONE;
}

static struct notifier_block wdf_panic_event_nb = {
	.notifier_call	= wdf_panic_event,
	.priority	= INT_MIN,
};

static int __init sprd_hang_debug_init(void)
{
	int cpu;
	struct proc_dir_entry *de = NULL;
	struct proc_dir_entry *df = NULL;

#if IS_ENABLED(CONFIG_SPRD_WATCHDOG_FIQ)
	int ret = 0;

	ret = sprd_wdt_fiq_get_dev(&wdd);
	if (ret) {
		pr_err("get public api error in sprd_wdt %d\n", ret);
		return ret;
	}

	if (wdd->ops->set_timeout)
		wdd->ops->set_timeout(wdd, (u32)g_timeout);

	if (wdd->ops->set_pretimeout)
		wdd->ops->set_pretimeout(wdd, (u32)g_pretimeout);

	if (wdd->ops->start)
		wdd->ops->start(wdd);

	pr_notice("%s\n", __func__);
#else
	pr_notice("No config sprd_wdt device\n");
#endif

	for_each_online_cpu(cpu) {
		cpu_feed_mask |= (1 << cpu);
	}

	BUG_ON(smpboot_register_percpu_thread(&hang_debug_threads));

	de = proc_mkdir("sprd_hang_debug", NULL);
	if (de) {
		df = proc_create("wdf_timeout", 0660, de, &hang_debug_proc_fops);
		if (!df)
			pr_err("create /proc/sprd_hang_debug/wdf_timeout failed\n");
		df = proc_create("wdt_disable", 0660, de, &wdt_disable_proc_fops);
		if (!df)
			pr_err("create /proc/sprd_hang_debug/wdt_disable failed\n");
	} else {
		pr_err("create /proc/sprd_hang_debug/ failed\n");
	}

	atomic_notifier_chain_register(&panic_notifier_list,
					&wdf_panic_event_nb);

	sprd_wdh_atf_init();
	send_ipi_init();
	return 0;
}

static void __exit sprd_hang_debug_exit(void)
{
	pr_emerg("This modules should not be removed!\n");
	send_ipi_exit();
}

late_initcall(sprd_hang_debug_init);
module_exit(sprd_hang_debug_exit);

MODULE_DESCRIPTION("sprd hang debug wdf driver");
MODULE_LICENSE("GPL v2");
