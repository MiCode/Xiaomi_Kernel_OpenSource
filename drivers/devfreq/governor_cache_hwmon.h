/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014, 2016, 2019, The Linux Foundation. All rights reserved.
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

/**
 * struct cache_hwmon - devfreq Cache HW monitor info
 * @start_hwmon:	Start the HW monitoring
 * @stop_hwmon:		Stop the HW monitoring
 * @meas_mrps_and_set_irq:	Return the measured count and set up the
 *				IRQ to fire if usage exceeds current
 *				measurement by @tol percent.
 * @dev:		device that this HW monitor can monitor.
 * @of_node:		OF node of device that this HW monitor can monitor.
 * @df:			Devfreq node that this HW montior is being used
 *			for. NULL when not actively in use, and non-NULL
 *			when in use.
 */
struct cache_hwmon {
	int (*start_hwmon)(struct cache_hwmon *hw, struct mrps_stats *mrps);
	void (*stop_hwmon)(struct cache_hwmon *hw);
	unsigned long (*meas_mrps_and_set_irq)(struct cache_hwmon *hw,
					unsigned int tol, unsigned int us,
					struct mrps_stats *mrps);
	struct device *dev;
	struct device_node *of_node;
	struct devfreq *df;
};

#ifdef CONFIG_DEVFREQ_GOV_QCOM_CACHE_HWMON
int register_cache_hwmon(struct device *dev, struct cache_hwmon *hwmon);
int update_cache_hwmon(struct cache_hwmon *hwmon);
#else
static inline int register_cache_hwmon(struct device *dev,
				       struct cache_hwmon *hwmon)
{
	return 0;
}
int update_cache_hwmon(struct cache_hwmon *hwmon)
{
	return 0;
}
#endif

#endif /* _GOVERNOR_CACHE_HWMON_H */
