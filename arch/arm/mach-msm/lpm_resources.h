/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_LPM_RESOURCES_H
#define __ARCH_ARM_MACH_MSM_LPM_RESOURCES_H

#include "pm.h"

enum {
	MSM_LPM_PXO_OFF = 0,
	MSM_LPM_PXO_ON = 1,
};

enum {
	MSM_LPM_L2_CACHE_HSFS_OPEN = 0,
	MSM_LPM_L2_CACHE_GDHS = 1,
	MSM_LPM_L2_CACHE_RETENTION = 2,
	MSM_LPM_L2_CACHE_ACTIVE = 3,
};

struct msm_rpmrs_limits {
	uint32_t pxo;
	uint32_t l2_cache;
	uint32_t vdd_mem_upper_bound;
	uint32_t vdd_mem_lower_bound;
	uint32_t vdd_dig_upper_bound;
	uint32_t vdd_dig_lower_bound;

	uint32_t latency_us[NR_CPUS];
	uint32_t power[NR_CPUS];
};

struct msm_rpmrs_level {
	enum msm_pm_sleep_mode sleep_mode;
	struct msm_rpmrs_limits rs_limits;
	bool available;
	uint32_t latency_us;
	uint32_t steady_state_power;
	uint32_t energy_overhead;
	uint32_t time_overhead_us;
};

#ifdef CONFIG_MSM_RPM_SMD

/**
 * msm_lpm_level_beyond_limit() - Check if the resources in a low power level
 * is beyond the limits of the driver votes received for those resources.This
 * function is used by lpm_levels to eliminate any low power level that cannot
 * be entered.
 *
 * @limits: pointer to the resource limits of a low power level.
 *
 * returns true if the resource limits are beyond driver resource votes.
 * false otherwise.
 */
bool msm_lpm_level_beyond_limit(struct msm_rpmrs_limits *limits);

/**
 * msm_lpmrs_enter_sleep() - Enter sleep flushes the sleep votes of low power
 * resources to the RPM driver, also configure the MPM if needed depending
 * on the low power mode being entered. L2 low power mode is also set in
 * this function.

 * @limits: pointer to the resource limits of the low power mode being entered.
 * @from_idle: bool to determine if this call being made as a part of
 *             idle power collapse.
 * @notify_rpm: bool that informs if this is an RPM notified power collapse.
 *
 * returns 0 on success.
 */
int msm_lpmrs_enter_sleep(struct msm_rpmrs_limits *limits,
	bool from_idle, bool notify_rpm);

/**
 * msm_lpmrs_exit_sleep() - Exit sleep, reset the MPM and L2 mode.
 * @ sclk_count - Sleep Clock count.
 * @ limits: pointer to resource limits of the most recent low power mode.
 * @from_idle: bool to determine if this call being made as a part of
 *             idle power collapse.
 * @notify_rpm: bool that informs if this is an RPM notified power collapse.
 */
void msm_lpmrs_exit_sleep(uint32_t sclk_count, struct msm_rpmrs_limits *limits,
	bool from_idle, bool notify_rpm);
/**
 * msm_lpmrs_module_init() - Init function that parses the device tree to
 * get the low power resource attributes and registers with RPM driver for
 * callback notification.
 *
 * returns 0 on success.
 */
int __init msm_lpmrs_module_init(void);

#else
static inline bool msm_lpm_level_beyond_limit(struct msm_rpmrs_limits *limits)
{
	return true;
}

static inline int msm_lpmrs_enter_sleep(struct msm_rpmrs_limits *limits,
	bool from_idle, bool notify_rpm)
{
	return 0;
}

static inline void msm_lpmrs_exit_sleep(uint32_t sclk_count,
		struct msm_rpmrs_limits *limits, bool from_idle,
		bool notify_rpm)
{
	return;
}

static inline int __init msm_lpmrs_module_init(void)
{
	return 0;
}
#endif /* CONFIG_MSM_RPM_SMD */

#endif
