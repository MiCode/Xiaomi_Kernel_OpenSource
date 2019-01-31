/*
 * Copyright (C) 2016 MediaTek Inc.
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
#include "eas_plus.h"

#include <mt-plat/met_drv.h>


/* To define limit of SODI */
int sodi_limit = DEFAULT_SODI_LIMIT;

/*
 * V1.04 support hybrid(EAS+HMP)
 * V1.05 if system is balanced, select CPU with max sparse capacity when wakeup
 * V1.06 add "turning" and "watershed" points to help HPS
 */

/* default scheduling */
static int sched_type = SCHED_HYBRID_LB;

/* watersched is updated by unified power table. */
static struct power_tuning_t power_tuning = {DEFAULT_TURNINGPOINT,
						DEFAULT_WATERSHED};

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

int sched_scheduler_switch(enum SCHED_LB_TYPE new_sched)
{
	unsigned long flags;

	if (sched_type >= SCHED_UNKNOWN_LB)
		return -1;

	spin_lock_irqsave(&sched_switch_lock, flags);
	sched_type = new_sched;
	spin_unlock_irqrestore(&sched_switch_lock, flags);

	return 0;
}
EXPORT_SYMBOL(sched_scheduler_switch);

bool is_game_mode;

#ifdef CONFIG_MTK_GPU_SUPPORT
void game_hint_notifier(int is_game)
{
	if (is_game) {
		sodi_limit = 120;
		is_game_mode = true;
	} else {
		sodi_limit = DEFAULT_SODI_LIMIT;
		is_game_mode = false;
	}
}
#endif
/* Try to get original capacity of all CPUs */
int show_cpu_capacity(char *buf, int buf_size)
{
	int cpu;
	int len = 0;

	for_each_possible_cpu(cpu) {
#if defined(CONFIG_CPU_FREQ_GOV_SCHED) || defined(CONFIG_CPU_FREQ_GOV_SCHEDPLUS)
		struct sched_capacity_reqs *scr;

		scr = &per_cpu(cpu_sched_capacity_reqs, cpu);
#endif
		len += snprintf(buf+len, buf_size-len,
			"cpu=%d orig=%4lu(%4lu) cap=%4lu max_cap=%4lu max=%4lu min=%4lu ",
			cpu,
			cpu_rq(cpu)->cpu_capacity_orig,
			cpu_rq(cpu)->cpu_capacity_hw,
			cpu_online(cpu)?cpu_rq(cpu)->cpu_capacity:0,
			cpu_online(cpu)?cpu_rq(cpu)->rd->max_cpu_capacity.val:0,
			/* limited frequency */
			cpu_online(cpu)?arch_scale_get_max_freq(cpu) / 1000 : 0,
			cpu_online(cpu)?arch_scale_get_min_freq(cpu) / 1000 : 0
			);

		len += snprintf(buf+len, buf_size-len,
			"cur_freq=%4luMHZ, cur=%4lu util=%4lu cfs=%4lu rt=%4lu (%s)\n",
			/* current frequency */
			cpu_online(cpu)?capacity_curr_of(cpu) *
			arch_scale_get_max_freq(cpu) /
			cpu_rq(cpu)->cpu_capacity_orig / 1000 : 0,

			/* current capacity */
			cpu_online(cpu)?capacity_curr_of(cpu):0,

			/* cpu utilization */
			cpu_online(cpu)?cpu_util(cpu):0,
#if defined(CONFIG_CPU_FREQ_GOV_SCHED) || defined(CONFIG_CPU_FREQ_GOV_SCHEDPLUS)
			scr->cfs,
			scr->rt,
#else
			0UL, 0UL,
#endif
			cpu_online(cpu)?"on":"off"
			);
	}

	return len;
}

/* read/write for watershed */
static ssize_t store_eas_watershed(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;

	if (sscanf(buf, "%iu", &val) != 0)
		power_tuning.watershed = val;

	return count;
}
static struct kobj_attribute eas_watershed_attr =
__ATTR(watershed, 0600, NULL,
		store_eas_watershed);

/* read/write for turning_point */
static ssize_t store_eas_turning_point(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;

	if (sscanf(buf, "%iu", &val) != 0 && (val < 100))
		power_tuning.turning_point = val;

	return count;
}
static struct kobj_attribute eas_turning_point_attr =
__ATTR(turning_point, 0600, NULL,
		store_eas_turning_point);

/* A knob for turn on/off energe-aware scheduling */
static ssize_t show_eas_knob(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	switch (sched_type) {
	case SCHED_HMP_LB:
		len += snprintf(buf, max_len, "scheduler= HMP\n\n");
		break;
	case SCHED_EAS_LB:
		len += snprintf(buf, max_len, "scheduler= EAS\n\n");
		break;
	case SCHED_HYBRID_LB:
		len += snprintf(buf, max_len, "scheduler= hybrid\n\n");
		break;
	default:
		len += snprintf(buf, max_len, "scheduler= unknown\n\n");
		break;
	}

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
	if (sscanf(buf, "%iu", &val) != 0) {
		if (val < SCHED_UNKNOWN_LB) {
			unsigned long flags;

			spin_lock_irqsave(&sched_switch_lock, flags);
			sched_type = val;
			spin_unlock_irqrestore(&sched_switch_lock, flags);
		}
	}

	return count;
}

static struct kobj_attribute eas_knob_attr =
__ATTR(enable, 0600, show_eas_knob,
		store_eas_knob);


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

	len += snprintf(buf+len, max_len - len, "\nwatershed=%d\n",
		power_tuning.watershed);
	len += snprintf(buf+len, max_len - len, "turning_point=%d\n",
		cpu_eff_tp);
	len += snprintf(buf+len, max_len - len, "tiny_thresh=%d\n",
		tiny_thresh);

#ifdef CONFIG_MTK_SCHED_RQAVG_KS
	sched_max_util_task(&max_cpu, &max_pid, &max_util, &boost);
#endif

	task = find_task_by_vpid(max_pid);

	len += snprintf(buf+len, max_len - len,
		"\nheaviest task pid=%d (%s) util=%d boost=%d run in cpu%d\n\n",
		max_pid, (task)?task->comm:"NULL", max_util, boost, max_cpu);

	len += snprintf(buf+len, max_len - len,
			"foreground boost=%d\n", group_boost_read(1));
	len += snprintf(buf+len, max_len - len,
			"top-app boost=%d\n", group_boost_read(3));

	return len;
}

/* To identify min capacity for stune to boost */
static ssize_t store_stune_task_thresh_knob(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (sscanf(buf, "%iu", &val) != 0) {
		if (val < 1024 || val >= 0)
			stune_task_threshold = val;
	}

	met_tag_oneshot(0, "sched_stune_filter", stune_task_threshold);

	return count;
}

static ssize_t show_stune_task_thresh_knob(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len += snprintf(buf, max_len, "stune_task_thresh=%d\n",
			stune_task_threshold);

	met_tag_oneshot(0, "sched_stune_filter",
			stune_task_threshold);

	return len;
}

static struct kobj_attribute eas_stune_task_thresh_attr =
__ATTR(stune_task_thresh, 0600,
	show_stune_task_thresh_knob,
	store_stune_task_thresh_knob);

static struct kobj_attribute eas_info_attr =
__ATTR(info, 0400, show_eas_info_attr, NULL);

/* To define capacity_margin */
static ssize_t store_cap_margin_knob(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
	int val = 0;

	if (sscanf(buf, "%iu", &val) != 0)
		capacity_margin_dvfs = val;
#endif
	return count;
}

static ssize_t show_cap_margin_knob(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
	unsigned int max_len = 4096;

	len += snprintf(buf, max_len, "capacity_margin_dvfs=%d\n",
			capacity_margin_dvfs);
#endif
	return len;
}

static struct kobj_attribute eas_cap_margin_dvfs_attr =
__ATTR(cap_margin_dvfs, 0600, show_cap_margin_knob,
		store_cap_margin_knob);

/* To set limit of SODI */
static ssize_t store_sodi_limit_knob(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (sscanf(buf, "%iu", &val) != 0)
		sodi_limit = val;

	return count;
}

static ssize_t show_sodi_limit_knob(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len += snprintf(buf, max_len, "sodi limit=%d\n", sodi_limit);

	return len;
}

static struct kobj_attribute eas_sodi_limit_attr =
__ATTR(sodi_limit, 0600, show_sodi_limit_knob,
		store_sodi_limit_knob);

static struct attribute *eas_attrs[] = {
	&eas_info_attr.attr,
	&eas_knob_attr.attr,
	&eas_watershed_attr.attr,
	&eas_turning_point_attr.attr,
	&eas_stune_task_thresh_attr.attr,
	&eas_cap_margin_dvfs_attr.attr,
	&eas_sodi_limit_attr.attr,
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

	snprintf(module_name, sizeof(module_name), "%s %s",
		"arctic",
#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
		"sched+"
#else
		"others"
#endif
	);

	ret = init_eas_attribs();

	if (ret)
		eas_info.init = 1;

#ifdef CONFIG_MTK_GPU_SUPPORT
	ged_kpi_set_game_hint_value_fp = game_hint_notifier;
#endif
	return ret;
}
late_initcall(eas_stats_init);
