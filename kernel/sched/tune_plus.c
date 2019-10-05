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

#define MET_STUNE_DEBUG 1

#if MET_STUNE_DEBUG
#include <mt-plat/met_drv.h>
#endif

int stune_task_threshold;

/* A mutex for set stune_task_threshold */
static DEFINE_MUTEX(stune_threshold_mutex);

int set_stune_task_threshold(int threshold)
{
	if (threshold > 1024 || threshold < -1)
		return -EINVAL;

	mutex_lock(&stune_threshold_mutex);

	if (threshold < 0)
		stune_task_threshold = default_stune_threshold;
	else
		stune_task_threshold = threshold;

	mutex_unlock(&stune_threshold_mutex);

#if MET_STUNE_DEBUG
	met_tag_oneshot(0, "sched_stune_threshold", stune_task_threshold);
#endif

	return 0;
}

int sched_stune_task_threshold_handler(struct ctl_table *table,
					int write, void __user *buffer,
					size_t *lenp, loff_t *ppos)
{
	int ret;
	int old_threshold;

	old_threshold = stune_task_threshold;
	ret = proc_dointvec(table, write, buffer, lenp, ppos);
	if (!ret && write) {
		ret = set_stune_task_threshold(stune_task_threshold);
		if (ret)
			stune_task_threshold = old_threshold;
	}

	return ret;
}

int boost_write_for_perf_idx(int idx, int boost_value)
{
	struct schedtune *ct;

	if (boost_value < 0 || boost_value > 100)
		printk_deferred("warn: boost value should be 0~100\n");

	if (boost_value >= 100)
		boost_value = 100;
	else if (boost_value <= 0)
		boost_value = 0;

	ct = allocated_group[idx];
	if (ct) {
		rcu_read_lock();
		ct->boost = boost_value;

		/* Update CPU boost */
		schedtune_boostgroup_update(ct->idx, ct->boost);
		rcu_read_unlock();

#if MET_STUNE_DEBUG
		/* foreground */
		if (ct->idx == 1)
			met_tag_oneshot(0, "sched_boost_fg", ct->boost);
		/* top-app */
		if (ct->idx == 3)
			met_tag_oneshot(0, "sched_boost_ta", ct->boost);
#endif
	} else {
		printk_deferred("error: stune group idx=%d is nonexistent\n",
				idx);
		return -EINVAL;
	}

	return 0;
}

int prefer_idle_for_perf_idx(int idx, int prefer_idle)
{
	struct schedtune *ct = allocated_group[idx];

	if (!ct)
		return -EINVAL;

	rcu_read_lock();
	ct->prefer_idle = prefer_idle;
	rcu_read_unlock();

#if MET_STUNE_DEBUG
	/* top-app */
	if (ct->idx == 3)
		met_tag_oneshot(0, "sched_prefer_idle_ta",
				ct->prefer_idle);
#endif

	return 0;
}

#ifdef CONFIG_MTK_SCHED_RQAVG_KS
/* mtk: a linear boost value for tuning */
int linear_real_boost(int linear_boost)
{
	int target_cpu, usage;
	int boost;
	int ta_org_cap;

	sched_max_util_task(&target_cpu, NULL, &usage, NULL);

	ta_org_cap = capacity_orig_of(target_cpu);

	if (usage >= SCHED_CAPACITY_SCALE)
		usage = SCHED_CAPACITY_SCALE;

	/*
	 * Conversion Formula of Linear Boost:
	 *
	 *   margin = (usage * linear_boost)/100;
	 *   margin = (original_cap - usage) * boost/100;
	 *   so
	 *   boost = (usage * linear_boost) / (original_cap - usage)
	 */
	if (ta_org_cap <= usage) {
		/* If target cpu is saturated, consider bigger one */
		boost = (SCHED_CAPACITY_SCALE - usage) ?
		   (usage * linear_boost)/(SCHED_CAPACITY_SCALE - usage) : 0;
	} else
		boost = (usage * linear_boost)/(ta_org_cap - usage);

	return boost;
}
EXPORT_SYMBOL(linear_real_boost);
#endif
