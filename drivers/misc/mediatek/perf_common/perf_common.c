// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/spinlock.h>
#include <linux/time.h>
#include <mt-plat/perf_common.h>
#include <perf_tracker.h>
#include <linux/cpu.h>
#include <linux/topology.h>
#ifdef CONFIG_MTK_CORE_CTL
#include <mt-plat/core_ctl.h>
#endif

static u64 checked_timestamp;
static bool long_trace_check_flag;
static DEFINE_SPINLOCK(check_lock);
static int perf_common_init;
#ifdef CONFIG_MTK_PERF_TRACKER
int cluster_nr = -1;
#endif

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

void perf_common(u64 wallclock)
{
	long mm_available = -1, mm_free = -1;

	if (!perf_do_check(wallclock))
		return;

#ifdef CONFIG_MTK_CORE_CTL
	/* period is 8ms */
	if (hit_long_check())
		core_ctl_tick(wallclock);
#endif

	if (unlikely(!perf_common_init))
		return;

	perf_tracker(wallclock, mm_available, mm_free);
}

static struct attribute *perf_attrs[] = {
#ifdef CONFIG_MTK_PERF_TRACKER
	&perf_tracker_enable_attr.attr,
#endif
	NULL,
};

static struct attribute_group perf_attr_group = {
	.attrs = perf_attrs,
};

static int init_perf_common(void)
{
	int ret = 0;
	struct kobject *kobj = NULL;

	perf_common_init = 1;
#ifdef CONFIG_MTK_PERF_TRACKER
	cluster_nr = arch_nr_clusters();

	if (unlikely(cluster_nr <= 0 || cluster_nr > 3))
		cluster_nr = 3;
#endif

	kobj = kobject_create_and_add("perf", &cpu_subsys.dev_root->kobj);

	if (kobj) {
		ret = sysfs_create_group(kobj, &perf_attr_group);
		if (ret)
			kobject_put(kobj);
		else
			kobject_uevent(kobj, KOBJ_ADD);
	}

	return 0;
}
late_initcall_sync(init_perf_common);
