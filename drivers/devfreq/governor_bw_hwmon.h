/*
 * Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

#ifndef _GOVERNOR_BW_HWMON_H
#define _GOVERNOR_BW_HWMON_H

#include <linux/kernel.h>
#include <linux/devfreq.h>

/**
 * struct bw_hwmon - dev BW HW monitor info
 * @start_hwmon:		Start the HW monitoring of the dev BW
 * @stop_hwmon:			Stop the HW monitoring of dev BW
 * @is_valid_irq:		Check whether the IRQ was triggered by the
 *				counters used to monitor dev BW.
 * @meas_bw_and_set_irq:	Return the measured bandwidth and set up the
 *				IRQ to fire if the usage exceeds current
 *				measurement by @tol percent.
 * @irq:			IRQ number that corresponds to this HW
 *				monitor.
 * @dev:			Pointer to device that this HW monitor can
 *				monitor.
 * @of_node:			OF node of device that this HW monitor can
 *				monitor.
 * @gov:			devfreq_governor struct that should be used
 *				when registering this HW monitor with devfreq.
 *				Only the name field is expected to be
 *				initialized.
 * @df:				Devfreq node that this HW monitor is being
 *				used for. NULL when not actively in use and
 *				non-NULL when in use.
 *
 * One of dev, of_node or governor_name needs to be specified for a
 * successful registration.
 *
 */
struct bw_hwmon {
	int (*start_hwmon)(struct bw_hwmon *hw, unsigned long mbps);
	void (*stop_hwmon)(struct bw_hwmon *hw);
	unsigned long (*meas_bw_and_set_irq)(struct bw_hwmon *hw,
					unsigned int tol, unsigned int us);
	struct device *dev;
	struct device_node *of_node;
	struct devfreq_governor *gov;

	struct devfreq *df;
};

#ifdef CONFIG_DEVFREQ_GOV_QCOM_BW_HWMON
int register_bw_hwmon(struct device *dev, struct bw_hwmon *hwmon);
int update_bw_hwmon(struct bw_hwmon *hwmon);
#else
static inline int register_bw_hwmon(struct device *dev,
					struct bw_hwmon *hwmon)
{
	return 0;
}
int update_bw_hwmon(struct bw_hwmon *hwmon)
{
	return 0;
}
#endif

#endif /* _GOVERNOR_BW_HWMON_H */
