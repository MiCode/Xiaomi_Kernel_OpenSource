// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2025, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kmemleak.h>
#include <trace/hooks/sched.h>

#include "walt.h"
#include "trace.h"

static int neg_five = -5;
static int three = 3;
static int four = 4;
static int five = 5;
static int two_hundred_fifty_five = 255;
static unsigned int ns_per_sec = NSEC_PER_SEC;
static unsigned int one_hundred_thousand = 100000;
static unsigned int two_hundred_million = 200000000;
static int __maybe_unused two = 2;
static int one_hundred = 100;
static int one_thousand = 1000;
static int one_thousand_twenty_four = 1024;
static int two_thousand = 2000;
static int max_nr_pipeline = MAX_NR_PIPELINE;

/*
 * CFS task prio range is [100 ... 139]
 * 120 is the default prio.
 * RTG boost range is [100 ... 119] because giving
 * boost for [120 .. 139] does not make sense.
 * 99 means disabled and it is the default value.
 */
static unsigned int min_cfs_boost_prio = 99;
static unsigned int max_cfs_boost_prio = 119;

unsigned int sysctl_sched_capacity_margin_up_pct[MAX_MARGIN_LEVELS];
unsigned int sysctl_sched_capacity_margin_dn_pct[MAX_MARGIN_LEVELS];
unsigned int sysctl_sched_busy_hyst_enable_cpus;
unsigned int sysctl_sched_busy_hyst;
unsigned int sysctl_sched_coloc_busy_hyst_enable_cpus;
unsigned int sysctl_sched_coloc_busy_hyst_cpu[WALT_NR_CPUS];
unsigned int sysctl_sched_coloc_busy_hyst_max_ms;
unsigned int sysctl_sched_coloc_busy_hyst_cpu_busy_pct[WALT_NR_CPUS];
unsigned int sysctl_sched_util_busy_hyst_enable_cpus;
unsigned int sysctl_sched_util_busy_hyst_cpu[WALT_NR_CPUS];
unsigned int sysctl_sched_util_busy_hyst_cpu_util[WALT_NR_CPUS];
unsigned int sysctl_sched_boost;
unsigned int sysctl_sched_wake_up_idle[2];
unsigned int sysctl_input_boost_ms;
unsigned int sysctl_input_boost_freq[WALT_NR_CPUS];
unsigned int sysctl_sched_boost_on_input;
unsigned int sysctl_sched_early_up[MAX_MARGIN_LEVELS];
unsigned int sysctl_sched_early_down[MAX_MARGIN_LEVELS];

/* sysctl nodes accessed by other files */
unsigned int __read_mostly sysctl_sched_coloc_downmigrate_ns;
unsigned int __read_mostly sysctl_sched_group_downmigrate_pct;
unsigned int __read_mostly sysctl_sched_group_upmigrate_pct;
unsigned int __read_mostly sysctl_sched_window_stats_policy;
unsigned int sysctl_sched_ravg_window_nr_ticks;
unsigned int sysctl_sched_walt_rotate_big_tasks;
unsigned int sysctl_sched_task_unfilter_period;
unsigned int sysctl_walt_low_latency_task_threshold; /* disabled by default */
unsigned int sysctl_sched_conservative_pl;
unsigned int sysctl_sched_min_task_util_for_boost;
unsigned int sysctl_sched_min_task_util_for_uclamp;
unsigned int sysctl_sched_min_task_util_for_colocation;
unsigned int sysctl_sched_many_wakeup_threshold;
const int sched_user_hint_max = 1000;
unsigned int sysctl_walt_rtg_cfs_boost_prio; /* disabled by default */
unsigned int sysctl_sched_sync_hint_enable;
unsigned int sysctl_panic_on_walt_bug;
unsigned int sysctl_sched_suppress_region2;
unsigned int sysctl_sched_skip_sp_newly_idle_lb;
unsigned int sysctl_sched_hyst_min_coloc_ns;
unsigned int sysctl_sched_asymcap_boost;
unsigned int sysctl_sched_long_running_rt_task_ms;
unsigned int sysctl_sched_idle_enough;
unsigned int sysctl_sched_cluster_util_thres_pct;
unsigned int sysctl_sched_idle_enough_clust[MAX_CLUSTERS];
unsigned int sysctl_sched_cluster_util_thres_pct_clust[MAX_CLUSTERS];
unsigned int sysctl_ed_boost_pct;
unsigned int sysctl_em_inflate_pct;
unsigned int sysctl_em_inflate_thres;
unsigned int sysctl_sched_heavy_nr;
unsigned int sysctl_max_freq_partial_halt;
unsigned int sysctl_freq_cap[MAX_CLUSTERS];
unsigned int sysctl_sched_sbt_pause_cpus;
unsigned int sysctl_sched_sbt_enable = 1;
unsigned int sysctl_sched_sbt_delay_windows;
unsigned int high_perf_cluster_freq_cap[MAX_CLUSTERS];
unsigned int sysctl_sched_pipeline_cpus;
unsigned int freq_cap[MAX_FREQ_CAP][MAX_CLUSTERS];
unsigned int sysctl_sched_pipeline_special;
unsigned int sysctl_sched_pipeline_util_thres;
unsigned int sysctl_ipc_freq_levels_cluster0[SMART_FMAX_IPC_MAX];
unsigned int sysctl_ipc_freq_levels_cluster1[SMART_FMAX_IPC_MAX];
unsigned int sysctl_ipc_freq_levels_cluster2[SMART_FMAX_IPC_MAX];
unsigned int sysctl_ipc_freq_levels_cluster3[SMART_FMAX_IPC_MAX];
unsigned int sysctl_legacy_freq_levels_cluster0[LEGACY_SMART_FREQ*2];
unsigned int sysctl_legacy_freq_levels_cluster1[LEGACY_SMART_FREQ*2];
unsigned int sysctl_legacy_freq_levels_cluster2[LEGACY_SMART_FREQ*2];
unsigned int sysctl_legacy_freq_levels_cluster3[LEGACY_SMART_FREQ*2];
unsigned int sysctl_sched_walt_core_util[WALT_NR_CPUS];
unsigned int sysctl_disable_minfreq_pause;
unsigned int sysctl_sched_storage_boost_disable;
unsigned int sysctl_pipeline_busy_boost_pct;
unsigned int sysctl_sched_lrpb_active_ms[NUM_PIPELINE_BUSY_THRES];
unsigned int sysctl_cluster01_load_sync[NUM_LOAD_SYNC_SETTINGS];
unsigned int sysctl_cluster01_load_sync_60fps[NUM_LOAD_SYNC_SETTINGS];
unsigned int sysctl_cluster02_load_sync[NUM_LOAD_SYNC_SETTINGS];
unsigned int sysctl_cluster03_load_sync[NUM_LOAD_SYNC_SETTINGS];
unsigned int sysctl_cluster10_load_sync[NUM_LOAD_SYNC_SETTINGS];
unsigned int sysctl_cluster10_load_sync_60fps[NUM_LOAD_SYNC_SETTINGS];
unsigned int sysctl_cluster12_load_sync[NUM_LOAD_SYNC_SETTINGS];
unsigned int sysctl_cluster13_load_sync[NUM_LOAD_SYNC_SETTINGS];
unsigned int sysctl_cluster20_load_sync[NUM_LOAD_SYNC_SETTINGS];
unsigned int sysctl_cluster21_load_sync[NUM_LOAD_SYNC_SETTINGS];
unsigned int sysctl_cluster23_load_sync[NUM_LOAD_SYNC_SETTINGS];
unsigned int sysctl_cluster30_load_sync[NUM_LOAD_SYNC_SETTINGS];
unsigned int sysctl_cluster31_load_sync[NUM_LOAD_SYNC_SETTINGS];
unsigned int sysctl_cluster32_load_sync[NUM_LOAD_SYNC_SETTINGS];
unsigned int load_sync_util_thres[MAX_CLUSTERS][MAX_CLUSTERS];
unsigned int load_sync_util_thres_60fps[MAX_CLUSTERS][MAX_CLUSTERS];
unsigned int load_sync_low_pct[MAX_CLUSTERS][MAX_CLUSTERS];
unsigned int load_sync_low_pct_60fps[MAX_CLUSTERS][MAX_CLUSTERS];
unsigned int load_sync_high_pct[MAX_CLUSTERS][MAX_CLUSTERS];
unsigned int load_sync_high_pct_60fps[MAX_CLUSTERS][MAX_CLUSTERS];
unsigned int sysctl_force_frequent_yielder;
unsigned int sysctl_pipeline_special_task_util_thres;
unsigned int sysctl_pipeline_non_special_task_util_thres;
unsigned int sysctl_pipeline_pin_thres_low_pct;
unsigned int sysctl_pipeline_pin_thres_high_pct;
unsigned int sysctl_pipeline_rearrange_delay_ms[2] = {100, 4};
unsigned int sysctl_single_thread_pipeline;
unsigned int sysctl_pipeline_swap_util_th = 100;

/* range is [1 .. INT_MAX] */
static int sysctl_task_read_pid = 1;

static int walt_proc_group_thresholds_handler(struct ctl_table *table, int write,
				       void __user *buffer, size_t *lenp,
				       loff_t *ppos)
{
	int ret;
	static DEFINE_MUTEX(mutex);
	struct rq *rq = cpu_rq(cpumask_first(cpu_possible_mask));
	unsigned long flags;
	unsigned int *data = (unsigned int *)table->data;
	int val;
	struct ctl_table tmp = {
		.data	= &val,
		.maxlen	= sizeof(int),
		.mode	= table->mode,
	};

	if (unlikely(num_sched_clusters <= 0))
		return -EPERM;

	mutex_lock(&mutex);

	if (!write) {
		ret = proc_dointvec(table, write, buffer, lenp, ppos);
		goto unlock_mutex;
	}

	ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
	if (ret)
		goto unlock_mutex;

	/*
	 * Ensure 0 <= sched_group_dowmigrate < sched_group_upmigrate,
	 * unless sched_group_downmigrate is 0, in which case sched_group_upmigrate can also be 0,
	 * which will disable colocation.
	 */
	if (data == &sysctl_sched_group_upmigrate_pct) {
		if (val < 0 ||
			(sysctl_sched_group_downmigrate_pct != 0 && val == 0) ||
			(val <= sysctl_sched_group_downmigrate_pct && val != 0)) {
			ret = -EINVAL;
			goto unlock_mutex;
		}
	} else {
		if (val < 0 || val >= sysctl_sched_group_upmigrate_pct) {
			ret = -EINVAL;
			goto unlock_mutex;
		}
	}
	*data = val;

	/*
	 * The load scale factor update happens with all
	 * rqs locked. so acquiring 1 CPU rq lock and
	 * updating the thresholds is sufficient for
	 * an atomic update.
	 */
	raw_spin_lock_irqsave(&rq->__lock, flags);
	walt_update_group_thresholds();
	raw_spin_unlock_irqrestore(&rq->__lock, flags);

unlock_mutex:
	mutex_unlock(&mutex);

	return ret;
}

static int walt_proc_user_hint_handler(struct ctl_table *table,
				int write, void __user *buffer, size_t *lenp,
				loff_t *ppos)
{
	int ret;
	unsigned int old_value;
	static DEFINE_MUTEX(mutex);

	mutex_lock(&mutex);

	old_value = sysctl_sched_user_hint;
	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	if (ret || !write || (old_value == sysctl_sched_user_hint))
		goto unlock;

	sched_user_hint_reset_time = jiffies + HZ;
	walt_irq_work_queue(&walt_migration_irq_work);

unlock:
	mutex_unlock(&mutex);
	return ret;
}

DECLARE_BITMAP(sysctl_bitmap, WALT_NR_CPUS);

static int walt_proc_sbt_pause_handler(struct ctl_table *table,
				       int write, void __user *buffer, size_t *lenp,
				       loff_t *ppos)
{
	int ret = 0;
	unsigned int old_value;
	unsigned long bitmask;
	const unsigned long *bitmaskp = &bitmask;
	static bool written_once;
	static DEFINE_MUTEX(mutex);

	mutex_lock(&mutex);

	old_value = sysctl_sched_sbt_pause_cpus;
	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	if (ret || !write || (old_value == sysctl_sched_sbt_pause_cpus))
		goto unlock;

	if (written_once) {
		sysctl_sched_sbt_pause_cpus = old_value;
		goto unlock;
	}

	bitmask = (unsigned long)sysctl_sched_sbt_pause_cpus;
	bitmap_copy(sysctl_bitmap, bitmaskp, WALT_NR_CPUS);
	cpumask_copy(&cpus_for_sbt_pause, to_cpumask(sysctl_bitmap));

	written_once = true;
unlock:
	mutex_unlock(&mutex);
	return ret;
}

/* pipeline cpus are non-prime cpus chosen to handle pipeline tasks, e.g. golds */
static int walt_proc_pipeline_cpus_handler(struct ctl_table *table,
					   int write, void __user *buffer, size_t *lenp,
					   loff_t *ppos)
{
	int ret = 0;
	unsigned int old_value;
	unsigned long bitmask;
	const unsigned long *bitmaskp = &bitmask;
	static bool written_once;
	static DEFINE_MUTEX(mutex);

	mutex_lock(&mutex);

	old_value = sysctl_sched_pipeline_cpus;
	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	if (ret || !write || (old_value == sysctl_sched_pipeline_cpus))
		goto unlock;

	if (written_once) {
		sysctl_sched_pipeline_cpus = old_value;
		goto unlock;
	}

	bitmask = (unsigned long)sysctl_sched_pipeline_cpus;
	bitmap_copy(sysctl_bitmap, bitmaskp, WALT_NR_CPUS);
	cpumask_copy(&cpus_for_pipeline, to_cpumask(sysctl_bitmap));

	written_once = true;
unlock:
	mutex_unlock(&mutex);
	return ret;
}

struct task_struct *pipeline_special_task;
static int sched_pipeline_special_handler(struct ctl_table *table,
					   int write, void __user *buffer, size_t *lenp,
					   loff_t *ppos)
{
	int ret = 0;
	static DEFINE_MUTEX(mutex);
	struct task_struct *pipeline_special_local;

	mutex_lock(&mutex);

	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	if (ret || !write)
		goto unlock;

	if (!sysctl_sched_pipeline_special) {
		remove_special_task();
		goto unlock;
	}

	pipeline_special_local =
		get_pid_task(find_vpid(sysctl_sched_pipeline_special), PIDTYPE_PID);
	if (!pipeline_special_local) {
		remove_special_task();
		sysctl_sched_pipeline_special = 0;
		ret = -ENOENT;
		goto unlock;
	}
	put_task_struct(pipeline_special_local);
	set_special_task(pipeline_special_local);
unlock:
	mutex_unlock(&mutex);
	return ret;
}

static int walt_single_thread_pipeline_handler(struct ctl_table *table,
					   int write, void __user *buffer, size_t *lenp,
					   loff_t *ppos)
{
	int ret = 0;
	unsigned int val;

	struct ctl_table tmp = {
		.data	= &val,
		.maxlen	= sizeof(val),
		.mode	= table->mode,
	};
	static DEFINE_MUTEX(mutex);

	mutex_lock(&mutex);

	val = sysctl_single_thread_pipeline;
	ret = proc_dointvec_minmax(&tmp, write, buffer, lenp, ppos);
	if (ret || !write || val ==  sysctl_single_thread_pipeline)
		goto unlock;

	walt_configure_single_thread_pipeline(val);
unlock:
	mutex_unlock(&mutex);
	return ret;
}

static int sched_ravg_window_handler(struct ctl_table *table,
				int write, void __user *buffer, size_t *lenp,
				loff_t *ppos)
{
	int ret = -EPERM;
	static DEFINE_MUTEX(mutex);
	int val;

	struct ctl_table tmp = {
		.data	= &val,
		.maxlen	= sizeof(val),
		.mode	= table->mode,
	};

	mutex_lock(&mutex);

	if (write && HZ != 250)
		goto unlock;

	val = sysctl_sched_ravg_window_nr_ticks;
	ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
	if (ret || !write || (val == sysctl_sched_ravg_window_nr_ticks))
		goto unlock;

	if (val != 2 && val != 3 && val != 4 && val != 5 && val != 8) {
		ret = -EINVAL;
		goto unlock;
	}

	sysctl_sched_ravg_window_nr_ticks = val;
	sched_window_nr_ticks_change();

unlock:
	mutex_unlock(&mutex);
	return ret;
}

static DEFINE_MUTEX(sysctl_pid_mutex);
static int sched_task_read_pid_handler(struct ctl_table *table, int write,
				       void __user *buffer, size_t *lenp,
				       loff_t *ppos)
{
	int ret;

	mutex_lock(&sysctl_pid_mutex);
	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	mutex_unlock(&sysctl_pid_mutex);

	return ret;
}

enum {
	TASK_BEGIN = 0,
	WAKE_UP_IDLE,
	INIT_TASK_LOAD,
	GROUP_ID,
	PER_TASK_BOOST,
	PER_TASK_BOOST_PERIOD_MS,
	LOW_LATENCY,
	PIPELINE,
	LOAD_BOOST,
	REDUCE_AFFINITY,
	MPAM_PART_ID,
};

static int sched_task_handler(struct ctl_table *table, int write,
				void __user *buffer, size_t *lenp,
				loff_t *ppos)
{
	int ret, param;
	struct task_struct *task;
	int pid_and_val[2] = {-1, -1};
	int val;
	struct walt_task_struct *wts;
	struct rq *rq;
	struct rq_flags rf;
	unsigned long bitmask;
	const unsigned long *bitmaskp = &bitmask;

	struct ctl_table tmp = {
		.data	= &pid_and_val,
		.maxlen	= sizeof(pid_and_val),
		.mode	= table->mode,
	};

	mutex_lock(&sysctl_pid_mutex);

	if (!write) {
		task = get_pid_task(find_vpid(sysctl_task_read_pid),
				PIDTYPE_PID);
		if (!task) {
			ret = -ENOENT;
			goto unlock_mutex;
		}
		wts = (struct walt_task_struct *) task->android_vendor_data1;
		pid_and_val[0] = sysctl_task_read_pid;
		param = (unsigned long)table->data;
		switch (param) {
		case WAKE_UP_IDLE:
			pid_and_val[1] = wts->wake_up_idle;
			break;
		case INIT_TASK_LOAD:
			pid_and_val[1] = wts->init_load_pct;
			break;
		case GROUP_ID:
			pid_and_val[1] = sched_get_group_id(task);
			break;
		case PER_TASK_BOOST:
			pid_and_val[1] = wts->boost;
			break;
		case PER_TASK_BOOST_PERIOD_MS:
			pid_and_val[1] =
				div64_ul(wts->boost_period,
					 1000000UL);
			break;
		case LOW_LATENCY:
			pid_and_val[1] = !!(wts->low_latency &
					 WALT_LOW_LATENCY_PROCFS_BIT);
			break;
		case PIPELINE:
			pid_and_val[1] = !!(wts->low_latency &
					 WALT_LOW_LATENCY_PIPELINE_BIT);
			break;
		case LOAD_BOOST:
			pid_and_val[1] = wts->load_boost;
			break;
		case REDUCE_AFFINITY:
			pid_and_val[1] = cpumask_bits(&wts->reduce_mask)[0];
			break;
		case MPAM_PART_ID:
			pid_and_val[1] = wts->mpam_part_id;
			break;
		default:
			ret = -EINVAL;
			goto put_task;
		}
		ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
		goto put_task;
	}

	ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
	if (ret)
		goto unlock_mutex;

	if (pid_and_val[0] <= 0) {
		ret = -ENOENT;
		goto unlock_mutex;
	}

	/* parsed the values successfully in pid_and_val[] array */
	task = get_pid_task(find_vpid(pid_and_val[0]), PIDTYPE_PID);
	if (!task) {
		ret = -ENOENT;
		goto unlock_mutex;
	}
	wts = (struct walt_task_struct *) task->android_vendor_data1;
	param = (unsigned long)table->data;
	val = pid_and_val[1];
	if (param != LOAD_BOOST && val < 0) {
		ret = -EINVAL;
		goto put_task;
	}
	switch (param) {
	case WAKE_UP_IDLE:
		wts->wake_up_idle = val;
		break;
	case INIT_TASK_LOAD:
		if (pid_and_val[1] < 0 || pid_and_val[1] > 100) {
			ret = -EINVAL;
			goto put_task;
		}
		wts->init_load_pct = val;
		break;
	case GROUP_ID:
		ret = sched_set_group_id(task, val);
		break;
	case PER_TASK_BOOST:
		if (val < TASK_BOOST_NONE || val >= TASK_BOOST_END) {
			ret = -EINVAL;
			goto put_task;
		}
		wts->boost = val;
		if (val == 0)
			wts->boost_period = 0;
		break;
	case PER_TASK_BOOST_PERIOD_MS:
		if (wts->boost == 0 && val) {
			/* setting boost period w/o boost is invalid */
			ret = -EINVAL;
			goto put_task;
		}
		wts->boost_period = (u64)val * 1000 * 1000;
		wts->boost_expires = sched_clock() + wts->boost_period;
		break;
	case LOW_LATENCY:
		if (val)
			wts->low_latency |= WALT_LOW_LATENCY_PROCFS_BIT;
		else
			wts->low_latency &= ~WALT_LOW_LATENCY_PROCFS_BIT;
		break;
	case PIPELINE:
		rq = task_rq_lock(task, &rf);
		if (READ_ONCE(task->__state) == TASK_DEAD) {
			ret = -EINVAL;
			task_rq_unlock(rq, task, &rf);
			goto put_task;
		}
		if (val) {
			ret = add_pipeline(wts);
			if (ret < 0) {
				task_rq_unlock(rq, task, &rf);
				goto put_task;
			}
			wts->low_latency |= WALT_LOW_LATENCY_PIPELINE_BIT;
		} else {
			wts->low_latency &= ~WALT_LOW_LATENCY_PIPELINE_BIT;
			remove_pipeline(wts);
		}
		task_rq_unlock(rq, task, &rf);
		break;
	case LOAD_BOOST:
		if (pid_and_val[1] < -90 || pid_and_val[1] > 90) {
			ret = -EINVAL;
			goto put_task;
		}
		wts->load_boost = val;
		if (val)
			wts->boosted_task_load = mult_frac((int64_t)1024, (int64_t)val, 100);
		else
			wts->boosted_task_load = 0;
		break;
	case REDUCE_AFFINITY:
		bitmask = (unsigned long) val;
		bitmap_copy(sysctl_bitmap, bitmaskp, WALT_NR_CPUS);
		cpumask_copy(&wts->reduce_mask, to_cpumask(sysctl_bitmap));
		break;
	default:
		ret = -EINVAL;
	}

	trace_sched_task_handler(task, param, val, CALLER_ADDR0, CALLER_ADDR1,
				CALLER_ADDR2, CALLER_ADDR3, CALLER_ADDR4, CALLER_ADDR5);
put_task:
	put_task_struct(task);
unlock_mutex:
	mutex_unlock(&sysctl_pid_mutex);

	return ret;
}

#ifdef CONFIG_PROC_SYSCTL
static void sched_update_updown_migrate_values(bool up)
{
	int i = 0, cpu;
	struct walt_sched_cluster *cluster;

	for_each_sched_cluster(cluster) {
		/*
		 * No need to worry about CPUs in last cluster
		 * if there are more than 2 clusters in the system
		 */
		for_each_cpu(cpu, &cluster->cpus) {
			if (up)
				sched_capacity_margin_up[cpu] =
				SCHED_FIXEDPOINT_SCALE * 100 /
				sysctl_sched_capacity_margin_up_pct[i];
			else
				sched_capacity_margin_down[cpu] =
				SCHED_FIXEDPOINT_SCALE * 100 /
				sysctl_sched_capacity_margin_dn_pct[i];
		}

		trace_sched_update_updown_migrate_values(up, i);
		if (++i >= num_sched_clusters - 1)
			break;
	}
}

int sched_updown_migrate_handler(struct ctl_table *table, int write,
				void __user *buffer, size_t *lenp,
				loff_t *ppos)
{
	int ret, i;
	unsigned int *data = (unsigned int *)table->data;
	static DEFINE_MUTEX(mutex);
	int cap_margin_levels = num_sched_clusters ? num_sched_clusters - 1 : 0;
	int val[MAX_MARGIN_LEVELS];
	struct ctl_table tmp = {
		.data	= &val,
		.maxlen	= sizeof(int) * MAX_MARGIN_LEVELS,
		.mode	= table->mode,
	};

	if (cap_margin_levels <= 0)
		return -EINVAL;

	mutex_lock(&mutex);

	if (!write) {
		tmp.maxlen = sizeof(int) * cap_margin_levels;
		tmp.data = table->data;
		ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
		goto unlock_mutex;
	}

	ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
	if (ret)
		goto unlock_mutex;

	/* check if valid pct values are passed in */
	for (i = 0; i < cap_margin_levels; i++) {
		if (val[i] <= 0 || val[i] > 100) {
			ret = -EINVAL;
			goto unlock_mutex;
		}
	}

	/* check up pct is greater than dn pct */
	if (data == &sysctl_sched_capacity_margin_up_pct[0]) {
		for (i = 0; i < cap_margin_levels; i++) {
			if (val[i] < sysctl_sched_capacity_margin_dn_pct[i]) {
				ret = -EINVAL;
				goto unlock_mutex;
			}
		}
	} else {
		for (i = 0; i < cap_margin_levels; i++) {
			if (sysctl_sched_capacity_margin_up_pct[i] < val[i]) {
				ret = -EINVAL;
				goto unlock_mutex;
			}
		}
	}

	/* all things checkout update the value */
	for (i = 0; i < cap_margin_levels; i++)
		data[i] = val[i];

	/* update individual cpu thresholds */
	sched_update_updown_migrate_values(data == &sysctl_sched_capacity_margin_up_pct[0]);

unlock_mutex:
	mutex_unlock(&mutex);

	return ret;
}

static void sched_update_updown_early_migrate_values(bool up)
{
	int i = 0, cpu;
	struct walt_sched_cluster *cluster;

	for_each_sched_cluster(cluster) {
		/*
		 * No need to worry about CPUs in last cluster
		 * if there are more than 2 clusters in the system
		 */
		for_each_cpu(cpu, &cluster->cpus) {
			if (up)
				sched_capacity_margin_early_up[cpu] = sysctl_sched_early_up[i];
			else
				sched_capacity_margin_early_down[cpu] = sysctl_sched_early_down[i];
		}

		trace_sched_update_updown_early_migrate_values(up, i);
		if (++i >= num_sched_clusters - 1)
			break;
	}
}

int sched_updown_early_migrate_handler(struct ctl_table *table, int write,
				void __user *buffer, size_t *lenp,
				loff_t *ppos)
{
	int ret, i;
	unsigned int *data = (unsigned int *)table->data;
	static DEFINE_MUTEX(mutex);
	int cap_margin_levels = num_sched_clusters ? num_sched_clusters - 1 : 0;
	int val[MAX_MARGIN_LEVELS];
	struct ctl_table tmp = {
		.data	= &val,
		.maxlen	= sizeof(int) * MAX_MARGIN_LEVELS,
		.mode	= table->mode,
	};

	if (cap_margin_levels <= 0)
		return -EINVAL;

	mutex_lock(&mutex);

	if (!write) {
		tmp.maxlen = sizeof(int) * cap_margin_levels;
		tmp.data = table->data;
		ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
		goto unlock_mutex;
	}

	ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
	if (ret)
		goto unlock_mutex;

	for (i = 0; i < cap_margin_levels; i++) {
		if (val[i] < 1024) {
			ret = -EINVAL;
			goto unlock_mutex;
		}
	}

	/* check up thresh is greater than dn thresh */
	if (data == &sysctl_sched_early_up[0]) {
		for (i = 0; i < cap_margin_levels; i++) {
			if (val[i] >= sysctl_sched_early_down[i]) {
				ret = -EINVAL;
				goto unlock_mutex;
			}
		}
	} else {
		for (i = 0; i < cap_margin_levels; i++) {
			if (sysctl_sched_early_up[i] >= val[i]) {
				ret = -EINVAL;
				goto unlock_mutex;
			}
		}
	}

	/* all things checkout update the value */
	for (i = 0; i < cap_margin_levels; i++)
		data[i] = val[i];

	/* update individual cpu thresholds */
	sched_update_updown_early_migrate_values(data == &sysctl_sched_early_up[0]);
unlock_mutex:
	mutex_unlock(&mutex);

	return ret;
}

int sched_freq_cap_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp,
		loff_t *ppos)
{
	int ret, i;
	unsigned int *data = (unsigned int *)table->data;
	static DEFINE_MUTEX(mutex);
	int val[MAX_CLUSTERS];
	struct ctl_table tmp = {
		.data	= &val,
		.maxlen	= sizeof(int) * MAX_CLUSTERS,
		.mode	= table->mode,
	};

	if (num_sched_clusters <= 0)
		return -EINVAL;

	mutex_lock(&mutex);

	if (!write) {
		tmp.maxlen = sizeof(int) * num_sched_clusters;
		tmp.data = table->data;
		ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
		goto unlock_mutex;
	}

	ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
	if (ret)
		goto unlock_mutex;

	for (i = 0; i < num_sched_clusters; i++) {
		if (val[i] < 0) {
			ret = -EINVAL;
			goto unlock_mutex;
		}
		data[i] = val[i];
	}

unlock_mutex:
	mutex_unlock(&mutex);
	return ret;
}

static DEFINE_MUTEX(idle_enough_mutex);

int sched_idle_enough_handler(struct ctl_table *table, int write,
			      void __user *buffer, size_t *lenp,
			      loff_t *ppos)
{
	int ret, i;

	mutex_lock(&idle_enough_mutex);

	ret = proc_douintvec_minmax(table, write, buffer, lenp, ppos);
	if (ret)
		goto unlock_mutex;

	/* update all per-cluster entries to match what was written */
	for (i = 0; i < MAX_CLUSTERS; i++)
		sysctl_sched_idle_enough_clust[i] = sysctl_sched_idle_enough;

unlock_mutex:
	mutex_unlock(&idle_enough_mutex);

	return ret;
}

int sched_idle_enough_clust_handler(struct ctl_table *table, int write,
				    void __user *buffer, size_t *lenp,
				    loff_t *ppos)
{
	int ret;
	int val[MAX_CLUSTERS];
	struct ctl_table tmp = {
		.data	= &val,
		.maxlen	= sizeof(unsigned int) * MAX_CLUSTERS,
		.mode	= table->mode,
	};

	if (num_sched_clusters <= 0)
		return -EINVAL;

	mutex_lock(&idle_enough_mutex);

	if (!write) {
		tmp.maxlen = sizeof(unsigned int) * num_sched_clusters;
		tmp.data = table->data;
		ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
		goto unlock_mutex;
	}

	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	if (ret)
		goto unlock_mutex;

	/* update the single-entry to match the first cluster updated here */
	sysctl_sched_idle_enough = sysctl_sched_idle_enough_clust[0];

unlock_mutex:
	mutex_unlock(&idle_enough_mutex);

	return ret;
}

static DEFINE_MUTEX(util_thres_mutex);

int sched_cluster_util_thres_pct_handler(struct ctl_table *table, int write,
					 void __user *buffer, size_t *lenp,
					 loff_t *ppos)
{
	int ret, i;

	mutex_lock(&util_thres_mutex);

	ret = proc_douintvec_minmax(table, write, buffer, lenp, ppos);
	if (ret)
		goto unlock_mutex;

	/* update all per-cluster entries to match what was written */
	for (i = 0; i < MAX_CLUSTERS; i++)
		sysctl_sched_cluster_util_thres_pct_clust[i] = sysctl_sched_cluster_util_thres_pct;

unlock_mutex:
	mutex_unlock(&util_thres_mutex);

	return ret;
}

int sched_cluster_util_thres_pct_clust_handler(struct ctl_table *table, int write,
					       void __user *buffer, size_t *lenp,
					       loff_t *ppos)
{
	int ret;
	int val[MAX_CLUSTERS];
	struct ctl_table tmp = {
		.data	= &val,
		.maxlen	= sizeof(int) * MAX_CLUSTERS,
		.mode	= table->mode,
	};

	if (num_sched_clusters <= 0)
		return -EINVAL;

	mutex_lock(&util_thres_mutex);

	if (!write) {
		tmp.maxlen = sizeof(int) * num_sched_clusters;
		tmp.data = table->data;
		ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
		goto unlock_mutex;
	}

	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	if (ret)
		goto unlock_mutex;

	/* update the single-entry to match the first cluster updated here */
	sysctl_sched_cluster_util_thres_pct = sysctl_sched_cluster_util_thres_pct_clust[0];

unlock_mutex:
	mutex_unlock(&util_thres_mutex);

	return ret;
}

static DEFINE_MUTEX(load_sync_mutex);
int sched_load_sync_handler(struct ctl_table *table, int write,
					       void __user *buffer, size_t *lenp,
					       loff_t *ppos)
{
	int ret;
	unsigned int *data = (unsigned int *)table->data;

	if (num_sched_clusters <= 0)
		return -EINVAL;

	mutex_lock(&load_sync_mutex);

	ret = proc_dointvec(table, write, buffer, lenp, ppos);
	if (ret)
		goto unlock_mutex;

	if (data == &sysctl_cluster01_load_sync[0]) {
		load_sync_util_thres[0][1] = data[0];
		load_sync_low_pct[0][1] = data[1];
		load_sync_high_pct[0][1] = data[2];
	} else if (data == &sysctl_cluster01_load_sync_60fps[0]) {
		load_sync_util_thres_60fps[0][1] = data[0];
		load_sync_low_pct_60fps[0][1] = data[1];
		load_sync_high_pct_60fps[0][1] = data[2];
	} else if (data == &sysctl_cluster02_load_sync[0]) {
		load_sync_util_thres[0][2] = data[0];
		load_sync_low_pct[0][2] = data[1];
		load_sync_high_pct[0][2] = data[2];
	} else if (data == &sysctl_cluster03_load_sync[0]) {
		load_sync_util_thres[0][3] = data[0];
		load_sync_low_pct[0][3] = data[1];
		load_sync_high_pct[0][3] = data[2];
	} else if (data == &sysctl_cluster10_load_sync[0]) {
		load_sync_util_thres[1][0] = data[0];
		load_sync_low_pct[1][0] = data[1];
		load_sync_high_pct[1][0] = data[2];
	} else if (data == &sysctl_cluster10_load_sync_60fps[0]) {
		load_sync_util_thres_60fps[1][0] = data[0];
		load_sync_low_pct_60fps[1][0] = data[1];
		load_sync_high_pct_60fps[1][0] = data[2];
	} else if (data == &sysctl_cluster12_load_sync[0]) {
		load_sync_util_thres[1][2] = data[0];
		load_sync_low_pct[1][2] = data[1];
		load_sync_high_pct[1][2] = data[2];
	} else if (data == &sysctl_cluster13_load_sync[0]) {
		load_sync_util_thres[1][3] = data[0];
		load_sync_low_pct[1][3] = data[1];
		load_sync_high_pct[1][3] = data[2];
	} else if (data == &sysctl_cluster20_load_sync[0]) {
		load_sync_util_thres[2][0] = data[0];
		load_sync_low_pct[2][0] = data[1];
		load_sync_high_pct[2][0] = data[2];
	} else if (data == &sysctl_cluster21_load_sync[0]) {
		load_sync_util_thres[2][1] = data[0];
		load_sync_low_pct[2][1] = data[1];
		load_sync_high_pct[2][1] = data[2];
	} else if (data == &sysctl_cluster23_load_sync[0]) {
		load_sync_util_thres[2][3] = data[0];
		load_sync_low_pct[2][3] = data[1];
		load_sync_high_pct[2][3] = data[2];
	} else if (data == &sysctl_cluster30_load_sync[0]) {
		load_sync_util_thres[3][0] = data[0];
		load_sync_low_pct[3][0] = data[1];
		load_sync_high_pct[3][0] = data[2];
	} else if (data == &sysctl_cluster31_load_sync[0]) {
		load_sync_util_thres[3][1] = data[0];
		load_sync_low_pct[3][1] = data[1];
		load_sync_high_pct[3][1] = data[2];
	} else if (data == &sysctl_cluster32_load_sync[0]) {
		load_sync_util_thres[3][2] = data[0];
		load_sync_low_pct[3][2] = data[1];
		load_sync_high_pct[3][2] = data[2];
	}

unlock_mutex:
	mutex_unlock(&load_sync_mutex);
	return ret;
}
//MIUI ADD: Task_Attribute_Sched
static int sys_miui_power_enhance_handler(struct ctl_table *table, int write,
					       void __user *buffer, size_t *lenp,
					       loff_t *ppos)
{
	int ret;
	struct walt_sched_cluster *cluster;
	ret = proc_douintvec_minmax(table, write, buffer, lenp, ppos);
	if (ret)
		goto unlock_mutex;

	if(miui_power_enhance_feat(MIUI_POWER_ENHANCE_TRAILBLAZER))
		debugfs_walt_features &= ~WALT_FEAT_TRAILBLAZER_BIT;
	else
		debugfs_walt_features |= WALT_FEAT_TRAILBLAZER_BIT;
	if(miui_power_enhance_feat(MIUI_POWER_ENHANCE_IPC))
	{
		for_each_sched_cluster(cluster) {
			if(cluster->id == (num_sched_clusters - 1))
			{
				sysctl_ipc_freq_levels_cluster1[0] = 2841600;
				sysctl_ipc_freq_levels_cluster1[1] = 3513600;
				sysctl_ipc_freq_levels_cluster1[2] = 4100000;
				sysctl_ipc_freq_levels_cluster1[3] = 4200000;
				sysctl_ipc_freq_levels_cluster1[4] = 2147483647;
				sched_smart_freq_level_update_by_cluser(cluster, sysctl_ipc_freq_levels_cluster1);
			}
		}
	}
	else
	{
		for_each_sched_cluster(cluster) {
			if(cluster->id == (num_sched_clusters - 1))
			{
				sysctl_ipc_freq_levels_cluster1[0] = 3513600;
				sysctl_ipc_freq_levels_cluster1[1] = 3800000;
				sysctl_ipc_freq_levels_cluster1[2] = 4100000;
				sysctl_ipc_freq_levels_cluster1[3] = 4200000;
				sysctl_ipc_freq_levels_cluster1[4] = 2147483647;
				sched_smart_freq_level_update_by_cluser(cluster, sysctl_ipc_freq_levels_cluster1);
			}
		}
	}
unlock_mutex:
	return ret;
}
//END Task_Attribute_Sched

#endif /* CONFIG_PROC_SYSCTL */

static int sysctl_sched_sibling_cluster_map[4] = {-1, -1, -1, -1};
static int sched_sibling_cluster_handler(struct ctl_table *table, int write,
				       void __user *buffer, size_t *lenp,
				       loff_t *ppos)
{
	int ret = -EACCES, i = 0;
	struct walt_sched_cluster *cluster;

	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	if (!ret && write) {
		for_each_sched_cluster(cluster)
			cluster->sibling_cluster = sysctl_sched_sibling_cluster_map[i++];
	}

	return ret;
}

static struct ctl_table cluster_01[] = {
	{
		.procname	= "load_sync_settings",
		.data		= &sysctl_cluster01_load_sync,
		.maxlen		= NUM_LOAD_SYNC_SETTINGS * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_load_sync_handler,
	},
	{
		.procname	= "load_sync_settings_60fps",
		.data		= &sysctl_cluster01_load_sync_60fps,
		.maxlen		= NUM_LOAD_SYNC_SETTINGS * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_load_sync_handler,
	},
};

static struct ctl_table cluster_02[] = {
	{
		.procname	= "load_sync_settings",
		.data		= &sysctl_cluster02_load_sync,
		.maxlen		= NUM_LOAD_SYNC_SETTINGS * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_load_sync_handler,
	},
};

static struct ctl_table cluster_03[] = {
	{
		.procname	= "load_sync_settings",
		.data		= &sysctl_cluster03_load_sync,
		.maxlen		= NUM_LOAD_SYNC_SETTINGS * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_load_sync_handler,
	},
};

static struct ctl_table cluster_10[] = {
	{
		.procname	= "load_sync_settings",
		.data		= &sysctl_cluster10_load_sync,
		.maxlen		= NUM_LOAD_SYNC_SETTINGS * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_load_sync_handler,
	},
	{
		.procname	= "load_sync_settings_60fps",
		.data		= &sysctl_cluster10_load_sync_60fps,
		.maxlen		= NUM_LOAD_SYNC_SETTINGS * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_load_sync_handler,
	},
};

static struct ctl_table cluster_12[] = {
	{
		.procname	= "load_sync_settings",
		.data		= &sysctl_cluster12_load_sync,
		.maxlen		= NUM_LOAD_SYNC_SETTINGS * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_load_sync_handler,
	},
};

static struct ctl_table cluster_13[] = {
	{
		.procname	= "load_sync_settings",
		.data		= &sysctl_cluster13_load_sync,
		.maxlen		= NUM_LOAD_SYNC_SETTINGS * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_load_sync_handler,
	},
};


static struct ctl_table cluster_20[] = {
	{
		.procname	= "load_sync_settings",
		.data		= &sysctl_cluster20_load_sync,
		.maxlen		= NUM_LOAD_SYNC_SETTINGS * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_load_sync_handler,
	},
};

static struct ctl_table cluster_21[] = {
	{
		.procname	= "load_sync_settings",
		.data		= &sysctl_cluster21_load_sync,
		.maxlen		= NUM_LOAD_SYNC_SETTINGS * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_load_sync_handler,
	},
};

static struct ctl_table cluster_23[] = {
	{
		.procname	= "load_sync_settings",
		.data		= &sysctl_cluster23_load_sync,
		.maxlen		= NUM_LOAD_SYNC_SETTINGS * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_load_sync_handler,
	},
};

static struct ctl_table cluster_30[] = {
	{
		.procname	= "load_sync_settings",
		.data		= &sysctl_cluster30_load_sync,
		.maxlen		= NUM_LOAD_SYNC_SETTINGS * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_load_sync_handler,
	},
};

static struct ctl_table cluster_31[] = {
	{
		.procname	= "load_sync_settings",
		.data		= &sysctl_cluster31_load_sync,
		.maxlen		= NUM_LOAD_SYNC_SETTINGS * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_load_sync_handler,
	},
};

static struct ctl_table cluster_32[] = {
	{
		.procname	= "load_sync_settings",
		.data		= &sysctl_cluster32_load_sync,
		.maxlen		= NUM_LOAD_SYNC_SETTINGS * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_load_sync_handler,
	},
};

static struct ctl_table smart_freq_cluster0[] = {
	{
		.procname	= "ipc_freq_levels",
		.data		= &sysctl_ipc_freq_levels_cluster0,
		.maxlen		= SMART_FMAX_IPC_MAX * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_smart_freq_ipc_handler,
	},
	{
		.procname	= "sched_smart_freq_dump_legacy_reason",
		.data		= &reason_dump,
		.maxlen		= 1024 * sizeof(char),
		.mode		= 0444,
		.proc_handler	= sched_smart_freq_legacy_dump_handler,
	},
	{
		.procname	= "sched_smart_freq_dump_ipc_reason",
		.data		= &reason_dump,
		.maxlen		= 1024 * sizeof(char),
		.mode		= 0444,
		.proc_handler	= sched_smart_freq_ipc_dump_handler,
	},
	{
		.procname	= "legacy_freq_levels",
		.data		= &sysctl_legacy_freq_levels_cluster0,
		.maxlen		= (LEGACY_SMART_FREQ*2) * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_smart_freq_legacy_freq_handler,
	},
};

static struct ctl_table smart_freq_cluster1[] = {
	{
		.procname	= "ipc_freq_levels",
		.data		= &sysctl_ipc_freq_levels_cluster1,
		.maxlen		= SMART_FMAX_IPC_MAX * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_smart_freq_ipc_handler,
	},
	{
		.procname	= "sched_smart_freq_dump_legacy_reason",
		.data		= &reason_dump,
		.maxlen		= 1024 * sizeof(char),
		.mode		= 0444,
		.proc_handler	= sched_smart_freq_legacy_dump_handler,
	},
	{
		.procname	= "sched_smart_freq_dump_ipc_reason",
		.data		= &reason_dump,
		.maxlen		= 1024 * sizeof(char),
		.mode		= 0444,
		.proc_handler	= sched_smart_freq_ipc_dump_handler,
	},
	{
		.procname	= "legacy_freq_levels",
		.data		= &sysctl_legacy_freq_levels_cluster1,
		.maxlen		= (LEGACY_SMART_FREQ*2) * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_smart_freq_legacy_freq_handler,
	},
};

static struct ctl_table smart_freq_cluster2[] = {
	{
		.procname	= "ipc_freq_levels",
		.data		= &sysctl_ipc_freq_levels_cluster2,
		.maxlen		= SMART_FMAX_IPC_MAX * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_smart_freq_ipc_handler,
	},
	{
		.procname	= "sched_smart_freq_dump_legacy_reason",
		.data		= &reason_dump,
		.maxlen		= 1024 * sizeof(char),
		.mode		= 0444,
		.proc_handler	= sched_smart_freq_legacy_dump_handler,
	},
	{
		.procname	= "sched_smart_freq_dump_ipc_reason",
		.data		= &reason_dump,
		.maxlen		= 1024 * sizeof(char),
		.mode		= 0444,
		.proc_handler	= sched_smart_freq_ipc_dump_handler,
	},
	{
		.procname	= "legacy_freq_levels",
		.data		= &sysctl_legacy_freq_levels_cluster2,
		.maxlen		= (LEGACY_SMART_FREQ*2) * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_smart_freq_legacy_freq_handler,
	},
};

static struct ctl_table smart_freq_cluster3[] = {
	{
		.procname	= "ipc_freq_levels",
		.data		= &sysctl_ipc_freq_levels_cluster3,
		.maxlen		= SMART_FMAX_IPC_MAX * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_smart_freq_ipc_handler,
	},
	{
		.procname	= "sched_smart_freq_dump_legacy_reason",
		.data		= &reason_dump,
		.maxlen		= 1024 * sizeof(char),
		.mode		= 0444,
		.proc_handler	= sched_smart_freq_legacy_dump_handler,
	},
	{
		.procname	= "sched_smart_freq_dump_ipc_reason",
		.data		= &reason_dump,
		.maxlen		= 1024 * sizeof(char),
		.mode		= 0444,
		.proc_handler	= sched_smart_freq_ipc_dump_handler,
	},
	{
		.procname	= "legacy_freq_levels",
		.data		= &sysctl_legacy_freq_levels_cluster3,
		.maxlen		= (LEGACY_SMART_FREQ*2) * sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_smart_freq_legacy_freq_handler,
	},
};

static struct ctl_table input_boost_sysctls[] = {
	{
		.procname	= "input_boost_ms",
		.data		= &sysctl_input_boost_ms,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &one_hundred_thousand,
	},
	{
		.procname	= "input_boost_freq",
		.data		= &sysctl_input_boost_freq,
		.maxlen		= sizeof(unsigned int) * 8,
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_boost_on_input",
		.data		= &sysctl_sched_boost_on_input,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
};

static struct ctl_table walt_table[] = {
	{
		.procname	= "sched_user_hint",
		.data		= &sysctl_sched_user_hint,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= walt_proc_user_hint_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= (void *)&sched_user_hint_max,
	},
	{
		.procname	= "sched_window_stats_policy",
		.data		= &sysctl_sched_window_stats_policy,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &four,
	},
	{
		.procname	= "sched_group_upmigrate",
		.data		= &sysctl_sched_group_upmigrate_pct,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= walt_proc_group_thresholds_handler,
	},
	{
		.procname	= "sched_group_downmigrate",
		.data		= &sysctl_sched_group_downmigrate_pct,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= walt_proc_group_thresholds_handler,
	},
	{
		.procname	= "sched_boost",
		.data		= &sysctl_sched_boost,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_boost_handler,
		.extra1		= &neg_five,
		.extra2		= &five,
	},
	{
		.procname	= "sched_conservative_pl",
		.data		= &sysctl_sched_conservative_pl,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "sched_many_wakeup_threshold",
		.data		= &sysctl_sched_many_wakeup_threshold,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &two,
		.extra2		= &one_thousand,
	},
	{
		.procname	= "sched_walt_rotate_big_tasks",
		.data		= &sysctl_sched_walt_rotate_big_tasks,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "sched_min_task_util_for_boost",
		.data		= &sysctl_sched_min_task_util_for_boost,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &one_thousand,
	},
	{
		.procname	= "sched_min_task_util_for_uclamp",
		.data		= &sysctl_sched_min_task_util_for_uclamp,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &one_thousand,
	},
	{
		.procname	= "sched_min_task_util_for_colocation",
		.data		= &sysctl_sched_min_task_util_for_colocation,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &one_thousand,
	},
	{
		.procname	= "sched_coloc_downmigrate_ns",
		.data		= &sysctl_sched_coloc_downmigrate_ns,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_douintvec_minmax,
	},
	{
		.procname	= "sched_task_unfilter_period",
		.data		= &sysctl_sched_task_unfilter_period,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ONE,
		.extra2		= &two_hundred_million,
	},
	{
		.procname	= "sched_busy_hysteresis_enable_cpus",
		.data		= &sysctl_sched_busy_hyst_enable_cpus,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_busy_hyst_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &two_hundred_fifty_five,
	},
	{
		.procname	= "sched_busy_hyst_ns",
		.data		= &sysctl_sched_busy_hyst,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_busy_hyst_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &ns_per_sec,
	},
	{
		.procname	= "sched_coloc_busy_hysteresis_enable_cpus",
		.data		= &sysctl_sched_coloc_busy_hyst_enable_cpus,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_busy_hyst_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &two_hundred_fifty_five,
	},
	{
		.procname	= "sched_coloc_busy_hyst_cpu_ns",
		.data		= &sysctl_sched_coloc_busy_hyst_cpu,
		.maxlen		= sizeof(unsigned int) * WALT_NR_CPUS,
		.mode		= 0644,
		.proc_handler	= sched_busy_hyst_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &ns_per_sec,
	},
	{
		.procname	= "sched_coloc_busy_hyst_max_ms",
		.data		= &sysctl_sched_coloc_busy_hyst_max_ms,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_busy_hyst_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &one_hundred_thousand,
	},
	{
		.procname	= "sched_coloc_busy_hyst_cpu_busy_pct",
		.data		= &sysctl_sched_coloc_busy_hyst_cpu_busy_pct,
		.maxlen		= sizeof(unsigned int) * WALT_NR_CPUS,
		.mode		= 0644,
		.proc_handler	= sched_busy_hyst_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &one_hundred,
	},
	{
		.procname	= "sched_util_busy_hysteresis_enable_cpus",
		.data		= &sysctl_sched_util_busy_hyst_enable_cpus,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_busy_hyst_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &two_hundred_fifty_five,
	},
	{
		.procname	= "sched_util_busy_hyst_cpu_ns",
		.data		= &sysctl_sched_util_busy_hyst_cpu,
		.maxlen		= sizeof(unsigned int) * WALT_NR_CPUS,
		.mode		= 0644,
		.proc_handler	= sched_busy_hyst_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &ns_per_sec,
	},
	{
		.procname	= "sched_util_busy_hyst_cpu_util",
		.data		= &sysctl_sched_util_busy_hyst_cpu_util,
		.maxlen		= sizeof(unsigned int) * WALT_NR_CPUS,
		.mode		= 0644,
		.proc_handler	= sched_busy_hyst_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &one_thousand,
	},
	{
		.procname	= "sched_ravg_window_nr_ticks",
		.data		= &sysctl_sched_ravg_window_nr_ticks,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_ravg_window_handler,
	},
	{
		.procname	= "sched_upmigrate",
		.data		= &sysctl_sched_capacity_margin_up_pct,
		.maxlen		= sizeof(unsigned int) * MAX_MARGIN_LEVELS,
		.mode		= 0644,
		.proc_handler	= sched_updown_migrate_handler,
	},
	{
		.procname	= "sched_downmigrate",
		.data		= &sysctl_sched_capacity_margin_dn_pct,
		.maxlen		= sizeof(unsigned int) * MAX_MARGIN_LEVELS,
		.mode		= 0644,
		.proc_handler	= sched_updown_migrate_handler,
	},
	{
		.procname	= "sched_early_upmigrate",
		.data		= &sysctl_sched_early_up,
		.maxlen		= sizeof(unsigned int) * MAX_MARGIN_LEVELS,
		.mode		= 0644,
		.proc_handler	= sched_updown_early_migrate_handler,
	},
	{
		.procname	= "sched_early_downmigrate",
		.data		= &sysctl_sched_early_down,
		.maxlen		= sizeof(unsigned int) * MAX_MARGIN_LEVELS,
		.mode		= 0644,
		.proc_handler	= sched_updown_early_migrate_handler,
	},
	{
		.procname	= "walt_rtg_cfs_boost_prio",
		.data		= &sysctl_walt_rtg_cfs_boost_prio,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_cfs_boost_prio,
		.extra2		= &max_cfs_boost_prio,
	},
	{
		.procname	= "walt_low_latency_task_threshold",
		.data		= &sysctl_walt_low_latency_task_threshold,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &one_thousand,
	},
	{
		.procname	= "sched_force_lb_enable",
		.data		= &sysctl_sched_force_lb_enable,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname       = "sched_sync_hint_enable",
		.data           = &sysctl_sched_sync_hint_enable,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname       = "sched_suppress_region2",
		.data           = &sysctl_sched_suppress_region2,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname       = "sched_skip_sp_newly_idle_lb",
		.data           = &sysctl_sched_skip_sp_newly_idle_lb,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname       = "sched_hyst_min_coloc_ns",
		.data           = &sysctl_sched_hyst_min_coloc_ns,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
	},
	{
		.procname       = "panic_on_walt_bug",
		.data           = &sysctl_panic_on_walt_bug,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_lib_name",
		.data		= sched_lib_name,
		.maxlen		= LIB_PATH_LENGTH,
		.mode		= 0644,
		.proc_handler	= proc_dostring,
	},
	{
		.procname	= "sched_lib_task",
		.data		= sched_lib_task,
		.maxlen		= LIB_PATH_LENGTH,
		.mode		= 0644,
		.proc_handler	= proc_dostring,
	},
	{
		.procname	= "sched_lib_mask_force",
		.data		= &sched_lib_mask_force,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_douintvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &two_hundred_fifty_five,
	},
	{
		.procname	= "sched_wake_up_idle",
		.data		= (int *) WAKE_UP_IDLE,
		.maxlen		= sizeof(unsigned int) * 2,
		.mode		= 0644,
		.proc_handler	= sched_task_handler,
	},
	{
		.procname	= "sched_init_task_load",
		.data		= (int *) INIT_TASK_LOAD,
		.maxlen		= sizeof(unsigned int) * 2,
		.mode		= 0644,
		.proc_handler	= sched_task_handler,
	},
	{
		.procname	= "sched_group_id",
		.data		= (int *) GROUP_ID,
		.maxlen		= sizeof(unsigned int) * 2,
		.mode		= 0644,
		.proc_handler	= sched_task_handler,
	},
	{
		.procname	= "sched_per_task_boost",
		.data		= (int *) PER_TASK_BOOST,
		.maxlen		= sizeof(unsigned int) * 2,
		.mode		= 0644,
		.proc_handler	= sched_task_handler,
	},
	{
		.procname	= "sched_per_task_boost_period_ms",
		.data		= (int *) PER_TASK_BOOST_PERIOD_MS,
		.maxlen		= sizeof(unsigned int) * 2,
		.mode		= 0644,
		.proc_handler	= sched_task_handler,
	},
	{
		.procname	= "sched_low_latency",
		.data		= (int *) LOW_LATENCY,
		.maxlen		= sizeof(unsigned int) * 2,
		.mode		= 0644,
		.proc_handler	= sched_task_handler,
	},
	{
		.procname	= "sched_pipeline",
		.data		= (int *) PIPELINE,
		.maxlen		= sizeof(unsigned int) * 2,
		.mode		= 0644,
		.proc_handler	= sched_task_handler,
	},
	{
		.procname	= "task_load_boost",
		.data		= (int *) LOAD_BOOST,
		.maxlen		= sizeof(unsigned int) * 2,
		.mode		= 0644,
		.proc_handler	= sched_task_handler,
	},
	{
		.procname	= "task_reduce_affinity",
		.data		= (int *) REDUCE_AFFINITY,
		.maxlen		= sizeof(unsigned int) * 2,
		.mode		= 0644,
		.proc_handler	= sched_task_handler,
	},
	{
		.procname	= "sched_task_read_pid",
		.data		= &sysctl_task_read_pid,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= sched_task_read_pid_handler,
		.extra1		= SYSCTL_ONE,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_enable_tp",
		.data		= &sysctl_sched_dynamic_tp_enable,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_dynamic_tp_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "sched_asymcap_boost",
		.data		= &sysctl_sched_asymcap_boost,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_douintvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "sched_cluster_util_thres_pct",
		.data		= &sysctl_sched_cluster_util_thres_pct,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_cluster_util_thres_pct_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_cluster_util_thres_pct_clust",
		.data		= &sysctl_sched_cluster_util_thres_pct_clust,
		.maxlen		= sizeof(unsigned int) * MAX_CLUSTERS,
		.mode		= 0644,
		.proc_handler	= sched_cluster_util_thres_pct_clust_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_idle_enough",
		.data		= &sysctl_sched_idle_enough,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_idle_enough_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_idle_enough_clust",
		.data		= &sysctl_sched_idle_enough_clust,
		.maxlen		= sizeof(unsigned int) * MAX_CLUSTERS,
		.mode		= 0644,
		.proc_handler	= sched_idle_enough_clust_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_long_running_rt_task_ms",
		.data		= &sysctl_sched_long_running_rt_task_ms,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_long_running_rt_task_ms_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &two_thousand,
	},
	{
		.procname	= "sched_ed_boost",
		.data		= &sysctl_ed_boost_pct,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_douintvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &one_hundred,
	},
	{
		.procname	= "sched_em_inflate_pct",
		.data		= &sysctl_em_inflate_pct,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_douintvec_minmax,
		.extra1		= &one_hundred,
		.extra2		= &one_thousand,
	},
	{
		.procname	= "sched_em_inflate_thres",
		.data		= &sysctl_em_inflate_thres,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_douintvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &one_thousand_twenty_four,
	},
	{
		.procname	= "sched_heavy_nr",
		.data		= &sysctl_sched_heavy_nr,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_douintvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &max_nr_pipeline,
	},
//MIUI ADD: Task_Attribute_Sched
    {
		.procname	= "sys_miui_power_enhance",
		.data		= &miui_power_enhance,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sys_miui_power_enhance_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
    },
//END Task_Attribute_Sched
	{
		.procname	= "sched_sbt_enable",
		.data		= &sysctl_sched_sbt_enable,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_douintvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "sched_sbt_pause_cpus",
		.data		= &sysctl_sched_sbt_pause_cpus,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= walt_proc_sbt_pause_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_sbt_delay_windows",
		.data		= &sysctl_sched_sbt_delay_windows,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_douintvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_pipeline_cpus",
		.data		= &sysctl_sched_pipeline_cpus,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= walt_proc_pipeline_cpus_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_max_freq_partial_halt",
		.data		= &sysctl_max_freq_partial_halt,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_douintvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_high_perf_cluster_freq_cap",
		.data		= &high_perf_cluster_freq_cap,
		.maxlen		= sizeof(unsigned int) * MAX_CLUSTERS,
		.mode		= 0644,
		.proc_handler	= sched_freq_cap_handler,
	},
	{
		.procname	= "mpam_part_id",
		.data		= (int *) MPAM_PART_ID,
		.maxlen		= sizeof(unsigned int) * 2,
		.mode		= 0444,
		.proc_handler	= sched_task_handler,
	},
	{
		.procname	= "sched_pipeline_special",
		.data		= &sysctl_sched_pipeline_special,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_pipeline_special_handler,
	},
	{
		.procname	= "sched_pipeline_util_thres",
		.data		= &sysctl_sched_pipeline_util_thres,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_douintvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_walt_core_util",
		.data		= &sysctl_sched_walt_core_util,
		.maxlen		= sizeof(unsigned int) * WALT_NR_CPUS,
		.mode		= 0444,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_disable_minfreq_pause",
		.data		= &sysctl_disable_minfreq_pause,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0664,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_pipeline_busy_boost_pct",
		.data		= &sysctl_pipeline_busy_boost_pct,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &one_hundred,
	},
	{
		.procname	= "sched_lrpb_active_ms",
		.data		= &sysctl_sched_lrpb_active_ms,
		.maxlen		= sizeof(unsigned int) * NUM_PIPELINE_BUSY_THRES,
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_storage_boost_disable",
		.data		= &sysctl_sched_storage_boost_disable,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_douintvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		/*
		 * A tuple to configure following delay:
		 * 1st val: delay between re-evaluation of pipeline tasks.
		 * 2nd val: number of windows to skip before re-arranging pipeline tasks
		 *          between prime and gold.
		 */
		.procname	= "sched_pipeline_rearrange_delay_ms",
		.data		= &sysctl_pipeline_rearrange_delay_ms,
		.maxlen		= sizeof(int) * 2,
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ONE,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_pipeline_special_task_util_thres",
		.data		= &sysctl_pipeline_special_task_util_thres,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_pipeline_non_special_task_util_thres",
		.data		= &sysctl_pipeline_non_special_task_util_thres,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_single_thread_pipeline",
		.data		= &sysctl_single_thread_pipeline,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= walt_single_thread_pipeline_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "sched_pipeline_pin_thres_low_pct",
		.data		= &sysctl_pipeline_pin_thres_low_pct,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_pipeline_pin_thres_high_pct",
		.data		= &sysctl_pipeline_pin_thres_high_pct,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "sched_sibling_cluster",
		.data		= &sysctl_sched_sibling_cluster_map,
		.maxlen		= sizeof(int) * 4,
		.mode		= 0644,
		.proc_handler	= sched_sibling_cluster_handler,
		.extra1		= SYSCTL_NEG_ONE,
		.extra2		= &three,
	},
	{
		.procname	= "sched_force_frequent_yielder",
		.data		= &sysctl_force_frequent_yielder,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "sched_pipeline_swap_util_th",
		.data		= &sysctl_pipeline_swap_util_th,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0444,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{ }
};

void walt_register_sysctl(void)
{
	struct ctl_table_header *hdr, *hdr2,
		*hdr3 = NULL, *hdr4 = NULL,
		*hdr5 = NULL, *hdr6 = NULL,
		*hdr7 = NULL, *hdr8 = NULL,
		*hdr9 = NULL, *hdr10 = NULL,
		*hdr11 = NULL, *hdr12 = NULL,
		*hdr13 = NULL, *hdr14 = NULL,
		*hdr15 = NULL, *hdr16 = NULL,
		*hdr17 = NULL, *hdr18 = NULL;

	hdr = register_sysctl("walt", walt_table);
	hdr2 = register_sysctl("walt/input_boost", input_boost_sysctls);

	if (num_sched_clusters >= 1) {
		hdr3 = register_sysctl("walt/cluster0/smart_freq", smart_freq_cluster0);
		kmemleak_not_leak(hdr3);
	}
	if (num_sched_clusters >= 2) {
		hdr4 = register_sysctl("walt/cluster1/smart_freq", smart_freq_cluster1);
		hdr7 = register_sysctl("walt/cluster0/cluster1", cluster_01);
		hdr8 = register_sysctl("walt/cluster1/cluster0", cluster_10);
		kmemleak_not_leak(hdr4);
		kmemleak_not_leak(hdr7);
		kmemleak_not_leak(hdr8);
	}
	if (num_sched_clusters >= 3) {
		hdr5 = register_sysctl("walt/cluster2/smart_freq", smart_freq_cluster2);
		hdr9 = register_sysctl("walt/cluster0/cluster2", cluster_02);
		hdr10 = register_sysctl("walt/cluster1/cluster2", cluster_12);
		hdr11 = register_sysctl("walt/cluster2/cluster0", cluster_20);
		hdr12 = register_sysctl("walt/cluster2/cluster1", cluster_21);
		kmemleak_not_leak(hdr5);
		kmemleak_not_leak(hdr9);
		kmemleak_not_leak(hdr10);
		kmemleak_not_leak(hdr11);
		kmemleak_not_leak(hdr12);
	}
	if (num_sched_clusters >= 4) {
		hdr6 = register_sysctl("walt/cluster3/smart_freq", smart_freq_cluster3);
		hdr13 = register_sysctl("walt/cluster0/cluster3", cluster_03);
		hdr14 = register_sysctl("walt/cluster1/cluster3", cluster_13);
		hdr15 = register_sysctl("walt/cluster2/cluster3", cluster_23);
		hdr16 = register_sysctl("walt/cluster3/cluster0", cluster_30);
		hdr17 = register_sysctl("walt/cluster3/cluster1", cluster_31);
		hdr18 = register_sysctl("walt/cluster3/cluster2", cluster_32);
		kmemleak_not_leak(hdr6);
		kmemleak_not_leak(hdr13);
		kmemleak_not_leak(hdr14);
		kmemleak_not_leak(hdr15);
		kmemleak_not_leak(hdr16);
		kmemleak_not_leak(hdr17);
		kmemleak_not_leak(hdr18);
	}

	kmemleak_not_leak(hdr);
	kmemleak_not_leak(hdr2);
}

