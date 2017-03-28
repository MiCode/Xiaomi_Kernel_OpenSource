/*
 * Copyright (c) 2014, 2016, The Linux Foundation. All rights reserved.
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

#ifndef _GOVERNOR_CACHE_HWMON_H
#define _GOVERNOR_CACHE_HWMON_H

#include <linux/kernel.h>
#include <linux/devfreq.h>

struct mrps_stats {
	unsigned long high;
	unsigned long med;
	unsigned long low;
	unsigned int busy_percent;
};

struct cache_hwmon {
	int (*start_hwmon)(struct devfreq *df, struct mrps_stats *mrps);
	void (*stop_hwmon)(struct devfreq *df);
	bool (*is_valid_irq)(struct devfreq *df);
	unsigned long (*meas_mrps_and_set_irq)(struct devfreq *df,
					unsigned int tol, unsigned int us,
					struct mrps_stats *mrps);
	int irq;
};

#ifdef CONFIG_DEVFREQ_GOV_QCOM_CACHE_HWMON
int register_cache_hwmon(struct cache_hwmon *hwmon);
#else
static inline int register_cache_hwmon(struct cache_hwmon *hwmon)
{
	return 0;
}
#endif

#endif /* _GOVERNOR_CACHE_HWMON_H */
