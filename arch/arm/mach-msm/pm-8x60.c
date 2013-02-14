/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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
#include <linux/completion.h>
#include <linux/cpuidle.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ktime.h>
#include <linux/pm.h>
#include <linux/pm_qos.h>
#include <linux/smp.h>
#include <linux/suspend.h>
#include <linux/tick.h>
#include <linux/platform_device.h>
#include <linux/regulator/krait-regulator.h>
#include <mach/msm_iomap.h>
#include <mach/socinfo.h>
#include <mach/system.h>
#include <mach/scm.h>
#include <mach/socinfo.h>
#include <mach/msm-krait-l2-accessors.h>
#include <asm/cacheflush.h>
#include <asm/hardware/gic.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/outercache.h>
#ifdef CONFIG_VFP
#include <asm/vfp.h>
#endif

#include "acpuclock.h"
#include "clock.h"
#include "avs.h"
#include <mach/cpuidle.h>
#include "idle.h"
#include "pm.h"
#include "scm-boot.h"
#include "spm.h"
#include "timer.h"
#include "pm-boot.h"
#include <mach/event_timer.h>
#define CREATE_TRACE_POINTS
#include "trace_msm_low_power.h"
/******************************************************************************
 * Debug Definitions
 *****************************************************************************/

enum {
	MSM_PM_DEBUG_SUSPEND = BIT(0),
	MSM_PM_DEBUG_POWER_COLLAPSE = BIT(1),
	MSM_PM_DEBUG_SUSPEND_LIMITS = BIT(2),
	MSM_PM_DEBUG_CLOCK = BIT(3),
	MSM_PM_DEBUG_RESET_VECTOR = BIT(4),
	MSM_PM_DEBUG_IDLE_CLK = BIT(5),
	MSM_PM_DEBUG_IDLE = BIT(6),
	MSM_PM_DEBUG_IDLE_LIMITS = BIT(7),
	MSM_PM_DEBUG_HOTPLUG = BIT(8),
};

static int msm_pm_debug_mask = 1;
module_param_named(
	debug_mask, msm_pm_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP
);
static int msm_pm_retention_tz_call;

/******************************************************************************
 * Sleep Modes and Parameters
 *****************************************************************************/
enum {
	MSM_PM_MODE_ATTR_SUSPEND,
	MSM_PM_MODE_ATTR_IDLE,
	MSM_PM_MODE_ATTR_NR,
};

#define SCM_L2_RETENTION	(0x2)
#define SCM_CMD_TERMINATE_PC	(0x2)

static char *msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_NR] = {
	[MSM_PM_MODE_ATTR_SUSPEND] = "suspend_enabled",
	[MSM_PM_MODE_ATTR_IDLE] = "idle_enabled",
};

struct msm_pm_kobj_attribute {
	unsigned int cpu;
	struct kobj_attribute ka;
};

#define GET_CPU_OF_ATTR(attr) \
	(container_of(attr, struct msm_pm_kobj_attribute, ka)->cpu)

struct msm_pm_sysfs_sleep_mode {
	struct kobject *kobj;
	struct attribute_group attr_group;
	struct attribute *attrs[MSM_PM_MODE_ATTR_NR + 1];
	struct msm_pm_kobj_attribute kas[MSM_PM_MODE_ATTR_NR];
};

static char *msm_pm_sleep_mode_labels[MSM_PM_SLEEP_MODE_NR] = {
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE] = "power_collapse",
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT] = "wfi",
	[MSM_PM_SLEEP_MODE_RETENTION] = "retention",
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE] =
		"standalone_power_collapse",
};

static struct msm_pm_init_data_type msm_pm_init_data;
static struct hrtimer pm_hrtimer;
static struct msm_pm_sleep_ops pm_sleep_ops;
static bool msm_pm_ldo_retention_enabled = true;
/*
 * Write out the attribute.
 */
static ssize_t msm_pm_mode_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int ret = -EINVAL;
	int i;

	for (i = 0; i < MSM_PM_SLEEP_MODE_NR; i++) {
		struct kernel_param kp;
		unsigned int cpu;
		struct msm_pm_platform_data *mode;

		if (msm_pm_sleep_mode_labels[i] == NULL)
			continue;

		if (strcmp(kobj->name, msm_pm_sleep_mode_labels[i]))
			continue;

		cpu = GET_CPU_OF_ATTR(attr);
		mode = &msm_pm_sleep_modes[MSM_PM_MODE(cpu, i)];

		if (!strcmp(attr->attr.name,
			msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_SUSPEND])) {
			u32 arg = mode->suspend_enabled;
			kp.arg = &arg;
			ret = param_get_ulong(buf, &kp);
		} else if (!strcmp(attr->attr.name,
			msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_IDLE])) {
			u32 arg = mode->idle_enabled;
			kp.arg = &arg;
			ret = param_get_ulong(buf, &kp);
		}

		break;
	}

	if (ret > 0) {
		strlcat(buf, "\n", PAGE_SIZE);
		ret++;
	}

	return ret;
}

/*
 * Read in the new attribute value.
 */
static ssize_t msm_pm_mode_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = -EINVAL;
	int i;

	for (i = 0; i < MSM_PM_SLEEP_MODE_NR; i++) {
		struct kernel_param kp;
		unsigned int cpu;
		struct msm_pm_platform_data *mode;

		if (msm_pm_sleep_mode_labels[i] == NULL)
			continue;

		if (strcmp(kobj->name, msm_pm_sleep_mode_labels[i]))
			continue;

		cpu = GET_CPU_OF_ATTR(attr);
		mode = &msm_pm_sleep_modes[MSM_PM_MODE(cpu, i)];

		if (!strcmp(attr->attr.name,
			msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_SUSPEND])) {
			kp.arg = &mode->suspend_enabled;
			ret = param_set_byte(buf, &kp);
		} else if (!strcmp(attr->attr.name,
			msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_IDLE])) {
			kp.arg = &mode->idle_enabled;
			ret = param_set_byte(buf, &kp);
		}

		break;
	}

	return ret ? ret : count;
}

/*
 * Add sysfs entries for one cpu.
 */
static int __init msm_pm_mode_sysfs_add_cpu(
	unsigned int cpu, struct kobject *modes_kobj)
{
	char cpu_name[8];
	struct kobject *cpu_kobj;
	struct msm_pm_sysfs_sleep_mode *mode = NULL;
	int i, j, k;
	int ret;

	snprintf(cpu_name, sizeof(cpu_name), "cpu%u", cpu);
	cpu_kobj = kobject_create_and_add(cpu_name, modes_kobj);
	if (!cpu_kobj) {
		pr_err("%s: cannot create %s kobject\n", __func__, cpu_name);
		ret = -ENOMEM;
		goto mode_sysfs_add_cpu_exit;
	}

	for (i = 0; i < MSM_PM_SLEEP_MODE_NR; i++) {
		int idx = MSM_PM_MODE(cpu, i);

		if ((!msm_pm_sleep_modes[idx].suspend_supported)
			&& (!msm_pm_sleep_modes[idx].idle_supported))
			continue;

		if (!msm_pm_sleep_mode_labels[i] ||
				!msm_pm_sleep_mode_labels[i][0])
			continue;

		mode = kzalloc(sizeof(*mode), GFP_KERNEL);
		if (!mode) {
			pr_err("%s: cannot allocate memory for attributes\n",
				__func__);
			ret = -ENOMEM;
			goto mode_sysfs_add_cpu_exit;
		}

		mode->kobj = kobject_create_and_add(
				msm_pm_sleep_mode_labels[i], cpu_kobj);
		if (!mode->kobj) {
			pr_err("%s: cannot create kobject\n", __func__);
			ret = -ENOMEM;
			goto mode_sysfs_add_cpu_exit;
		}

		for (k = 0, j = 0; k < MSM_PM_MODE_ATTR_NR; k++) {
			if ((k == MSM_PM_MODE_ATTR_IDLE) &&
				!msm_pm_sleep_modes[idx].idle_supported)
				continue;
			if ((k == MSM_PM_MODE_ATTR_SUSPEND) &&
			     !msm_pm_sleep_modes[idx].suspend_supported)
				continue;
			sysfs_attr_init(&mode->kas[j].ka.attr);
			mode->kas[j].cpu = cpu;
			mode->kas[j].ka.attr.mode = 0644;
			mode->kas[j].ka.show = msm_pm_mode_attr_show;
			mode->kas[j].ka.store = msm_pm_mode_attr_store;
			mode->kas[j].ka.attr.name = msm_pm_mode_attr_labels[k];
			mode->attrs[j] = &mode->kas[j].ka.attr;
			j++;
		}
		mode->attrs[j] = NULL;

		mode->attr_group.attrs = mode->attrs;
		ret = sysfs_create_group(mode->kobj, &mode->attr_group);
		if (ret) {
			pr_err("%s: cannot create kobject attribute group\n",
				__func__);
			goto mode_sysfs_add_cpu_exit;
		}
	}

	ret = 0;

mode_sysfs_add_cpu_exit:
	if (ret) {
		if (mode && mode->kobj)
			kobject_del(mode->kobj);
		kfree(mode);
	}

	return ret;
}

/*
 * Add sysfs entries for the sleep modes.
 */
static int __init msm_pm_mode_sysfs_add(void)
{
	struct kobject *module_kobj;
	struct kobject *modes_kobj;
	unsigned int cpu;
	int ret;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("%s: cannot find kobject for module %s\n",
			__func__, KBUILD_MODNAME);
		ret = -ENOENT;
		goto mode_sysfs_add_exit;
	}

	modes_kobj = kobject_create_and_add("modes", module_kobj);
	if (!modes_kobj) {
		pr_err("%s: cannot create modes kobject\n", __func__);
		ret = -ENOMEM;
		goto mode_sysfs_add_exit;
	}

	for_each_possible_cpu(cpu) {
		ret = msm_pm_mode_sysfs_add_cpu(cpu, modes_kobj);
		if (ret)
			goto mode_sysfs_add_exit;
	}

	ret = 0;

mode_sysfs_add_exit:
	return ret;
}

/******************************************************************************
 * Configure Hardware before/after Low Power Mode
 *****************************************************************************/

/*
 * Configure hardware registers in preparation for Apps power down.
 */
static void msm_pm_config_hw_before_power_down(void)
{
	return;
}

/*
 * Clear hardware registers after Apps powers up.
 */
static void msm_pm_config_hw_after_power_up(void)
{
}

/*
 * Configure hardware registers in preparation for SWFI.
 */
static void msm_pm_config_hw_before_swfi(void)
{
	return;
}

/*
 * Configure/Restore hardware registers in preparation for Retention.
 */

static void msm_pm_config_hw_after_retention(void)
{
	int ret;

	ret = msm_spm_set_low_power_mode(MSM_SPM_MODE_CLOCK_GATING, false);
	WARN_ON(ret);
	krait_power_mdd_enable(smp_processor_id(), false);
}

static void msm_pm_config_hw_before_retention(void)
{
	krait_power_mdd_enable(smp_processor_id(), true);
	return;
}


/******************************************************************************
 * Suspend Max Sleep Time
 *****************************************************************************/

#ifdef CONFIG_MSM_SLEEP_TIME_OVERRIDE
static int msm_pm_sleep_time_override;
module_param_named(sleep_time_override,
	msm_pm_sleep_time_override, int, S_IRUGO | S_IWUSR | S_IWGRP);
#endif

#define SCLK_HZ (32768)
#define MSM_PM_SLEEP_TICK_LIMIT (0x6DDD000)

static uint32_t msm_pm_max_sleep_time;

/*
 * Convert time from nanoseconds to slow clock ticks, then cap it to the
 * specified limit
 */
static int64_t msm_pm_convert_and_cap_time(int64_t time_ns, int64_t limit)
{
	do_div(time_ns, NSEC_PER_SEC / SCLK_HZ);
	return (time_ns > limit) ? limit : time_ns;
}

/*
 * Set the sleep time for suspend.  0 means infinite sleep time.
 */
void msm_pm_set_max_sleep_time(int64_t max_sleep_time_ns)
{
	if (max_sleep_time_ns == 0) {
		msm_pm_max_sleep_time = 0;
	} else {
		msm_pm_max_sleep_time = (uint32_t)msm_pm_convert_and_cap_time(
			max_sleep_time_ns, MSM_PM_SLEEP_TICK_LIMIT);

		if (msm_pm_max_sleep_time == 0)
			msm_pm_max_sleep_time = 1;
	}

	if (msm_pm_debug_mask & MSM_PM_DEBUG_SUSPEND)
		pr_info("%s: Requested %lld ns Giving %u sclk ticks\n",
			__func__, max_sleep_time_ns, msm_pm_max_sleep_time);
}
EXPORT_SYMBOL(msm_pm_set_max_sleep_time);

struct reg_data {
	uint32_t reg;
	uint32_t val;
};

static struct reg_data reg_saved_state[] = {
	{ .reg = 0x4501, },
	{ .reg = 0x5501, },
	{ .reg = 0x6501, },
	{ .reg = 0x7501, },
	{ .reg = 0x0500, },
};

static unsigned int active_vdd;
static bool msm_pm_save_cp15;
static const unsigned int pc_vdd = 0x98;

static void msm_pm_save_cpu_reg(void)
{
	int i;

	/* Only on core0 */
	if (smp_processor_id())
		return;

	/**
	 * On some targets, L2 PC will turn off may reset the core
	 * configuration for the mux and the default may not make the core
	 * happy when it resumes.
	 * Save the active vdd, and set the core vdd to QSB max vdd, so that
	 * when the core resumes, it is capable of supporting the current QSB
	 * rate. Then restore the active vdd before switching the acpuclk rate.
	 */
	if (msm_pm_get_l2_flush_flag() == 1) {
		active_vdd = msm_spm_get_vdd(0);
		for (i = 0; i < ARRAY_SIZE(reg_saved_state); i++)
			reg_saved_state[i].val =
				get_l2_indirect_reg(reg_saved_state[i].reg);
		msm_spm_set_vdd(0, pc_vdd);
	}
}

static void msm_pm_restore_cpu_reg(void)
{
	int i;

	/* Only on core0 */
	if (smp_processor_id())
		return;

	if (msm_pm_get_l2_flush_flag() == 1) {
		for (i = 0; i < ARRAY_SIZE(reg_saved_state); i++)
			set_l2_indirect_reg(reg_saved_state[i].reg,
					reg_saved_state[i].val);
		msm_spm_set_vdd(0, active_vdd);
	}
}

static void msm_pm_swfi(void)
{
	msm_pm_config_hw_before_swfi();
	msm_arch_idle();
}


static void msm_pm_retention(void)
{
	int ret = 0;

	msm_pm_config_hw_before_retention();
	ret = msm_spm_set_low_power_mode(MSM_SPM_MODE_POWER_RETENTION, false);
	WARN_ON(ret);

	if (msm_pm_retention_tz_call)
		scm_call_atomic1(SCM_SVC_BOOT, SCM_CMD_TERMINATE_PC,
					SCM_L2_RETENTION);
	else
		msm_arch_idle();

	msm_pm_config_hw_after_retention();
}

static bool __ref msm_pm_spm_power_collapse(
	unsigned int cpu, bool from_idle, bool notify_rpm)
{
	void *entry;
	bool collapsed = 0;
	int ret;

	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: notify_rpm %d\n",
			cpu, __func__, (int) notify_rpm);

	ret = msm_spm_set_low_power_mode(
			MSM_SPM_MODE_POWER_COLLAPSE, notify_rpm);
	WARN_ON(ret);

	entry = (!cpu || from_idle) ?
		msm_pm_collapse_exit : msm_secondary_startup;
	msm_pm_boot_config_before_pc(cpu, virt_to_phys(entry));

	if (MSM_PM_DEBUG_RESET_VECTOR & msm_pm_debug_mask)
		pr_info("CPU%u: %s: program vector to %p\n",
			cpu, __func__, entry);

	collapsed = msm_pm_collapse();

	msm_pm_boot_config_after_pc(cpu);

	if (collapsed) {
		cpu_init();
		local_fiq_enable();
	}

	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: msm_pm_collapse returned, collapsed %d\n",
			cpu, __func__, collapsed);

	ret = msm_spm_set_low_power_mode(MSM_SPM_MODE_CLOCK_GATING, false);
	WARN_ON(ret);
	return collapsed;
}

static bool msm_pm_power_collapse_standalone(bool from_idle)
{
	unsigned int cpu = smp_processor_id();
	unsigned int avsdscr;
	unsigned int avscsr;
	bool collapsed;

	avsdscr = avs_get_avsdscr();
	avscsr = avs_get_avscsr();
	avs_set_avscsr(0); /* Disable AVS */

	collapsed = msm_pm_spm_power_collapse(cpu, from_idle, false);

	avs_set_avsdscr(avsdscr);
	avs_set_avscsr(avscsr);
	return collapsed;
}

static bool msm_pm_power_collapse(bool from_idle)
{
	unsigned int cpu = smp_processor_id();
	unsigned long saved_acpuclk_rate;
	unsigned int avsdscr;
	unsigned int avscsr;
	bool collapsed;

	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: idle %d\n",
			cpu, __func__, (int)from_idle);

	msm_pm_config_hw_before_power_down();
	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: pre power down\n", cpu, __func__);

	avsdscr = avs_get_avsdscr();
	avscsr = avs_get_avscsr();
	avs_set_avscsr(0); /* Disable AVS */

	if (cpu_online(cpu))
		saved_acpuclk_rate = acpuclk_power_collapse();
	else
		saved_acpuclk_rate = 0;

	if (MSM_PM_DEBUG_CLOCK & msm_pm_debug_mask)
		pr_info("CPU%u: %s: change clock rate (old rate = %lu)\n",
			cpu, __func__, saved_acpuclk_rate);

	if (msm_pm_save_cp15)
		msm_pm_save_cpu_reg();

	collapsed = msm_pm_spm_power_collapse(cpu, from_idle, true);

	if (msm_pm_save_cp15)
		msm_pm_restore_cpu_reg();

	if (cpu_online(cpu)) {
		if (MSM_PM_DEBUG_CLOCK & msm_pm_debug_mask)
			pr_info("CPU%u: %s: restore clock rate to %lu\n",
				cpu, __func__, saved_acpuclk_rate);
		if (acpuclk_set_rate(cpu, saved_acpuclk_rate, SETRATE_PC) < 0)
			pr_err("CPU%u: %s: failed to restore clock rate(%lu)\n",
				cpu, __func__, saved_acpuclk_rate);
	} else {
		unsigned int gic_dist_enabled;
		unsigned int gic_dist_pending;
		gic_dist_enabled = readl_relaxed(
				MSM_QGIC_DIST_BASE + GIC_DIST_ENABLE_CLEAR);
		gic_dist_pending = readl_relaxed(
				MSM_QGIC_DIST_BASE + GIC_DIST_PENDING_SET);
		mb();
		gic_dist_pending &= gic_dist_enabled;

		if (gic_dist_pending)
			pr_err("CPU %d interrupted during hotplug.Pending int 0x%x\n",
					cpu, gic_dist_pending);
	}


	avs_set_avsdscr(avsdscr);
	avs_set_avscsr(avscsr);
	msm_pm_config_hw_after_power_up();
	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: post power up\n", cpu, __func__);

	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: return\n", cpu, __func__);
	return collapsed;
}

static void msm_pm_target_init(void)
{
	if (cpu_is_apq8064())
		msm_pm_save_cp15 = true;
}

static int64_t msm_pm_timer_enter_idle(void)
{
	if (msm_pm_init_data.use_sync_timer)
		return ktime_to_ns(tick_nohz_get_sleep_length());

	return msm_timer_enter_idle();
}

static void msm_pm_timer_exit_idle(bool timer_halted)
{
	if (msm_pm_init_data.use_sync_timer)
		return;

	msm_timer_exit_idle((int) timer_halted);
}

static int64_t msm_pm_timer_enter_suspend(int64_t *period)
{
	int64_t time = 0;

	if (msm_pm_init_data.use_sync_timer)
		return ktime_to_ns(ktime_get());

	time = msm_timer_get_sclk_time(period);
	if (!time)
		pr_err("%s: Unable to read sclk.\n", __func__);

	return time;
}

static int64_t msm_pm_timer_exit_suspend(int64_t time, int64_t period)
{
	if (msm_pm_init_data.use_sync_timer)
		return ktime_to_ns(ktime_get()) - time;

	if (time != 0) {
		int64_t end_time = msm_timer_get_sclk_time(NULL);
		if (end_time != 0) {
			time = end_time - time;
			if (time < 0)
				time += period;
		} else
			time = 0;
	}

	return time;
}

/**
 * pm_hrtimer_cb() : Callback function for hrtimer created if the
 *                   core needs to be awake to handle an event.
 * @hrtimer : Pointer to hrtimer
 */
static enum hrtimer_restart pm_hrtimer_cb(struct hrtimer *hrtimer)
{
	return HRTIMER_NORESTART;
}

/**
 * msm_pm_set_timer() : Set an hrtimer to wakeup the core in time
 *                      to handle an event.
 */
static void msm_pm_set_timer(uint32_t modified_time_us)
{
	u64 modified_time_ns = modified_time_us * NSEC_PER_USEC;
	ktime_t modified_ktime = ns_to_ktime(modified_time_ns);
	pm_hrtimer.function = pm_hrtimer_cb;
	hrtimer_start(&pm_hrtimer, modified_ktime, HRTIMER_MODE_ABS);
}

/******************************************************************************
 * External Idle/Suspend Functions
 *****************************************************************************/

void arch_idle(void)
{
	return;
}

static inline void msm_pm_ftrace_lpm_enter(unsigned int cpu,
		uint32_t latency, uint32_t sleep_us,
		uint32_t wake_up,
		enum msm_pm_sleep_mode mode)
{
	switch (mode) {
	case MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT:
		trace_msm_pm_enter_wfi(cpu, latency, sleep_us, wake_up);
		break;
	case MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE:
		trace_msm_pm_enter_spc(cpu, latency, sleep_us, wake_up);
		break;
	case MSM_PM_SLEEP_MODE_POWER_COLLAPSE:
		trace_msm_pm_enter_pc(cpu, latency, sleep_us, wake_up);
		break;
	case MSM_PM_SLEEP_MODE_RETENTION:
		trace_msm_pm_enter_ret(cpu, latency, sleep_us, wake_up);
		break;
	default:
		break;
	}
}

static inline void msm_pm_ftrace_lpm_exit(unsigned int cpu,
		enum msm_pm_sleep_mode mode,
		bool success)
{
	switch (mode) {
	case MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT:
		trace_msm_pm_exit_wfi(cpu, success);
		break;
	case MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE:
		trace_msm_pm_exit_spc(cpu, success);
		break;
	case MSM_PM_SLEEP_MODE_POWER_COLLAPSE:
		trace_msm_pm_exit_pc(cpu, success);
		break;
	case MSM_PM_SLEEP_MODE_RETENTION:
		trace_msm_pm_exit_ret(cpu, success);
		break;
	default:
		break;
	}
}

static int msm_pm_idle_prepare(struct cpuidle_device *dev,
		struct cpuidle_driver *drv, int index,
		void **msm_pm_idle_rs_limits)
{
	int i;
	unsigned int power_usage = -1;
	int ret = MSM_PM_SLEEP_MODE_NOT_SELECTED;
	uint32_t modified_time_us = 0;
	struct msm_pm_time_params time_param;

	time_param.latency_us =
		(uint32_t) pm_qos_request(PM_QOS_CPU_DMA_LATENCY);
	time_param.sleep_us =
		(uint32_t) (ktime_to_us(tick_nohz_get_sleep_length())
								& UINT_MAX);
	time_param.modified_time_us = 0;

	if (!dev->cpu)
		time_param.next_event_us =
			(uint32_t) (ktime_to_us(get_next_event_time())
								& UINT_MAX);
	else
		time_param.next_event_us = 0;

	for (i = 0; i < dev->state_count; i++) {
		struct cpuidle_state *state = &drv->states[i];
		struct cpuidle_state_usage *st_usage = &dev->states_usage[i];
		enum msm_pm_sleep_mode mode;
		bool allow;
		uint32_t power;
		int idx;

		mode = (enum msm_pm_sleep_mode) cpuidle_get_statedata(st_usage);
		idx = MSM_PM_MODE(dev->cpu, mode);

		allow = msm_pm_sleep_modes[idx].idle_enabled &&
				msm_pm_sleep_modes[idx].idle_supported;

		switch (mode) {
		case MSM_PM_SLEEP_MODE_POWER_COLLAPSE:
			if (num_online_cpus() > 1) {
				allow = false;
				break;
			}
			/* fall through */
		case MSM_PM_SLEEP_MODE_RETENTION:
			/*
			 * The Krait BHS regulator doesn't have enough head
			 * room to drive the retention voltage on LDO and so
			 * has disabled retention
			 */
			if (!msm_pm_ldo_retention_enabled)
				break;

			if (!allow)
				break;

			if (msm_pm_retention_tz_call &&
				num_online_cpus() > 1) {
				allow = false;
				break;
			}
			/* fall through */

		case MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE:
			if (!allow)
				break;
			/* fall through */

		case MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT:
			if (!allow)
				break;
			/* fall through */

			if (pm_sleep_ops.lowest_limits)
				*msm_pm_idle_rs_limits =
					pm_sleep_ops.lowest_limits(
						true, mode,
						&time_param, &power);

			if (MSM_PM_DEBUG_IDLE & msm_pm_debug_mask)
				pr_info("CPU%u: %s: %s, latency %uus, "
					"sleep %uus, limit %p\n",
					dev->cpu, __func__, state->desc,
					time_param.latency_us,
					time_param.sleep_us,
					*msm_pm_idle_rs_limits);

			if (!*msm_pm_idle_rs_limits)
				allow = false;
			break;

		default:
			allow = false;
			break;
		}

		if (MSM_PM_DEBUG_IDLE & msm_pm_debug_mask)
			pr_info("CPU%u: %s: allow %s: %d\n",
				dev->cpu, __func__, state->desc, (int)allow);

		if (allow) {
			if (power < power_usage) {
				power_usage = power;
				modified_time_us = time_param.modified_time_us;
				ret = mode;
			}

		}
	}

	if (modified_time_us && !dev->cpu)
		msm_pm_set_timer(modified_time_us);

	msm_pm_ftrace_lpm_enter(dev->cpu, time_param.latency_us,
			time_param.sleep_us, time_param.next_event_us,
			ret);

	return ret;
}

enum msm_pm_sleep_mode msm_pm_idle_enter(struct cpuidle_device *dev,
	struct cpuidle_driver *drv, int index)
{
	int64_t time;
	bool collapsed = 1;
	int exit_stat = -1;
	enum msm_pm_sleep_mode sleep_mode;
	void *msm_pm_idle_rs_limits = NULL;
	int sleep_delay = 1;
	int ret = -ENODEV;
	int64_t timer_expiration = 0;
	int notify_rpm = false;
	bool timer_halted = false;

	sleep_mode = msm_pm_idle_prepare(dev, drv, index,
		&msm_pm_idle_rs_limits);

	if (!msm_pm_idle_rs_limits) {
		sleep_mode = MSM_PM_SLEEP_MODE_NOT_SELECTED;
		goto cpuidle_enter_bail;
	}

	if (MSM_PM_DEBUG_IDLE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: mode %d\n",
			smp_processor_id(), __func__, sleep_mode);

	time = ktime_to_ns(ktime_get());

	if (sleep_mode == MSM_PM_SLEEP_MODE_POWER_COLLAPSE) {
		notify_rpm = true;
		timer_expiration = msm_pm_timer_enter_idle();

		sleep_delay = (uint32_t) msm_pm_convert_and_cap_time(
			timer_expiration, MSM_PM_SLEEP_TICK_LIMIT);
		if (sleep_delay == 0) /* 0 would mean infinite time */
			sleep_delay = 1;
	}

	if (pm_sleep_ops.enter_sleep)
		ret = pm_sleep_ops.enter_sleep(sleep_delay,
			msm_pm_idle_rs_limits,
			true, notify_rpm);
	if (!ret) {

		switch (sleep_mode) {
		case MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT:
			msm_pm_swfi();
			exit_stat = MSM_PM_STAT_IDLE_WFI;
			break;

		case MSM_PM_SLEEP_MODE_RETENTION:
			msm_pm_retention();
			exit_stat = MSM_PM_STAT_RETENTION;
			break;

		case MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE:
			collapsed = msm_pm_power_collapse_standalone(true);
			exit_stat = MSM_PM_STAT_IDLE_STANDALONE_POWER_COLLAPSE;
			break;

		case MSM_PM_SLEEP_MODE_POWER_COLLAPSE:
			if (MSM_PM_DEBUG_IDLE_CLK & msm_pm_debug_mask)
				clock_debug_print_enabled();

			collapsed = msm_pm_power_collapse(true);
			timer_halted = true;

			exit_stat = MSM_PM_STAT_IDLE_POWER_COLLAPSE;
			msm_pm_timer_exit_idle(timer_halted);
			break;

		case MSM_PM_SLEEP_MODE_NOT_SELECTED:
			goto cpuidle_enter_bail;
			break;

		default:
			__WARN();
			goto cpuidle_enter_bail;
			break;
		}
		if (pm_sleep_ops.exit_sleep)
			pm_sleep_ops.exit_sleep(msm_pm_idle_rs_limits,
					true, notify_rpm, collapsed);

		time = ktime_to_ns(ktime_get()) - time;
		msm_pm_ftrace_lpm_exit(smp_processor_id(), sleep_mode,
					collapsed);
		if (exit_stat >= 0)
			msm_pm_add_stat(exit_stat, time);
		do_div(time, 1000);
		dev->last_residency = (int) time;
		return sleep_mode;

	} else if (sleep_mode == MSM_PM_SLEEP_MODE_POWER_COLLAPSE) {
		msm_pm_timer_exit_idle(timer_halted);
		sleep_mode = MSM_PM_SLEEP_MODE_NOT_SELECTED;
	} else
		sleep_mode = MSM_PM_SLEEP_MODE_NOT_SELECTED;

cpuidle_enter_bail:
	dev->last_residency = 0;
	return sleep_mode;
}

void msm_pm_cpu_enter_lowpower(unsigned int cpu)
{
	int i;
	bool allow[MSM_PM_SLEEP_MODE_NR];

	for (i = 0; i < MSM_PM_SLEEP_MODE_NR; i++) {
		struct msm_pm_platform_data *mode;

		mode = &msm_pm_sleep_modes[MSM_PM_MODE(cpu, i)];
		allow[i] = mode->suspend_supported && mode->suspend_enabled;
	}

	if (MSM_PM_DEBUG_HOTPLUG & msm_pm_debug_mask)
		pr_notice("CPU%u: %s: shutting down cpu\n", cpu, __func__);

	if (allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE])
		msm_pm_power_collapse(false);
	else if (allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE])
		msm_pm_power_collapse_standalone(false);
	else if (allow[MSM_PM_SLEEP_MODE_RETENTION])
		msm_pm_retention();
	else
		msm_pm_swfi();
}

static void msm_pm_ack_retention_disable(void *data)
{
	/*
	 * This is a NULL function to ensure that the core has woken up
	 * and is safe to disable retention.
	 */
}
/**
 * msm_pm_enable_retention() - Disable/Enable retention on all cores
 * @enable: Enable/Disable retention
 *
 */
void msm_pm_enable_retention(bool enable)
{
	msm_pm_ldo_retention_enabled = enable;
	/*
	 * If retention is being disabled, wakeup all online core to ensure
	 * that it isn't executing retention. Offlined cores need not be woken
	 * up as they enter the deepest sleep mode, namely RPM assited power
	 * collapse
	 */
	if (!enable)
		smp_call_function_many(cpu_online_mask,
				msm_pm_ack_retention_disable,
				NULL, true);
}
EXPORT_SYMBOL(msm_pm_enable_retention);

static int msm_pm_enter(suspend_state_t state)
{
	bool allow[MSM_PM_SLEEP_MODE_NR];
	int i;
	int64_t period = 0;
	int64_t time = msm_pm_timer_enter_suspend(&period);
	struct msm_pm_time_params time_param;

	time_param.latency_us = -1;
	time_param.sleep_us = -1;
	time_param.next_event_us = 0;

	if (MSM_PM_DEBUG_SUSPEND & msm_pm_debug_mask)
		pr_info("%s\n", __func__);

	if (smp_processor_id()) {
		__WARN();
		goto enter_exit;
	}


	for (i = 0; i < MSM_PM_SLEEP_MODE_NR; i++) {
		struct msm_pm_platform_data *mode;

		mode = &msm_pm_sleep_modes[MSM_PM_MODE(0, i)];
		allow[i] = mode->suspend_supported && mode->suspend_enabled;
	}

	if (allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE]) {
		void *rs_limits = NULL;
		int ret = -ENODEV;
		uint32_t power;

		if (MSM_PM_DEBUG_SUSPEND & msm_pm_debug_mask)
			pr_info("%s: power collapse\n", __func__);

		clock_debug_print_enabled();

#ifdef CONFIG_MSM_SLEEP_TIME_OVERRIDE
		if (msm_pm_sleep_time_override > 0) {
			int64_t ns = NSEC_PER_SEC *
				(int64_t) msm_pm_sleep_time_override;
			msm_pm_set_max_sleep_time(ns);
			msm_pm_sleep_time_override = 0;
		}
#endif /* CONFIG_MSM_SLEEP_TIME_OVERRIDE */
		if (pm_sleep_ops.lowest_limits)
			rs_limits = pm_sleep_ops.lowest_limits(false,
			MSM_PM_SLEEP_MODE_POWER_COLLAPSE, &time_param, &power);

		if (rs_limits) {
			if (pm_sleep_ops.enter_sleep)
				ret = pm_sleep_ops.enter_sleep(
						msm_pm_max_sleep_time,
						rs_limits, false, true);
			if (!ret) {
				int collapsed = msm_pm_power_collapse(false);
				if (pm_sleep_ops.exit_sleep) {
					pm_sleep_ops.exit_sleep(rs_limits,
						false, true, collapsed);
				}
			}
		} else {
			pr_err("%s: cannot find the lowest power limit\n",
				__func__);
		}
		time = msm_pm_timer_exit_suspend(time, period);
		msm_pm_add_stat(MSM_PM_STAT_SUSPEND, time);
	} else if (allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE]) {
		if (MSM_PM_DEBUG_SUSPEND & msm_pm_debug_mask)
			pr_info("%s: standalone power collapse\n", __func__);
		msm_pm_power_collapse_standalone(false);
	} else if (allow[MSM_PM_SLEEP_MODE_RETENTION]) {
		if (MSM_PM_DEBUG_SUSPEND & msm_pm_debug_mask)
			pr_info("%s: retention\n", __func__);
		msm_pm_retention();
	} else if (allow[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT]) {
		if (MSM_PM_DEBUG_SUSPEND & msm_pm_debug_mask)
			pr_info("%s: swfi\n", __func__);
		msm_pm_swfi();
	}


enter_exit:
	if (MSM_PM_DEBUG_SUSPEND & msm_pm_debug_mask)
		pr_info("%s: return\n", __func__);

	return 0;
}

static struct platform_suspend_ops msm_pm_ops = {
	.enter = msm_pm_enter,
	.valid = suspend_valid_only_mem,
};

/******************************************************************************
 * Initialization routine
 *****************************************************************************/
void msm_pm_set_sleep_ops(struct msm_pm_sleep_ops *ops)
{
	if (ops)
		pm_sleep_ops = *ops;
}

void __init msm_pm_set_tz_retention_flag(unsigned int flag)
{
	msm_pm_retention_tz_call = flag;
}

static int __devinit msm_pc_debug_probe(struct platform_device *pdev)
{
	struct resource *res = NULL;
	int i ;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		goto fail;

	msm_pc_debug_counters_phys = res->start;
	WARN_ON(resource_size(res) < SZ_64);
	msm_pc_debug_counters = devm_ioremap_nocache(&pdev->dev, res->start,
					resource_size(res));

	if (!msm_pc_debug_counters)
		goto fail;

	for (i = 0; i < resource_size(res)/4; i++)
		__raw_writel(0, msm_pc_debug_counters + i * 4);
	return 0;
fail:
	msm_pc_debug_counters = 0;
	msm_pc_debug_counters_phys = 0;
	return -EFAULT;
}

static struct of_device_id msm_pc_debug_table[] = {
	{.compatible = "qcom,pc-cntr"},
	{},
};

static struct platform_driver msm_pc_counter_driver = {
	.probe = msm_pc_debug_probe,
	.driver = {
		.name = "pc-cntr",
		.owner = THIS_MODULE,
		.of_match_table = msm_pc_debug_table,
	},
};

static int __init msm_pm_init(void)
{
	pgd_t *pc_pgd;
	pmd_t *pmd;
	unsigned long pmdval;
	enum msm_pm_time_stats_id enable_stats[] = {
		MSM_PM_STAT_IDLE_WFI,
		MSM_PM_STAT_RETENTION,
		MSM_PM_STAT_IDLE_STANDALONE_POWER_COLLAPSE,
		MSM_PM_STAT_IDLE_POWER_COLLAPSE,
		MSM_PM_STAT_SUSPEND,
	};
	unsigned long exit_phys;

	/* Page table for cores to come back up safely. */

	pc_pgd = pgd_alloc(&init_mm);
	if (!pc_pgd)
		return -ENOMEM;

	exit_phys = virt_to_phys(msm_pm_collapse_exit);

	pmd = pmd_offset(pud_offset(pc_pgd + pgd_index(exit_phys),exit_phys),
					exit_phys);
	pmdval = (exit_phys & PGDIR_MASK) |
		     PMD_TYPE_SECT | PMD_SECT_AP_WRITE;
	pmd[0] = __pmd(pmdval);
	pmd[1] = __pmd(pmdval + (1 << (PGDIR_SHIFT - 1)));

	msm_saved_state_phys =
		allocate_contiguous_ebi_nomap(CPU_SAVED_STATE_SIZE *
					      num_possible_cpus(), 4);
	if (!msm_saved_state_phys)
		return -ENOMEM;
	msm_saved_state = ioremap_nocache(msm_saved_state_phys,
					  CPU_SAVED_STATE_SIZE *
					  num_possible_cpus());
	if (!msm_saved_state)
		return -ENOMEM;

	/* It is remotely possible that the code in msm_pm_collapse_exit()
	 * which turns on the MMU with this mapping is in the
	 * next even-numbered megabyte beyond the
	 * start of msm_pm_collapse_exit().
	 * Map this megabyte in as well.
	 */
	pmd[2] = __pmd(pmdval + (2 << (PGDIR_SHIFT - 1)));
	flush_pmd_entry(pmd);
	msm_pm_pc_pgd = virt_to_phys(pc_pgd);
	clean_caches((unsigned long)&msm_pm_pc_pgd, sizeof(msm_pm_pc_pgd),
		     virt_to_phys(&msm_pm_pc_pgd));

	msm_pm_mode_sysfs_add();
	msm_pm_add_stats(enable_stats, ARRAY_SIZE(enable_stats));

	suspend_set_ops(&msm_pm_ops);
	msm_pm_target_init();
	hrtimer_init(&pm_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	msm_cpuidle_init();
	platform_driver_register(&msm_pc_counter_driver);

	return 0;
}

late_initcall(msm_pm_init);

static void __devinit msm_pm_set_flush_fn(uint32_t pc_mode)
{
	msm_pm_disable_l2_fn = NULL;
	msm_pm_enable_l2_fn = NULL;
	msm_pm_flush_l2_fn = outer_flush_all;

	if (pc_mode == MSM_PM_PC_NOTZ_L2_EXT) {
		msm_pm_disable_l2_fn = outer_disable;
		msm_pm_enable_l2_fn = outer_resume;
	}
}

static int __devinit msm_pm_8x60_probe(struct platform_device *pdev)
{
	char *key = NULL;
	uint32_t val = 0;
	int ret = 0;

	if (!pdev->dev.of_node) {
		struct msm_pm_init_data_type *d = pdev->dev.platform_data;

		if (!d)
			goto pm_8x60_probe_done;

		msm_pm_init_data.pc_mode = d->pc_mode;
		msm_pm_set_flush_fn(msm_pm_init_data.pc_mode);
		msm_pm_init_data.use_sync_timer = d->use_sync_timer;
	} else {
		key = "qcom,pc-mode";
		ret = of_property_read_u32(pdev->dev.of_node, key, &val);

		if (ret) {
			pr_debug("%s: Cannot read %s,defaulting to 0",
					__func__, key);
			val = MSM_PM_PC_TZ_L2_INT;
			ret = 0;
		}

		msm_pm_init_data.pc_mode = val;
		msm_pm_set_flush_fn(msm_pm_init_data.pc_mode);

		key = "qcom,use-sync-timer";
		msm_pm_init_data.use_sync_timer =
			of_property_read_bool(pdev->dev.of_node, key);
	}

pm_8x60_probe_done:
	return ret;
}

static struct of_device_id msm_pm_8x60_table[] = {
		{.compatible = "qcom,pm-8x60"},
		{},
};

static struct platform_driver msm_pm_8x60_driver = {
		.probe = msm_pm_8x60_probe,
		.driver = {
			.name = "pm-8x60",
			.owner = THIS_MODULE,
			.of_match_table = msm_pm_8x60_table,
		},
};

static int __init msm_pm_8x60_init(void)
{
	return platform_driver_register(&msm_pm_8x60_driver);
}
module_init(msm_pm_8x60_init);
