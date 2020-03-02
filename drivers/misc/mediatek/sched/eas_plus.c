/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/sched.h>
#include <linux/arch_topology.h>
#include "eas_plus.h"

#include <mt-plat/met_drv.h>

/*
 * V1.04 support hybrid(EAS+HMP)
 * V1.05 if system is balanced, select CPU with max sparse capacity when wakeup
 * V1.06 add "turning" and "watershed" points to help HPS
 */

struct eas_data {
	struct attribute_group *attr_group;
	struct kobject *kobj;
	int init;
};

struct eas_data eas_info;

static int ver_major = 1;
static int ver_minor = 8;

static char module_name[128];

/* A lock for scheduling switcher */
DEFINE_SPINLOCK(sched_switch_lock);

#if defined(CONFIG_SCHED_DEBUG) && \
defined(CONFIG_DEFAULT_USE_ENERGY_AWARE) && defined(CONFIG_SCHED_HMP)
int sched_scheduler_switch(enum SCHED_LB_TYPE new_sched)
{
	unsigned long flags;

	if (new_sched >= SCHED_UNKNOWN_LB)
		return -1;

	spin_lock_irqsave(&sched_switch_lock, flags);
	switch (new_sched) {
	case SCHED_EAS_LB:
		sysctl_sched_features &= ~(1 << __SCHED_FEAT_SCHED_HMP);
		sysctl_sched_features |= 1 << __SCHED_FEAT_ENERGY_AWARE;
		break;
	case SCHED_HMP_LB:
		sysctl_sched_features &= ~(1 << __SCHED_FEAT_ENERGY_AWARE);
		sysctl_sched_features |= 1 << __SCHED_FEAT_SCHED_HMP;
		break;
	case SCHED_HYBRID_LB:
		sysctl_sched_features |= 1 << __SCHED_FEAT_ENERGY_AWARE;
		sysctl_sched_features |= 1 << __SCHED_FEAT_SCHED_HMP;
		break;
	default:
		break;
	}
	spin_unlock_irqrestore(&sched_switch_lock, flags);

	return 0;
}
#else
int sched_scheduler_switch(enum SCHED_LB_TYPE new_sched)
{
	return -EINVAL;
}
#endif
EXPORT_SYMBOL(sched_scheduler_switch);

/* Try to get original capacity of all CPUs */
int show_cpu_capacity(char *buf, int buf_size)
{
	int cpu;
	int len = 0;

	for_each_possible_cpu(cpu) {
		len += snprintf(buf+len, buf_size-len,
			"cpu=%d orig=%4lu(%4lu) max_cap=%4lu max=%4lu min=%4lu ",
			cpu,
			cpu_rq(cpu)->cpu_capacity_orig,
			cpu_online(cpu) ? cpu_rq(cpu)->cpu_capacity : 0,
			cpu_online(cpu) ?
				cpu_rq(cpu)->rd->max_cpu_capacity.val : 0,
			/* limited frequency */
			cpu_online(cpu) ? arch_max_cpu_freq(NULL, cpu) *
				arch_max_freq_scale(NULL, cpu) >>
				SCHED_CAPACITY_SHIFT : 0,
			cpu_online(cpu) ? arch_max_cpu_freq(NULL, cpu) *
				arch_min_freq_scale(NULL, cpu) >>
				SCHED_CAPACITY_SHIFT : 0
			);

		len += snprintf(buf+len, buf_size-len,
			"cur_freq=%4luMHZ, cur=%4lu util=%4lu(%s)\n",
			/* current frequency */
			cpu_online(cpu) ? (arch_max_cpu_freq(NULL, cpu) *
			arch_scale_freq_capacity(NULL, cpu) >>
			SCHED_CAPACITY_SHIFT) / 1000 : 0,

			/* current capacity */
			cpu_online(cpu) ? capacity_curr_of(cpu) : 0,

			/* cpu utilization */
			cpu_online(cpu) ? get_cpu_util(cpu) : 0,

			/* cpu on/off */
			cpu_online(cpu) ? "on" : "off"
			);
	}

	return len;
}

#ifdef CONFIG_SCHED_DEBUG
/* A knob for turn on/off energe-aware scheduling */
static ssize_t show_eas_knob(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	if (sched_feat(SCHED_HMP) && sched_feat(ENERGY_AWARE))
		len += snprintf(buf, max_len, "scheduler= hybrid\n\n");
	else if (sched_feat(SCHED_HMP))
		len += snprintf(buf, max_len, "scheduler= HMP\n\n");
	else if (sched_feat(ENERGY_AWARE))
		len += snprintf(buf, max_len, "scheduler= EAS\n\n");
	else
		len += snprintf(buf, max_len, "scheduler= unknown\n\n");

	return len;
}

static ssize_t store_eas_knob(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;

	/*
	 * 0: HMP
	 * 1: EAS
	 * 2: Hybrid
	 */
	if (sscanf(buf, "%iu", &val) != 0)
		sched_scheduler_switch(val);

	return count;
}

static struct kobj_attribute eas_knob_attr =
__ATTR(enable, 0600, show_eas_knob,
		store_eas_knob);
#endif


/* For read info for EAS */
static ssize_t show_eas_info_attr(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;
	int max_cpu = -1, max_pid = 0, max_util = 0, boost = 0;
	struct task_struct *task = NULL;
	int cpu = smp_processor_id();

	len += snprintf(buf, max_len, "version=%d.%d(%s)\n\n",
		ver_major, ver_minor, module_name);
	len += show_cpu_capacity(buf+len, max_len - len);

	len += snprintf(buf+len, max_len - len, "max_cap_cpu=%d\n",
		cpu_rq(cpu)->rd->max_cpu_capacity.cpu);

	len += snprintf(buf+len, max_len - len, "max_cap_orig_cpu=%d\n",
		cpu_rq(cpu)->rd->max_cap_orig_cpu);

	len += snprintf(buf+len, max_len - len, "min_cap_orig_cpu=%d\n",
		cpu_rq(cpu)->rd->min_cap_orig_cpu);

	len += snprintf(buf+len, max_len - len,
			"turning_point=%d, big_cpu_eff_tp=%llu\n",
		cpu_eff_tp, big_cpu_eff_tp);

#ifdef CONFIG_MTK_SCHED_RQAVG_KS
	sched_max_util_task(&max_cpu, &max_pid, &max_util, &boost);
#endif

	task = find_task_by_vpid(max_pid);

	len += snprintf(buf+len, max_len - len,
		"\nheaviest task pid=%d (%s) util=%d boost=%d run in cpu%d\n\n",
		max_pid, (task)?task->comm:"NULL", max_util, boost, max_cpu);

	len += snprintf(buf+len, max_len - len,
			"foreground boost=%d, prefer_idle=%d\n",
			group_boost_read(1), group_prefer_idle_read(1));
	len += snprintf(buf+len, max_len - len,
			"top-app boost=%d, prefer_idle=%d\n",
			group_boost_read(3), group_prefer_idle_read(3));

	return len;
}

static struct kobj_attribute eas_info_attr =
__ATTR(info, 0400, show_eas_info_attr, NULL);

static struct attribute *eas_attrs[] = {
	&eas_info_attr.attr,
#ifdef CONFIG_SCHED_DEBUG
	&eas_knob_attr.attr,
#endif
	NULL,
};

static struct attribute_group eas_attr_group = {
	.attrs = eas_attrs,
};

static int init_eas_attribs(void)
{
	int err;

	eas_info.attr_group = &eas_attr_group;

	/* Create /sys/devices/system/cpu/eas/... */
	eas_info.kobj = kobject_create_and_add("eas",
				&cpu_subsys.dev_root->kobj);
	if (!eas_info.kobj)
		return -ENOMEM;

	err = sysfs_create_group(eas_info.kobj, eas_info.attr_group);
	if (err)
		kobject_put(eas_info.kobj);
	else
		kobject_uevent(eas_info.kobj, KOBJ_ADD);

	return err;
}

static int __init eas_stats_init(void)
{
	int ret = 0;

	eas_info.init = 0;

	snprintf(module_name, sizeof(module_name), "%s",
		"arctic"
	);

	ret = init_eas_attribs();

	if (ret)
		eas_info.init = 1;

	return ret;
}
late_initcall(eas_stats_init);
