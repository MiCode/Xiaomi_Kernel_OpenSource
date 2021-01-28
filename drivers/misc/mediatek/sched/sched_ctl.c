/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/cpu.h>
#include <linux/sysfs.h>
#include <linux/kthread.h>
#include <linux/kobject.h>
#include <linux/irq_work.h>
#include <linux/sched/task.h>
#include <uapi/linux/sched/types.h>
#include "rq_stats.h"
#include "sched_ctl.h"
//TODO: remove comment after met ready
//#include <mt-plat/met_drv.h>
#include <mt-plat/mtk_sched.h>

#define SCHED_HINT_THROTTLE_NSEC 10000000 /* 10ms for throttle */

struct sched_hint_data {
	struct task_struct *task;
	struct irq_work irq_work;
	int sys_cap;
	int sys_util;
	ktime_t throttle;
	struct attribute_group *attr_group;
	struct kobject *kobj;
};

/* global */
static u64 sched_hint_check_timestamp;
static u64 sched_hint_check_interval;
static struct sched_hint_data g_shd;
static int sched_hint_inited;
#ifdef CONFIG_MTK_SCHED_SYSHINT
static int sched_hint_on = 1; /* default on */
#else
static int sched_hint_on; /* default off */
#endif
static enum sched_status_t kthread_status = SCHED_STATUS_INIT;
static enum sched_status_t sched_status = SCHED_STATUS_INIT;
static int sched_hint_loading_thresh = 5; /* 5% (max 100%) */
static BLOCKING_NOTIFIER_HEAD(sched_hint_notifier_list);
static DEFINE_SPINLOCK(status_lock);

static int sched_hint_status(int util, int cap)
{
	enum sched_status_t status;

	if (((util * 100) / cap) > sched_hint_loading_thresh)
		status = SCHED_STATUS_OVERUTIL;
	else
		status = SCHED_STATUS_UNDERUTIL;

	return status;
}

/* kernel thread */
static int sched_hint_thread(void *data)
{
	do {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();

		if (kthread_should_stop())
			break;

		/* status change from scheduler hint */
		if (kthread_status != sched_status) {
			int ret;

			kthread_status = sched_status;

			//TODO: remove comment after met ready
			//met_tag_oneshot(0, "sched_hint", kthread_status);

			ret = blocking_notifier_call_chain
					(&sched_hint_notifier_list,
					kthread_status, NULL);

			/* reset throttle time */
			g_shd.throttle = ktime_add_ns(
					ktime_get(),
					SCHED_HINT_THROTTLE_NSEC);
		}

		#if 0
		if (true) { /* debugging code */
			int iter_cpu;

			for (iter_cpu = 0; iter_cpu < nr_cpu_ids; iter_cpu++)
				#ifdef CONFIG_MTK_SCHED_CPULOAD
				met_tag_oneshot(0, met_cpu_load[iter_cpu],
						sched_get_cpu_load(iter_cpu));
				#endif
		}
		#endif
	} while (!kthread_should_stop());


	return 0;
}

static void sched_irq_work(struct irq_work *irq_work)
{
	struct sched_hint_data *shd;

	if (!irq_work)
		return;

	shd = container_of(irq_work, struct sched_hint_data, irq_work);

	wake_up_process(shd->task);
}

static bool hint_need(void)
{
	return (kthread_status == sched_status) ? false : true;
}

static bool do_check(u64 wallclock)
{
	bool do_check = false;
	unsigned long flags;

	/* check interval */
	spin_lock_irqsave(&status_lock, flags);
	if ((wallclock - sched_hint_check_timestamp)
			>= sched_hint_check_interval) {
		sched_hint_check_timestamp = wallclock;
		do_check = true;
	}
	spin_unlock_irqrestore(&status_lock, flags);

	/* is throttled ? */
	if (ktime_before(ktime_get(), g_shd.throttle))
		do_check = false;

	return do_check;
}

void sched_hint_check(u64 wallclock)
{
	if (!sched_hint_inited || !sched_hint_on)
		return;

	if (do_check(wallclock)) {
		if (hint_need())
			/* wake up ksched_hint */
			wake_up_process(g_shd.task);
	}
}

/* scheduler update hint in fair.c */
void update_sched_hint(int sys_util, int sys_cap)
{
	ktime_t throttle = g_shd.throttle;
	ktime_t now = ktime_get();

	if (!sched_hint_inited || !sched_hint_on)
		return;

	if (ktime_before(now, throttle)) {
		/* met_tag_oneshot(0, "sched_hint_throttle", 1); */
		return;
	}

	if (!sys_cap)
		return;

	g_shd.sys_util =  sys_util;
	g_shd.sys_cap = sys_cap;

	sched_status = sched_hint_status(sys_util, sys_cap);

	if (kthread_status != sched_status)
		irq_work_queue_on(&g_shd.irq_work, smp_processor_id());
}

/* A user-space interfaces: */
static ssize_t store_sched_enable(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;

	if (sscanf(buf, "%iu", &val) != 0)
		sched_hint_on = (val) ? 1 : 0;

	return count;
}

static ssize_t store_sched_load_thresh(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;

	if (sscanf(buf, "%iu", &val) != 0) {
		if (val >= 0 && val < 100)
			sched_hint_loading_thresh = val;
	}
	return count;
}

static ssize_t show_sched_info(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len +=  snprintf(buf, max_len, "capacity total=%d\n", g_shd.sys_cap);
	len +=  snprintf(buf+len, max_len - len,
		"capacity used=%d\n", g_shd.sys_util);

	if (sched_hint_on) {
		if (kthread_status != SCHED_STATUS_INIT)
			len +=  snprintf(buf+len,
					max_len - len,
					"status=(%s)\n",
					(kthread_status !=
					SCHED_STATUS_OVERUTIL) ?
					"under" : "over");
		else
			len +=  snprintf(buf+len,
					max_len - len,
					"status=(init)\n");
	} else
		len +=  snprintf(buf+len, max_len - len, "status=(off)\n");

	len +=  snprintf(buf+len, max_len - len, "load thresh=%d%c\n",
					sched_hint_loading_thresh, '%');

	return len;
}

static ssize_t show_walt_info(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

static ssize_t store_walt_info(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count);

static struct kobj_attribute sched_enable_attr =
__ATTR(hint_enable, 0200 /* S_IWUSR */, NULL, store_sched_enable);

static struct kobj_attribute sched_load_thresh_attr =
__ATTR(hint_load_thresh, 0200 /* S_IWUSR */, NULL, store_sched_load_thresh);

static struct kobj_attribute sched_info_attr =
__ATTR(hint_info, 0400 /* S_IRUSR */, show_sched_info, NULL);

static struct kobj_attribute sched_walt_info_attr =
__ATTR(walt_debug, 0600 /* S_IWUSR | S_IRUSR */,
			show_walt_info, store_walt_info);

static struct attribute *sched_attrs[] = {
	&sched_info_attr.attr,
	&sched_load_thresh_attr.attr,
	&sched_enable_attr.attr,
	&sched_walt_info_attr.attr,
	NULL,
};

static struct attribute_group sched_attr_group = {
	.attrs = sched_attrs,
};

int register_sched_hint_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&sched_hint_notifier_list, nb);
}
EXPORT_SYMBOL(register_sched_hint_notifier);

int unregister_sched_hint_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister
				(&sched_hint_notifier_list, nb);
}
EXPORT_SYMBOL(unregister_sched_hint_notifier);

/* init function */
static int __init sched_hint_init(void)
{
	struct sched_param param;
	int ret;

	/* create thread */
	g_shd.task = kthread_create(sched_hint_thread, NULL, "ksched_hint");

	if (IS_ERR_OR_NULL(g_shd.task)) {
		pr_info("%s: failed to create ksched_hint thread.\n", __func__);
		goto err;
	}

	/* priority setting */
	param.sched_priority = 120;
	ret = sched_setscheduler_nocheck(g_shd.task, SCHED_NORMAL, &param);

	if (ret)
		pr_info("%s: failed to set sched_fifo\n", __func__);

	/* keep thread alive */
	get_task_struct(g_shd.task);

	/* init throttle time */
	g_shd.throttle = ktime_get();

	wake_up_process(g_shd.task);

	/* init irq_work */
	init_irq_work(&g_shd.irq_work, sched_irq_work);

	/* check interval 20ms */
	sched_hint_check_interval = CPU_LOAD_AVG_DEFAULT_MS * NSEC_PER_MSEC;

	/* enable sched hint */
	sched_hint_inited = 1;

	/*
	 * create a sched in cpu_subsys:
	 * /sys/devices/system/cpu/sched/...
	 */
	g_shd.attr_group = &sched_attr_group;
	g_shd.kobj = kobject_create_and_add("sched",
					&cpu_subsys.dev_root->kobj);

	if (g_shd.kobj) {
		ret = sysfs_create_group(g_shd.kobj, g_shd.attr_group);
		if (ret)
			kobject_put(g_shd.kobj);
		else
			kobject_uevent(g_shd.kobj, KOBJ_ADD);
	}

	return 0;


err:
	return -ENOMEM;
}

late_initcall(sched_hint_init);

/*
 * sched_ktime_clock()
 *  - to get wall time but not to update in suspended.
 */
#include <linux/syscore_ops.h>

static ktime_t ktime_last;
static bool sched_ktime_suspended;

u64 sched_ktime_clock(void)
{
	if (unlikely(sched_ktime_suspended))
		return ktime_to_ns(ktime_last);
	return ktime_get_ns();
}

static void sched_resume(void)
{
	sched_ktime_suspended = false;
}

static int sched_suspend(void)
{
	ktime_last = ktime_get();
	sched_ktime_suspended = true;
	return 0;
}

static struct syscore_ops sched_syscore_ops = {
	.resume = sched_resume,
	.suspend = sched_suspend
};

static int __init sched_init_ops(void)
{
	register_syscore_ops(&sched_syscore_ops);
	return 0;
}
late_initcall(sched_init_ops);

/* schedule loading trackign change
 * 0: default
 * 1: walt
 */
static int lt_walt_table[LT_UNKNOWN_USER];
static int walt_dbg;

static char walt_maker_name[LT_UNKNOWN_USER+1][32] = {
	"powerHAL",
	"fpsGO",
	"sched",
	"debug",
	"unknown"
};

int sched_walt_enable(int user, int en)
{
	int walted = 0;
	int i;
	unsigned int user_mask = 0;

	if ((user < 0) || (user >= LT_UNKNOWN_USER))
		return -1;

	lt_walt_table[user] = en;

	for (i = 0; i < LT_UNKNOWN_USER; i++) {
		walted |= lt_walt_table[i];
		user_mask |= (lt_walt_table[i] << i);
	}

	if (walt_dbg) {
		walted = lt_walt_table[LT_WALT_DEBUG];
		user_mask = 0xFFFF;
	}

#ifdef CONFIG_SCHED_WALT
	sysctl_sched_use_walt_cpu_util  = walted;
	sysctl_sched_use_walt_task_util = walted;
	trace_sched_ctl_walt(user_mask, walted);
#endif

	return 0;
}
EXPORT_SYMBOL(sched_walt_enable);

static ssize_t show_walt_info(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;
	int i;
	int supported = 0;

	for (i = 0; i < LT_UNKNOWN_USER; i++)
		len +=  snprintf(buf+len, max_len - len, "walt[%d] = %d (%s)\n",
				i, lt_walt_table[i], walt_maker_name[i]);
#ifdef CONFIG_SCHED_WALT
	supported = 1;
#endif
	len +=  snprintf(buf+len, max_len - len,
			"\nWALT support:%d\n", supported);
	len +=  snprintf(buf+len, max_len - len,
			"debug mode:%d\n", walt_dbg);
	len +=  snprintf(buf+len, max_len - len,
			"format: echo (debug:walt)\n");

	return len;
}
/*
 * argu1: enable debug mode
 * argu2: walted or not.
 *
 * echo 1 0 : enable debug mode, foring walted off.
 */
static ssize_t store_walt_info(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int walted = -1;

	if (sscanf(buf, "%d:%d", &walt_dbg, &walted) <= 0)
		return count;

	if (walted < 0 || walted > 1 || walt_dbg < 0 || walt_dbg > 1)
		return count;

	if (walt_dbg)
		sched_walt_enable(LT_WALT_DEBUG, walted);
	else
		sched_walt_enable(LT_WALT_DEBUG, 0);

	return count;
}
