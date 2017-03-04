/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
 * Copyright (C) 2006-2007 Adam Belay <abelay@novell.com>
 * Copyright (C) 2009 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/irqchip/msm-mpm-irq.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/tick.h>
#include <linux/suspend.h>
#include <linux/pm_qos.h>
#include <linux/of_platform.h>
#include <linux/smp.h>
#include <linux/remote_spinlock.h>
#include <linux/msm_remote_spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/coresight-cti.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/cpu_pm.h>
#include <linux/arm-smccc.h>
#include <soc/qcom/spm.h>
#include <soc/qcom/pm.h>
#include <soc/qcom/rpm-notifier.h>
#include <soc/qcom/event_timer.h>
#include <soc/qcom/lpm-stats.h>
#include <soc/qcom/jtag.h>
#include <asm/cputype.h>
#include <asm/arch_timer.h>
#include <asm/cacheflush.h>
#include <asm/suspend.h>
#include <asm/cpuidle.h>
#include "lpm-levels.h"
#include "lpm-workarounds.h"
#include <trace/events/power.h>
#define CREATE_TRACE_POINTS
#include <trace/events/trace_msm_low_power.h>
#include "../../drivers/clk/msm/clock.h"

#define SCLK_HZ (32768)
#define SCM_HANDOFF_LOCK_ID "S:7"
#define PSCI_POWER_STATE(reset) (reset << 30)
#define PSCI_AFFINITY_LEVEL(lvl) ((lvl & 0x3) << 24)
static remote_spinlock_t scm_handoff_lock;

enum {
	MSM_LPM_LVL_DBG_SUSPEND_LIMITS = BIT(0),
	MSM_LPM_LVL_DBG_IDLE_LIMITS = BIT(1),
};

enum debug_event {
	CPU_ENTER,
	CPU_EXIT,
	CLUSTER_ENTER,
	CLUSTER_EXIT,
	PRE_PC_CB,
};

struct lpm_debug {
	cycle_t time;
	enum debug_event evt;
	int cpu;
	uint32_t arg1;
	uint32_t arg2;
	uint32_t arg3;
	uint32_t arg4;
};

struct lpm_cluster *lpm_root_node;

#define MAXSAMPLES 5

static bool lpm_prediction = true;
module_param_named(lpm_prediction,
	lpm_prediction, bool, S_IRUGO | S_IWUSR | S_IWGRP);

static uint32_t ref_stddev = 100;
module_param_named(
	ref_stddev, ref_stddev, uint, S_IRUGO | S_IWUSR | S_IWGRP
);

static uint32_t tmr_add = 100;
module_param_named(
	tmr_add, tmr_add, uint, S_IRUGO | S_IWUSR | S_IWGRP
);

struct lpm_history {
	uint32_t resi[MAXSAMPLES];
	int mode[MAXSAMPLES];
	int nsamp;
	uint32_t hptr;
	uint32_t hinvalid;
	uint32_t htmr_wkup;
	int64_t stime;
};

static DEFINE_PER_CPU(struct lpm_history, hist);

static DEFINE_PER_CPU(struct lpm_cluster*, cpu_cluster);
static bool suspend_in_progress;
static struct hrtimer lpm_hrtimer;
static struct hrtimer histtimer;
static struct lpm_debug *lpm_debug;
static phys_addr_t lpm_debug_phys;
static const int num_dbg_elements = 0x100;
static int lpm_cpu_callback(struct notifier_block *cpu_nb,
				unsigned long action, void *hcpu);

static void cluster_unprepare(struct lpm_cluster *cluster,
		const struct cpumask *cpu, int child_idx, bool from_idle,
		int64_t time);
static void cluster_prepare(struct lpm_cluster *cluster,
		const struct cpumask *cpu, int child_idx, bool from_idle,
		int64_t time);

static struct notifier_block __refdata lpm_cpu_nblk = {
	.notifier_call = lpm_cpu_callback,
};

static bool menu_select;
module_param_named(
	menu_select, menu_select, bool, S_IRUGO | S_IWUSR | S_IWGRP
);

static int msm_pm_sleep_time_override;
module_param_named(sleep_time_override,
	msm_pm_sleep_time_override, int, S_IRUGO | S_IWUSR | S_IWGRP);
static uint64_t suspend_wake_time;

static bool print_parsed_dt;
module_param_named(
	print_parsed_dt, print_parsed_dt, bool, S_IRUGO | S_IWUSR | S_IWGRP
);

static bool sleep_disabled;
module_param_named(sleep_disabled,
	sleep_disabled, bool, S_IRUGO | S_IWUSR | S_IWGRP);

s32 msm_cpuidle_get_deep_idle_latency(void)
{
	return 10;
}

void lpm_suspend_wake_time(uint64_t wakeup_time)
{
	if (wakeup_time <= 0) {
		suspend_wake_time = msm_pm_sleep_time_override;
		return;
	}

	if (msm_pm_sleep_time_override &&
		(msm_pm_sleep_time_override < wakeup_time))
		suspend_wake_time = msm_pm_sleep_time_override;
	else
		suspend_wake_time = wakeup_time;
}
EXPORT_SYMBOL(lpm_suspend_wake_time);

static uint32_t least_cluster_latency(struct lpm_cluster *cluster,
					struct latency_level *lat_level)
{
	struct list_head *list;
	struct lpm_cluster_level *level;
	struct lpm_cluster *n;
	struct power_params *pwr_params;
	uint32_t latency = 0;
	int i;

	if (!cluster->list.next) {
		for (i = 0; i < cluster->nlevels; i++) {
			level = &cluster->levels[i];
			pwr_params = &level->pwr;
			if (lat_level->reset_level == level->reset_level) {
				if ((latency > pwr_params->latency_us)
						|| (!latency))
					latency = pwr_params->latency_us;
				break;
			}
		}
	} else {
		list_for_each(list, &cluster->parent->child) {
			n = list_entry(list, typeof(*n), list);
			if (lat_level->level_name) {
				if (strcmp(lat_level->level_name,
						 n->cluster_name))
					continue;
			}
			for (i = 0; i < n->nlevels; i++) {
				level = &n->levels[i];
				pwr_params = &level->pwr;
				if (lat_level->reset_level ==
						level->reset_level) {
					if ((latency > pwr_params->latency_us)
								|| (!latency))
						latency =
						pwr_params->latency_us;
					break;
				}
			}
		}
	}
	return latency;
}

static uint32_t least_cpu_latency(struct list_head *child,
				struct latency_level *lat_level)
{
	struct list_head *list;
	struct lpm_cpu_level *level;
	struct power_params *pwr_params;
	struct lpm_cpu *cpu;
	struct lpm_cluster *n;
	uint32_t latency = 0;
	int i;

	list_for_each(list, child) {
		n = list_entry(list, typeof(*n), list);
		if (lat_level->level_name) {
			if (strcmp(lat_level->level_name, n->cluster_name))
				continue;
		}
		cpu = n->cpu;
		for (i = 0; i < cpu->nlevels; i++) {
			level = &cpu->levels[i];
			pwr_params = &level->pwr;
			if (lat_level->reset_level == level->reset_level) {
				if ((latency > pwr_params->latency_us)
							|| (!latency))
					latency = pwr_params->latency_us;
				break;
			}
		}
	}
	return latency;
}

static struct lpm_cluster *cluster_aff_match(struct lpm_cluster *cluster,
							int affinity_level)
{
	struct lpm_cluster *n;

	if ((cluster->aff_level == affinity_level)
		|| ((cluster->cpu) && (affinity_level == 0)))
		return cluster;
	else if (!cluster->cpu) {
		n =  list_entry(cluster->child.next, typeof(*n), list);
		return cluster_aff_match(n, affinity_level);
	} else
		return NULL;
}

int lpm_get_latency(struct latency_level *level, uint32_t *latency)
{
	struct lpm_cluster *cluster;
	uint32_t val;

	if (!lpm_root_node) {
		pr_err("%s: lpm_probe not completed\n", __func__);
		return -EAGAIN;
	}

	if ((level->affinity_level < 0)
		|| (level->affinity_level > lpm_root_node->aff_level)
		|| (level->reset_level < LPM_RESET_LVL_RET)
		|| (level->reset_level > LPM_RESET_LVL_PC)
		|| !latency)
		return -EINVAL;

	cluster = cluster_aff_match(lpm_root_node, level->affinity_level);
	if (!cluster) {
		pr_err("%s:No matching cluster found for affinity_level:%d\n",
					__func__, level->affinity_level);
		return -EINVAL;
	}

	if (level->affinity_level == 0)
		val = least_cpu_latency(&cluster->parent->child, level);
	else
		val = least_cluster_latency(cluster, level);

	if (!val) {
		pr_err("%s:No mode with affinity_level:%d reset_level:%d\n",
			__func__, level->affinity_level, level->reset_level);
		return -EINVAL;
	}

	*latency = val;

	return 0;
}
EXPORT_SYMBOL(lpm_get_latency);

static void update_debug_pc_event(enum debug_event event, uint32_t arg1,
		uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
	struct lpm_debug *dbg;
	int idx;
	static DEFINE_SPINLOCK(debug_lock);
	static int pc_event_index;

	if (!lpm_debug)
		return;

	spin_lock(&debug_lock);
	idx = pc_event_index++;
	dbg = &lpm_debug[idx & (num_dbg_elements - 1)];

	dbg->evt = event;
	dbg->time = arch_counter_get_cntvct();
	dbg->cpu = raw_smp_processor_id();
	dbg->arg1 = arg1;
	dbg->arg2 = arg2;
	dbg->arg3 = arg3;
	dbg->arg4 = arg4;
	spin_unlock(&debug_lock);
}

static int lpm_cpu_callback(struct notifier_block *cpu_nb,
	unsigned long action, void *hcpu)
{
	unsigned long cpu = (unsigned long) hcpu;
	struct lpm_cluster *cluster = per_cpu(cpu_cluster, (unsigned int) cpu);

	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_DYING:
		cluster_prepare(cluster, get_cpu_mask((unsigned int) cpu),
					NR_LPM_LEVELS, false, 0);
		break;
	case CPU_STARTING:
		cluster_unprepare(cluster, get_cpu_mask((unsigned int) cpu),
					NR_LPM_LEVELS, false, 0);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static enum hrtimer_restart lpm_hrtimer_cb(struct hrtimer *h)
{
	return HRTIMER_NORESTART;
}

static void histtimer_cancel(void)
{
	hrtimer_try_to_cancel(&histtimer);
}

static enum hrtimer_restart histtimer_fn(struct hrtimer *h)
{
	int cpu = raw_smp_processor_id();
	struct lpm_history *history = &per_cpu(hist, cpu);

	history->hinvalid = 1;
	return HRTIMER_NORESTART;
}

static void histtimer_start(uint32_t time_us)
{
	uint64_t time_ns = time_us * NSEC_PER_USEC;
	ktime_t hist_ktime = ns_to_ktime(time_ns);

	histtimer.function = histtimer_fn;
	hrtimer_start(&histtimer, hist_ktime, HRTIMER_MODE_REL_PINNED);
}

static void cluster_timer_init(struct lpm_cluster *cluster)
{
	struct list_head *list;

	if (!cluster)
		return;

	hrtimer_init(&cluster->histtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	list_for_each(list, &cluster->child) {
		struct lpm_cluster *n;

		n = list_entry(list, typeof(*n), list);
		cluster_timer_init(n);
	}
}

static void clusttimer_cancel(void)
{
	int cpu = raw_smp_processor_id();
	struct lpm_cluster *cluster = per_cpu(cpu_cluster, cpu);

	hrtimer_try_to_cancel(&cluster->histtimer);
	hrtimer_try_to_cancel(&cluster->parent->histtimer);
}

static enum hrtimer_restart clusttimer_fn(struct hrtimer *h)
{
	struct lpm_cluster *cluster = container_of(h,
				struct lpm_cluster, histtimer);

	cluster->history.hinvalid = 1;
	return HRTIMER_NORESTART;
}

static void clusttimer_start(struct lpm_cluster *cluster, uint32_t time_us)
{
	uint64_t time_ns = time_us * NSEC_PER_USEC;
	ktime_t clust_ktime = ns_to_ktime(time_ns);

	cluster->histtimer.function = clusttimer_fn;
	hrtimer_start(&cluster->histtimer, clust_ktime,
				HRTIMER_MODE_REL_PINNED);
}

static void msm_pm_set_timer(uint32_t modified_time_us)
{
	u64 modified_time_ns = modified_time_us * NSEC_PER_USEC;
	ktime_t modified_ktime = ns_to_ktime(modified_time_ns);

	lpm_hrtimer.function = lpm_hrtimer_cb;
	hrtimer_start(&lpm_hrtimer, modified_ktime, HRTIMER_MODE_REL_PINNED);
}

int set_l2_mode(struct low_power_ops *ops, int mode, bool notify_rpm)
{
	int lpm = mode;
	int rc = 0;
	struct low_power_ops *cpu_ops = per_cpu(cpu_cluster,
			smp_processor_id())->lpm_dev;

	if (cpu_ops->tz_flag & MSM_SCM_L2_OFF ||
			cpu_ops->tz_flag & MSM_SCM_L2_GDHS)
		coresight_cti_ctx_restore();

	switch (mode) {
	case MSM_SPM_MODE_STANDALONE_POWER_COLLAPSE:
	case MSM_SPM_MODE_POWER_COLLAPSE:
	case MSM_SPM_MODE_FASTPC:
		cpu_ops->tz_flag = MSM_SCM_L2_OFF;
		coresight_cti_ctx_save();
		break;
	case MSM_SPM_MODE_GDHS:
		cpu_ops->tz_flag = MSM_SCM_L2_GDHS;
		coresight_cti_ctx_save();
		break;
	case MSM_SPM_MODE_CLOCK_GATING:
	case MSM_SPM_MODE_RETENTION:
	case MSM_SPM_MODE_DISABLED:
		cpu_ops->tz_flag = MSM_SCM_L2_ON;
		break;
	default:
		cpu_ops->tz_flag = MSM_SCM_L2_ON;
		lpm = MSM_SPM_MODE_DISABLED;
		break;
	}
	rc = msm_spm_config_low_power_mode(ops->spm, lpm, notify_rpm);

	if (rc)
		pr_err("%s: Failed to set L2 low power mode %d, ERR %d",
				__func__, lpm, rc);

	return rc;
}

int set_l3_mode(struct low_power_ops *ops, int mode, bool notify_rpm)
{
	struct low_power_ops *cpu_ops = per_cpu(cpu_cluster,
			smp_processor_id())->lpm_dev;

	switch (mode) {
	case MSM_SPM_MODE_STANDALONE_POWER_COLLAPSE:
	case MSM_SPM_MODE_POWER_COLLAPSE:
	case MSM_SPM_MODE_FASTPC:
		cpu_ops->tz_flag |= MSM_SCM_L3_PC_OFF;
		break;
	default:
		break;
	}
	return msm_spm_config_low_power_mode(ops->spm, mode, notify_rpm);
}


int set_system_mode(struct low_power_ops *ops, int mode, bool notify_rpm)
{
	return msm_spm_config_low_power_mode(ops->spm, mode, notify_rpm);
}

static int set_device_mode(struct lpm_cluster *cluster, int ndevice,
		struct lpm_cluster_level *level)
{
	struct low_power_ops *ops;

	if (use_psci)
		return 0;

	ops = &cluster->lpm_dev[ndevice];
	if (ops && ops->set_mode)
		return ops->set_mode(ops, level->mode[ndevice],
				level->notify_rpm);
	else
		return -EINVAL;
}

static uint64_t lpm_cpuidle_predict(struct cpuidle_device *dev,
		struct lpm_cpu *cpu, int *idx_restrict,
		uint32_t *idx_restrict_time)
{
	int i, j, divisor;
	uint64_t max, avg, stddev;
	int64_t thresh = LLONG_MAX;
	struct lpm_history *history = &per_cpu(hist, dev->cpu);
	uint32_t *min_residency = get_per_cpu_min_residency(dev->cpu);

	if (!lpm_prediction)
		return 0;

	/*
	 * Samples are marked invalid when woken-up due to timer,
	 * so donot predict.
	 */
	if (history->hinvalid) {
		history->hinvalid = 0;
		history->htmr_wkup = 1;
		history->stime = 0;
		return 0;
	}

	/*
	 * Predict only when all the samples are collected.
	 */
	if (history->nsamp < MAXSAMPLES) {
		history->stime = 0;
		return 0;
	}

	/*
	 * Check if the samples are not much deviated, if so use the
	 * average of those as predicted sleep time. Else if any
	 * specific mode has more premature exits return the index of
	 * that mode.
	 */

again:
	max = avg = divisor = stddev = 0;
	for (i = 0; i < MAXSAMPLES; i++) {
		int64_t value = history->resi[i];

		if (value <= thresh) {
			avg += value;
			divisor++;
			if (value > max)
				max = value;
		}
	}
	do_div(avg, divisor);

	for (i = 0; i < MAXSAMPLES; i++) {
		int64_t value = history->resi[i];

		if (value <= thresh) {
			int64_t diff = value - avg;

			stddev += diff * diff;
		}
	}
	do_div(stddev, divisor);
	stddev = int_sqrt(stddev);

	/*
	 * If the deviation is less, return the average, else
	 * ignore one maximum sample and retry
	 */
	if (((avg > stddev * 6) && (divisor >= (MAXSAMPLES - 1)))
					|| stddev <= ref_stddev) {
		history->stime = ktime_to_us(ktime_get()) + avg;
		return avg;
	} else if (divisor  > (MAXSAMPLES - 1)) {
		thresh = max - 1;
		goto again;
	}

	/*
	 * Find the number of premature exits for each of the mode,
	 * excluding clockgating mode, and they are more than fifty
	 * percent restrict that and deeper modes.
	 */
	if (history->htmr_wkup != 1) {
		for (j = 1; j < cpu->nlevels; j++) {
			uint32_t failed = 0;
			uint64_t total = 0;

			for (i = 0; i < MAXSAMPLES; i++) {
				if ((history->mode[i] == j) &&
					(history->resi[i] < min_residency[j])) {
					failed++;
					total += history->resi[i];
				}
			}
			if (failed > (MAXSAMPLES/2)) {
				*idx_restrict = j;
				do_div(total, failed);
				*idx_restrict_time = total;
				history->stime = ktime_to_us(ktime_get())
						+ *idx_restrict_time;
				break;
			}
		}
	}
	return 0;
}

static inline void invalidate_predict_history(struct cpuidle_device *dev)
{
	struct lpm_history *history = &per_cpu(hist, dev->cpu);

	if (!lpm_prediction)
		return;

	if (history->hinvalid) {
		history->hinvalid = 0;
		history->htmr_wkup = 1;
		history->stime = 0;
	}
}

static void clear_predict_history(void)
{
	struct lpm_history *history;
	int i;
	unsigned int cpu;

	if (!lpm_prediction)
		return;

	for_each_possible_cpu(cpu) {
		history = &per_cpu(hist, cpu);
		for (i = 0; i < MAXSAMPLES; i++) {
			history->resi[i]  = 0;
			history->mode[i] = -1;
			history->hptr = 0;
			history->nsamp = 0;
			history->stime = 0;
		}
	}
}

static void update_history(struct cpuidle_device *dev, int idx);

static int cpu_power_select(struct cpuidle_device *dev,
		struct lpm_cpu *cpu)
{
	int best_level = -1;
	uint32_t latency_us = pm_qos_request_for_cpu(PM_QOS_CPU_DMA_LATENCY,
							dev->cpu);
	uint32_t sleep_us =
		(uint32_t)(ktime_to_us(tick_nohz_get_sleep_length()));
	uint32_t modified_time_us = 0;
	uint32_t next_event_us = 0;
	int i, idx_restrict;
	uint32_t lvl_latency_us = 0;
	uint64_t predicted = 0;
	uint32_t htime = 0, idx_restrict_time = 0;
	uint32_t next_wakeup_us = sleep_us;
	uint32_t *min_residency = get_per_cpu_min_residency(dev->cpu);
	uint32_t *max_residency = get_per_cpu_max_residency(dev->cpu);

	if (!cpu)
		return -EINVAL;

	if (sleep_disabled)
		return 0;

	idx_restrict = cpu->nlevels + 1;

	next_event_us = (uint32_t)(ktime_to_us(get_next_event_time(dev->cpu)));

	for (i = 0; i < cpu->nlevels; i++) {
		struct lpm_cpu_level *level = &cpu->levels[i];
		struct power_params *pwr_params = &level->pwr;
		enum msm_pm_sleep_mode mode = level->mode;
		bool allow;

		allow = lpm_cpu_mode_allow(dev->cpu, i, true);

		if (!allow)
			continue;

		lvl_latency_us = pwr_params->latency_us;

		if (latency_us < lvl_latency_us)
			break;

		if (next_event_us) {
			if (next_event_us < lvl_latency_us)
				break;

			if (((next_event_us - lvl_latency_us) < sleep_us) ||
					(next_event_us < sleep_us))
				next_wakeup_us = next_event_us - lvl_latency_us;
		}

		if (!i) {
			/*
			 * If the next_wake_us itself is not sufficient for
			 * deeper low power modes than clock gating do not
			 * call prediction.
			 */
			if (next_wakeup_us > max_residency[i]) {
				predicted = lpm_cpuidle_predict(dev, cpu,
					&idx_restrict, &idx_restrict_time);
				if (predicted < min_residency[i])
					predicted = 0;
			} else
				invalidate_predict_history(dev);
		}

		if (i >= idx_restrict)
			break;

		best_level = i;

		if (next_event_us && next_event_us < sleep_us &&
			(mode != MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT))
			modified_time_us
				= next_event_us - lvl_latency_us;
		else
			modified_time_us = 0;

		if (predicted ? (predicted <= max_residency[i])
			: (next_wakeup_us <= max_residency[i]))
			break;
	}

	if (modified_time_us)
		msm_pm_set_timer(modified_time_us);

	/*
	 * Start timer to avoid staying in shallower mode forever
	 * incase of misprediciton
	 */
	if ((predicted || (idx_restrict != (cpu->nlevels + 1)))
			&& ((best_level >= 0)
			&& (best_level < (cpu->nlevels-1)))) {
		htime = predicted + tmr_add;
		if (htime == tmr_add)
			htime = idx_restrict_time;
		else if (htime > max_residency[best_level])
			htime = max_residency[best_level];

		if ((next_wakeup_us > htime) &&
			((next_wakeup_us - htime) > max_residency[best_level]))
			histtimer_start(htime);
	}

	trace_cpu_power_select(best_level, sleep_us, latency_us, next_event_us);

	trace_cpu_pred_select(idx_restrict_time ? 2 : (predicted ? 1 : 0),
			predicted, htime);

	return best_level;
}

static uint64_t get_cluster_sleep_time(struct lpm_cluster *cluster,
		struct cpumask *mask, bool from_idle, uint32_t *pred_time)
{
	int cpu;
	int next_cpu = raw_smp_processor_id();
	ktime_t next_event;
	struct cpumask online_cpus_in_cluster;
	struct lpm_history *history;
	int64_t prediction = LONG_MAX;

	next_event.tv64 = KTIME_MAX;
	if (!suspend_wake_time)
		suspend_wake_time =  msm_pm_sleep_time_override;
	if (!from_idle) {
		if (mask)
			cpumask_copy(mask, cpumask_of(raw_smp_processor_id()));
		if (!suspend_wake_time)
			return ~0ULL;
		else
			return USEC_PER_SEC * suspend_wake_time;
	}

	cpumask_and(&online_cpus_in_cluster,
			&cluster->num_children_in_sync, cpu_online_mask);

	for_each_cpu(cpu, &online_cpus_in_cluster) {
		ktime_t *next_event_c;

		next_event_c = get_next_event_cpu(cpu);
		if (next_event_c->tv64 < next_event.tv64) {
			next_event.tv64 = next_event_c->tv64;
			next_cpu = cpu;
		}

		if (from_idle && lpm_prediction) {
			history = &per_cpu(hist, cpu);
			if (history->stime && (history->stime < prediction))
				prediction = history->stime;
		}
	}

	if (mask)
		cpumask_copy(mask, cpumask_of(next_cpu));

	if (from_idle && lpm_prediction) {
		if (prediction > ktime_to_us(ktime_get()))
			*pred_time = prediction - ktime_to_us(ktime_get());
	}

	if (ktime_to_us(next_event) > ktime_to_us(ktime_get()))
		return ktime_to_us(ktime_sub(next_event, ktime_get()));
	else
		return 0;
}

static int cluster_predict(struct lpm_cluster *cluster,
				uint32_t *pred_us)
{
	int i, j;
	int ret = 0;
	struct cluster_history *history = &cluster->history;
	int64_t cur_time = ktime_to_us(ktime_get());

	if (!lpm_prediction)
		return 0;

	if (history->hinvalid) {
		history->hinvalid = 0;
		history->htmr_wkup = 1;
		history->flag = 0;
		return ret;
	}

	if (history->nsamp == MAXSAMPLES) {
		for (i = 0; i < MAXSAMPLES; i++) {
			if ((cur_time - history->stime[i])
					> CLUST_SMPL_INVLD_TIME)
				history->nsamp--;
		}
	}

	if (history->nsamp < MAXSAMPLES) {
		history->flag = 0;
		return ret;
	}

	if (history->flag == 2)
		history->flag = 0;

	if (history->htmr_wkup != 1) {
		uint64_t total = 0;

		if (history->flag == 1) {
			for (i = 0; i < MAXSAMPLES; i++)
				total += history->resi[i];
			do_div(total, MAXSAMPLES);
			*pred_us = total;
			return 2;
		}

		for (j = 1; j < cluster->nlevels; j++) {
			uint32_t failed = 0;

			total = 0;
			for (i = 0; i < MAXSAMPLES; i++) {
				if ((history->mode[i] == j) && (history->resi[i]
				< cluster->levels[j].pwr.min_residency)) {
					failed++;
					total += history->resi[i];
				}
			}

			if (failed > (MAXSAMPLES-2)) {
				do_div(total, failed);
				*pred_us = total;
				history->flag = 1;
				return 1;
			}
		}
	}

	return ret;
}

static void update_cluster_history_time(struct cluster_history *history,
						int idx, uint64_t start)
{
	history->entry_idx = idx;
	history->entry_time = start;
}

static void update_cluster_history(struct cluster_history *history, int idx)
{
	uint32_t tmr = 0;
	uint32_t residency = 0;
	struct lpm_cluster *cluster =
			container_of(history, struct lpm_cluster, history);

	if (!lpm_prediction)
		return;

	if ((history->entry_idx == -1) || (history->entry_idx == idx)) {
		residency = ktime_to_us(ktime_get()) - history->entry_time;
		history->stime[history->hptr] = history->entry_time;
	} else
		return;

	if (history->htmr_wkup) {
		if (!history->hptr)
			history->hptr = MAXSAMPLES-1;
		else
			history->hptr--;

		history->resi[history->hptr] += residency;

		history->htmr_wkup = 0;
		tmr = 1;
	} else {
		history->resi[history->hptr] = residency;
	}

	history->mode[history->hptr] = idx;

	history->entry_idx = INT_MIN;
	history->entry_time = 0;

	if (history->nsamp < MAXSAMPLES)
		history->nsamp++;

	trace_cluster_pred_hist(cluster->cluster_name,
		history->mode[history->hptr], history->resi[history->hptr],
		history->hptr, tmr);

	(history->hptr)++;

	if (history->hptr >= MAXSAMPLES)
		history->hptr = 0;
}

static void clear_cl_history_each(struct cluster_history *history)
{
	int i;

	for (i = 0; i < MAXSAMPLES; i++) {
		history->resi[i]  = 0;
		history->mode[i] = -1;
		history->stime[i] = 0;
	}
	history->hptr = 0;
	history->nsamp = 0;
	history->flag = 0;
	history->hinvalid = 0;
	history->htmr_wkup = 0;
}

static void clear_cl_predict_history(void)
{
	struct lpm_cluster *cluster = lpm_root_node;
	struct list_head *list;

	if (!lpm_prediction)
		return;

	clear_cl_history_each(&cluster->history);

	list_for_each(list, &cluster->child) {
		struct lpm_cluster *n;

		n = list_entry(list, typeof(*n), list);
		clear_cl_history_each(&n->history);
	}
}

static int cluster_select(struct lpm_cluster *cluster, bool from_idle,
							int *ispred)
{
	int best_level = -1;
	int i;
	struct cpumask mask;
	uint32_t latency_us = ~0U;
	uint32_t sleep_us;
	uint32_t cpupred_us = 0, pred_us = 0;
	int pred_mode = 0, predicted = 0;

	if (!cluster)
		return -EINVAL;

	sleep_us = (uint32_t)get_cluster_sleep_time(cluster, NULL,
						from_idle, &cpupred_us);

	if (from_idle) {
		pred_mode = cluster_predict(cluster, &pred_us);

		if (cpupred_us && pred_mode && (cpupred_us < pred_us))
			pred_us = cpupred_us;

		if (pred_us && pred_mode && (pred_us < sleep_us))
			predicted = 1;

		if (predicted && (pred_us == cpupred_us))
			predicted = 2;
	}

	if (cpumask_and(&mask, cpu_online_mask, &cluster->child_cpus))
		latency_us = pm_qos_request_for_cpumask(PM_QOS_CPU_DMA_LATENCY,
							&mask);

	/*
	 * If atleast one of the core in the cluster is online, the cluster
	 * low power modes should be determined by the idle characteristics
	 * even if the last core enters the low power mode as a part of
	 * hotplug.
	 */

	if (!from_idle && num_online_cpus() > 1 &&
		cpumask_intersects(&cluster->child_cpus, cpu_online_mask))
		from_idle = true;

	for (i = 0; i < cluster->nlevels; i++) {
		struct lpm_cluster_level *level = &cluster->levels[i];
		struct power_params *pwr_params = &level->pwr;

		if (!lpm_cluster_mode_allow(cluster, i, from_idle))
			continue;

		if (level->last_core_only &&
			cpumask_weight(cpu_online_mask) > 1)
			continue;

		if (!cpumask_equal(&cluster->num_children_in_sync,
					&level->num_cpu_votes))
			continue;

		if (from_idle && latency_us < pwr_params->latency_us)
			break;

		if (sleep_us < pwr_params->time_overhead_us)
			break;

		if (suspend_in_progress && from_idle && level->notify_rpm)
			continue;

		if (level->notify_rpm && msm_rpm_waiting_for_ack())
			continue;

		best_level = i;

		if (from_idle &&
			(predicted ? (pred_us <= pwr_params->max_residency)
			: (sleep_us <= pwr_params->max_residency)))
			break;
	}

	if ((best_level == (cluster->nlevels - 1)) && (pred_mode == 2))
		cluster->history.flag = 2;

	*ispred = predicted;

	trace_cluster_pred_select(cluster->cluster_name, best_level, sleep_us,
						latency_us, predicted, pred_us);

	return best_level;
}

static void cluster_notify(struct lpm_cluster *cluster,
		struct lpm_cluster_level *level, bool enter)
{
	if (level->is_reset && enter)
		cpu_cluster_pm_enter(cluster->aff_level);
	else if (level->is_reset && !enter)
		cpu_cluster_pm_exit(cluster->aff_level);
}

static int cluster_configure(struct lpm_cluster *cluster, int idx,
		bool from_idle, int predicted)
{
	struct lpm_cluster_level *level = &cluster->levels[idx];
	int ret, i;

	if (!cpumask_equal(&cluster->num_children_in_sync, &cluster->child_cpus)
			|| is_IPI_pending(&cluster->num_children_in_sync)) {
		return -EPERM;
	}

	if (idx != cluster->default_level) {
		update_debug_pc_event(CLUSTER_ENTER, idx,
			cluster->num_children_in_sync.bits[0],
			cluster->child_cpus.bits[0], from_idle);
		trace_cluster_enter(cluster->cluster_name, idx,
			cluster->num_children_in_sync.bits[0],
			cluster->child_cpus.bits[0], from_idle);
		lpm_stats_cluster_enter(cluster->stats, idx);

		if (from_idle && lpm_prediction)
			update_cluster_history_time(&cluster->history, idx,
						ktime_to_us(ktime_get()));
	}

	for (i = 0; i < cluster->ndevices; i++) {
		ret = set_device_mode(cluster, i, level);
		if (ret)
			goto failed_set_mode;
	}

	if (level->notify_rpm) {
		struct cpumask nextcpu, *cpumask;
		uint64_t us;
		uint32_t pred_us;

		us = get_cluster_sleep_time(cluster, &nextcpu,
						from_idle, &pred_us);
		cpumask = level->disable_dynamic_routing ? NULL : &nextcpu;

		ret = msm_rpm_enter_sleep(0, cpumask);
		if (ret) {
			pr_info("Failed msm_rpm_enter_sleep() rc = %d\n", ret);
			goto failed_set_mode;
		}

		us = (us + 1) * 1000;
		clear_predict_history();
		clear_cl_predict_history();

		do_div(us, NSEC_PER_SEC/SCLK_HZ);
		msm_mpm_enter_sleep(us, from_idle, cpumask);
	}

	/* Notify cluster enter event after successfully config completion */
	cluster_notify(cluster, level, true);

	sched_set_cluster_dstate(&cluster->child_cpus, idx, 0, 0);

	cluster->last_level = idx;

	if (predicted && (idx < (cluster->nlevels - 1))) {
		struct power_params *pwr_params = &cluster->levels[idx].pwr;

		tick_broadcast_exit();
		clusttimer_start(cluster, pwr_params->max_residency + tmr_add);
		tick_broadcast_enter();
	}

	return 0;

failed_set_mode:

	for (i = 0; i < cluster->ndevices; i++) {
		int rc = 0;
		level = &cluster->levels[cluster->default_level];
		rc = set_device_mode(cluster, i, level);
		BUG_ON(rc);
	}
	return ret;
}

static void cluster_prepare(struct lpm_cluster *cluster,
		const struct cpumask *cpu, int child_idx, bool from_idle,
		int64_t start_time)
{
	int i;
	int predicted = 0;

	if (!cluster)
		return;

	if (cluster->min_child_level > child_idx)
		return;

	spin_lock(&cluster->sync_lock);
	cpumask_or(&cluster->num_children_in_sync, cpu,
			&cluster->num_children_in_sync);

	for (i = 0; i < cluster->nlevels; i++) {
		struct lpm_cluster_level *lvl = &cluster->levels[i];

		if (child_idx >= lvl->min_child_level)
			cpumask_or(&lvl->num_cpu_votes, cpu,
					&lvl->num_cpu_votes);
	}

	/*
	 * cluster_select() does not make any configuration changes. So its ok
	 * to release the lock here. If a core wakes up for a rude request,
	 * it need not wait for another to finish its cluster selection and
	 * configuration process
	 */

	if (!cpumask_equal(&cluster->num_children_in_sync,
				&cluster->child_cpus))
		goto failed;

	i = cluster_select(cluster, from_idle, &predicted);

	if (((i < 0) || (i == cluster->default_level))
				&& predicted && from_idle) {
		update_cluster_history_time(&cluster->history,
					-1, ktime_to_us(ktime_get()));

		if (i < 0) {
			struct power_params *pwr_params =
						&cluster->levels[0].pwr;

			tick_broadcast_exit();
			clusttimer_start(cluster,
					pwr_params->max_residency + tmr_add);
			tick_broadcast_enter();
		}
	}

	if (i < 0)
		goto failed;

	if (cluster_configure(cluster, i, from_idle, predicted))
		goto failed;

	cluster->stats->sleep_time = start_time;
	cluster_prepare(cluster->parent, &cluster->num_children_in_sync, i,
			from_idle, start_time);

	spin_unlock(&cluster->sync_lock);
	return;
failed:
	spin_unlock(&cluster->sync_lock);
	cluster->stats->sleep_time = 0;
	return;
}

static void cluster_unprepare(struct lpm_cluster *cluster,
		const struct cpumask *cpu, int child_idx, bool from_idle,
		int64_t end_time)
{
	struct lpm_cluster_level *level;
	bool first_cpu;
	int last_level, i, ret;

	if (!cluster)
		return;

	if (cluster->min_child_level > child_idx)
		return;

	spin_lock(&cluster->sync_lock);
	last_level = cluster->default_level;
	first_cpu = cpumask_equal(&cluster->num_children_in_sync,
				&cluster->child_cpus);
	cpumask_andnot(&cluster->num_children_in_sync,
			&cluster->num_children_in_sync, cpu);

	for (i = 0; i < cluster->nlevels; i++) {
		struct lpm_cluster_level *lvl = &cluster->levels[i];

		if (child_idx >= lvl->min_child_level)
			cpumask_andnot(&lvl->num_cpu_votes,
					&lvl->num_cpu_votes, cpu);
	}

	if (from_idle && first_cpu &&
		(cluster->last_level == cluster->default_level))
		update_cluster_history(&cluster->history, cluster->last_level);

	if (!first_cpu || cluster->last_level == cluster->default_level)
		goto unlock_return;

	if (cluster->stats->sleep_time)
		cluster->stats->sleep_time = end_time -
			cluster->stats->sleep_time;
	lpm_stats_cluster_exit(cluster->stats, cluster->last_level, true);

	level = &cluster->levels[cluster->last_level];
	if (level->notify_rpm) {
		msm_rpm_exit_sleep();

		/* If RPM bumps up CX to turbo, unvote CX turbo vote
		 * during exit of rpm assisted power collapse to
		 * reduce the power impact
		 */

		lpm_wa_cx_unvote_send();
		msm_mpm_exit_sleep(from_idle);
	}

	update_debug_pc_event(CLUSTER_EXIT, cluster->last_level,
			cluster->num_children_in_sync.bits[0],
			cluster->child_cpus.bits[0], from_idle);
	trace_cluster_exit(cluster->cluster_name, cluster->last_level,
			cluster->num_children_in_sync.bits[0],
			cluster->child_cpus.bits[0], from_idle);

	last_level = cluster->last_level;
	cluster->last_level = cluster->default_level;

	for (i = 0; i < cluster->ndevices; i++) {
		level = &cluster->levels[cluster->default_level];
		ret = set_device_mode(cluster, i, level);

		BUG_ON(ret);

	}
	sched_set_cluster_dstate(&cluster->child_cpus, 0, 0, 0);

	cluster_notify(cluster, &cluster->levels[last_level], false);

	if (from_idle)
		update_cluster_history(&cluster->history, last_level);

	cluster_unprepare(cluster->parent, &cluster->child_cpus,
			last_level, from_idle, end_time);
unlock_return:
	spin_unlock(&cluster->sync_lock);
}

static inline void cpu_prepare(struct lpm_cluster *cluster, int cpu_index,
				bool from_idle)
{
	struct lpm_cpu_level *cpu_level = &cluster->cpu->levels[cpu_index];
	bool jtag_save_restore =
			cluster->cpu->levels[cpu_index].jtag_save_restore;

	/* Use broadcast timer for aggregating sleep mode within a cluster.
	 * A broadcast timer could be used in the following scenarios
	 * 1) The architected timer HW gets reset during certain low power
	 * modes and the core relies on a external(broadcast) timer to wake up
	 * from sleep. This information is passed through device tree.
	 * 2) The CPU low power mode could trigger a system low power mode.
	 * The low power module relies on Broadcast timer to aggregate the
	 * next wakeup within a cluster, in which case, CPU switches over to
	 * use broadcast timer.
	 */
	if (from_idle && (cpu_level->use_bc_timer ||
			(cpu_index >= cluster->min_child_level)))
		tick_broadcast_enter();

	if (from_idle && ((cpu_level->mode == MSM_PM_SLEEP_MODE_POWER_COLLAPSE)
		|| (cpu_level->mode ==
			MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE)
			|| (cpu_level->is_reset)))
		cpu_pm_enter();

	/*
	 * Save JTAG registers for 8996v1.0 & 8996v2.x in C4 LPM
	 */
	if (jtag_save_restore)
		msm_jtag_save_state();
}

static inline void cpu_unprepare(struct lpm_cluster *cluster, int cpu_index,
				bool from_idle)
{
	struct lpm_cpu_level *cpu_level = &cluster->cpu->levels[cpu_index];
	bool jtag_save_restore =
			cluster->cpu->levels[cpu_index].jtag_save_restore;

	if (from_idle && (cpu_level->use_bc_timer ||
			(cpu_index >= cluster->min_child_level)))
		tick_broadcast_exit();

	if (from_idle && ((cpu_level->mode == MSM_PM_SLEEP_MODE_POWER_COLLAPSE)
		|| (cpu_level->mode ==
			MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE)
		|| cpu_level->is_reset))
		cpu_pm_exit();

	/*
	 * Restore JTAG registers for 8996v1.0 & 8996v2.x in C4 LPM
	 */
	if (jtag_save_restore)
		msm_jtag_restore_state();
}

int get_cluster_id(struct lpm_cluster *cluster, int *aff_lvl)
{
	int state_id = 0;

	if (!cluster)
		return 0;

	spin_lock(&cluster->sync_lock);

	if (!cpumask_equal(&cluster->num_children_in_sync,
				&cluster->child_cpus))
		goto unlock_and_return;

	state_id |= get_cluster_id(cluster->parent, aff_lvl);

	if (cluster->last_level != cluster->default_level) {
		struct lpm_cluster_level *level
			= &cluster->levels[cluster->last_level];

		state_id |= (level->psci_id & cluster->psci_mode_mask)
					<< cluster->psci_mode_shift;
		(*aff_lvl)++;
	}
unlock_and_return:
	spin_unlock(&cluster->sync_lock);
	return state_id;
}

#if !defined(CONFIG_CPU_V7)
bool psci_enter_sleep(struct lpm_cluster *cluster, int idx, bool from_idle)
{
	/*
	 * idx = 0 is the default LPM state
	 */
	if (!idx) {
		stop_critical_timings();
		wfi();
		start_critical_timings();
		return 1;
	} else {
		int affinity_level = 0;
		int state_id = get_cluster_id(cluster, &affinity_level);
		int power_state =
			PSCI_POWER_STATE(cluster->cpu->levels[idx].is_reset);
		bool success = false;

		if (cluster->cpu->levels[idx].hyp_psci) {
			stop_critical_timings();
			__invoke_psci_fn_smc(0xC4000021, 0, 0, 0);
			start_critical_timings();
			return 1;
		}

		affinity_level = PSCI_AFFINITY_LEVEL(affinity_level);
		state_id |= (power_state | affinity_level
			| cluster->cpu->levels[idx].psci_id);

		update_debug_pc_event(CPU_ENTER, state_id,
						0xdeaffeed, 0xdeaffeed, true);
		stop_critical_timings();
		success = !arm_cpuidle_suspend(state_id);
		start_critical_timings();
		update_debug_pc_event(CPU_EXIT, state_id,
						success, 0xdeaffeed, true);
		return success;
	}
}
#elif defined(CONFIG_ARM_PSCI)
bool psci_enter_sleep(struct lpm_cluster *cluster, int idx, bool from_idle)
{
	if (!idx) {
		stop_critical_timings();
		wfi();
		start_critical_timings();
		return 1;
	} else {
		int affinity_level = 0;
		int state_id = get_cluster_id(cluster, &affinity_level);
		int power_state =
			PSCI_POWER_STATE(cluster->cpu->levels[idx].is_reset);
		bool success = false;

		affinity_level = PSCI_AFFINITY_LEVEL(affinity_level);
		state_id |= (power_state | affinity_level
			| cluster->cpu->levels[idx].psci_id);

		update_debug_pc_event(CPU_ENTER, state_id,
						0xdeaffeed, 0xdeaffeed, true);
		stop_critical_timings();
		success = !arm_cpuidle_suspend(state_id);
		start_critical_timings();
		update_debug_pc_event(CPU_EXIT, state_id,
						success, 0xdeaffeed, true);
		return success;
	}
}
#else
bool psci_enter_sleep(struct lpm_cluster *cluster, int idx, bool from_idle)
{
	WARN_ONCE(true, "PSCI cpu_suspend ops not supported\n");
	return false;
}
#endif

static int lpm_cpuidle_select(struct cpuidle_driver *drv,
		struct cpuidle_device *dev)
{
	struct lpm_cluster *cluster = per_cpu(cpu_cluster, dev->cpu);
	int idx;

	if (!cluster)
		return 0;

	idx = cpu_power_select(dev, cluster->cpu);

	if (idx < 0)
		return -EPERM;

	return idx;
}

static void update_history(struct cpuidle_device *dev, int idx)
{
	struct lpm_history *history = &per_cpu(hist, dev->cpu);
	uint32_t tmr = 0;

	if (!lpm_prediction)
		return;

	if (history->htmr_wkup) {
		if (!history->hptr)
			history->hptr = MAXSAMPLES-1;
		else
			history->hptr--;

		history->resi[history->hptr] += dev->last_residency;
		history->htmr_wkup = 0;
		tmr = 1;
	} else
		history->resi[history->hptr] = dev->last_residency;

	history->mode[history->hptr] = idx;

	trace_cpu_pred_hist(history->mode[history->hptr],
		history->resi[history->hptr], history->hptr, tmr);

	if (history->nsamp < MAXSAMPLES)
		history->nsamp++;

	(history->hptr)++;
	if (history->hptr >= MAXSAMPLES)
		history->hptr = 0;
}

static int lpm_cpuidle_enter(struct cpuidle_device *dev,
		struct cpuidle_driver *drv, int idx)
{
	struct lpm_cluster *cluster = per_cpu(cpu_cluster, dev->cpu);
	bool success = true;
	const struct cpumask *cpumask = get_cpu_mask(dev->cpu);
	int64_t start_time = ktime_to_ns(ktime_get()), end_time;
	struct power_params *pwr_params;

	pwr_params = &cluster->cpu->levels[idx].pwr;
	sched_set_cpu_cstate(smp_processor_id(), idx + 1,
		pwr_params->energy_overhead, pwr_params->latency_us);

	pwr_params = &cluster->cpu->levels[idx].pwr;

	cpu_prepare(cluster, idx, true);
	cluster_prepare(cluster, cpumask, idx, true, ktime_to_ns(ktime_get()));

	trace_cpu_idle_enter(idx);
	lpm_stats_cpu_enter(idx, start_time);

	if (need_resched() || (idx < 0))
		goto exit;

	BUG_ON(!use_psci);
	success = psci_enter_sleep(cluster, idx, true);

exit:
	end_time = ktime_to_ns(ktime_get());
	lpm_stats_cpu_exit(idx, end_time, success);

	cluster_unprepare(cluster, cpumask, idx, true, end_time);
	cpu_unprepare(cluster, idx, true);
	sched_set_cpu_cstate(smp_processor_id(), 0, 0, 0);
	end_time = ktime_to_ns(ktime_get()) - start_time;
	do_div(end_time, 1000);
	dev->last_residency = end_time;
	update_history(dev, idx);
	trace_cpu_idle_exit(idx, success);
	local_irq_enable();
	if (lpm_prediction) {
		histtimer_cancel();
		clusttimer_cancel();
	}
	return idx;
}

#ifdef CONFIG_CPU_IDLE_MULTIPLE_DRIVERS
static int cpuidle_register_cpu(struct cpuidle_driver *drv,
		struct cpumask *mask)
{
	struct cpuidle_device *device;
	int cpu, ret;


	if (!mask || !drv)
		return -EINVAL;

	drv->cpumask = mask;
	ret = cpuidle_register_driver(drv);
	if (ret) {
		pr_err("Failed to register cpuidle driver %d\n", ret);
		goto failed_driver_register;
	}

	for_each_cpu(cpu, mask) {
		device = &per_cpu(cpuidle_dev, cpu);
		device->cpu = cpu;

		ret = cpuidle_register_device(device);
		if (ret) {
			pr_err("Failed to register cpuidle driver for cpu:%u\n",
					cpu);
			goto failed_driver_register;
		}
	}
	return ret;
failed_driver_register:
	for_each_cpu(cpu, mask)
		cpuidle_unregister_driver(drv);
	return ret;
}
#else
static int cpuidle_register_cpu(struct cpuidle_driver *drv,
		struct  cpumask *mask)
{
	return cpuidle_register(drv, NULL);
}
#endif

static struct cpuidle_governor lpm_governor = {
	.name =		"qcom",
	.rating =	30,
	.select =	lpm_cpuidle_select,
	.owner =	THIS_MODULE,
};

static int cluster_cpuidle_register(struct lpm_cluster *cl)
{
	int i = 0, ret = 0;
	unsigned cpu;
	struct lpm_cluster *p = NULL;

	if (!cl->cpu) {
		struct lpm_cluster *n;

		list_for_each_entry(n, &cl->child, list) {
			ret = cluster_cpuidle_register(n);
			if (ret)
				break;
		}
		return ret;
	}

	cl->drv = kzalloc(sizeof(*cl->drv), GFP_KERNEL);
	if (!cl->drv)
		return -ENOMEM;

	cl->drv->name = "msm_idle";

	for (i = 0; i < cl->cpu->nlevels; i++) {
		struct cpuidle_state *st = &cl->drv->states[i];
		struct lpm_cpu_level *cpu_level = &cl->cpu->levels[i];
		snprintf(st->name, CPUIDLE_NAME_LEN, "C%u\n", i);
		snprintf(st->desc, CPUIDLE_DESC_LEN, cpu_level->name);
		st->flags = 0;
		st->exit_latency = cpu_level->pwr.latency_us;
		st->power_usage = cpu_level->pwr.ss_power;
		st->target_residency = 0;
		st->enter = lpm_cpuidle_enter;
	}

	cl->drv->state_count = cl->cpu->nlevels;
	cl->drv->safe_state_index = 0;
	for_each_cpu(cpu, &cl->child_cpus)
		per_cpu(cpu_cluster, cpu) = cl;

	for_each_possible_cpu(cpu) {
		if (cpu_online(cpu))
			continue;
		p = per_cpu(cpu_cluster, cpu);
		while (p) {
			int j;
			spin_lock(&p->sync_lock);
			cpumask_set_cpu(cpu, &p->num_children_in_sync);
			for (j = 0; j < p->nlevels; j++)
				cpumask_copy(&p->levels[j].num_cpu_votes,
						&p->num_children_in_sync);
			spin_unlock(&p->sync_lock);
			p = p->parent;
		}
	}
	ret = cpuidle_register_cpu(cl->drv, &cl->child_cpus);

	if (ret) {
		kfree(cl->drv);
		return -ENOMEM;
	}
	return 0;
}

/**
 * init_lpm - initializes the governor
 */
static int __init init_lpm(void)
{
	return cpuidle_register_governor(&lpm_governor);
}

postcore_initcall(init_lpm);

static void register_cpu_lpm_stats(struct lpm_cpu *cpu,
		struct lpm_cluster *parent)
{
	const char **level_name;
	int i;

	level_name = kzalloc(cpu->nlevels * sizeof(*level_name), GFP_KERNEL);

	if (!level_name)
		return;

	for (i = 0; i < cpu->nlevels; i++)
		level_name[i] = cpu->levels[i].name;

	lpm_stats_config_level("cpu", level_name, cpu->nlevels,
			parent->stats, &parent->child_cpus);

	kfree(level_name);
}

static void register_cluster_lpm_stats(struct lpm_cluster *cl,
		struct lpm_cluster *parent)
{
	const char **level_name;
	int i;
	struct lpm_cluster *child;

	if (!cl)
		return;

	level_name = kzalloc(cl->nlevels * sizeof(*level_name), GFP_KERNEL);

	if (!level_name)
		return;

	for (i = 0; i < cl->nlevels; i++)
		level_name[i] = cl->levels[i].level_name;

	cl->stats = lpm_stats_config_level(cl->cluster_name, level_name,
			cl->nlevels, parent ? parent->stats : NULL, NULL);

	kfree(level_name);

	if (cl->cpu) {
		register_cpu_lpm_stats(cl->cpu, cl);
		return;
	}

	list_for_each_entry(child, &cl->child, list)
		register_cluster_lpm_stats(child, cl);
}

static int lpm_suspend_prepare(void)
{
	suspend_in_progress = true;
	msm_mpm_suspend_prepare();
	lpm_stats_suspend_enter();

	return 0;
}

static void lpm_suspend_wake(void)
{
	suspend_in_progress = false;
	msm_mpm_suspend_wake();
	lpm_stats_suspend_exit();
}

static int lpm_suspend_enter(suspend_state_t state)
{
	int cpu = raw_smp_processor_id();
	struct lpm_cluster *cluster = per_cpu(cpu_cluster, cpu);
	struct lpm_cpu *lpm_cpu = cluster->cpu;
	const struct cpumask *cpumask = get_cpu_mask(cpu);
	int idx;

	for (idx = lpm_cpu->nlevels - 1; idx >= 0; idx--) {

		if (lpm_cpu_mode_allow(cpu, idx, false))
			break;
	}
	if (idx < 0) {
		pr_err("Failed suspend\n");
		return 0;
	}
	cpu_prepare(cluster, idx, false);
	cluster_prepare(cluster, cpumask, idx, false, 0);
	if (idx > 0)
		update_debug_pc_event(CPU_ENTER, idx, 0xdeaffeed,
					0xdeaffeed, false);

	/*
	 * Print the clocks which are enabled during system suspend
	 * This debug information is useful to know which are the
	 * clocks that are enabled and preventing the system level
	 * LPMs(XO and Vmin).
	 */
	clock_debug_print_enabled();

	BUG_ON(!use_psci);
	psci_enter_sleep(cluster, idx, true);

	if (idx > 0)
		update_debug_pc_event(CPU_EXIT, idx, true, 0xdeaffeed,
					false);

	cluster_unprepare(cluster, cpumask, idx, false, 0);
	cpu_unprepare(cluster, idx, false);
	return 0;
}

static const struct platform_suspend_ops lpm_suspend_ops = {
	.enter = lpm_suspend_enter,
	.valid = suspend_valid_only_mem,
	.prepare_late = lpm_suspend_prepare,
	.wake = lpm_suspend_wake,
};

static int lpm_probe(struct platform_device *pdev)
{
	int ret;
	int size;
	struct kobject *module_kobj = NULL;

	get_online_cpus();
	lpm_root_node = lpm_of_parse_cluster(pdev);

	if (IS_ERR_OR_NULL(lpm_root_node)) {
		pr_err("%s(): Failed to probe low power modes\n", __func__);
		put_online_cpus();
		return PTR_ERR(lpm_root_node);
	}

	if (print_parsed_dt)
		cluster_dt_walkthrough(lpm_root_node);

	/*
	 * Register hotplug notifier before broadcast time to ensure there
	 * to prevent race where a broadcast timer might not be setup on for a
	 * core.  BUG in existing code but no known issues possibly because of
	 * how late lpm_levels gets initialized.
	 */
	suspend_set_ops(&lpm_suspend_ops);
	hrtimer_init(&lpm_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer_init(&histtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cluster_timer_init(lpm_root_node);

	ret = remote_spin_lock_init(&scm_handoff_lock, SCM_HANDOFF_LOCK_ID);
	if (ret) {
		pr_err("%s: Failed initializing scm_handoff_lock (%d)\n",
			__func__, ret);
		put_online_cpus();
		return ret;
	}

	size = num_dbg_elements * sizeof(struct lpm_debug);
	lpm_debug = dma_alloc_coherent(&pdev->dev, size,
			&lpm_debug_phys, GFP_KERNEL);
	register_cluster_lpm_stats(lpm_root_node, NULL);

	ret = cluster_cpuidle_register(lpm_root_node);
	put_online_cpus();
	if (ret) {
		pr_err("%s()Failed to register with cpuidle framework\n",
				__func__);
		goto failed;
	}
	register_hotcpu_notifier(&lpm_cpu_nblk);
	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("%s: cannot find kobject for module %s\n",
			__func__, KBUILD_MODNAME);
		ret = -ENOENT;
		goto failed;
	}

	ret = create_cluster_lvl_nodes(lpm_root_node, module_kobj);
	if (ret) {
		pr_err("%s(): Failed to create cluster level nodes\n",
				__func__);
		goto failed;
	}

	return 0;
failed:
	free_cluster_node(lpm_root_node);
	lpm_root_node = NULL;
	return ret;
}

static struct of_device_id lpm_mtch_tbl[] = {
	{.compatible = "qcom,lpm-levels"},
	{},
};

static struct platform_driver lpm_driver = {
	.probe = lpm_probe,
	.driver = {
		.name = "lpm-levels",
		.owner = THIS_MODULE,
		.of_match_table = lpm_mtch_tbl,
	},
};

static int __init lpm_levels_module_init(void)
{
	int rc;
	rc = platform_driver_register(&lpm_driver);
	if (rc) {
		pr_info("Error registering %s\n", lpm_driver.driver.name);
		goto fail;
	}

fail:
	return rc;
}
late_initcall(lpm_levels_module_init);

enum msm_pm_l2_scm_flag lpm_cpu_pre_pc_cb(unsigned int cpu)
{
	struct lpm_cluster *cluster = per_cpu(cpu_cluster, cpu);
	enum msm_pm_l2_scm_flag retflag = MSM_SCM_L2_ON;

	/*
	 * No need to acquire the lock if probe isn't completed yet
	 * In the event of the hotplug happening before lpm probe, we want to
	 * flush the cache to make sure that L2 is flushed. In particular, this
	 * could cause incoherencies for a cluster architecture. This wouldn't
	 * affect the idle case as the idle driver wouldn't be registered
	 * before the probe function
	 */
	if (!cluster)
		return MSM_SCM_L2_OFF;

	/*
	 * Assumes L2 only. What/How parameters gets passed into TZ will
	 * determine how this function reports this info back in msm-pm.c
	 */
	spin_lock(&cluster->sync_lock);

	if (!cluster->lpm_dev) {
		retflag = MSM_SCM_L2_OFF;
		goto unlock_and_return;
	}

	if (!cpumask_equal(&cluster->num_children_in_sync,
						&cluster->child_cpus))
		goto unlock_and_return;

	if (cluster->lpm_dev)
		retflag = cluster->lpm_dev->tz_flag;
	/*
	 * The scm_handoff_lock will be release by the secure monitor.
	 * It is used to serialize power-collapses from this point on,
	 * so that both Linux and the secure context have a consistent
	 * view regarding the number of running cpus (cpu_count).
	 *
	 * It must be acquired before releasing the cluster lock.
	 */
unlock_and_return:
	update_debug_pc_event(PRE_PC_CB, retflag, 0xdeadbeef, 0xdeadbeef,
			0xdeadbeef);
	trace_pre_pc_cb(retflag);
	remote_spin_lock_rlock_id(&scm_handoff_lock,
				  REMOTE_SPINLOCK_TID_START + cpu);
	spin_unlock(&cluster->sync_lock);
	return retflag;
}
