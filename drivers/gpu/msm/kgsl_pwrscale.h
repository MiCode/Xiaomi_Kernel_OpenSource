/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2010-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __KGSL_PWRSCALE_H
#define __KGSL_PWRSCALE_H

#include "kgsl_pwrctrl.h"
#include "msm_adreno_devfreq.h"

/* devfreq governor call window in usec */
#define KGSL_GOVERNOR_CALL_INTERVAL 10000

struct kgsl_power_stats {
	u64 busy_time;
	u64 ram_time;
	u64 ram_wait;
};

/**
 * struct kgsl_pwrscale - Power scaling settings for a KGSL device
 * @devfreqptr - Pointer to the devfreq device
 * @gpu_profile - GPU profile data for the devfreq device
 * @bus_profile - Bus specific data for the bus devfreq device
 * @freq_table - GPU frequencies for the DCVS algorithm
 * @last_governor - Prior devfreq governor
 * @accum_stats - Accumulated statistics for various frequency calculations
 * @enabled - Whether or not power scaling is enabled
 * @time - Last submitted sample timestamp
 * @on_time - Timestamp when gpu busy begins
 * @devfreq_wq - Main devfreq workqueue
 * @devfreq_suspend_ws - Pass device suspension to devfreq
 * @devfreq_resume_ws - Pass device resume to devfreq
 * @devfreq_notify_ws - Notify devfreq to update sampling
 * @next_governor_call - Timestamp after which the governor may be notified of
 * a new sample
 * @cooling_dev - Thermal cooling device handle
 * @ctxt_aware_enable - Whether or not ctxt aware DCVS feature is enabled
 * @ctxt_aware_busy_penalty - The time in microseconds required to trigger
 * ctxt aware power level jump
 * @ctxt_aware_target_pwrlevel - pwrlevel to jump on in case of ctxt aware
 * power level jump
 */
struct kgsl_pwrscale {
	struct devfreq *devfreqptr;
	struct msm_adreno_extended_profile gpu_profile;
	struct msm_busmon_extended_profile bus_profile;
	unsigned long freq_table[KGSL_MAX_PWRLEVELS];
	char last_governor[DEVFREQ_NAME_LEN];
	struct kgsl_power_stats accum_stats;
	bool enabled;
	ktime_t time;
	s64 on_time;
	struct workqueue_struct *devfreq_wq;
	struct work_struct devfreq_suspend_ws;
	struct work_struct devfreq_resume_ws;
	struct work_struct devfreq_notify_ws;
	ktime_t next_governor_call;
	struct thermal_cooling_device *cooling_dev;
	bool ctxt_aware_enable;
	unsigned int ctxt_aware_target_pwrlevel;
	unsigned int ctxt_aware_busy_penalty;
	/** @busmondev: A child device for the busmon  governor */
	struct device busmondev;
	/** @bus_devfreq: Pointer to the bus devfreq device */
	struct devfreq *bus_devfreq;
	/** @devfreq_enabled: Whether or not devfreq is enabled */
	bool devfreq_enabled;
};

/**
 * kgsl_pwrscale_init - Initialize the pwrscale subsystem
 * @device: A GPU device handle
 * @pdev: A pointer to the GPU platform device
 * @governor: default devfreq governor to use for GPU frequency scaling
 *
 * Return: 0 on success or negative on failure
 */
int kgsl_pwrscale_init(struct kgsl_device *device, struct platform_device *pdev,
		const char *governor);
void kgsl_pwrscale_close(struct kgsl_device *device);

void kgsl_pwrscale_update(struct kgsl_device *device);
void kgsl_pwrscale_update_stats(struct kgsl_device *device);
void kgsl_pwrscale_busy(struct kgsl_device *device);
void kgsl_pwrscale_sleep(struct kgsl_device *device);
void kgsl_pwrscale_wake(struct kgsl_device *device);

void kgsl_pwrscale_enable(struct kgsl_device *device);
void kgsl_pwrscale_disable(struct kgsl_device *device, bool turbo);

int kgsl_devfreq_target(struct device *dev, unsigned long *freq, u32 flags);
int kgsl_devfreq_get_dev_status(struct device *dev,
			struct devfreq_dev_status *stat);
int kgsl_devfreq_get_cur_freq(struct device *dev, unsigned long *freq);

int kgsl_busmon_target(struct device *dev, unsigned long *freq, u32 flags);
int kgsl_busmon_get_dev_status(struct device *dev,
			struct devfreq_dev_status *stat);
int kgsl_busmon_get_cur_freq(struct device *dev, unsigned long *freq);

int msm_adreno_tz_init(void);

void msm_adreno_tz_exit(void);

int devfreq_gpubw_init(void);

void devfreq_gpubw_exit(void);
#endif
