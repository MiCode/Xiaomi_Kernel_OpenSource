/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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

static DEFINE_PER_CPU(struct lpm_cluster*, cpu_cluster);
static bool suspend_in_progress;
static struct hrtimer lpm_hrtimer;
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
	dbg->time = arch_counter_get_cntpct();
	dbg->cpu = raw_smp_processor_id();
	dbg->arg1 = arg1;
	dbg->arg2 = arg2;
	dbg->arg3 = arg3;
	dbg->arg4 = arg4;
	spin_unlock(&debug_lock);
}

static void setup_broadcast_timer(void *arg)
{
	unsigned long reason = (unsigned long)arg;
	int cpu = raw_smp_processor_id();

	reason = reason ?
		CLOCK_EVT_NOTIFY_BROADCAST_ON : CLOCK_EVT_NOTIFY_BROADCAST_OFF;

	clockevents_notify(reason, &cpu);
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
	case CPU_ONLINE:
		smp_call_function_single(cpu, setup_broadcast_timer,
					(void *)true, 1);
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
	int i;
	uint32_t lvl_latency_us = 0;
	uint32_t *residency = get_per_cpu_max_residency(dev->cpu);

	if (!cpu)
		return -EINVAL;

	if (sleep_disabled)
		return 0;

	next_event_us = (uint32_t)(ktime_to_us(get_next_event_time(dev->cpu)));

	for (i = 0; i < cpu->nlevels; i++) {
		struct lpm_cpu_level *level = &cpu->levels[i];
		struct power_params *pwr_params = &level->pwr;
		uint32_t next_wakeup_us = sleep_us;
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
				continue;

			if (((next_event_us - lvl_latency_us) < sleep_us) ||
					(next_event_us < sleep_us))
				next_wakeup_us = next_event_us - lvl_latency_us;
		}

		if (next_wakeup_us <= residency[i]) {
			best_level = i;
			if (next_event_us && next_event_us < sleep_us &&
				(mode != MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT))
				modified_time_us
					= next_event_us - lvl_latency_us;
			else
				modified_time_us = 0;
			break;
		}
	}

	if (modified_time_us)
		msm_pm_set_timer(modified_time_us);

	trace_cpu_power_select(best_level, sleep_us, latency_us, next_event_us);

	return best_level;
}

static uint64_t get_cluster_sleep_time(struct lpm_cluster *cluster,
		struct cpumask *mask, bool from_idle)
{
	int cpu;
	int next_cpu = raw_smp_processor_id();
	ktime_t next_event;
	struct tick_device *td;
	struct cpumask online_cpus_in_cluster;

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
		td = &per_cpu(tick_cpu_device, cpu);
		if (td->evtdev->next_event.tv64 < next_event.tv64) {
			next_event.tv64 = td->evtdev->next_event.tv64;
			next_cpu = cpu;
		}
	}

	if (mask)
		cpumask_copy(mask, cpumask_of(next_cpu));


	if (ktime_to_us(next_event) > ktime_to_us(ktime_get()))
		return ktime_to_us(ktime_sub(next_event, ktime_get()));
	else
		return 0;
}

static int cluster_select(struct lpm_cluster *cluster, bool from_idle)
{
	int best_level = -1;
	int i;
	struct cpumask mask;
	uint32_t latency_us = ~0U;
	uint32_t sleep_us;

	if (!cluster)
		return -EINVAL;

	sleep_us = (uint32_t)get_cluster_sleep_time(cluster, NULL, from_idle);

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
			continue;

		if (sleep_us < pwr_params->time_overhead_us)
			continue;

		if (suspend_in_progress && from_idle && level->notify_rpm)
			continue;

		if (level->notify_rpm && msm_rpm_waiting_for_ack())
			continue;

		if (sleep_us <= pwr_params->max_residency) {
			best_level = i;
			break;
		}
	}

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
		bool from_idle)
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
	}

	for (i = 0; i < cluster->ndevices; i++) {
		ret = set_device_mode(cluster, i, level);
		if (ret)
			goto failed_set_mode;
	}

	if (level->notify_rpm) {
		struct cpumask nextcpu, *cpumask;
		uint64_t us;

		us = get_cluster_sleep_time(cluster, &nextcpu, from_idle);
		cpumask = level->disable_dynamic_routing ? NULL : &nextcpu;

		ret = msm_rpm_enter_sleep(0, cpumask);
		if (ret) {
			pr_info("Failed msm_rpm_enter_sleep() rc = %d\n", ret);
			goto failed_set_mode;
		}

		do_div(us, USEC_PER_SEC/SCLK_HZ);
		msm_mpm_enter_sleep(us, from_idle, cpumask);
	}

	/* Notify cluster enter event after successfully config completion */
	cluster_notify(cluster, level, true);

	sched_set_cluster_dstate(&cluster->child_cpus, idx, 0, 0);

	cluster->last_level = idx;
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

	i = cluster_select(cluster, from_idle);

	if (i < 0)
		goto failed;

	if (cluster_configure(cluster, i, from_idle))
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
	cluster_unprepare(cluster->parent, &cluster->child_cpus,
			last_level, from_idle, end_time);
unlock_return:
	spin_unlock(&cluster->sync_lock);
}

static inline void cpu_prepare(struct lpm_cluster *cluster, int cpu_index,
				bool from_idle)
{
	struct lpm_cpu_level *cpu_level = &cluster->cpu->levels[cpu_index];
	unsigned int cpu = raw_smp_processor_id();
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
		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &cpu);

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
	unsigned int cpu = raw_smp_processor_id();
	bool jtag_save_restore =
			cluster->cpu->levels[cpu_index].jtag_save_restore;

	if (from_idle && (cpu_level->use_bc_timer ||
			(cpu_index >= cluster->min_child_level)))
		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &cpu);

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
asmlinkage int __invoke_psci_fn_smc(u64, u64, u64, u64);
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
		success = !cpu_suspend(state_id);
		start_critical_timings();
		update_debug_pc_event(CPU_EXIT, state_id,
						success, 0xdeaffeed, true);
		return success;
	}
}
#elif defined(CONFIG_ARM_PSCI)
bool psci_enter_sleep(struct lpm_cluster *cluster, int idx, bool from_idle)
{
	int affinity_level = 0;
	int state_id = get_cluster_id(cluster, &affinity_level);
	int power_state = PSCI_POWER_STATE(cluster->cpu->levels[idx].is_reset);

	affinity_level = PSCI_AFFINITY_LEVEL(affinity_level);
	if (!idx) {
		wfi();
		return 1;
	}

	state_id |= (power_state | affinity_level
				| cluster->cpu->levels[idx].psci_id);

	return !cpu_suspend(state_id);
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

	if (idx < 0) {
		local_irq_enable();
		return -EPERM;
	}

	return idx;
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

	cpu_prepare(cluster, idx, true);
	cluster_prepare(cluster, cpumask, idx, true, ktime_to_ns(ktime_get()));

	if (need_resched() || (idx < 0))
		goto exit;

	trace_cpu_idle_enter(idx);
	lpm_stats_cpu_enter(idx, start_time);

	if (!use_psci) {
		if (idx > 0)
			update_debug_pc_event(CPU_ENTER, idx, 0xdeaffeed,
					0xdeaffeed, true);
		success = msm_cpu_pm_enter_sleep(cluster->cpu->levels[idx].mode,
				true);

		if (idx > 0)
			update_debug_pc_event(CPU_EXIT, idx, success,
							0xdeaffeed, true);
	} else {
		success = psci_enter_sleep(cluster, idx, true);
	}

exit:
	end_time = ktime_to_ns(ktime_get());
	lpm_stats_cpu_exit(idx, end_time, success);

	cluster_unprepare(cluster, cpumask, idx, true, end_time);
	cpu_unprepare(cluster, idx, true);

	sched_set_cpu_cstate(smp_processor_id(), 0, 0, 0);
	trace_cpu_idle_exit(idx, success);
	end_time = ktime_to_ns(ktime_get()) - start_time;
	dev->last_residency = do_div(end_time, 1000);
	local_irq_enable();

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
	int64_t time = ktime_to_ns(ktime_get());

	for (idx = lpm_cpu->nlevels - 1; idx >= 0; idx--) {

		if (lpm_cpu_mode_allow(cpu, idx, false))
			break;
	}
	if (idx < 0) {
		pr_err("Failed suspend\n");
		return 0;
	}
	cpu_prepare(cluster, idx, false);
	cluster_prepare(cluster, cpumask, idx, false, time);
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

	if (!use_psci)
		msm_cpu_pm_enter_sleep(cluster->cpu->levels[idx].mode, false);
	else
		psci_enter_sleep(cluster, idx, true);

	if (idx > 0)
		update_debug_pc_event(CPU_EXIT, idx, true, 0xdeaffeed,
					false);

	time = ktime_to_ns(ktime_get());
	cluster_unprepare(cluster, cpumask, idx, false, time);
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

	lpm_root_node = lpm_of_parse_cluster(pdev);

	if (IS_ERR_OR_NULL(lpm_root_node)) {
		pr_err("%s(): Failed to probe low power modes\n", __func__);
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
	register_hotcpu_notifier(&lpm_cpu_nblk);
	get_cpu();
	on_each_cpu(setup_broadcast_timer, (void *)true, 1);
	put_cpu();
	suspend_set_ops(&lpm_suspend_ops);
	hrtimer_init(&lpm_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	ret = remote_spin_lock_init(&scm_handoff_lock, SCM_HANDOFF_LOCK_ID);
	if (ret) {
		pr_err("%s: Failed initializing scm_handoff_lock (%d)\n",
			__func__, ret);
		return ret;
	}

	size = num_dbg_elements * sizeof(struct lpm_debug);
	lpm_debug = dma_alloc_coherent(&pdev->dev, size,
			&lpm_debug_phys, GFP_KERNEL);
	register_cluster_lpm_stats(lpm_root_node, NULL);

	ret = cluster_cpuidle_register(lpm_root_node);
	if (ret) {
		pr_err("%s()Failed to register with cpuidle framework\n",
				__func__);
		goto failed;
	}

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

/**
 * lpm_cpu_hotplug_enter(): Called by dying CPU to terminate in low power mode
 *
 * @cpu: cpuid of the dying CPU
 *
 * Called from platform_cpu_kill() to terminate hotplug in a low power mode
 */
void lpm_cpu_hotplug_enter(unsigned int cpu)
{
	enum msm_pm_sleep_mode mode = MSM_PM_SLEEP_MODE_NR;
	struct lpm_cluster *cluster = per_cpu(cpu_cluster, cpu);
	int i;
	int idx = -1;

	/*
	 * If lpm isn't probed yet, try to put cpu into the one of the modes
	 * available
	 */
	if (!cluster) {
		if (msm_spm_is_mode_avail(
					MSM_SPM_MODE_POWER_COLLAPSE)){
			mode = MSM_PM_SLEEP_MODE_POWER_COLLAPSE;
		} else if (msm_spm_is_mode_avail(
				MSM_SPM_MODE_FASTPC)) {
			mode = MSM_PM_SLEEP_MODE_FASTPC;
		} else if (msm_spm_is_mode_avail(
				MSM_SPM_MODE_RETENTION)) {
			mode = MSM_PM_SLEEP_MODE_RETENTION;
		} else {
			pr_err("No mode avail for cpu%d hotplug\n", cpu);
			BUG_ON(1);
			return;
		}
	} else {
		struct lpm_cpu *lpm_cpu;
		uint32_t ss_pwr = ~0U;

		lpm_cpu = cluster->cpu;
		for (i = 0; i < lpm_cpu->nlevels; i++) {
			if (ss_pwr < lpm_cpu->levels[i].pwr.ss_power)
				continue;
			ss_pwr = lpm_cpu->levels[i].pwr.ss_power;
			idx = i;
			mode = lpm_cpu->levels[i].mode;
		}

		if (mode == MSM_PM_SLEEP_MODE_NR)
			return;

		BUG_ON(idx < 0);
		cluster_prepare(cluster, get_cpu_mask(cpu), idx, false, 0);
	}

	msm_cpu_pm_enter_sleep(mode, false);
}


