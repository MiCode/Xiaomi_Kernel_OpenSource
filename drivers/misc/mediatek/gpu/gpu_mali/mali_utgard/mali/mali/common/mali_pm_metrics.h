/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2014, 2016 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __MALI_PM_METRICS_H__
#define __MALI_PM_METRICS_H__

#ifdef CONFIG_MALI_DEVFREQ
#include "mali_osk_locks.h"
#include "mali_group.h"

struct mali_device;

/**
 * Metrics data collected for use by the power management framework.
 */
struct mali_pm_metrics_data {
	ktime_t time_period_start;
	u64 time_busy;
	u64 time_idle;
	u64 prev_busy;
	u64 prev_idle;
	u32 num_running_gp_cores;
	u32 num_running_pp_cores;
	ktime_t time_period_start_gp;
	u64 time_busy_gp;
	u64 time_idle_gp;
	ktime_t time_period_start_pp;
	u64 time_busy_pp[MALI_MAX_NUMBER_OF_PHYSICAL_PP_GROUPS];
	u64 time_idle_pp[MALI_MAX_NUMBER_OF_PHYSICAL_PP_GROUPS];
	mali_bool gpu_active;
	_mali_osk_spinlock_irq_t *lock;
};

/**
 * Initialize/start the Mali GPU pm_metrics metrics reporting.
 *
 * @return _MALI_OSK_ERR_OK on success, otherwise failure.
 */
_mali_osk_errcode_t mali_pm_metrics_init(struct mali_device *mdev);

/**
 * Terminate the Mali GPU pm_metrics metrics reporting
 */
void mali_pm_metrics_term(struct mali_device *mdev);

/**
 * Should be called when a job is about to execute a GPU job
 */
void mali_pm_record_gpu_active(mali_bool is_gp);

/**
 * Should be called when a job is finished
 */
void mali_pm_record_gpu_idle(mali_bool is_gp);

void mali_pm_reset_dvfs_utilisation(struct mali_device *mdev);

void mali_pm_get_dvfs_utilisation(struct mali_device *mdev, unsigned long *total_out, unsigned long *busy_out);

void mali_pm_metrics_spin_lock(void);

void mali_pm_metrics_spin_unlock(void);
#else
void mali_pm_record_gpu_idle(mali_bool is_gp) {}
void mali_pm_record_gpu_active(mali_bool is_gp) {}
#endif
#endif /* __MALI_PM_METRICS_H__ */
