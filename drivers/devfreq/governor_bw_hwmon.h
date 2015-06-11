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
 * @set_thres:			Set the count threshold to generate an IRQ
 * @get_bytes_and_clear:	Get the bytes transferred since the last call
 *				and reset the counter to start over.
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
	int (*suspend_hwmon)(struct bw_hwmon *hw);
	int (*resume_hwmon)(struct bw_hwmon *hw);
	unsigned long (*set_thres)(struct bw_hwmon *hw, unsigned long bytes);
	unsigned long (*get_bytes_and_clear)(struct bw_hwmon *hw);
	struct device *dev;
	struct device_node *of_node;
	struct devfreq_governor *gov;

	struct devfreq *df;
};

#ifdef CONFIG_DEVFREQ_GOV_QCOM_BW_HWMON
int register_bw_hwmon(struct device *dev, struct bw_hwmon *hwmon);
int update_bw_hwmon(struct bw_hwmon *hwmon);
int bw_hwmon_sample_end(struct bw_hwmon *hwmon);
#else
static inline int register_bw_hwmon(struct device *dev,
					struct bw_hwmon *hwmon)
{
	return 0;
}
static inline int update_bw_hwmon(struct bw_hwmon *hwmon)
{
	return 0;
}
static inline int bw_hwmon_sample_end(struct bw_hwmon *hwmon)
{
	return 0;
}
#endif

#endif /* _GOVERNOR_BW_HWMON_H */
