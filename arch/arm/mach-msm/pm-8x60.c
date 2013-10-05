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

#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
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
#include <linux/irqchip/arm-gic.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/regulator/krait-regulator.h>
#include <linux/cpu.h>
#include <mach/socinfo.h>
#include <mach/scm.h>
#include <mach/socinfo.h>
#define CREATE_TRACE_POINTS
#include <mach/trace_msm_low_power.h>
#include <mach/msm-krait-l2-accessors.h>
#include <mach/msm_bus.h>
#include <mach/mpm.h>
#include <asm/cacheflush.h>
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
#include <linux/cpu_pm.h>

#define SCM_L2_RETENTION	(0x2)
#define SCM_CMD_TERMINATE_PC	(0x2)

#define GET_CPU_OF_ATTR(attr) \
	(container_of(attr, struct msm_pm_kobj_attribute, ka)->cpu)

#define SCLK_HZ (32768)

#define NUM_OF_COUNTERS 3
#define MAX_BUF_SIZE  512

static int msm_pm_debug_mask = 1;
module_param_named(
	debug_mask, msm_pm_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP
);

static int msm_pm_sleep_time_override;
module_param_named(sleep_time_override,
	msm_pm_sleep_time_override, int, S_IRUGO | S_IWUSR | S_IWGRP);

static bool use_acpuclk_apis;

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

enum {
	MSM_PM_MODE_ATTR_SUSPEND,
	MSM_PM_MODE_ATTR_IDLE,
	MSM_PM_MODE_ATTR_NR,
};

static char *msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_NR] = {
	[MSM_PM_MODE_ATTR_SUSPEND] = "suspend_enabled",
	[MSM_PM_MODE_ATTR_IDLE] = "idle_enabled",
};

struct msm_pm_kobj_attribute {
	unsigned int cpu;
	struct kobj_attribute ka;
};

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

static struct hrtimer pm_hrtimer;
static struct msm_pm_sleep_ops pm_sleep_ops;
static bool msm_pm_ldo_retention_enabled = true;
static bool msm_pm_use_sync_timer;
static struct msm_pm_cp15_save_data cp15_data;
static bool msm_pm_retention_calls_tz;
static bool msm_no_ramp_down_pc;
static struct msm_pm_sleep_status_data *msm_pm_slp_sts;
static bool msm_pm_pc_reset_timer;

DEFINE_PER_CPU(struct clk *, cpu_clks);
static struct clk *l2_clk;

static int msm_pm_get_pc_mode(struct device_node *node,
		const char *key, uint32_t *pc_mode_val)
{
	struct pc_mode_of {
		uint32_t mode;
		char *mode_name;
	};
	int i;
	struct pc_mode_of pc_modes[] = {
				{MSM_PM_PC_TZ_L2_INT, "tz_l2_int"},
				{MSM_PM_PC_NOTZ_L2_EXT, "no_tz_l2_ext"},
				{MSM_PM_PC_TZ_L2_EXT , "tz_l2_ext"} };
	int ret;
	const char *pc_mode_str;

	ret = of_property_read_string(node, key, &pc_mode_str);
	if (ret) {
		pr_debug("%s: Cannot read %s,defaulting to 0", __func__, key);
		pc_mode_val = MSM_PM_PC_TZ_L2_INT;
		ret = 0;
	} else {
		ret = -EINVAL;
		for (i = 0; i < ARRAY_SIZE(pc_modes); i++) {
			if (!strncmp(pc_mode_str, pc_modes[i].mode_name,
				strlen(pc_modes[i].mode_name))) {
				*pc_mode_val = pc_modes[i].mode;
				ret = 0;
				break;
			}
		}
	}
	return ret;
}

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

static int msm_pm_mode_sysfs_add_cpu(
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

int msm_pm_mode_sysfs_add(void)
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
}

static void msm_pm_config_hw_before_retention(void)
{
	return;
}

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
		cp15_data.active_vdd = msm_spm_get_vdd(0);
		for (i = 0; i < cp15_data.reg_saved_state_size; i++)
			cp15_data.reg_val[i] =
				get_l2_indirect_reg(
					cp15_data.reg_data[i]);
		msm_spm_set_vdd(0, cp15_data.qsb_pc_vdd);
	}
}

static void msm_pm_restore_cpu_reg(void)
{
	int i;

	/* Only on core0 */
	if (smp_processor_id())
		return;

	if (msm_pm_get_l2_flush_flag() == 1) {
		for (i = 0; i < cp15_data.reg_saved_state_size; i++)
			set_l2_indirect_reg(
					cp15_data.reg_data[i],
					cp15_data.reg_val[i]);
		msm_spm_set_vdd(0, cp15_data.active_vdd);
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

	if (msm_pm_retention_calls_tz)
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
	bool save_cpu_regs = !cpu || from_idle;

	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: notify_rpm %d\n",
			cpu, __func__, (int) notify_rpm);

	if (from_idle == true)
		cpu_pm_enter();

	ret = msm_spm_set_low_power_mode(
			MSM_SPM_MODE_POWER_COLLAPSE, notify_rpm);
	WARN_ON(ret);

	entry = save_cpu_regs ?  msm_pm_collapse_exit : msm_secondary_startup;

	msm_pm_boot_config_before_pc(cpu, virt_to_phys(entry));

	if (MSM_PM_DEBUG_RESET_VECTOR & msm_pm_debug_mask)
		pr_info("CPU%u: %s: program vector to %p\n",
			cpu, __func__, entry);
	if (from_idle && msm_pm_pc_reset_timer)
		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &cpu);

	collapsed = save_cpu_regs ? msm_pm_collapse() : msm_pm_pc_hotplug();

	if (from_idle && msm_pm_pc_reset_timer)
		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &cpu);

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

	if (from_idle == true)
		cpu_pm_exit();

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

static int ramp_down_last_cpu(int cpu)
{
	struct clk *cpu_clk = per_cpu(cpu_clks, cpu);
	int ret = 0;

	if (use_acpuclk_apis) {
		ret = acpuclk_power_collapse();
		if (MSM_PM_DEBUG_CLOCK & msm_pm_debug_mask)
			pr_info("CPU%u: %s: change clk rate(old rate = %d)\n",
					cpu, __func__, ret);
	} else {
		clk_disable(cpu_clk);
		clk_disable(l2_clk);
	}
	return ret;
}

static int ramp_up_first_cpu(int cpu, int saved_rate)
{
	struct clk *cpu_clk = per_cpu(cpu_clks, cpu);
	int rc = 0;

	if (MSM_PM_DEBUG_CLOCK & msm_pm_debug_mask)
		pr_info("CPU%u: %s: restore clock rate\n",
				cpu, __func__);

	if (use_acpuclk_apis) {
		rc = acpuclk_set_rate(cpu, saved_rate, SETRATE_PC);
		if (rc)
			pr_err("CPU:%u: Error restoring cpu clk\n", cpu);
	} else {
		if (l2_clk) {
			rc = clk_enable(l2_clk);
			if (rc)
				pr_err("%s(): Error restoring l2 clk\n",
						__func__);
		}

		if (cpu_clk) {
			int ret = clk_enable(cpu_clk);

			if (ret) {
				pr_err("%s(): Error restoring cpu clk\n",
						__func__);
				return ret;
			}
		}
	}

	return rc;
}

static bool msm_pm_power_collapse(bool from_idle)
{
	unsigned int cpu = smp_processor_id();
	unsigned long saved_acpuclk_rate = 0;
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

	if (cpu_online(cpu) && !msm_no_ramp_down_pc)
		saved_acpuclk_rate = ramp_down_last_cpu(cpu);

	if (cp15_data.save_cp15)
		msm_pm_save_cpu_reg();

	collapsed = msm_pm_spm_power_collapse(cpu, from_idle, true);

	if (cp15_data.save_cp15)
		msm_pm_restore_cpu_reg();

	if (cpu_online(cpu) && !msm_no_ramp_down_pc)
		ramp_up_first_cpu(cpu, saved_acpuclk_rate);
	else
		__WARN();

	avs_set_avsdscr(avsdscr);
	avs_set_avscsr(avscsr);
	msm_pm_config_hw_after_power_up();
	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: post power up\n", cpu, __func__);

	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: return\n", cpu, __func__);
	return collapsed;
}

static int64_t msm_pm_timer_enter_idle(void)
{
	if (msm_pm_use_sync_timer)
		return ktime_to_ns(tick_nohz_get_sleep_length());

	return msm_timer_enter_idle();
}

static void msm_pm_timer_exit_idle(bool timer_halted)
{
	if (msm_pm_use_sync_timer)
		return;

	msm_timer_exit_idle((int) timer_halted);
}

static int64_t msm_pm_timer_enter_suspend(int64_t *period)
{
	int64_t time = 0;

	if (msm_pm_use_sync_timer) {
		struct timespec ts;
		getnstimeofday(&ts);
		return timespec_to_ns(&ts);
	}

	time = msm_timer_get_sclk_time(period);
	if (!time)
		pr_err("%s: Unable to read sclk.\n", __func__);

	return time;
}

static int64_t msm_pm_timer_exit_suspend(int64_t time, int64_t period)
{
	if (msm_pm_use_sync_timer) {
		struct timespec ts;
		getnstimeofday(&ts);

		return timespec_to_ns(&ts) - time;
	}

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
	hrtimer_start(&pm_hrtimer, modified_ktime, HRTIMER_MODE_REL);
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
		void **msm_pm_idle_rs_limits,
		const struct msm_cpuidle_state *states)
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
		enum msm_pm_sleep_mode mode;
		bool allow;
		uint32_t power;
		int idx;
		void *rs_limits = NULL;

		mode = states[i].mode_nr;
		idx = MSM_PM_MODE(dev->cpu, mode);

		allow = msm_pm_sleep_modes[idx].idle_enabled &&
				msm_pm_sleep_modes[idx].idle_supported;

		switch (mode) {
		case MSM_PM_SLEEP_MODE_POWER_COLLAPSE:
			if (num_online_cpus() > 1)
				allow = false;
			break;
		case MSM_PM_SLEEP_MODE_RETENTION:
			/*
			 * The Krait BHS regulator doesn't have enough head
			 * room to drive the retention voltage on LDO and so
			 * has disabled retention
			 */
			if (!msm_pm_ldo_retention_enabled)
				allow = false;

			if (msm_pm_retention_calls_tz && num_online_cpus() > 1)
				allow = false;
			break;
		case MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE:
		case MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT:
			break;
		default:
			allow = false;
			break;
		}

		if (!allow)
			continue;

		if (pm_sleep_ops.lowest_limits)
			rs_limits = pm_sleep_ops.lowest_limits(true,
					mode, &time_param, &power);

		if (MSM_PM_DEBUG_IDLE & msm_pm_debug_mask)
			pr_info("CPU%u:%s:%s, latency %uus, slp %uus, lim %p\n",
					dev->cpu, __func__, state->desc,
					time_param.latency_us,
					time_param.sleep_us, rs_limits);
		if (!rs_limits)
			continue;

		if (power < power_usage) {
			power_usage = power;
			modified_time_us = time_param.modified_time_us;
			ret = mode;
			*msm_pm_idle_rs_limits = rs_limits;
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
	struct cpuidle_driver *drv, int index,
	const struct msm_cpuidle_state *states)
{
	int64_t time;
	bool collapsed = 1;
	int exit_stat = -1;
	enum msm_pm_sleep_mode sleep_mode;
	void *msm_pm_idle_rs_limits = NULL;
	uint32_t sleep_delay = 1;
	int ret = -ENODEV;
	int notify_rpm = false;
	bool timer_halted = false;

	sleep_mode = msm_pm_idle_prepare(dev, drv, index,
		&msm_pm_idle_rs_limits, states);

	if (!msm_pm_idle_rs_limits) {
		sleep_mode = MSM_PM_SLEEP_MODE_NOT_SELECTED;
		goto cpuidle_enter_bail;
	}

	if (MSM_PM_DEBUG_IDLE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: mode %d\n",
			smp_processor_id(), __func__, sleep_mode);

	time = ktime_to_ns(ktime_get());

	if (sleep_mode == MSM_PM_SLEEP_MODE_POWER_COLLAPSE) {
		int64_t ns = msm_pm_timer_enter_idle();
		notify_rpm = true;
		do_div(ns, NSEC_PER_SEC / SCLK_HZ);
		sleep_delay = (uint32_t)ns;

		if (sleep_delay == 0) /* 0 would mean infinite time */
			sleep_delay = 1;
	}

	if (pm_sleep_ops.enter_sleep)
		ret = pm_sleep_ops.enter_sleep(sleep_delay,
			msm_pm_idle_rs_limits, true, notify_rpm);
	if (ret)
		goto cpuidle_enter_bail;

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
		if (collapsed)
			exit_stat = MSM_PM_STAT_IDLE_STANDALONE_POWER_COLLAPSE;
		else
			exit_stat
			    = MSM_PM_STAT_IDLE_FAILED_STANDALONE_POWER_COLLAPSE;
		break;

	case MSM_PM_SLEEP_MODE_POWER_COLLAPSE:
		if (MSM_PM_DEBUG_IDLE_CLK & msm_pm_debug_mask)
			clock_debug_print_enabled();

		collapsed = msm_pm_power_collapse(true);
		timer_halted = true;

		if (collapsed)
			exit_stat = MSM_PM_STAT_IDLE_POWER_COLLAPSE;
		else
			exit_stat = MSM_PM_STAT_IDLE_FAILED_POWER_COLLAPSE;

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
		pm_sleep_ops.exit_sleep(msm_pm_idle_rs_limits, true,
				notify_rpm, collapsed);

	time = ktime_to_ns(ktime_get()) - time;
	msm_pm_ftrace_lpm_exit(smp_processor_id(), sleep_mode, collapsed);
	if (exit_stat >= 0)
		msm_pm_add_stat(exit_stat, time);
	do_div(time, 1000);
	dev->last_residency = (int) time;
	return sleep_mode;

cpuidle_enter_bail:
	dev->last_residency = 0;
	if (sleep_mode == MSM_PM_SLEEP_MODE_POWER_COLLAPSE)
		msm_pm_timer_exit_idle(timer_halted);
	sleep_mode = MSM_PM_SLEEP_MODE_NOT_SELECTED;
	return sleep_mode;
}

int msm_pm_wait_cpu_shutdown(unsigned int cpu)
{
	int timeout = 0;

	if (!msm_pm_slp_sts)
		return 0;
	if (!msm_pm_slp_sts[cpu].base_addr)
		return 0;
	while (1) {
		/*
		 * Check for the SPM of the core being hotplugged to set
		 * its sleep state.The SPM sleep state indicates that the
		 * core has been power collapsed.
		 */
		int acc_sts = __raw_readl(msm_pm_slp_sts[cpu].base_addr);

		if (acc_sts & msm_pm_slp_sts[cpu].mask)
			return 0;
		udelay(100);
		WARN(++timeout == 20, "CPU%u didn't collape within 2ms\n",
					cpu);
	}

	return -EBUSY;
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
	if (enable == msm_pm_ldo_retention_enabled)
		return;

	msm_pm_ldo_retention_enabled = enable;
	/*
	 * If retention is being disabled, wakeup all online core to ensure
	 * that it isn't executing retention. Offlined cores need not be woken
	 * up as they enter the deepest sleep mode, namely RPM assited power
	 * collapse
	 */
	if (!enable) {
		preempt_disable();
		smp_call_function_many(cpu_online_mask,
				msm_pm_ack_retention_disable,
				NULL, true);
		preempt_enable();


	}
}
EXPORT_SYMBOL(msm_pm_enable_retention);

static int64_t suspend_time, suspend_period;
static int collapsed;
static int suspend_power_collapsed;

static int msm_pm_enter(suspend_state_t state)
{
	bool allow[MSM_PM_SLEEP_MODE_NR];
	int i;
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
		uint32_t msm_pm_max_sleep_time = 0;

		if (MSM_PM_DEBUG_SUSPEND & msm_pm_debug_mask)
			pr_info("%s: power collapse\n", __func__);

		clock_debug_print_enabled();

		if (msm_pm_sleep_time_override > 0) {
			int64_t ns = NSEC_PER_SEC *
				(int64_t) msm_pm_sleep_time_override;
			do_div(ns, NSEC_PER_SEC / SCLK_HZ);
			msm_pm_max_sleep_time = (uint32_t) ns;
		}

		if (pm_sleep_ops.lowest_limits)
			rs_limits = pm_sleep_ops.lowest_limits(false,
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE, &time_param, &power);

		if (rs_limits) {
			if (pm_sleep_ops.enter_sleep)
				ret = pm_sleep_ops.enter_sleep(
						msm_pm_max_sleep_time,
						rs_limits, false, true);
			if (!ret) {
				collapsed = msm_pm_power_collapse(false);
				if (pm_sleep_ops.exit_sleep) {
					pm_sleep_ops.exit_sleep(rs_limits,
						false, true, collapsed);
				}
			}
		} else {
			pr_err("%s: cannot find the lowest power limit\n",
				__func__);
		}
		suspend_power_collapsed = true;
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

void msm_pm_set_sleep_ops(struct msm_pm_sleep_ops *ops)
{
	if (ops)
		pm_sleep_ops = *ops;
}

static int msm_suspend_prepare(void)
{
	suspend_time = msm_pm_timer_enter_suspend(&suspend_period);
	msm_mpm_suspend_prepare();
	return 0;
}

static void msm_suspend_wake(void)
{
	msm_mpm_suspend_wake();
	if (suspend_power_collapsed) {
		suspend_time = msm_pm_timer_exit_suspend(suspend_time,
				suspend_period);
		if (collapsed)
			msm_pm_add_stat(MSM_PM_STAT_SUSPEND, suspend_time);
		else
			msm_pm_add_stat(MSM_PM_STAT_FAILED_SUSPEND,
					suspend_time);
		suspend_power_collapsed = false;
	}
}

static const struct platform_suspend_ops msm_pm_ops = {
	.enter = msm_pm_enter,
	.valid = suspend_valid_only_mem,
	.prepare_late = msm_suspend_prepare,
	.wake = msm_suspend_wake,
};

static int msm_pm_snoc_client_probe(struct platform_device *pdev)
{
	int rc = 0;
	static struct msm_bus_scale_pdata *msm_pm_bus_pdata;
	static uint32_t msm_pm_bus_client;

	msm_pm_bus_pdata = msm_bus_cl_get_pdata(pdev);

	if (msm_pm_bus_pdata) {
		msm_pm_bus_client =
			msm_bus_scale_register_client(msm_pm_bus_pdata);

		if (!msm_pm_bus_client) {
			pr_err("%s: Failed to register SNOC client",
							__func__);
			rc = -ENXIO;
			goto snoc_cl_probe_done;
		}

		rc = msm_bus_scale_client_update_request(msm_pm_bus_client, 1);

		if (rc)
			pr_err("%s: Error setting bus rate", __func__);
	}

snoc_cl_probe_done:
	return rc;
}

static int msm_cpu_status_probe(struct platform_device *pdev)
{
	struct msm_pm_sleep_status_data *pdata;
	char *key;
	u32 cpu;

	if (!pdev)
		return -EFAULT;

	msm_pm_slp_sts =
		kzalloc(sizeof(*msm_pm_slp_sts) * num_possible_cpus(),
				GFP_KERNEL);

	if (!msm_pm_slp_sts)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		struct resource *res;
		u32 offset;
		int rc;
		u32 mask;

		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res)
			goto fail_free_mem;

		key = "qcom,cpu-alias-addr";
		rc = of_property_read_u32(pdev->dev.of_node, key, &offset);

		if (rc)
			goto fail_free_mem;

		key = "qcom,sleep-status-mask";
		rc = of_property_read_u32(pdev->dev.of_node, key,
					&mask);
		if (rc)
			goto fail_free_mem;

		for_each_possible_cpu(cpu) {
			msm_pm_slp_sts[cpu].base_addr =
				ioremap(res->start + cpu * offset,
					resource_size(res));
			msm_pm_slp_sts[cpu].mask = mask;

			if (!msm_pm_slp_sts[cpu].base_addr)
				goto failed_of_node;
		}

	} else {
		pdata = pdev->dev.platform_data;
		if (!pdev->dev.platform_data)
			goto fail_free_mem;

		for_each_possible_cpu(cpu) {
			msm_pm_slp_sts[cpu].base_addr =
				pdata->base_addr + cpu * pdata->cpu_offset;
			msm_pm_slp_sts[cpu].mask = pdata->mask;
		}
	}

	return 0;

failed_of_node:
	pr_info("%s(): Failed to key=%s\n", __func__, key);
	for_each_possible_cpu(cpu) {
		if (msm_pm_slp_sts[cpu].base_addr)
			iounmap(msm_pm_slp_sts[cpu].base_addr);
	}
fail_free_mem:
	kfree(msm_pm_slp_sts);
	return -EINVAL;

};

static struct of_device_id msm_slp_sts_match_tbl[] = {
	{.compatible = "qcom,cpu-sleep-status"},
	{},
};

static struct platform_driver msm_cpu_status_driver = {
	.probe = msm_cpu_status_probe,
	.driver = {
		.name = "cpu_slp_status",
		.owner = THIS_MODULE,
		.of_match_table = msm_slp_sts_match_tbl,
	},
};

static struct of_device_id msm_snoc_clnt_match_tbl[] = {
	{.compatible = "qcom,pm-snoc-client"},
	{},
};

static struct platform_driver msm_cpu_pm_snoc_client_driver = {
	.probe = msm_pm_snoc_client_probe,
	.driver = {
		.name = "pm_snoc_client",
		.owner = THIS_MODULE,
		.of_match_table = msm_snoc_clnt_match_tbl,
	},
};

#ifdef CONFIG_ARM_LPAE
static int msm_pm_idmap_add_pmd(pud_t *pud, unsigned long addr,
				unsigned long end, unsigned long prot)
{
	pmd_t *pmd;
	unsigned long next;

	if (pud_none_or_clear_bad(pud) || (pud_val(*pud) & L_PGD_SWAPPER)) {
		pmd = pmd_alloc_one(&init_mm, addr);
		if (!pmd)
			return -ENOMEM;

		pud_populate(&init_mm, pud, pmd);
		pmd += pmd_index(addr);
	} else {
		pmd = pmd_offset(pud, addr);
	}

	do {
		next = pmd_addr_end(addr, end);
		*pmd = __pmd((addr & PMD_MASK) | prot);
		flush_pmd_entry(pmd);
	} while (pmd++, addr = next, addr != end);

	return 0;
}
#else   /* !CONFIG_ARM_LPAE */
static int msm_pm_idmap_add_pmd(pud_t *pud, unsigned long addr,
				unsigned long end, unsigned long prot)
{
	pmd_t *pmd = pmd_offset(pud, addr);

	addr = (addr & PMD_MASK) | prot;
	pmd[0] = __pmd(addr);
	addr += SECTION_SIZE;
	pmd[1] = __pmd(addr);
	flush_pmd_entry(pmd);

	return 0;
}
#endif  /* CONFIG_ARM_LPAE */

static int msm_pm_idmap_add_pud(pgd_t *pgd, unsigned long addr,
					unsigned long end,
					unsigned long prot)
{
	pud_t *pud = pud_offset(pgd, addr);
	unsigned long next;
	int ret;

	do {
		next = pud_addr_end(addr, end);
		ret = msm_pm_idmap_add_pmd(pud, addr, next, prot);
		if (ret)
			return ret;
	} while (pud++, addr = next, addr != end);

	return 0;
}

static int msm_pm_add_idmap(pgd_t *pgd, unsigned long addr,
						unsigned long end,
						unsigned long prot)
{
	unsigned long next;
	int ret;

	pgd += pgd_index(addr);
	do {
		next = pgd_addr_end(addr, end);
		ret = msm_pm_idmap_add_pud(pgd, addr, next, prot);
		if (ret)
			return ret;
	} while (pgd++, addr = next, addr != end);

	return 0;
}

static int msm_pm_setup_pagetable(void)
{
	pgd_t *pc_pgd;
	unsigned long exit_phys;
	unsigned long end;
	int ret;

	/* Page table for cores to come back up safely. */
	pc_pgd = pgd_alloc(&init_mm);
	if (!pc_pgd)
		return -ENOMEM;

	exit_phys = virt_to_phys(msm_pm_collapse_exit);

	/*
	 * Make the (hopefully) reasonable assumption that the code size of
	 * msm_pm_collapse_exit won't be more than a section in size
	 */
	end = exit_phys + SECTION_SIZE;

	ret = msm_pm_add_idmap(pc_pgd, exit_phys, end,
			PMD_TYPE_SECT | PMD_SECT_AP_WRITE | PMD_SECT_AF);

	if (ret)
		return ret;

	msm_pm_pc_pgd = virt_to_phys(pc_pgd);
	clean_caches((unsigned long)&msm_pm_pc_pgd, sizeof(msm_pm_pc_pgd),
		     virt_to_phys(&msm_pm_pc_pgd));

	return 0;
}

static int __init msm_pm_setup_saved_state(void)
{
	int ret;
	dma_addr_t temp_phys;

	ret = msm_pm_setup_pagetable();
	if (ret)
		return ret;

	msm_saved_state = dma_zalloc_coherent(NULL, CPU_SAVED_STATE_SIZE *
						num_possible_cpus(),
						&temp_phys, 0);

	if (!msm_saved_state)
		return -ENOMEM;

	/*
	 * Explicitly cast here since msm_saved_state_phys is defined
	 * in assembly and we want to avoid any kind of truncation
	 * or endian problems.
	 */
	msm_saved_state_phys = (unsigned long)temp_phys;

	return 0;
}
arch_initcall(msm_pm_setup_saved_state);

static void setup_broadcast_timer(void *arg)
{
	int cpu = smp_processor_id();

	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ON, &cpu);
}

static int setup_broadcast_cpuhp_notify(struct notifier_block *n,
		unsigned long action, void *hcpu)
{
	int cpu = (unsigned long)hcpu;

	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_ONLINE:
		smp_call_function_single(cpu, setup_broadcast_timer, NULL, 1);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block setup_broadcast_notifier = {
	.notifier_call = setup_broadcast_cpuhp_notify,
};

static int msm_pm_init(void)
{
	enum msm_pm_time_stats_id enable_stats[] = {
		MSM_PM_STAT_IDLE_WFI,
		MSM_PM_STAT_RETENTION,
		MSM_PM_STAT_IDLE_STANDALONE_POWER_COLLAPSE,
		MSM_PM_STAT_IDLE_POWER_COLLAPSE,
		MSM_PM_STAT_SUSPEND,
	};
	msm_pm_mode_sysfs_add();
	msm_pm_add_stats(enable_stats, ARRAY_SIZE(enable_stats));
	suspend_set_ops(&msm_pm_ops);
	hrtimer_init(&pm_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	msm_cpuidle_init();

	if (msm_pm_pc_reset_timer) {
		on_each_cpu(setup_broadcast_timer, NULL, 1);
		register_cpu_notifier(&setup_broadcast_notifier);
	}

	return 0;
}

static void msm_pm_set_flush_fn(uint32_t pc_mode)
{
	msm_pm_disable_l2_fn = NULL;
	msm_pm_enable_l2_fn = NULL;
	msm_pm_flush_l2_fn = outer_flush_all;

	if (pc_mode == MSM_PM_PC_NOTZ_L2_EXT) {
		msm_pm_disable_l2_fn = outer_disable;
		msm_pm_enable_l2_fn = outer_resume;
	}
}

struct msm_pc_debug_counters_buffer {
	void __iomem *reg;
	u32 len;
	char buf[MAX_BUF_SIZE];
};

static inline u32 msm_pc_debug_counters_read_register(
		void __iomem *reg, int index , int offset)
{
	return readl_relaxed(reg + (index * 4 + offset) * 4);
}

static char *counter_name[] = {
		"PC Entry Counter",
		"Warmboot Entry Counter",
		"PC Bailout Counter"
};

static int msm_pc_debug_counters_copy(
		struct msm_pc_debug_counters_buffer *data)
{
	int j;
	u32 stat;
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		data->len += scnprintf(data->buf + data->len,
				sizeof(data->buf)-data->len,
				"CPU%d\n", cpu);

			for (j = 0; j < NUM_OF_COUNTERS; j++) {
				stat = msm_pc_debug_counters_read_register(
						data->reg, cpu, j);
				data->len += scnprintf(data->buf + data->len,
					sizeof(data->buf)-data->len,
					"\t%s : %d\n", counter_name[j],
					stat);
		}

	}

	return data->len;
}

static int msm_pc_debug_counters_file_read(struct file *file,
		char __user *bufu, size_t count, loff_t *ppos)
{
	struct msm_pc_debug_counters_buffer *data;

	data = file->private_data;

	if (!data)
		return -EINVAL;

	if (!bufu)
		return -EINVAL;

	if (!access_ok(VERIFY_WRITE, bufu, count))
		return -EFAULT;

	if (*ppos >= data->len && data->len == 0)
		data->len = msm_pc_debug_counters_copy(data);

	return simple_read_from_buffer(bufu, count, ppos,
			data->buf, data->len);
}

static int msm_pc_debug_counters_file_open(struct inode *inode,
		struct file *file)
{
	struct msm_pc_debug_counters_buffer *buf;
	void __iomem *msm_pc_debug_counters_reg;

	msm_pc_debug_counters_reg = inode->i_private;

	if (!msm_pc_debug_counters_reg)
		return -EINVAL;

	file->private_data = kzalloc(
		sizeof(struct msm_pc_debug_counters_buffer), GFP_KERNEL);

	if (!file->private_data) {
		pr_err("%s: ERROR kmalloc failed to allocate %d bytes\n",
		__func__, sizeof(struct msm_pc_debug_counters_buffer));

		return -ENOMEM;
	}

	buf = file->private_data;
	buf->reg = msm_pc_debug_counters_reg;

	return 0;
}

static int msm_pc_debug_counters_file_close(struct inode *inode,
		struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static const struct file_operations msm_pc_debug_counters_fops = {
	.open = msm_pc_debug_counters_file_open,
	.read = msm_pc_debug_counters_file_read,
	.release = msm_pc_debug_counters_file_close,
	.llseek = no_llseek,
};

static int msm_pm_clk_init(struct platform_device *pdev)
{
	bool synced_clocks;
	u32 cpu;
	char clk_name[] = "cpu??_clk";
	bool cpu_as_clocks;
	char *key;

	key = "qcom,cpus-as-clocks";
	cpu_as_clocks = of_property_read_bool(pdev->dev.of_node, key);

	if (!cpu_as_clocks) {
		use_acpuclk_apis = true;
		return 0;
	}

	key = "qcom,synced-clocks";
	synced_clocks = of_property_read_bool(pdev->dev.of_node, key);

	for_each_possible_cpu(cpu) {
		struct clk *clk;
		snprintf(clk_name, sizeof(clk_name), "cpu%d_clk", cpu);
		clk = devm_clk_get(&pdev->dev, clk_name);
		if (IS_ERR(clk)) {
			if (cpu && synced_clocks)
				return 0;
			else
				return PTR_ERR(clk);
		}
		per_cpu(cpu_clks, cpu) = clk;
	}

	if (synced_clocks)
		return 0;

	l2_clk = devm_clk_get(&pdev->dev, "l2_clk");

	return PTR_RET(l2_clk);
}

static int msm_pm_8x60_probe(struct platform_device *pdev)
{
	char *key = NULL;
	struct dentry *dent = NULL;
	struct resource *res = NULL;
	int i ;
	struct msm_pm_init_data_type pdata_local;
	int ret = 0;

	memset(&pdata_local, 0, sizeof(struct msm_pm_init_data_type));

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		msm_pc_debug_counters_phys = res->start;
		WARN_ON(resource_size(res) < SZ_64);
		msm_pc_debug_counters = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
		if (msm_pc_debug_counters)
			for (i = 0; i < resource_size(res)/4; i++)
				__raw_writel(0, msm_pc_debug_counters + i * 4);

	}

	if (!msm_pc_debug_counters) {
		msm_pc_debug_counters = 0;
		msm_pc_debug_counters_phys = 0;
	} else {
		dent = debugfs_create_file("pc_debug_counter", S_IRUGO, NULL,
				msm_pc_debug_counters,
				&msm_pc_debug_counters_fops);
		if (!dent)
			pr_err("%s: ERROR debugfs_create_file failed\n",
					__func__);
	}

	if (!pdev->dev.of_node) {
		struct msm_pm_init_data_type *d = pdev->dev.platform_data;

		if (!d)
			goto pm_8x60_probe_done;

		memcpy(&pdata_local, d, sizeof(struct msm_pm_init_data_type));

	} else {
		ret = msm_pm_clk_init(pdev);
		if (ret) {
			pr_info("msm_pm_clk_init returned error\n");
			return ret;
		}

		key = "qcom,pc-mode";
		ret = msm_pm_get_pc_mode(pdev->dev.of_node,
				key,
				&pdata_local.pc_mode);
		if (ret) {
			pr_debug("%s: Error reading key %s",
					__func__, key);
			return -EINVAL;
		}

		key = "qcom,use-sync-timer";
		pdata_local.use_sync_timer =
			of_property_read_bool(pdev->dev.of_node, key);

		key = "qcom,saw-turns-off-pll";
		msm_no_ramp_down_pc = of_property_read_bool(pdev->dev.of_node,
					key);

		key = "qcom,pc-resets-timer";
		msm_pm_pc_reset_timer = of_property_read_bool(
				pdev->dev.of_node, key);
	}

	if (pdata_local.cp15_data.reg_data &&
		pdata_local.cp15_data.reg_saved_state_size > 0) {
		cp15_data.reg_data = kzalloc(sizeof(uint32_t) *
				pdata_local.cp15_data.reg_saved_state_size,
				GFP_KERNEL);
		if (!cp15_data.reg_data)
			return -ENOMEM;

		cp15_data.reg_val = kzalloc(sizeof(uint32_t) *
				pdata_local.cp15_data.reg_saved_state_size,
				GFP_KERNEL);
		if (cp15_data.reg_val)
			return -ENOMEM;

		memcpy(cp15_data.reg_data, pdata_local.cp15_data.reg_data,
			pdata_local.cp15_data.reg_saved_state_size *
			sizeof(uint32_t));
	}

	msm_pm_set_flush_fn(pdata_local.pc_mode);
	msm_pm_use_sync_timer = pdata_local.use_sync_timer;
	msm_pm_retention_calls_tz = pdata_local.retention_calls_tz;

pm_8x60_probe_done:
	msm_pm_init();
	if (pdev->dev.of_node)
		of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);

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
	int rc;

	rc = platform_driver_register(&msm_cpu_pm_snoc_client_driver);

	if (rc) {
		pr_err("%s(): failed to register driver %s\n", __func__,
				msm_cpu_pm_snoc_client_driver.driver.name);
		return rc;
	}

	return platform_driver_register(&msm_pm_8x60_driver);
}
device_initcall(msm_pm_8x60_init);

void __init msm_pm_sleep_status_init(void)
{
	platform_driver_register(&msm_cpu_status_driver);
}
