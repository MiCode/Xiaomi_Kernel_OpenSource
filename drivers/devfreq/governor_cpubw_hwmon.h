/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#ifndef _GOVERNOR_CPUBW_HWMON_H
#define _GOVERNOR_CPUBW_HWMON_H

#include <linux/kernel.h>
#include <linux/devfreq.h>

/**
 * struct cpubw_hwmon - CPU BW HW monitor ops
 * @start_hwmon:		Start the HW monitoring of the CPU BW
 * @stop_hwmon:			Stop the HW monitoring of CPU BW
 * @is_valid_irq:		Check whether the IRQ was triggered by the
 *				counters used to monitor CPU BW.
 * @meas_bw_and_set_irq:	Return the measured bandwidth and set up the
 *				IRQ to fire if the usage exceeds current
 *				measurement by @tol percent.
 */
struct cpubw_hwmon {
	int (*start_hwmon)(struct devfreq *df, unsigned long mbps);
	void (*stop_hwmon)(struct devfreq *df);
	bool (*is_valid_irq)(struct devfreq *df);
	unsigned long (*meas_bw_and_set_irq)(struct devfreq *df,
					unsigned int tol, unsigned int us);
	int irq;
};

#ifdef CONFIG_DEVFREQ_GOV_MSM_CPUBW_HWMON
int register_cpubw_hwmon(struct cpubw_hwmon *hwmon);
#else
static inline int register_cpubw_hwmon(struct cpubw_hwmon *hwmon)
{
	return 0;
}
#endif

#endif /* _GOVERNOR_CPUBW_HWMON_H */
