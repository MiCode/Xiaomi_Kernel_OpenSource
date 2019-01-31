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

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
static char met_dvfs_info2[5][32] = {
	"sched_dvfs_boostmin_id0",
	"sched_dvfs_boostmin_id1",
	"sched_dvfs_boostmin_id2",
	"NULL",
	"NULL"
};

static char met_dvfs_info3[5][32] = {
	"sched_dvfs_capmin_id0",
	"sched_dvfs_capmin_id1",
	"sched_dvfs_capmin_id2",
	"NULL",
	"NULL"
};
#endif

int sys_boosted;

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
void update_freq_fastpath(void)
{
	int cid;

	if (!sched_freq())
		return;

#if MET_STUNE_DEBUG
	met_tag_oneshot(0, "sched_dvfsfast_path", 1);
#endif

#ifdef CONFIG_MTK_SCHED_VIP_TASKS
	/* force migrating vip task to higher idle cpu */
	vip_task_force_migrate();
#endif

	/* for each cluster*/
	for (cid = 0; cid < arch_get_nr_clusters(); cid++) {
		unsigned long capacity = 0;
		int cpu;
		struct cpumask cls_cpus;
		int first_cpu = -1;
		unsigned int freq_new = 0;
		unsigned long req_cap = 0;

		arch_get_cluster_cpus(&cls_cpus, cid);

		for_each_cpu(cpu, &cls_cpus) {
			struct sched_capacity_reqs *scr;

			if (!cpu_online(cpu) || cpu_isolated(cpu))
				continue;

			if (first_cpu < 0)
				first_cpu = cpu;

			scr = &per_cpu(cpu_sched_capacity_reqs, cpu);

			/* find boosted util per cpu.  */
			req_cap = boosted_cpu_util(cpu);

			/* Convert scale-invariant capacity to cpu. */
			req_cap = req_cap * SCHED_CAPACITY_SCALE /
						capacity_orig_of(cpu);

			req_cap += scr->rt;

			/* Add DVFS margin. */
			req_cap = req_cap * capacity_margin_dvfs /
						SCHED_CAPACITY_SCALE;

			req_cap += scr->dl;

			/* find max boosted util */
			capacity = max(capacity, req_cap);

			trace_sched_cpufreq_fastpath_request(cpu,
					req_cap, cpu_util(cpu),
					boosted_cpu_util(cpu),
					(int)scr->rt);
		} /* visit cpu over cluster */

		if (capacity > 0) { /* update freq in fast path */
			freq_new = capacity * arch_scale_get_max_freq(first_cpu)
					>> SCHED_CAPACITY_SHIFT;

			trace_sched_cpufreq_fastpath(cid, req_cap, freq_new);

			update_cpu_freq_quick(first_cpu, freq_new);
		}
	} /* visit each cluster */
#if MET_STUNE_DEBUG
	met_tag_oneshot(0, "sched_dvfsfast_path", 0);
#endif
}

void set_min_boost_freq(int boost_value, int cpu_clus)
{
	int max_clus_nr = arch_get_nr_clusters();
	int max_freq, max_cap, floor_cap, floor_freq;

	if (cpu_clus >= max_clus_nr || cpu_clus < 0)
		return;

	max_freq = schedtune_target_cap[cpu_clus].freq;
	max_cap = schedtune_target_cap[cpu_clus].cap;

	floor_cap = (max_cap * (int)(boost_value+1) / 100);
	floor_freq = (floor_cap * max_freq / max_cap);
	min_boost_freq[cpu_clus] =
		mt_cpufreq_find_close_freq(cpu_clus, floor_freq);
}

void set_cap_min_freq(int cap_min)
{
	int max_clus_nr = arch_get_nr_clusters();
	int max_freq, max_cap, min_freq;
	int i;

	for (i = 0; i < max_clus_nr; i++) {
		max_freq = schedtune_target_cap[i].freq;
		max_cap = schedtune_target_cap[i].cap;

		min_freq = (cap_min * max_freq / max_cap);
		cap_min_freq[i] = mt_cpufreq_find_close_freq(i, min_freq);
	}
}
#endif

int stune_task_threshold_for_perf_idx(bool filter)
{
	if (!default_stune_threshold)
		return -EINVAL;

	if (filter)
		stune_task_threshold = default_stune_threshold;
	else
		stune_task_threshold = 0;

#if MET_STUNE_DEBUG
	met_tag_oneshot(0, "sched_stune_filter", stune_task_threshold);
#endif
	return 0;
}

int capacity_min_write_for_perf_idx(int idx, int capacity_min)
{
	struct schedtune *ct = allocated_group[idx];

	if (!ct)
		return -EINVAL;

	if (capacity_min < 0 || capacity_min > 1024) {
		printk_deferred("warning: capacity_min should be 0~1024\n");
		if (capacity_min > 1024)
			capacity_min = 1024;
		else if (capacity_min < 0)
			capacity_min = 0;
	}

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
	set_cap_min_freq(capacity_min);
#endif
	rcu_read_lock();
	ct->capacity_min = capacity_min;

	/* Update CPU capacity_min */
	schedtune_boostgroup_update_capacity_min(ct->idx, ct->capacity_min);
	rcu_read_unlock();

#if MET_STUNE_DEBUG
	/* top-app */
	if (ct->idx == 3)
		met_tag_oneshot(0, "sched_boost_top_capacity_min",
				ct->capacity_min);
#endif

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
		met_tag_oneshot(0, "sched_top_prefer_idle",
				ct->prefer_idle);
#endif

	return 0;
}

int boost_write_for_perf_idx(int group_idx, int boost_value)
{
	struct schedtune *ct;
	unsigned int threshold_idx;
	int boost_pct;
	bool dvfs_on_demand = false;
	int idx = 0;
	int ctl_no = div64_s64(boost_value, 1000);
	int cluster;
	int cap_min = 0;
#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
	int floor = 0;
	int c0, c1, i;
#endif

	switch (ctl_no) {
	case 4:
		/* min cpu capacity request */
		boost_value -= ctl_no * 1000;

		if (boost_value < 0 || boost_value > 100) {
			printk_deferred("warning: boost for capacity_min should be 0~100\n");
			if (boost_value > 100)
				boost_value = 100;
			else if (boost_value < 0)
				boost_value = 0;
		}

		ct = allocated_group[group_idx];
		if (ct) {
			cap_min = div64_s64(boost_value * 1024, 100);

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
			set_cap_min_freq(cap_min);
#endif
			rcu_read_lock();
			ct->capacity_min = cap_min;
			/* Update CPU capacity_min */
			schedtune_boostgroup_update_capacity_min(
						ct->idx, ct->capacity_min);
			rcu_read_unlock();

#if MET_STUNE_DEBUG
			/* top-app */
			if (ct->idx == 3)
				met_tag_oneshot(
					0, "sched_boost_top_capacity_min",
					ct->capacity_min);
#endif
		}

		/* boost4xxx: no boost only capacity_min */
		boost_value = 0;

		stune_task_threshold = default_stune_threshold;
		break;
	case 3:
		/* a floor of cpu frequency */
		boost_value -= ctl_no * 1000;
		cluster = (int)boost_value / 100;
		boost_value = (int)boost_value % 100;

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
		if (cluster > 0 && cluster <= 0x2) { /* only two cluster */
			floor = 1;
			c0 = cluster & 0x1;
			c1 = cluster & 0x2;

			/* cluster 0 */
			if (c0)
				set_min_boost_freq(boost_value, 0);
			else
				min_boost_freq[0] = 0;

			/* cluster 1 */
			if (c1)
				set_min_boost_freq(boost_value, 1);
			else
				min_boost_freq[1] = 0;
		}
#endif
		stune_task_threshold = default_stune_threshold;
		break;
	case 2:
		/* dvfs short cut */
		boost_value -= 2000;
		stune_task_threshold = default_stune_threshold;
		dvfs_on_demand = true;
		break;
	case 1:
		/* boost all tasks */
		boost_value -= 1000;
		stune_task_threshold = 0;
		break;
	case 0:
		/* boost big task only */
		stune_task_threshold = default_stune_threshold;
		break;
	default:
		printk_deferred("warning: perf ctrl no should be 0~1\n");
		return -EINVAL;
	}

	if (boost_value < -100 || boost_value > 100)
		printk_deferred("warning: perf boost value should be -100~100\n");

	if (boost_value >= 100)
		boost_value = 100;
	else if (boost_value <= -100)
		boost_value = -100;

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
	for (i = 0; i < cpu_cluster_nr; i++) {
		if (!floor)
			min_boost_freq[i] = 0;
		if (!cap_min)
			cap_min_freq[i] = 0;

#if MET_STUNE_DEBUG
		met_tag_oneshot(0, met_dvfs_info2[i], min_boost_freq[i]);
		met_tag_oneshot(0, met_dvfs_info3[i], cap_min_freq[i]);
#endif
	}
#endif /* CONFIG_CPU_FREQ_GOV_SCHEDPLUS */

	if (!cap_min) {
		ct = allocated_group[group_idx];
		if (ct) {
			rcu_read_lock();
			ct->capacity_min = 0;
			/* Update CPU capacity_min */
			schedtune_boostgroup_update_capacity_min(
					ct->idx, ct->capacity_min);
			rcu_read_unlock();

#if MET_STUNE_DEBUG
			/* top-app */
			if (ct->idx == 3)
				met_tag_oneshot(
					0, "sched_boost_top_capacity_min",
					ct->capacity_min);
#endif
		}
	}

	if (boost_value < 0)
		global_negative_flag = true; /* set all group negative */
	else
		global_negative_flag = false;

	sys_boosted = boost_value;

	for (idx = 0; idx < BOOSTGROUPS_COUNT; idx++) {
		/* positive path or google original path */
		if (!global_negative_flag)
			idx = group_idx;

		ct = allocated_group[idx];

		if (ct) {
			rcu_read_lock();

			boost_pct = boost_value;

			/*
			 * Update threshold params for Performance Boost (B)
			 * and Performance Constraint (C) regions.
			 * The current implementatio uses the same cuts for both
			 * B and C regions.
			 */
			threshold_idx = clamp(boost_pct, 0, 99) / 10;
			ct->perf_boost_idx = threshold_idx;
			ct->perf_constrain_idx = threshold_idx;

			ct->boost = boost_value;

			trace_sched_tune_config(ct->boost);

			/* Update CPU boost */
			schedtune_boostgroup_update(ct->idx, ct->boost);
			rcu_read_unlock();

#if MET_STUNE_DEBUG
			/* foreground */
			if (ct->idx == 1)
				met_tag_oneshot(0, "sched_boost_fg", ct->boost);
			/* top-app */
			if (ct->idx == 3)
				met_tag_oneshot(
					0, "sched_boost_top", ct->boost);
#endif

			if (!global_negative_flag)
				break;

		} else {
			/* positive path or google original path */
			if (!global_negative_flag) {
				printk_deferred("error: perf boost for stune group no exist: idx=%d\n",
						idx);
				return -EINVAL;
			}

			break;
		}
	}

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
	if (dvfs_on_demand)
		update_freq_fastpath();
#endif

	return 0;
}

int group_boost_read(int group_idx)
{
	struct schedtune *ct;
	int boost = 0;

	ct = allocated_group[group_idx];
	if (ct) {
		rcu_read_lock();
		boost = ct->boost;
		rcu_read_unlock();
	}

	return boost;
}
EXPORT_SYMBOL(group_boost_read);

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
