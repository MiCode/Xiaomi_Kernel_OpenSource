/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2014, The Linux Foundation. All rights reserved.
 * Author: San Mehat <san@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARCH_ARM_MACH_MSM_PM_H
#define __ARCH_ARM_MACH_MSM_PM_H

#include <linux/types.h>
#include <linux/cpuidle.h>

#ifdef CONFIG_SMP
extern void msm_secondary_startup(void);
#else
#define msm_secondary_startup NULL
#endif

enum msm_pm_sleep_mode {
	MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT,
	MSM_PM_SLEEP_MODE_RETENTION,
	MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE,
	MSM_PM_SLEEP_MODE_POWER_COLLAPSE,
	MSM_PM_SLEEP_MODE_POWER_COLLAPSE_SUSPEND,
	MSM_PM_SLEEP_MODE_NR,
	MSM_PM_SLEEP_MODE_NOT_SELECTED,
};

enum msm_pm_l2_scm_flag {
	MSM_SCM_L2_ON = 0,
	MSM_SCM_L2_OFF = 1,
	MSM_SCM_L2_RET = 2,
	MSM_SCM_L2_GDHS = 3,
};

#define MSM_PM_MODE(cpu, mode_nr)  ((cpu) * MSM_PM_SLEEP_MODE_NR + (mode_nr))

struct msm_pm_time_params {
	uint32_t latency_us;
	uint32_t sleep_us;
	uint32_t next_event_us;
	uint32_t modified_time_us;
};

struct msm_pm_sleep_status_data {
	void *base_addr;
	uint32_t cpu_offset;
	uint32_t mask;
};

int msm_pm_mode_sysfs_add(const char *);

/**
 * msm_pm_sleep_mode_allow() - API to determine if sleep mode is allowed.
 * @cpu:	CPU on which to check for the sleep mode.
 * @mode:	Sleep Mode to check for.
 * @idle:	Idle or Suspend Sleep Mode.
 *
 * Helper function to determine if a Idle or Suspend
 * Sleep mode is allowed for a specific CPU.
 *
 * Return: 1 for allowed; 0 if not allowed.
 */
int msm_pm_sleep_mode_allow(unsigned int, unsigned int, bool);

/**
 * msm_pm_sleep_mode_supported() - API to determine if sleep mode is
 * supported.
 * @cpu:	CPU on which to check for the sleep mode.
 * @mode:	Sleep Mode to check for.
 * @idle:	Idle or Suspend Sleep Mode.
 *
 * Helper function to determine if a Idle or Suspend
 * Sleep mode is allowed and enabled for a specific CPU.
 *
 * Return: 1 for supported; 0 if not supported.
 */
int msm_pm_sleep_mode_supported(unsigned int, unsigned int, bool);

enum msm_pm_pc_mode_type {
	MSM_PM_PC_TZ_L2_INT,   /*Power collapse terminates in TZ;
					integrated L2 cache controller */
	MSM_PM_PC_NOTZ_L2_EXT, /* Power collapse doesn't terminate in
					TZ; external L2 cache controller */
	MSM_PM_PC_TZ_L2_EXT,   /* Power collapse terminates in TZ;
					external L2 cache controller */
};

struct msm_pm_init_data_type {
	enum msm_pm_pc_mode_type pc_mode;
};

struct msm_pm_cpr_ops {
	void (*cpr_suspend)(void);
	void (*cpr_resume)(void);
};

void __init msm_pm_set_tz_retention_flag(unsigned int flag);
void msm_pm_enable_retention(bool enable);
bool msm_pm_retention_enabled(void);
void msm_cpu_pm_enter_sleep(enum msm_pm_sleep_mode mode, bool from_idle);

#ifdef CONFIG_MSM_PM
void msm_pm_set_rpm_wakeup_irq(unsigned int irq);
int msm_pm_wait_cpu_shutdown(unsigned int cpu);
void __init msm_pm_sleep_status_init(void);
void msm_pm_set_l2_flush_flag(enum msm_pm_l2_scm_flag flag);
void lpm_cpu_hotplug_enter(unsigned int cpu);
s32 msm_cpuidle_get_deep_idle_latency(void);
#else
static inline void msm_pm_set_rpm_wakeup_irq(unsigned int irq) {}
static inline int msm_pm_wait_cpu_shutdown(unsigned int cpu) { return 0; }
static inline void msm_pm_sleep_status_init(void) {};
static inline void msm_pm_set_l2_flush_flag(unsigned int flag) { }
static inline void lpm_cpu_hotplug_enter(unsigned int cpu) {};
static inline s32 msm_cpuidle_get_deep_idle_latency(void) { return 0; }
#endif

#ifdef CONFIG_HOTPLUG_CPU
int msm_platform_secondary_init(unsigned int cpu);
#else
static inline int msm_platform_secondary_init(unsigned int cpu) { return 0; }
#endif

enum msm_pm_time_stats_id {
	MSM_PM_STAT_REQUESTED_IDLE = 0,
	MSM_PM_STAT_IDLE_SPIN,
	MSM_PM_STAT_IDLE_WFI,
	MSM_PM_STAT_RETENTION,
	MSM_PM_STAT_IDLE_STANDALONE_POWER_COLLAPSE,
	MSM_PM_STAT_IDLE_FAILED_STANDALONE_POWER_COLLAPSE,
	MSM_PM_STAT_IDLE_POWER_COLLAPSE,
	MSM_PM_STAT_IDLE_FAILED_POWER_COLLAPSE,
	MSM_PM_STAT_SUSPEND,
	MSM_PM_STAT_FAILED_SUSPEND,
	MSM_PM_STAT_NOT_IDLE,
	MSM_PM_STAT_COUNT
};

#ifdef CONFIG_MSM_IDLE_STATS
void msm_pm_add_stats(enum msm_pm_time_stats_id *enable_stats, int size);
void msm_pm_add_stat(enum msm_pm_time_stats_id id, int64_t t);
void msm_pm_l2_add_stat(uint32_t id, int64_t t);
#else
static inline void msm_pm_add_stats(enum msm_pm_time_stats_id *enable_stats,
		int size) {}
static inline void msm_pm_add_stat(enum msm_pm_time_stats_id id, int64_t t) {}
static inline void msm_pm_l2_add_stat(uint32_t id, int64_t t) {}
#endif

void msm_pm_set_cpr_ops(struct msm_pm_cpr_ops *ops);
extern unsigned long msm_pc_debug_counters_phys;
#endif  /* __ARCH_ARM_MACH_MSM_PM_H */
