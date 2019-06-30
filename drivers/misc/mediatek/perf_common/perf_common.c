// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <trace/hooks/sched.h>

#include <perf_tracker_internal.h>

static u64 checked_timestamp;
static bool long_trace_check_flag;
static DEFINE_SPINLOCK(check_lock);
static int perf_common_init;
static atomic_t perf_in_progress;

static inline bool perf_do_check(u64 wallclock)
{
	bool do_check = false;
	unsigned long flags;

	/* check interval */
	spin_lock_irqsave(&check_lock, flags);
	if ((s64)(wallclock - checked_timestamp)
			>= (s64)(2 * NSEC_PER_MSEC)) {
		checked_timestamp = wallclock;
		long_trace_check_flag = !long_trace_check_flag;
		do_check = true;
	}
	spin_unlock_irqrestore(&check_lock, flags);

	return do_check;
}

bool hit_long_check(void)
{
	bool do_check = false;
	unsigned long flags;

	spin_lock_irqsave(&check_lock, flags);
	if (long_trace_check_flag)
		do_check = true;
	spin_unlock_irqrestore(&check_lock, flags);
	return do_check;
}

static void perf_common(void *data, struct rq *rq)
{
	u64 wallclock;

	wallclock = ktime_get_ns();
	if (!perf_do_check(wallclock))
		return;

	if (unlikely(!perf_common_init))
		return;

	atomic_inc(&perf_in_progress);
	perf_tracker(wallclock, hit_long_check());
	atomic_dec(&perf_in_progress);
}

static struct attribute *perf_attrs[] = {
#if IS_ENABLED(CONFIG_MTK_PERF_TRACKER)
	&perf_tracker_enable_attr.attr,
#endif
	NULL,
};

static struct attribute_group perf_attr_group = {
	.attrs = perf_attrs,
};

static struct kobject *kobj;
static int init_perf_common_sysfs(void)
{
	int ret = 0;

	kobj = kobject_create_and_add("perf", &cpu_subsys.dev_root->kobj);
	if (!kobj)
		return -ENOMEM;

	ret = sysfs_create_group(kobj, &perf_attr_group);
	if (ret)
		goto error;
	kobject_uevent(kobj, KOBJ_ADD);
	return 0;

error:
	kobject_put(kobj);
	kobj = NULL;
	return ret;
}

static void cleanup_perf_common_sysfs(void)
{
	if (kobj) {
		sysfs_remove_group(kobj, &perf_attr_group);
		kobject_put(kobj);
		kobj = NULL;
	}
}

static int __init init_perf_common(void)
{
	int ret = 0;

	ret = init_perf_common_sysfs();
	if (ret)
		goto out;

	ret = register_trace_android_vh_scheduler_tick(perf_common, NULL);
	if (ret) {
		pr_debug("perf_comm: register hooks failed, returned %d\n", ret);
		goto register_failed;
	}
	perf_common_init = 1;
	atomic_set(&perf_in_progress, 0);
	return 0;

register_failed:
	cleanup_perf_common_sysfs();
out:
	return ret;
}

static void __exit exit_perf_common(void)
{
	while (atomic_read(&perf_in_progress) > 0)
		udelay(30);
	unregister_trace_android_vh_scheduler_tick(perf_common, NULL);
	cleanup_perf_common_sysfs();
}

module_init(init_perf_common);
module_exit(exit_perf_common);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MediaTek performance tracker");
