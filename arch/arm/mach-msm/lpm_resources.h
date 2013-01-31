/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ARCH_ARM_MACH_MSM_LPM_RESOURCES_H
#define __ARCH_ARM_MACH_MSM_LPM_RESOURCES_H

#include "pm.h"
#include "test-lpm.h"

enum {
	MSM_LPM_PXO_OFF,
	MSM_LPM_PXO_ON
};

enum {
	MSM_LPM_L2_CACHE_HSFS_OPEN,
	MSM_LPM_L2_CACHE_GDHS,
	MSM_LPM_L2_CACHE_RETENTION,
	MSM_LPM_L2_CACHE_ACTIVE,
};

struct msm_rpmrs_limits {
	uint32_t pxo;
	uint32_t l2_cache;
	uint32_t vdd_mem_upper_bound;
	uint32_t vdd_mem_lower_bound;
	uint32_t vdd_dig_upper_bound;
	uint32_t vdd_dig_lower_bound;
	bool irqs_detectable;
	bool gpio_detectable;

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

enum {
	MSM_LPM_STATE_ENTER = 0,
	MSM_LPM_STATE_EXIT = 1,
};

#define MSM_PM(field) MSM_LPM_##field

/**
 * msm_pm_get_pxo() -  get the limits for pxo
 * @limits:            pointer to the msm_rpmrs_limits structure
 *
 * This function gets the limits to the resource pxo on
 * 8974
 */

uint32_t msm_pm_get_pxo(struct msm_rpmrs_limits *limits);

/**
 * msm_pm_get_l2_cache() -  get the limits for l2 cache
 * @limits:            pointer to the msm_rpmrs_limits structure
 *
 * This function gets the limits to the resource l2 cache
 * on 8974
 */

uint32_t msm_pm_get_l2_cache(struct msm_rpmrs_limits *limits);

/**
 * msm_pm_get_vdd_mem() -  get the limits for pxo
 * @limits:            pointer to the msm_rpmrs_limits structure
 *
 * This function gets the limits to the resource vdd mem
 * on 8974
 */

uint32_t msm_pm_get_vdd_mem(struct msm_rpmrs_limits *limits);

/**
 * msm_pm_get_vdd_dig() -  get the limits for vdd dig
 * @limits:            pointer to the msm_rpmrs_limits structure
 *
 * This function gets the limits to the resource on 8974
 */

uint32_t msm_pm_get_vdd_dig(struct msm_rpmrs_limits *limits);

/**
 * msm_lpm_get_xo_value() - get the enum value for xo
 * @node		pointer to the device node
 * @key			pxo property key
 * @xo_val		xo enum value
 */
int msm_lpm_get_xo_value(struct device_node *node,
			char *key, uint32_t *xo_val);

/**
 * msm_lpm_get_l2_cache_value() - get the enum value for l2 cache
 * @node                pointer to the device node
 * @key                 l2 cache property key
 * @l2_val              l2 mode enum value
 */
int msm_lpm_get_l2_cache_value(struct device_node *node,
				char *key, uint32_t *l2_val);

/**
 * struct msm_lpm_sleep_data - abstraction to get sleep data
 * @limits:	pointer to the msm_rpmrs_limits structure
 * @kernel_sleep:	kernel sleep time as decided by the power calculation
 *			algorithm
 *
 * This structure is an abstraction to get the limits and kernel sleep time
 * during enter sleep.
 */

struct msm_lpm_sleep_data {
	struct msm_rpmrs_limits *limits;
	uint32_t kernel_sleep;
};

/**
 * msm_lpm_register_notifier() - register for notifications
 * @cpu:               cpu to debug
 * @level_iter:        low power level index to debug
 * @nb:       notifier block to callback on notifications
 * @is_latency_measure: is it latency measure
 *
 * This function sets the permitted level to the index of the
 * level under test and registers notifier for callback.
 */

int msm_lpm_register_notifier(int cpu, int level_iter,
		struct notifier_block *nb, bool is_latency_measure);

/**
 * msm_lpm_unregister_notifier() - unregister from notifications
 * @cpu:               cpu to debug
 * @nb:       notifier block to callback on notifications
 *
 * This function sets the permitted level to a value one more than
 * available levels count which indicates that all levels are
 * permitted and it also unregisters notifier for callback.
 */

int msm_lpm_unregister_notifier(int cpu, struct notifier_block *nb);

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

 * @sclk_count: wakeup counter for RPM.
 * @limits: pointer to the resource limits of the low power mode being entered.
 * @from_idle: bool to determine if this call being made as a part of
 *             idle power collapse.
 * @notify_rpm: bool that informs if this is an RPM notified power collapse.
 *
 * returns 0 on success.
 */
int msm_lpmrs_enter_sleep(uint32_t sclk_count, struct msm_rpmrs_limits *limits,
	bool from_idle, bool notify_rpm);

/**
 * msm_lpmrs_exit_sleep() - Exit sleep, reset the MPM and L2 mode.
 * @ limits: pointer to resource limits of the most recent low power mode.
 * @from_idle: bool to determine if this call being made as a part of
 *             idle power collapse.
 * @notify_rpm: bool that informs if this is an RPM notified power collapse.
 * @collapsed: bool that informs if the Krait was power collapsed.
 */
void msm_lpmrs_exit_sleep(struct msm_rpmrs_limits *limits,
	bool from_idle, bool notify_rpm, bool collapsed);
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

static inline int msm_lpmrs_enter_sleep(uint32_t sclk_count,
	struct msm_rpmrs_limits *limits, bool from_idle, bool notify_rpm)
{
	return 0;
}

static inline void msm_lpmrs_exit_sleep(struct msm_rpmrs_limits *limits,
	bool from_idle, bool notify_rpm, bool collapsed)
{
	return;
}

static inline int __init msm_lpmrs_module_init(void)
{
	return 0;
}
#endif /* CONFIG_MSM_RPM_SMD */

#endif
