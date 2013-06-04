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
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/irqreturn.h>

#ifndef MSM_IOMMU_PERFMON_H
#define MSM_IOMMU_PERFMON_H

/**
 * struct iommu_pmon_counter - container for a performance counter.
 * @counter_no:          counter number within the group
 * @absolute_counter_no: counter number within IOMMU PMU
 * @value:               cached counter value
 * @overflow_count:      no of times counter has overflowed
 * @enabled:             indicates whether counter is enabled or not
 * @current_event_class: current selected event class, -1 if none
 * @counter_dir:         debugfs directory for this counter
 * @cnt_group:           group this counter belongs to
 */
struct iommu_pmon_counter {
	unsigned int counter_no;
	unsigned int absolute_counter_no;
	unsigned long value;
	unsigned long overflow_count;
	unsigned int enabled;
	int current_event_class;
	struct dentry *counter_dir;
	struct iommu_pmon_cnt_group *cnt_group;
};

/**
 * struct iommu_pmon_cnt_group - container for a perf mon counter group.
 * @grp_no:       group number
 * @num_counters: number of counters in this group
 * @counters:     list of counter in this group
 * @group_dir:    debugfs directory for this group
 * @pmon:         pointer to the iommu_pmon object this group belongs to
 */
struct iommu_pmon_cnt_group {
	unsigned int grp_no;
	unsigned int num_counters;
	struct iommu_pmon_counter *counters;
	struct dentry *group_dir;
	struct iommu_pmon *pmon;
};

/**
 * struct iommu_info - container for a perf mon iommu info.
 * @iommu_name: name of the iommu from device tree
 * @base:       virtual base address for this iommu
 * @evt_irq:    irq number for event overflow interrupt
 * @iommu_dev:  pointer to iommu device
 * @ops:        iommu access operations pointer.
 * @hw_ops:     iommu pm hw access operations pointer.
 * @always_on:  1 if iommu is always on, 0 otherwise.
 */
struct iommu_info {
	const char *iommu_name;
	void *base;
	int evt_irq;
	struct device *iommu_dev;
	struct iommu_access_ops *ops;
	struct iommu_pm_hw_ops *hw_ops;
	unsigned int always_on;
};

/**
 * struct iommu_pmon - main container for a perf mon data.
 * @iommu_dir:            debugfs directory for this iommu
 * @iommu:                iommu_info instance
 * @iommu_list:           iommu_list head
 * @cnt_grp:              list of counter groups
 * @num_groups:           number of counter groups
 * @num_counters:         number of counters per group
 * @event_cls_supported:  an array of event classes supported for this PMU
 * @nevent_cls_supported: number of event classes supported.
 * @enabled:              Indicates whether perf. mon is enabled or not
 * @iommu_attached        Indicates whether iommu is attached or not.
 * @lock:                 mutex used to synchronize access to shared data
 */
struct iommu_pmon {
	struct dentry *iommu_dir;
	struct iommu_info iommu;
	struct list_head iommu_list;
	struct iommu_pmon_cnt_group *cnt_grp;
	u32 num_groups;
	u32 num_counters;
	u32 *event_cls_supported;
	u32 nevent_cls_supported;
	unsigned int enabled;
	unsigned int iommu_attach_count;
	struct mutex lock;
};

/**
 * struct iommu_hw_ops - Callbacks for accessing IOMMU HW
 * @initialize_hw: Call to do any initialization before enabling ovf interrupts
 * @is_hw_access_ok: Returns 1 if we can access HW, 0 otherwise
 * @grp_enable: Call to enable a counter group
 * @grp_disable: Call to disable a counter group
 * @enable_pm: Call to enable PM
 * @disable_pm: Call to disable PM
 * @reset_counters:  Call to reset counters
 * @check_for_overflow:  Call to check for overflow
 * @evt_ovfl_int_handler: Overflow interrupt handler callback
 * @counter_enable: Call to enable counters
 * @counter_disable: Call to disable counters
 * @ovfl_int_enable: Call to enable overflow interrupts
 * @ovfl_int_disable: Call to disable overflow interrupts
 * @set_event_class: Call to set event class
 * @read_counter: Call to read a counter value
 */
struct iommu_pm_hw_ops {
	void (*initialize_hw)(const struct iommu_pmon *);
	unsigned int (*is_hw_access_OK)(const struct iommu_pmon *);
	void (*grp_enable)(struct iommu_info *, unsigned int);
	void (*grp_disable)(struct iommu_info *, unsigned int);
	void (*enable_pm)(struct iommu_info *);
	void (*disable_pm)(struct iommu_info *);
	void (*reset_counters)(const struct iommu_info *);
	void (*check_for_overflow)(struct iommu_pmon *);
	irqreturn_t (*evt_ovfl_int_handler)(int, void *);
	void (*counter_enable)(struct iommu_info *,
			       struct iommu_pmon_counter *);
	void (*counter_disable)(struct iommu_info *,
			       struct iommu_pmon_counter *);
	void (*ovfl_int_enable)(struct iommu_info *,
				const struct iommu_pmon_counter *);
	void (*ovfl_int_disable)(struct iommu_info *,
				const struct iommu_pmon_counter *);
	void (*set_event_class)(struct iommu_pmon *pmon, unsigned int,
				unsigned int);
	unsigned int (*read_counter)(struct iommu_pmon_counter *);
};

#define MSM_IOMMU_PMU_NO_EVENT_CLASS -1

#ifdef CONFIG_MSM_IOMMU_PMON

/**
 * Get pointer to PMU hardware access functions for IOMMUv0 PMU
 */
struct iommu_pm_hw_ops *iommu_pm_get_hw_ops_v0(void);

/**
 * Get pointer to PMU hardware access functions for IOMMUv1 PMU
 */
struct iommu_pm_hw_ops *iommu_pm_get_hw_ops_v1(void);

/**
 * Allocate memory for performance monitor structure. Must
 * be called before iommu_pm_iommu_register
 */
struct iommu_pmon *msm_iommu_pm_alloc(struct device *iommu_dev);

/**
 * Free memory previously allocated with iommu_pm_alloc
 */
void msm_iommu_pm_free(struct device *iommu_dev);

/**
 * Register iommu with the performance monitor module.
 */
int msm_iommu_pm_iommu_register(struct iommu_pmon *info);

/**
 * Unregister iommu with the performance monitor module.
 */
void msm_iommu_pm_iommu_unregister(struct device *dev);

/**
 * Called by iommu driver when attaching is complete
 * Must NOT be called with IOMMU mutexes held.
 * @param iommu_dev IOMMU device that is attached
  */
void msm_iommu_attached(struct device *dev);

/**
 * Called by iommu driver before detaching.
 * Must NOT be called with IOMMU mutexes held.
 * @param iommu_dev IOMMU device that is going to be detached
  */
void msm_iommu_detached(struct device *dev);
#else
static inline struct iommu_pm_hw_ops *iommu_pm_get_hw_ops_v0(void)
{
	return NULL;
}

static inline struct iommu_pm_hw_ops *iommu_pm_get_hw_ops_v1(void)
{
	return NULL;
}

static inline struct iommu_pmon *msm_iommu_pm_alloc(struct device *iommu_dev)
{
	return NULL;
}

static inline void msm_iommu_pm_free(struct device *iommu_dev)
{
	return;
}

static inline int msm_iommu_pm_iommu_register(struct iommu_pmon *info)
{
	return -EIO;
}

static inline void msm_iommu_pm_iommu_unregister(struct device *dev)
{
}

static inline void msm_iommu_attached(struct device *dev)
{
}

static inline void msm_iommu_detached(struct device *dev)
{
}
#endif
#endif
