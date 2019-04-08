/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2017, 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _GOVERNOR_MEMLAT_H
#define _GOVERNOR_MEMLAT_H

#include <linux/kernel.h>
#include <linux/devfreq.h>

/**
 * struct dev_stats - Device stats
 * @inst_count:			Number of instructions executed.
 * @mem_count:			Number of memory accesses made.
 * @freq:			Effective frequency of the device in the
 *				last interval.
 */
struct dev_stats {
	int id;
	unsigned long inst_count;
	unsigned long mem_count;
	unsigned long freq;
	unsigned long stall_pct;
};

struct core_dev_map {
	unsigned int core_mhz;
	unsigned int target_freq;
};

/**
 * struct memlat_hwmon - Memory Latency HW monitor info
 * @start_hwmon:		Start the HW monitoring
 * @stop_hwmon:			Stop the HW monitoring
 * @get_cnt:			Return the number of intructions executed,
 *				memory accesses and effective frequency
 * @dev:			Pointer to device that this HW monitor can
 *				monitor.
 * @of_node:			OF node of device that this HW monitor can
 *				monitor.
 * @df:				Devfreq node that this HW monitor is being
 *				used for. NULL when not actively in use and
 *				non-NULL when in use.
 * @num_cores:			Number of cores that are monitored by the
 *				hardware monitor.
 * @core_stats:			Array containing instruction count, memory
 *				accesses and effective frequency for each core.
 *
 * One of dev or of_node needs to be specified for a successful registration.
 *
 */
struct memlat_hwmon {
	int (*start_hwmon)(struct memlat_hwmon *hw);
	void (*stop_hwmon)(struct memlat_hwmon *hw);
	unsigned long (*get_cnt)(struct memlat_hwmon *hw);
	struct device_node *(*get_child_of_node)(struct device *dev);
	void (*request_update_ms)(struct memlat_hwmon *hw,
				  unsigned int update_ms);
	struct device *dev;
	struct device_node *of_node;

	unsigned int num_cores;
	struct dev_stats *core_stats;

	struct devfreq *df;
	struct core_dev_map *freq_map;
	bool should_ignore_df_monitor;
};

#ifdef CONFIG_DEVFREQ_GOV_MEMLAT
int register_memlat(struct device *dev, struct memlat_hwmon *hw);
int register_compute(struct device *dev, struct memlat_hwmon *hw);
int update_memlat(struct memlat_hwmon *hw);
#else
static inline int register_memlat(struct device *dev,
				  struct memlat_hwmon *hw)
{
	return 0;
}
static inline int register_compute(struct device *dev,
				   struct memlat_hwmon *hw)
{
	return 0;
}
static inline int update_memlat(struct memlat_hwmon *hw)
{
	return 0;
}
#endif

#endif /* _GOVERNOR_BW_HWMON_H */
