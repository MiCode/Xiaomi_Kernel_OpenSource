/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/tick.h>
#include <linux/suspend.h>
#include <linux/pm_qos.h>
#include <linux/of_platform.h>
#include <mach/mpm.h>
#include <mach/cpuidle.h>
#include <mach/event_timer.h>
#include "pm.h"
#include "rpm-notifier.h"
#include "spm.h"
#include "idle.h"

#define SCLK_HZ (32768)

enum {
	MSM_LPM_LVL_DBG_SUSPEND_LIMITS = BIT(0),
	MSM_LPM_LVL_DBG_IDLE_LIMITS = BIT(1),
};

struct power_params {
	uint32_t latency_us;
	uint32_t ss_power;
	uint32_t energy_overhead;
	uint32_t time_overhead_us;
	uint32_t target_residency_us;
};

struct lpm_cpu_level {
	const char *name;
	enum msm_pm_sleep_mode mode;
	struct power_params pwr;
	bool use_bc_timer;
	bool sync;
};

struct lpm_system_level {
	const char *name;
	uint32_t l2_mode;
	struct power_params pwr;
	enum msm_pm_sleep_mode min_cpu_mode;
	int num_cpu_votes;
	bool notify_rpm;
	bool available;
	bool sync;
};

struct lpm_system_state {
	struct lpm_cpu_level *cpu_level;
	int num_cpu_levels;
	struct lpm_system_level *system_level;
	int num_system_levels;
	enum msm_pm_sleep_mode sync_cpu_mode;
	int last_entered_cluster_index;
	bool allow_synched_levels;
	bool no_l2_saw;
	struct spinlock sync_lock;
	int num_cores_in_sync;
};

static struct lpm_system_state sys_state;
static bool suspend_in_progress;

struct lpm_lookup_table {
	uint32_t modes;
	const char *mode_name;
};

static void lpm_system_level_update(void);
static void setup_broadcast_timer(void *arg);
static int lpm_cpu_callback(struct notifier_block *cpu_nb,
				unsigned long action, void *hcpu);

static struct notifier_block __refdata lpm_cpu_nblk = {
	.notifier_call = lpm_cpu_callback,
};

static uint32_t allowed_l2_mode;
static uint32_t sysfs_dbg_l2_mode = MSM_SPM_L2_MODE_POWER_COLLAPSE;
static uint32_t default_l2_mode;


static ssize_t lpm_levels_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t lpm_levels_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count);


static int lpm_lvl_dbg_msk;

module_param_named(
	debug_mask, lpm_lvl_dbg_msk, int, S_IRUGO | S_IWUSR | S_IWGRP
);

static bool menu_select;
module_param_named(
	menu_select, menu_select, bool, S_IRUGO | S_IWUSR | S_IWGRP
);

static int msm_pm_sleep_time_override;
module_param_named(sleep_time_override,
	msm_pm_sleep_time_override, int, S_IRUGO | S_IWUSR | S_IWGRP);

static int num_powered_cores;
static struct hrtimer lpm_hrtimer;

static struct kobj_attribute lpm_l2_kattr = __ATTR(l2,  S_IRUGO|S_IWUSR,\
		lpm_levels_attr_show, lpm_levels_attr_store);

static struct attribute *lpm_levels_attr[] = {
	&lpm_l2_kattr.attr,
	NULL,
};

static struct attribute_group lpm_levels_attr_grp = {
	.attrs = lpm_levels_attr,
};

/* SYSFS */
static ssize_t lpm_levels_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct kernel_param kp;
	int rc;

	kp.arg = &sysfs_dbg_l2_mode;

	rc = param_get_uint(buf, &kp);

	if (rc > 0) {
		strlcat(buf, "\n", PAGE_SIZE);
		rc++;
	}

	return rc;
}

static ssize_t lpm_levels_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct kernel_param kp;
	unsigned int temp;
	int rc;

	kp.arg = &temp;
	rc = param_set_uint(buf, &kp);
	if (rc)
		return rc;

	sysfs_dbg_l2_mode = temp;
	lpm_system_level_update();

	return count;
}

static int msm_pm_get_sleep_mode_value(const char *mode_name)
{
	struct lpm_lookup_table pm_sm_lookup[] = {
		{MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT,
			"wfi"},
		{MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE,
			"standalone_pc"},
		{MSM_PM_SLEEP_MODE_POWER_COLLAPSE,
			"pc"},
		{MSM_PM_SLEEP_MODE_RETENTION,
			"retention"},
	};
	int i;
	int ret = -EINVAL;

	for (i = 0; i < ARRAY_SIZE(pm_sm_lookup); i++) {
		if (!strcmp(mode_name, pm_sm_lookup[i].mode_name)) {
			ret = pm_sm_lookup[i].modes;
			break;
		}
	}
	return ret;
}

static int lpm_set_l2_mode(struct lpm_system_state *system_state,
				int sleep_mode)
{
	int lpm = sleep_mode;
	int rc = 0;

	if (system_state->no_l2_saw)
		goto bail_set_l2_mode;

	msm_pm_set_l2_flush_flag(MSM_SCM_L2_ON);

	switch (sleep_mode) {
	case MSM_SPM_L2_MODE_POWER_COLLAPSE:
		msm_pm_set_l2_flush_flag(MSM_SCM_L2_OFF);
		break;
	case MSM_SPM_L2_MODE_GDHS:
		msm_pm_set_l2_flush_flag(MSM_SCM_L2_GDHS);
		break;
	case MSM_SPM_L2_MODE_PC_NO_RPM:
		msm_pm_set_l2_flush_flag(MSM_SCM_L2_OFF);
		break;
	case MSM_SPM_L2_MODE_RETENTION:
	case MSM_SPM_L2_MODE_DISABLED:
		break;
	default:
		lpm = MSM_SPM_L2_MODE_DISABLED;
		break;
	}

	rc = msm_spm_l2_set_low_power_mode(lpm, true);

	if (rc) {
		if (rc == -ENXIO)
			WARN_ON_ONCE(1);
		else
			pr_err("%s: Failed to set L2 low power mode %d, ERR %d",
					__func__, lpm, rc);
	}
bail_set_l2_mode:
	return rc;
}

static void lpm_system_level_update(void)
{
	int i;
	struct lpm_system_level *l = NULL;
	uint32_t max_l2_mode;
	static DEFINE_MUTEX(lpm_lock);

	mutex_lock(&lpm_lock);

	if (num_powered_cores == 1)
		allowed_l2_mode = MSM_SPM_L2_MODE_POWER_COLLAPSE;
	else if (sys_state.allow_synched_levels)
		allowed_l2_mode = MSM_SPM_L2_MODE_POWER_COLLAPSE;
	else
		allowed_l2_mode = default_l2_mode;
	max_l2_mode = min(allowed_l2_mode, sysfs_dbg_l2_mode);

	for (i = 0; i < sys_state.num_system_levels; i++) {
		l = &sys_state.system_level[i];
		l->available = !(l->l2_mode > max_l2_mode);
	}
	mutex_unlock(&lpm_lock);
}

static int lpm_system_mode_select(
		struct lpm_system_state *system_state,
		uint32_t sleep_us, bool from_idle)
{
	int best_level = -1;
	int i;
	uint32_t best_level_pwr = ~0U;
	uint32_t pwr;
	uint32_t latency_us = pm_qos_request(PM_QOS_CPU_DMA_LATENCY);

	if (!system_state->system_level)
		return -EINVAL;

	for (i = 0; i < system_state->num_system_levels; i++) {
		struct lpm_system_level *system_level =
			&system_state->system_level[i];
		struct power_params *pwr_param = &system_level->pwr;

		if (!system_level->available)
			continue;

		if (system_level->sync &&
			system_level->num_cpu_votes != num_powered_cores)
			continue;

		if (latency_us < pwr_param->latency_us && from_idle)
			continue;

		if (sleep_us < pwr_param->time_overhead_us)
			continue;

		/*
		 * After the suspend prepare notifications its possible
		 * for the CPU to enter a system sleep mode. But MPM would have
		 * already requested a XO clock based on the wakeup irqs. To
		 * prevent suspend votes from being overriden by idle irqs, MPM
		 * doesn't send an updated MPM vote after suspend_prepare
		 * callback.
		 * To ensure that XO sleep vote isn't used if and when the
		 * device enters idle PC after suspend prepare callback,
		 * disallow any low power modes that notifies RPM after suspend
		 * prepare function is called
		 */
		if (suspend_in_progress && system_level->notify_rpm &&
				from_idle)
			continue;

		if ((sleep_us >> 10) > pwr_param->time_overhead_us) {
			pwr = pwr_param->ss_power;
		} else {
			pwr = pwr_param->ss_power;
			pwr -= (pwr_param->time_overhead_us
					* pwr_param->ss_power) / sleep_us;
			pwr += pwr_param->energy_overhead / sleep_us;
		}

		if (best_level_pwr >= pwr) {
			best_level = i;
			best_level_pwr = pwr;
		}
	}
	return best_level;
}

static void lpm_system_prepare(struct lpm_system_state *system_state,
		int index, bool from_idle)
{
	struct lpm_system_level *lvl;
	struct clock_event_device *bc = tick_get_broadcast_device()->evtdev;
	uint32_t sclk;
	int64_t us = (~0ULL);
	int dbg_mask;
	int ret;
	const struct cpumask *nextcpu;

	spin_lock(&system_state->sync_lock);
	if (index < 0 ||
			num_powered_cores != system_state->num_cores_in_sync) {
		spin_unlock(&system_state->sync_lock);
		return;
	}

	if (from_idle) {
		dbg_mask = lpm_lvl_dbg_msk & MSM_LPM_LVL_DBG_IDLE_LIMITS;
		us = ktime_to_us(ktime_sub(bc->next_event, ktime_get()));
		nextcpu = bc->cpumask;
	} else {
		dbg_mask = lpm_lvl_dbg_msk & MSM_LPM_LVL_DBG_SUSPEND_LIMITS;
		nextcpu = cpumask_of(smp_processor_id());
	}

	lvl = &system_state->system_level[index];

	ret = lpm_set_l2_mode(system_state, lvl->l2_mode);

	if (ret && ret != -ENXIO) {
		pr_warn("%s(): Cannot set L2 Mode %d, ret:%d\n",
				__func__, lvl->l2_mode, ret);
		goto bail_system_sleep;
	}

	if (lvl->notify_rpm) {
		ret = msm_rpm_enter_sleep(dbg_mask, nextcpu);
		if (ret) {
			pr_err("rpm_enter_sleep() failed with rc = %d\n", ret);
			goto bail_system_sleep;
		}


		if (!from_idle)
			us = USEC_PER_SEC * msm_pm_sleep_time_override;

		do_div(us, USEC_PER_SEC/SCLK_HZ);
		sclk = (uint32_t)us;
		msm_mpm_enter_sleep(sclk, from_idle, nextcpu);
	}
	system_state->last_entered_cluster_index = index;
	spin_unlock(&system_state->sync_lock);
	return;

bail_system_sleep:
	if (default_l2_mode != system_state->system_level[index].l2_mode)
		lpm_set_l2_mode(system_state, default_l2_mode);
	spin_unlock(&system_state->sync_lock);
}

static void lpm_system_unprepare(struct lpm_system_state *system_state,
		int cpu_index, bool from_idle)
{
	int index, i;
	struct lpm_cpu_level *cpu_level = &system_state->cpu_level[cpu_index];
	bool first_core_up;

	if (cpu_level->mode < system_state->sync_cpu_mode)
		return;

	spin_lock(&system_state->sync_lock);

	first_core_up = (system_state->num_cores_in_sync == num_powered_cores);

	system_state->num_cores_in_sync--;

	if (!system_state->system_level)
		goto unlock_and_return;

	index = system_state->last_entered_cluster_index;

	for (i = 0; i < system_state->num_system_levels; i++) {
		struct lpm_system_level *system_lvl
					= &system_state->system_level[i];
		if (cpu_level->mode >= system_lvl->min_cpu_mode)
			system_lvl->num_cpu_votes--;
	}

	if (!first_core_up || index < 0)
		goto unlock_and_return;

	if (default_l2_mode != system_state->system_level[index].l2_mode)
		lpm_set_l2_mode(system_state, default_l2_mode);

	if (system_state->system_level[index].notify_rpm) {
		msm_rpm_exit_sleep();
		msm_mpm_exit_sleep(from_idle);
	}
unlock_and_return:
	system_state->last_entered_cluster_index = -1;
	spin_unlock(&system_state->sync_lock);
}

s32 msm_cpuidle_get_deep_idle_latency(void)
{
	int i;
	struct lpm_cpu_level *level = sys_state.cpu_level;

	if (!level)
		return 0;

	for (i = 0; i < sys_state.num_cpu_levels; i++, level++) {
		if (level->mode == MSM_PM_SLEEP_MODE_POWER_COLLAPSE)
			break;
	}

	if (i ==  sys_state.num_cpu_levels)
		return 0;
	else
		return level->pwr.latency_us;
}

static int lpm_cpu_callback(struct notifier_block *cpu_nb,
	unsigned long action, void *hcpu)
{
	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_UP_PREPARE:
		++num_powered_cores;
		lpm_system_level_update();
		break;
	case CPU_DEAD:
	case CPU_UP_CANCELED:
		num_powered_cores = num_online_cpus();
		lpm_system_level_update();
		break;
	case CPU_ONLINE:
		smp_call_function_single((unsigned long)hcpu,
				setup_broadcast_timer, (void *)true, 1);
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

static noinline int lpm_cpu_power_select(struct cpuidle_device *dev, int *index)
{
	int best_level = -1;
	uint32_t best_level_pwr = ~0U;
	uint32_t latency_us = pm_qos_request(PM_QOS_CPU_DMA_LATENCY);
	uint32_t sleep_us =
		(uint32_t)(ktime_to_us(tick_nohz_get_sleep_length()));
	uint32_t modified_time_us = 0;
	uint32_t next_event_us = 0;
	uint32_t power;
	int i;

	if (!sys_state.cpu_level)
		return -EINVAL;

	if (!dev->cpu)
		next_event_us = (uint32_t)(ktime_to_us(get_next_event_time()));

	for (i = 0; i < sys_state.num_cpu_levels; i++) {
		struct lpm_cpu_level *level = &sys_state.cpu_level[i];
		struct power_params *pwr = &level->pwr;
		uint32_t next_wakeup_us = sleep_us;
		enum msm_pm_sleep_mode mode = level->mode;
		bool allow;

		if (level->sync && num_online_cpus() > 1
				&& !sys_state.allow_synched_levels)
			continue;

		allow = msm_cpu_pm_check_mode(dev->cpu, mode, true);

		if (!allow)
			continue;

		if (latency_us < pwr->latency_us)
			continue;

		if (next_event_us) {
			if (next_event_us < pwr->latency_us)
				continue;

			if (((next_event_us - pwr->latency_us) < sleep_us)
					|| (next_event_us < sleep_us)) {
				next_wakeup_us = next_event_us
					- pwr->latency_us;
			}
		}

		if (next_wakeup_us <= pwr->time_overhead_us)
			continue;

		if ((MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE == mode)
			|| (MSM_PM_SLEEP_MODE_POWER_COLLAPSE == mode))
			if (!dev->cpu && msm_rpm_waiting_for_ack())
					break;

		if ((next_wakeup_us >> 10) > pwr->time_overhead_us) {
			power = pwr->ss_power;
		} else {
			power = pwr->ss_power;
			power -= (pwr->time_overhead_us * pwr->ss_power)
					/ next_wakeup_us;
			power += pwr->energy_overhead / next_wakeup_us;
		}

		if (best_level_pwr >= power) {
			best_level = i;
			best_level_pwr = power;
			if (next_event_us < sleep_us &&
				(mode != MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT))
				modified_time_us = next_event_us
							- pwr->latency_us;
			else
				modified_time_us = 0;
		}
	}

	if (modified_time_us && !dev->cpu)
		msm_pm_set_timer(modified_time_us);

	return best_level;
}

static int lpm_get_l2_cache_value(const char *l2_str)
{
	int i;
	struct lpm_lookup_table l2_mode_lookup[] = {
		{MSM_SPM_L2_MODE_POWER_COLLAPSE, "l2_cache_pc"},
		{MSM_SPM_L2_MODE_PC_NO_RPM, "l2_cache_pc_no_rpm"},
		{MSM_SPM_L2_MODE_GDHS, "l2_cache_gdhs"},
		{MSM_SPM_L2_MODE_RETENTION, "l2_cache_retention"},
		{MSM_SPM_L2_MODE_DISABLED, "l2_cache_active"}
	};

	for (i = 0; i < ARRAY_SIZE(l2_mode_lookup); i++)
		if (!strcmp(l2_str, l2_mode_lookup[i].mode_name))
			return  l2_mode_lookup[i].modes;
	return -EINVAL;
}

static int lpm_levels_sysfs_add(void)
{
	struct kobject *module_kobj = NULL;
	struct kobject *low_power_kobj = NULL;
	int rc = 0;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("%s: cannot find kobject for module %s\n",
			__func__, KBUILD_MODNAME);
		rc = -ENOENT;
		goto resource_sysfs_add_exit;
	}

	low_power_kobj = kobject_create_and_add(
				"enable_low_power", module_kobj);
	if (!low_power_kobj) {
		pr_err("%s: cannot create kobject\n", __func__);
		rc = -ENOMEM;
		goto resource_sysfs_add_exit;
	}

	rc = sysfs_create_group(low_power_kobj, &lpm_levels_attr_grp);
resource_sysfs_add_exit:
	if (rc) {
		if (low_power_kobj) {
			sysfs_remove_group(low_power_kobj,
					&lpm_levels_attr_grp);
			kobject_del(low_power_kobj);
		}
	}

	return rc;
}
static int lpm_cpu_menu_select(struct cpuidle_device *dev, int *index)
{
	int j;

	for (; *index >= 0; (*index)--) {
		int mode = 0;
		bool allow = false;

		allow = msm_cpu_pm_check_mode(dev->cpu, mode, true);

		if (!allow)
			continue;

		for (j = sys_state.num_cpu_levels; j >= 0; j--) {
			struct lpm_cpu_level *l = &sys_state.cpu_level[j];
			if (mode == l->mode)
				return j;
		}
	}
	return -EPERM;
}

static inline void lpm_cpu_prepare(struct lpm_system_state *system_state,
		int cpu_index, bool from_idle)
{
	struct lpm_cpu_level *cpu_level = &system_state->cpu_level[cpu_index];
	unsigned int cpu = smp_processor_id();

	/* Use broadcast timer for aggregating sleep mode within a cluster.
	 * A broadcast timer could be used because of harware restriction or
	 * to ensure that we BC timer is used incase a cpu mode could trigger
	 * a cluster level sleep
	 */
	if (from_idle && (cpu_level->use_bc_timer ||
			(cpu_level->mode >= system_state->sync_cpu_mode)))
		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &cpu);
}

static inline void lpm_cpu_unprepare(struct lpm_system_state *system_state,
		int cpu_index, bool from_idle)
{
	struct lpm_cpu_level *cpu_level = &system_state->cpu_level[cpu_index];
	unsigned int cpu = smp_processor_id();

	if (from_idle && (cpu_level->use_bc_timer ||
			(cpu_level->mode >= system_state->sync_cpu_mode)))
		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &cpu);
}

static int lpm_system_select(struct lpm_system_state *system_state,
		int cpu_index, bool from_idle)
{
	uint64_t us = (~0ULL);
	struct clock_event_device *ed;
	struct lpm_cpu_level *cpu_level = &system_state->cpu_level[cpu_index];
	int i;
	bool last_core_down;

	if (cpu_level->mode < system_state->sync_cpu_mode)
		return -EINVAL;

	spin_lock(&system_state->sync_lock);

	last_core_down =
		(++system_state->num_cores_in_sync == num_powered_cores);

	if (!system_state->system_level) {
		spin_unlock(&system_state->sync_lock);
		return -EINVAL;
	}

	for (i = 0; i < system_state->num_system_levels; i++) {
		struct lpm_system_level *system_lvl =
			&system_state->system_level[i];
		if (cpu_level->mode >= system_lvl->min_cpu_mode)
			system_lvl->num_cpu_votes++;
	}
	spin_unlock(&system_state->sync_lock);

	if (!last_core_down)
		return -EBUSY;

	ed = tick_get_broadcast_device()->evtdev;
	if (!ed)
		return -EINVAL;

	if (from_idle)
		us = ktime_to_us(ktime_sub(ed->next_event, ktime_get()));
	else
		us = (~0ULL);

	return lpm_system_mode_select(system_state, (uint32_t)(us), from_idle);
}

static void lpm_enter_low_power(struct lpm_system_state *system_state,
		int cpu_index, bool from_idle)
{
	int idx;
	struct lpm_cpu_level *cpu_level = &system_state->cpu_level[cpu_index];

	lpm_cpu_prepare(system_state, cpu_index, from_idle);

	idx = lpm_system_select(system_state, cpu_index, from_idle);

	lpm_system_prepare(system_state, idx, from_idle);

	msm_cpu_pm_enter_sleep(cpu_level->mode, from_idle);

	lpm_system_unprepare(system_state, cpu_index, from_idle);

	lpm_cpu_unprepare(system_state, cpu_index, from_idle);
}

static int lpm_cpuidle_enter(struct cpuidle_device *dev,
		struct cpuidle_driver *drv, int index)
{
	int64_t time = ktime_to_ns(ktime_get());
	int idx;

	idx = menu_select ? lpm_cpu_menu_select(dev, &index) :
			lpm_cpu_power_select(dev, &index);
	if (idx < 0) {
		local_irq_enable();
		return -EPERM;
	}

	lpm_enter_low_power(&sys_state, idx, true);

	time = ktime_to_ns(ktime_get()) - time;
	do_div(time, 1000);
	dev->last_residency = (int)time;
	local_irq_enable();
	return idx;
}

static int lpm_suspend_enter(suspend_state_t state)
{
	int i;

	for (i = sys_state.num_cpu_levels - 1; i >= 0; i--) {
		bool allow = msm_cpu_pm_check_mode(smp_processor_id(),
				sys_state.cpu_level[i].mode, false);
		if (allow)
			break;
	}

	if (i < 0)
		return -EINVAL;

	lpm_enter_low_power(&sys_state, i,  false);

	return 0;
}

static int lpm_suspend_prepare(void)
{
	suspend_in_progress = true;
	msm_mpm_suspend_prepare();
	return 0;
}

static void lpm_suspend_wake(void)
{
	msm_mpm_suspend_wake();
	suspend_in_progress = false;
}

static struct platform_device lpm_dev = {
	.name = "msm_pm",
	.id = -1,
};

static const struct platform_suspend_ops lpm_suspend_ops = {
	.enter = lpm_suspend_enter,
	.valid = suspend_valid_only_mem,
	.prepare_late = lpm_suspend_prepare,
	.wake = lpm_suspend_wake,
};

static void setup_broadcast_timer(void *arg)
{
	unsigned long reason = (unsigned long)arg;
	int cpu = smp_processor_id();

	reason = reason ?
		CLOCK_EVT_NOTIFY_BROADCAST_ON : CLOCK_EVT_NOTIFY_BROADCAST_OFF;

	clockevents_notify(reason, &cpu);
}

static struct cpuidle_driver msm_cpuidle_driver = {
	.name = "msm_idle",
	.owner = THIS_MODULE,
};

static void lpm_cpuidle_init(void)
{
	int i = 0;
	int state_count = 0;

	if (!sys_state.cpu_level)
		return;
	BUG_ON(sys_state.num_cpu_levels > CPUIDLE_STATE_MAX);

	for (i = 0; i < sys_state.num_cpu_levels; i++) {
		struct cpuidle_state *st = &msm_cpuidle_driver.states[i];
		struct lpm_cpu_level *cpu_level = &sys_state.cpu_level[i];
		snprintf(st->name, CPUIDLE_NAME_LEN, "C%u\n", i);
		snprintf(st->desc, CPUIDLE_DESC_LEN, cpu_level->name);
		st->flags = 0;
		st->exit_latency = cpu_level->pwr.latency_us;
		st->power_usage = cpu_level->pwr.ss_power;
		st->target_residency = 0;
		st->enter = lpm_cpuidle_enter;
		state_count++;
	}
	msm_cpuidle_driver.state_count = state_count;
	msm_cpuidle_driver.safe_state_index = 0;

	if (cpuidle_register(&msm_cpuidle_driver, NULL))
		pr_err("%s(): Failed to register CPUIDLE device\n", __func__);
}

static int lpm_parse_power_params(struct device_node *node,
		struct power_params *pwr)
{
	char *key;
	int ret;

	key = "qcom,latency-us";
	ret  = of_property_read_u32(node, key, &pwr->latency_us);
	if (ret)
		goto fail;

	key = "qcom,ss-power";
	ret = of_property_read_u32(node, key, &pwr->ss_power);
	if (ret)
		goto fail;

	key = "qcom,energy-overhead";
	ret = of_property_read_u32(node, key, &pwr->energy_overhead);
	if (ret)
		goto fail;

	key = "qcom,time-overhead";
	ret = of_property_read_u32(node, key, &pwr->time_overhead_us);
	if (ret)
		goto fail;
fail:
	if (ret)
		pr_err("%s(): Error reading %s\n", __func__, key);
	return ret;
}

static int lpm_cpu_probe(struct platform_device *pdev)
{
	struct lpm_cpu_level *level = NULL, *l;
	struct device_node *node = NULL;
	int num_levels = 0;
	char *key;
	int ret;

	for_each_child_of_node(pdev->dev.of_node, node)
		num_levels++;

	level = kzalloc(num_levels * sizeof(struct lpm_cpu_level),
			GFP_KERNEL);

	if (!level)
		return -ENOMEM;

	l = &level[0];
	for_each_child_of_node(pdev->dev.of_node, node) {

		key = "qcom,mode";
		ret = of_property_read_string(node, key, &l->name);

		if (ret) {
			pr_err("%s(): Cannot read cpu mode%s\n", __func__, key);
			goto fail;
		}

		l->mode = msm_pm_get_sleep_mode_value(l->name);

		if (l->mode < 0) {
			pr_err("%s():Cannot parse cpu mode:%s\n", __func__,
					l->name);
			goto fail;
		}

		if (l->mode == MSM_PM_SLEEP_MODE_POWER_COLLAPSE)
			l->sync = true;

		key = "qcom,use-broadcast-timer";
		l->use_bc_timer = of_property_read_bool(node, key);

		ret = lpm_parse_power_params(node, &l->pwr);
		if (ret) {
			pr_err("%s(): cannot Parse power params\n", __func__);
			goto fail;
		}
		l++;
	}
	sys_state.cpu_level = level;
	sys_state.num_cpu_levels = num_levels;
	return ret;
fail:
	kfree(level);
	return ret;
}

static int lpm_system_probe(struct platform_device *pdev)
{
	struct lpm_system_level *level = NULL, *l;
	int num_levels = 0;
	struct device_node *node;
	char *key;
	int ret;

	for_each_child_of_node(pdev->dev.of_node, node)
		num_levels++;

	level = kzalloc(num_levels * sizeof(struct lpm_system_level),
			GFP_KERNEL);

	if (!level)
		return -ENOMEM;

	l = &level[0];
	for_each_child_of_node(pdev->dev.of_node, node) {

		key = "qcom,l2";
		ret = of_property_read_string(node, key, &l->name);
		if (ret) {
			pr_err("%s(): Failed to read L2 mode\n", __func__);
			goto fail;
		}

		l->l2_mode = lpm_get_l2_cache_value(l->name);

		if (l->l2_mode < 0) {
			pr_err("%s(): Failed to read l2 cache mode\n",
					__func__);
			goto fail;
		}

		key = "qcom,send-rpm-sleep-set";
		l->notify_rpm = of_property_read_bool(node, key);

		if (l->l2_mode >= MSM_SPM_L2_MODE_GDHS)
			l->sync = true;

		ret = lpm_parse_power_params(node, &l->pwr);
		if (ret) {
			pr_err("%s(): Failed to parse power params\n",
					__func__);
			goto fail;
		}

		key = "qcom,sync-cpus";
		l->sync = of_property_read_bool(node, key);

		if (l->sync) {
			const char *name;

			key = "qcom,min-cpu-mode";
			ret = of_property_read_string(node, key, &name);
			if (ret) {
				pr_err("%s(): Required key %snot found\n",
						__func__, name);
				goto fail;
			}

			l->min_cpu_mode = msm_pm_get_sleep_mode_value(name);

			if (l->min_cpu_mode < 0) {
				pr_err("%s(): Cannot parse cpu mode:%s\n",
						__func__, name);
				goto fail;
			}

			if (l->min_cpu_mode < sys_state.sync_cpu_mode)
				sys_state.sync_cpu_mode = l->min_cpu_mode;
		}

		l++;
	}
	sys_state.system_level = level;
	sys_state.num_system_levels = num_levels;
	sys_state.last_entered_cluster_index = -1;
	return ret;
fail:
	kfree(level);
	return ret;
}

static int lpm_probe(struct platform_device *pdev)
{
	struct device_node *node = NULL;
	char *key = NULL;
	int ret;

	node = pdev->dev.of_node;

	key = "qcom,allow-synced-levels";
	sys_state.allow_synched_levels = of_property_read_bool(node, key);

	key = "qcom,no-l2-saw";
	sys_state.no_l2_saw = of_property_read_bool(node, key);

	sys_state.sync_cpu_mode = MSM_PM_SLEEP_MODE_NR;
	spin_lock_init(&sys_state.sync_lock);
	sys_state.num_cores_in_sync = 0;

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);

	if (ret)
		goto fail;

	/* Do the following two steps only if L2 SAW is present */
	num_powered_cores = num_online_cpus();

	if (!sys_state.no_l2_saw) {
		int ret;
		const char *l2;
		key = "qcom,default-l2-state";
		ret = of_property_read_string(node, key, &l2);
		if (ret) {
			pr_err("%s(): Failed to read default L2 mode\n",
					__func__);
			goto fail;
		}

		default_l2_mode = lpm_get_l2_cache_value(l2);
		if (default_l2_mode < 0) {
			pr_err("%s(): Unable to parse default L2 mode\n",
					__func__);
			goto fail;
		}

		if (lpm_levels_sysfs_add())
			goto fail;
		msm_pm_set_l2_flush_flag(MSM_SCM_L2_ON);
	} else {
		msm_pm_set_l2_flush_flag(MSM_SCM_L2_OFF);
		default_l2_mode = MSM_SPM_L2_MODE_POWER_COLLAPSE;
	}

	get_cpu();
	on_each_cpu(setup_broadcast_timer, (void *)true, 1);
	put_cpu();

	register_hotcpu_notifier(&lpm_cpu_nblk);

	lpm_system_level_update();
	platform_device_register(&lpm_dev);
	suspend_set_ops(&lpm_suspend_ops);
	hrtimer_init(&lpm_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	lpm_cpuidle_init();
	return 0;
fail:
	pr_err("%s: Error in name %s key %s\n", __func__, node->full_name, key);
	return -EFAULT;
}

static struct of_device_id cpu_modes_mtch_tbl[] = {
	{.compatible = "qcom,cpu-modes"},
	{},
};

static struct platform_driver cpu_modes_driver = {
	.probe = lpm_cpu_probe,
	.driver = {
		.name = "cpu-modes",
		.owner = THIS_MODULE,
		.of_match_table = cpu_modes_mtch_tbl,
	},
};

static struct of_device_id system_modes_mtch_tbl[] = {
	{.compatible = "qcom,system-modes"},
	{},
};

static struct platform_driver system_modes_driver = {
	.probe = lpm_system_probe,
	.driver = {
		.name = "system-modes",
		.owner = THIS_MODULE,
		.of_match_table = system_modes_mtch_tbl,
	},
};

static struct of_device_id lpm_levels_match_table[] = {
	{.compatible = "qcom,lpm-levels"},
	{},
};

static struct platform_driver lpm_levels_driver = {
	.probe = lpm_probe,
	.driver = {
		.name = "lpm-levels",
		.owner = THIS_MODULE,
		.of_match_table = lpm_levels_match_table,
	},
};

static int __init lpm_levels_module_init(void)
{
	int rc;
	rc = platform_driver_register(&cpu_modes_driver);
	if (rc) {
		pr_err("Error registering %s\n", cpu_modes_driver.driver.name);
		goto fail;
	}

	rc = platform_driver_register(&system_modes_driver);
	if (rc) {
		platform_driver_unregister(&cpu_modes_driver);
		pr_err("Error registering %s\n",
				system_modes_driver.driver.name);
		goto fail;
	}

	rc = platform_driver_register(&lpm_levels_driver);
	if (rc) {
		platform_driver_unregister(&cpu_modes_driver);
		platform_driver_unregister(&system_modes_driver);
		pr_err("Error registering %s\n",
				lpm_levels_driver.driver.name);
	}
fail:
	return rc;
}
late_initcall(lpm_levels_module_init);
