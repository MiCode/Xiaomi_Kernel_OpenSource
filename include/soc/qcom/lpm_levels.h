/* Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
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
#ifndef __SOC_QCOM_LPM_LEVEL_H__
#define __SOC_QCOM_LPM_LEVEL_H__

struct system_pm_ops {
	int (*enter)(struct cpumask *mask);
	void (*exit)(bool success);
	int (*update_wakeup)(bool);
	bool (*sleep_allowed)(void);
};

#if defined(CONFIG_MSM_PM) || defined(CONFIG_MSM_PM_LEGACY)
uint32_t register_system_pm_ops(struct system_pm_ops *pm_ops);
void update_ipi_history(int cpu);
#else
static inline uint32_t register_system_pm_ops(struct system_pm_ops *pm_ops)
{ return -ENODEV; }
static inline void update_ipi_history(int cpu) {}
#endif

#endif
